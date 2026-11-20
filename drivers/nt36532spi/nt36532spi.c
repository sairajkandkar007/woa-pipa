/*
 * nt36532spi.c - pipa touch SPI, stage 1: own GENI SPI (FIFO, polled) on QUP0 SE4 + NT36532 probe.
 *
 * The Kona qcspi8250.sys cannot be used on pipa: in AddDevice it maps a hard-coded 0x03900000
 * (SM8150-style TLMM tile), which on SM8250 is unpowered LPASS space -> CPU stall + watchdog.
 *
 * This driver binds to ACPI\QCOM250F (\_SB.SPI4, repurposed to SE4 @0x990000 by make_pipa_k.py
 * --pipa-spi4). QualcommPEP handles the device's clocks, bus votes and pins (gpio8-11 qup4) on D0.
 * Register programming follows Linux drivers/spi/spi-geni-qcom.c + drivers/soc/qcom/qcom-geni-se.c.
 *
 * Stage 1: init the SE for SPI mode 0, 8-bit words, FIFO mode, ~1 MHz, then read a few NT36532
 * registers using the Novatek SPI protocol (Linux nt36xxx: page select 0xFF, read = addr & 0x7F,
 * one dummy byte). Everything is logged with DbgPrint ("nt365:"). No HID yet.
 */
#include <ntddk.h>
#include <wdf.h>

#define LOG(fmt, ...) DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "nt365: " fmt "\n", __VA_ARGS__)

/* GENI common */
#define GENI_FORCE_DEFAULT_REG   0x020
#define GENI_OUTPUT_CTRL         0x024
#define GENI_CGC_CTRL            0x028
#define GENI_STATUS              0x040
#define GENI_SER_M_CLK_CFG       0x048
#define GENI_SER_S_CLK_CFG       0x04C
#define GENI_FW_REVISION_RO      0x068
#define GENI_CLK_SEL             0x07C
#define SE_GENI_BYTE_GRAN        0x254
#define SE_GENI_DMA_MODE_EN      0x258
#define SE_GENI_TX_PACKING_CFG0  0x260
#define SE_GENI_TX_PACKING_CFG1  0x264
#define SE_GENI_RX_PACKING_CFG0  0x284
#define SE_GENI_RX_PACKING_CFG1  0x288
#define SE_GENI_M_CMD0           0x600
#define SE_GENI_M_CMD_CTRL_REG   0x604
#define SE_GENI_M_IRQ_STATUS     0x610
#define SE_GENI_M_IRQ_EN         0x614
#define SE_GENI_M_IRQ_CLEAR      0x618
#define SE_GENI_S_IRQ_CLEAR      0x648
#define SE_GENI_TX_FIFOn         0x700
#define SE_GENI_RX_FIFOn         0x780
#define SE_GENI_TX_FIFO_STATUS   0x800
#define SE_GENI_RX_FIFO_STATUS   0x804
#define SE_GENI_TX_WATERMARK_REG 0x80C
#define SE_GENI_RX_WATERMARK_REG 0x810
#define SE_GENI_RX_RFR_WATERMARK_REG 0x814
#define SE_GSI_EVENT_EN          0xE18
#define SE_IRQ_EN                0xE1C
#define SE_HW_PARAM_0            0xE24
#define SE_DMA_GENERAL_CFG       0xE30
#define SE_DMA_TX_IRQ_CLR        0xC44
#define SE_DMA_RX_IRQ_CLR        0xD44
/* SPI */
#define SE_SPI_CPHA              0x224
#define SE_SPI_LOOPBACK          0x22C
#define SE_SPI_CPOL              0x230
#define SE_SPI_DEMUX_OUTPUT_INV  0x24C
#define SE_SPI_DEMUX_SEL         0x250
#define SE_SPI_TRANS_CFG         0x25C
#define SE_SPI_WORD_LEN          0x268
#define SE_SPI_TX_TRANS_LEN      0x26C
#define SE_SPI_RX_TRANS_LEN      0x270
#define SE_SPI_PRE_POST_CMD_DLY  0x274

#define M_CMD_DONE        (1u << 0)
#define M_CMD_ERR_MASK    (0x3Eu)        /* overrun, illegal, failure, cancel, abort */
#define M_RX_FIFO_WM      (1u << 26)
#define M_RX_FIFO_LAST    (1u << 27)
#define M_TX_FIFO_WM      (1u << 30)
#define SPI_FULL_DUPLEX   3u
#define RX_FIFO_WC_MSK    0x01FFFFFFu
#define RX_LAST           (1u << 31)
#define RX_LAST_BYTE_VALID_SHIFT 28
#define MAX_XFER          60u            /* stay inside one 16-word FIFO */

typedef struct _DEV_CTX {
    PUCHAR se;
    ULONG seLen;
    BOOLEAN inited;
} DEV_CTX, *PDEV_CTX;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEV_CTX, GetCtx)

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD NtDeviceAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE NtPrepare;
EVT_WDF_DEVICE_RELEASE_HARDWARE NtRelease;
EVT_WDF_DEVICE_D0_ENTRY NtD0Entry;

static __forceinline ULONG rd(PDEV_CTX c, ULONG o) { return READ_REGISTER_ULONG((volatile ULONG *)(c->se + o)); }
static __forceinline VOID wr(PDEV_CTX c, ULONG o, ULONG v) { WRITE_REGISTER_ULONG((volatile ULONG *)(c->se + o), v); }

/* Linux geni_se_config_packing(se, 8, 4, msb_to_lsb=true, tx, rx) */
static VOID GeniPacking8(PDEV_CTX c)
{
    ULONG cfg[4] = { 0 }, i, idx = 7, cfg0, cfg1;
    for (i = 0; i < 4; i++) {
        cfg[i] = (idx << 5) | (1u << 4) | (7u << 1);
        idx = ((i + 1) * 8) + 7;
    }
    cfg[3] |= 1u;
    cfg0 = cfg[0] | (cfg[1] << 10);
    cfg1 = cfg[2] | (cfg[3] << 10);
    wr(c, SE_GENI_TX_PACKING_CFG0, cfg0); wr(c, SE_GENI_TX_PACKING_CFG1, cfg1);
    wr(c, SE_GENI_RX_PACKING_CFG0, cfg0); wr(c, SE_GENI_RX_PACKING_CFG1, cfg1);
    wr(c, SE_GENI_BYTE_GRAN, 0);  /* bpw / 16 */
}

static NTSTATUS GeniInit(PDEV_CTX c)
{
    ULONG fw = rd(c, GENI_FW_REVISION_RO), proto = (fw >> 8) & 0xFF, hw0 = rd(c, SE_HW_PARAM_0);
    LOG("SE fw_rev=%08X proto=%lu hw_param0=%08X tx_fifo_depth=%lu status=%08X", fw, proto, hw0, (hw0 >> 16) & 0x3F, rd(c, GENI_STATUS));
    if (proto != 1) { LOG("SE is not loaded with SPI firmware - refusing"); return STATUS_DEVICE_CONFIGURATION_ERROR; }
    /* geni_se_io_init */
    wr(c, GENI_CGC_CTRL, rd(c, GENI_CGC_CTRL) | 0x7F);
    wr(c, SE_DMA_GENERAL_CFG, rd(c, SE_DMA_GENERAL_CFG) | 0xFu /* DMA_RX|DMA_TX_CLK_CGC_ON, DMA_AHB_SLV_CFG_ON, AHB_SEC_SLV_CLK_CGC_ON */);
    wr(c, GENI_OUTPUT_CTRL, 0x7F);
    wr(c, GENI_FORCE_DEFAULT_REG, 1);
    /* irq clear + FIFO mode */
    wr(c, SE_GENI_M_IRQ_CLEAR, 0xFFFFFFFF); wr(c, SE_GENI_S_IRQ_CLEAR, 0xFFFFFFFF);
    wr(c, SE_DMA_TX_IRQ_CLR, 0xFFFFFFFF); wr(c, SE_DMA_RX_IRQ_CLR, 0xFFFFFFFF);
    wr(c, SE_GENI_RX_WATERMARK_REG, 1);
    wr(c, SE_GENI_RX_RFR_WATERMARK_REG, 14);
    wr(c, SE_GSI_EVENT_EN, 0);
    wr(c, SE_GENI_DMA_MODE_EN, rd(c, SE_GENI_DMA_MODE_EN) & ~1u);
    wr(c, SE_IRQ_EN, rd(c, SE_IRQ_EN) | (1u << 2) | (1u << 3));
    wr(c, SE_GENI_M_IRQ_EN, rd(c, SE_GENI_M_IRQ_EN) | M_CMD_DONE | M_CMD_ERR_MASK | M_RX_FIFO_WM | M_RX_FIFO_LAST | M_TX_FIFO_WM);
    /* SPI mode 0, 8-bit words, CS0, no loopback */
    wr(c, SE_SPI_LOOPBACK, 0);
    wr(c, SE_SPI_DEMUX_SEL, 0);
    wr(c, SE_SPI_DEMUX_OUTPUT_INV, 0);
    wr(c, SE_SPI_CPHA, 0);
    wr(c, SE_SPI_CPOL, 0);
    wr(c, SE_SPI_WORD_LEN, 8 - 4);
    wr(c, SE_SPI_TRANS_CFG, rd(c, SE_SPI_TRANS_CFG) & ~(1u << 1));  /* no CS toggle between words */
    wr(c, SE_SPI_PRE_POST_CMD_DLY, 0);
    GeniPacking8(c);
    /* clock: DFS level 0 (XO 19.2 MHz), divider 20 -> ~0.96 MHz (or half with oversampling 2) */
    wr(c, GENI_CLK_SEL, 0);
    wr(c, GENI_SER_M_CLK_CFG, (20u << 4) | 1u);
    wr(c, GENI_SER_S_CLK_CFG, (20u << 4) | 1u);
    LOG("SE init done: m_clk=%08X trans_cfg=%08X word_len=%08X", rd(c, GENI_SER_M_CLK_CFG), rd(c, SE_SPI_TRANS_CFG), rd(c, SE_SPI_WORD_LEN));
    return STATUS_SUCCESS;
}

/* full-duplex, len <= MAX_XFER, polled; CS asserted for the whole transfer */
static NTSTATUS GeniXfer(PDEV_CTX c, const UCHAR *tx, UCHAR *rx, ULONG len)
{
    ULONG words = (len + 3) / 4, i, got = 0, st, spin;
    if (len == 0 || len > MAX_XFER) return STATUS_INVALID_PARAMETER;
    wr(c, SE_GENI_M_IRQ_CLEAR, 0xFFFFFFFF);
    wr(c, SE_SPI_TX_TRANS_LEN, len);
    wr(c, SE_SPI_RX_TRANS_LEN, len);
    wr(c, SE_GENI_M_CMD0, (SPI_FULL_DUPLEX << 27) | 0);
    for (i = 0; i < words; i++) {
        ULONG w = 0, b;
        for (b = 0; b < 4 && i * 4 + b < len; b++) w |= (ULONG)tx[i * 4 + b] << (8 * b);
        wr(c, SE_GENI_TX_FIFOn, w);
    }
    for (spin = 0; spin < 100000; spin++) {
        st = rd(c, SE_GENI_M_IRQ_STATUS);
        if (st & (M_RX_FIFO_WM | M_RX_FIFO_LAST)) {
            ULONG fs = rd(c, SE_GENI_RX_FIFO_STATUS), wc = fs & RX_FIFO_WC_MSK, k;
            for (k = 0; k < wc; k++) {
                ULONG w = rd(c, SE_GENI_RX_FIFOn), b;
                for (b = 0; b < 4 && got < len; b++, got++) rx[got] = (UCHAR)(w >> (8 * b));
            }
            wr(c, SE_GENI_M_IRQ_CLEAR, st & (M_RX_FIFO_WM | M_RX_FIFO_LAST));
        }
        if (st & M_TX_FIFO_WM) { wr(c, SE_GENI_TX_WATERMARK_REG, 0); wr(c, SE_GENI_M_IRQ_CLEAR, M_TX_FIFO_WM); }
        if (st & M_CMD_ERR_MASK) { LOG("xfer error m_irq=%08X", st); wr(c, SE_GENI_M_IRQ_CLEAR, st); return STATUS_IO_DEVICE_ERROR; }
        if ((st & M_CMD_DONE) && got >= len) { wr(c, SE_GENI_M_IRQ_CLEAR, st); return STATUS_SUCCESS; }
        KeStallExecutionProcessor(2);
    }
    LOG("xfer timeout m_irq=%08X rx_status=%08X tx_status=%08X geni_status=%08X got=%lu",
        rd(c, SE_GENI_M_IRQ_STATUS), rd(c, SE_GENI_RX_FIFO_STATUS), rd(c, SE_GENI_TX_FIFO_STATUS), rd(c, GENI_STATUS), got);
    /* cancel the command so the SE is idle again */
    wr(c, SE_GENI_M_CMD_CTRL_REG, 1u << 2);
    KeStallExecutionProcessor(100);
    wr(c, SE_GENI_M_IRQ_CLEAR, 0xFFFFFFFF);
    return STATUS_IO_TIMEOUT;
}

/* Novatek: page select then read n bytes at addr (Linux nvt_set_page + CTP_SPI_READ) */
static NTSTATUS NvtRead(PDEV_CTX c, ULONG addr, UCHAR *out, ULONG n)
{
    UCHAR tx[MAX_XFER] = { 0 }, rx[MAX_XFER] = { 0 };
    NTSTATUS st;
    if (n + 2 > MAX_XFER) return STATUS_INVALID_PARAMETER;
    tx[0] = 0xFF | 0x80; tx[1] = (UCHAR)((addr >> 15) & 0xFF); tx[2] = (UCHAR)((addr >> 7) & 0xFF);
    st = GeniXfer(c, tx, rx, 3);
    if (!NT_SUCCESS(st)) return st;
    RtlZeroMemory(tx, sizeof(tx));
    tx[0] = (UCHAR)(addr & 0x7F);
    st = GeniXfer(c, tx, rx, n + 2);           /* cmd, dummy, data... */
    if (NT_SUCCESS(st)) RtlCopyMemory(out, rx + 2, n);
    return st;
}

static NTSTATUS NvtWrite1(PDEV_CTX c, ULONG addr, UCHAR v)
{
    UCHAR tx[4] = { 0xFF | 0x80, (UCHAR)((addr >> 15) & 0xFF), (UCHAR)((addr >> 7) & 0xFF), 0 }, rx[4];
    NTSTATUS st = GeniXfer(c, tx, rx, 3);
    if (!NT_SUCCESS(st)) return st;
    tx[0] = (UCHAR)((addr & 0x7F) | 0x80); tx[1] = v;
    return GeniXfer(c, tx, rx, 2);
}

static VOID Dump(PCSTR what, ULONG addr, const UCHAR *b, ULONG n, NTSTATUS st)
{
    LOG("%s @%05lX st=%08X: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
        what, addr, st, b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7], b[8], b[9], b[10], b[11], b[12], b[13], b[14], b[15]);
    UNREFERENCED_PARAMETER(n);
}

NTSTATUS DriverEntry(PDRIVER_OBJECT drv, PUNICODE_STRING path)
{
    WDF_DRIVER_CONFIG cfg;
    WDF_DRIVER_CONFIG_INIT(&cfg, NtDeviceAdd);
    return WdfDriverCreate(drv, path, WDF_NO_OBJECT_ATTRIBUTES, &cfg, WDF_NO_HANDLE);
}

NTSTATUS NtDeviceAdd(WDFDRIVER drv, PWDFDEVICE_INIT init)
{
    WDF_PNPPOWER_EVENT_CALLBACKS pnp; WDF_OBJECT_ATTRIBUTES a; WDFDEVICE dev;
    UNREFERENCED_PARAMETER(drv);
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnp);
    pnp.EvtDevicePrepareHardware = NtPrepare;
    pnp.EvtDeviceReleaseHardware = NtRelease;
    pnp.EvtDeviceD0Entry = NtD0Entry;
    WdfDeviceInitSetPnpPowerEventCallbacks(init, &pnp);
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&a, DEV_CTX);
    return WdfDeviceCreate(&init, &a, &dev);
}

NTSTATUS NtPrepare(WDFDEVICE dev, WDFCMRESLIST raw, WDFCMRESLIST xl)
{
    PDEV_CTX c = GetCtx(dev); ULONG i;
    UNREFERENCED_PARAMETER(raw);
    for (i = 0; i < WdfCmResourceListGetCount(xl); i++) {
        PCM_PARTIAL_RESOURCE_DESCRIPTOR r = WdfCmResourceListGetDescriptor(xl, i);
        if (r->Type == CmResourceTypeMemory && !c->se) {
            if (r->u.Memory.Start.QuadPart != 0x990000) {   /* only pipa's SE4 - never a Kona MTP DSDT SPI4 */
                LOG("SE mem %I64X is not pipa SE4 (0x990000) - not touching it (old/unpatched DSDT?)", r->u.Memory.Start.QuadPart);
                return STATUS_SUCCESS;
            }
            c->seLen = r->u.Memory.Length;
            c->se = (PUCHAR)MmMapIoSpaceEx(r->u.Memory.Start, c->seLen, PAGE_READWRITE | PAGE_NOCACHE);
            LOG("SE mem %I64X len %lX mapped=%p", r->u.Memory.Start.QuadPart, c->seLen, c->se);
        }
    }
    return c->se ? STATUS_SUCCESS : STATUS_INSUFFICIENT_RESOURCES;
}

NTSTATUS NtRelease(WDFDEVICE dev, WDFCMRESLIST xl)
{
    PDEV_CTX c = GetCtx(dev); UNREFERENCED_PARAMETER(xl);
    if (c->se) { MmUnmapIoSpace(c->se, c->seLen); c->se = NULL; }
    return STATUS_SUCCESS;
}

NTSTATUS NtD0Entry(WDFDEVICE dev, WDF_POWER_DEVICE_STATE prev)
{
    PDEV_CTX c = GetCtx(dev);
    static const ULONG addrs[] = { 0x3F004 /* chip ver trim (nt36xxx) */, 0x3F000, 0x21C00 /* event buf (nt36xxx default) */ };
    UCHAR b[16]; ULONG i; NTSTATUS st;
    UNREFERENCED_PARAMETER(prev);
    LOG("D0 entry (from D%d)", (int)prev - 1);
    if (!c->se) return STATUS_SUCCESS;
    if (!NT_SUCCESS(GeniInit(c))) return STATUS_SUCCESS;   /* never fail start during bring-up */
    c->inited = TRUE;
    /* 1) controller self-test: internal loopback, rx must equal tx */
    {
        UCHAR tx[8] = { 0xA5, 0x5A, 0x01, 0x80, 0x3C, 0xC3, 0xFF, 0x00 }, rx[8] = { 0 };
        wr(c, SE_SPI_LOOPBACK, 1);
        st = GeniXfer(c, tx, rx, 8);
        wr(c, SE_SPI_LOOPBACK, 0);
        LOG("loopback st=%08X rx=%02X %02X %02X %02X %02X %02X %02X %02X (%s)", st, rx[0], rx[1], rx[2], rx[3], rx[4], rx[5], rx[6], rx[7],
            RtlCompareMemory(tx, rx, 8) == 8 ? "MATCH" : "mismatch");
    }
    /* 2) plain reads */
    for (i = 0; i < RTL_NUMBER_OF(addrs); i++) {
        RtlZeroMemory(b, sizeof(b));
        st = NvtRead(c, addrs[i], b, 16);
        Dump("read", addrs[i], b, 16, st);
    }
    /* 3) Linux nvt_ts_check_chip_ver_trim: bootloader reset (0x69 -> SWRST_N8 0x3F0FE), then trim id */
    for (i = 0; i < 3; i++) {
        st = NvtWrite1(c, 0x3F0FE, 0x69);
        KeStallExecutionProcessor(10000);
        RtlZeroMemory(b, sizeof(b));
        if (NT_SUCCESS(st)) st = NvtRead(c, 0x3F004, b, 16);
        Dump("trim-after-reset", 0x3F004, b, 16, st);
        KeStallExecutionProcessor(10000);
    }
    return STATUS_SUCCESS;
}
