/*
 * pipapwr.c - Xiaomi Pad 6 (pipa) radio power: QCA6390 enable lines + BT UART pins.
 * Binds to ACPI\PIPA6390 (\_SB.WPWR, appended by make_pipa_k.py --pipa-wpwr).
 *
 * From pipa-mainline sm8250-xiaomi-pipa.dts (qca6390-pmu):
 *   wlan-enable-gpios = <&tlmm 20 GPIO_ACTIVE_HIGH>;  bt-enable-gpios = <&tlmm 21 GPIO_ACTIVE_HIGH>;
 *   pinctrl: function gpio, drive-strength 16, bias-pull-up.
 * uart6 (BT HCI, QUP0 SE6 @0x998000): gpio16-19 function qup6.
 * PMU supply rails (S4A/S5A/S6A/S8C, L16A) are expected on from boot; not voted here.
 * Stage 2 (BT): Qualcomm's qcuart cannot open uart6 here (its clock comes from a qcpep-private P-state
 * interface our PEP lacks: open -> STATUS_DEVICE_NOT_READY). So after power-on this driver drives the
 * GENI UART itself in polled FIFO mode (Linux qcom_geni_serial.c / qcom-geni-se.c): votes
 * gcc_qupv3_wrap0_s6_clk (GCC 0x152008 bit 16), 115200 8N1, sends H4 HCI_Reset and the QCA version
 * request (btqca.c EDL_PATCH_VER_REQ 0x19 via opcode 0xFC00) and logs the reply bytes.
 * TLMM north tile 0x0F900000, ctl = 0x1000*pin: [1:0] pull, [5:2] func, [8:6] drive, [9] OE; io: [0] in, [1] out.
 */
#include <ntddk.h>
#include <wdf.h>

#define LOG(fmt, ...) DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "pipapwr: " fmt "\n", __VA_ARGS__)
#define TLMM_NORTH 0x0F900000ull
#define PULL_UP    3u
#define DRV_16MA   7u

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD PwrDeviceAdd;
EVT_WDF_DEVICE_D0_ENTRY PwrD0Entry;

static PUCHAR Map(ULONG pin)
{
    PHYSICAL_ADDRESS a; a.QuadPart = TLMM_NORTH + 0x1000ull * pin;
    return (PUCHAR)MmMapIoSpaceEx(a, 0x1000, PAGE_READWRITE | PAGE_NOCACHE);
}
static VOID Show(PCSTR n, ULONG pin)
{
    PUCHAR p = Map(pin); if (!p) return;
    LOG("gpio%lu (%s) ctl=%08X io=%08X", pin, n, READ_REGISTER_ULONG((volatile ULONG *)p), READ_REGISTER_ULONG((volatile ULONG *)(p + 4)));
    MmUnmapIoSpace(p, 0x1000);
}
static VOID SetFunc(ULONG pin, ULONG func, ULONG pull, ULONG drv)
{
    PUCHAR p = Map(pin); if (!p) return;
    WRITE_REGISTER_ULONG((volatile ULONG *)p, pull | (func << 2) | (drv << 6));
    MmUnmapIoSpace(p, 0x1000);
}
static VOID SetOut(ULONG pin, ULONG v)
{
    PUCHAR p = Map(pin); if (!p) return;
    WRITE_REGISTER_ULONG((volatile ULONG *)(p + 4), v ? 2u : 0u);
    WRITE_REGISTER_ULONG((volatile ULONG *)p, PULL_UP | (0u << 2) | (DRV_16MA << 6) | (1u << 9));
    MmUnmapIoSpace(p, 0x1000);
}


/* ---------------- GENI UART (QUP0 SE6 @0x998000) polled probe ---------------- */
#define SE6_BASE        0x00998000ull
#define QUP0_WRAP       0x009C0000ull
#define GCC_VOTE_PAGE   0x00152000ull
#define GCC_RCG_PAGE    0x00117000ull
#define R(o)            READ_REGISTER_ULONG((volatile ULONG *)(se + (o)))
#define W(o, v)         WRITE_REGISTER_ULONG((volatile ULONG *)(se + (o)), (v))
#define GENI_FORCE_DEFAULT_REG 0x20
#define GENI_OUTPUT_CTRL 0x24
#define GENI_CGC_CTRL   0x28
#define SE_GENI_STATUS  0x40
#define GENI_SER_M_CLK_CFG 0x48
#define GENI_SER_S_CLK_CFG 0x4c
#define GENI_FW_REVISION_RO 0x68
#define SE_GENI_CLK_SEL 0x7c
#define SE_UART_LOOPBACK_CFG 0x22c
#define SE_GENI_DMA_MODE_EN 0x258
#define SE_UART_TX_TRANS_CFG 0x25c
#define SE_GENI_TX_PACKING_CFG0 0x260
#define SE_GENI_TX_PACKING_CFG1 0x264
#define SE_UART_TX_WORD_LEN 0x268
#define SE_UART_TX_STOP_BIT_LEN 0x26c
#define SE_UART_TX_TRANS_LEN 0x270
#define SE_UART_RX_TRANS_CFG 0x280
#define SE_GENI_RX_PACKING_CFG0 0x284
#define SE_GENI_RX_PACKING_CFG1 0x288
#define SE_UART_RX_WORD_LEN 0x28c
#define SE_UART_RX_STALE_CNT 0x294
#define SE_UART_TX_PARITY_CFG 0x2a4
#define SE_UART_RX_PARITY_CFG 0x2a8
#define SE_UART_MANUAL_RFR 0x2ac
#define SE_GENI_M_CMD0  0x600
#define SE_GENI_M_CMD_CTRL_REG 0x604
#define SE_GENI_M_IRQ_STATUS 0x610
#define SE_GENI_M_IRQ_CLEAR 0x618
#define SE_GENI_S_CMD0  0x630
#define SE_GENI_S_CMD_CTRL_REG 0x634
#define SE_GENI_S_IRQ_STATUS 0x640
#define SE_GENI_S_IRQ_CLEAR 0x648
#define SE_GENI_TX_FIFOn 0x700
#define SE_GENI_RX_FIFOn 0x780
#define SE_GENI_TX_FIFO_STATUS 0x800
#define SE_GENI_RX_FIFO_STATUS 0x804
#define SE_GENI_RX_WATERMARK_REG 0x810
#define SE_GENI_RX_RFR_WATERMARK_REG 0x814
#define SE_GENI_IOS     0x908
#define SE_GSI_EVENT_EN 0xe18
#define SE_IRQ_EN       0xe1c
#define SE_HW_PARAM_1   0xe28
#define SE_DMA_GENERAL_CFG 0xe30

static PUCHAR MapPhys(ULONGLONG pa, SIZE_T len)
{
    PHYSICAL_ADDRESS a; a.QuadPart = (LONGLONG)pa;
    return (PUCHAR)MmMapIoSpaceEx(a, len, PAGE_READWRITE | PAGE_NOCACHE);
}

static VOID MsSleep(ULONG ms) { LARGE_INTEGER t; t.QuadPart = -10000LL * ms; KeDelayExecutionThread(KernelMode, FALSE, &t); }

static VOID UartSetup(PUCHAR se, ULONG div)
{
    ULONG v;
    /* stop anything running on either sequencer */
    if (R(SE_GENI_STATUS) & (1u << 12)) { W(SE_GENI_S_CMD_CTRL_REG, 1u << 1); KeStallExecutionProcessor(1000); }
    if (R(SE_GENI_STATUS) & 1u)         { W(SE_GENI_M_CMD_CTRL_REG, 1u << 1); KeStallExecutionProcessor(1000); }
    /* geni_se_init(): irq clear, io init, fifo mode */
    W(SE_GSI_EVENT_EN, 0); W(SE_GENI_M_IRQ_CLEAR, 0xFFFFFFFF); W(SE_GENI_S_IRQ_CLEAR, 0xFFFFFFFF);
    W(GENI_CGC_CTRL, R(GENI_CGC_CTRL) | 0x7F);
    W(SE_DMA_GENERAL_CFG, R(SE_DMA_GENERAL_CFG) | 0xF);
    W(GENI_OUTPUT_CTRL, 0x7F);
    W(GENI_FORCE_DEFAULT_REG, 1);
    W(SE_IRQ_EN, R(SE_IRQ_EN) | 0xC);           /* GENI M/S irq (status only, we poll) */
    W(SE_GENI_DMA_MODE_EN, R(SE_GENI_DMA_MODE_EN) & ~1u);
    /* packing: 8 bit words, 4 per FIFO entry, lsb first (values from geni_se_config_packing) */
    W(SE_GENI_TX_PACKING_CFG0, 0x4380E); W(SE_GENI_TX_PACKING_CFG1, 0xC3E0E);
    W(SE_GENI_RX_PACKING_CFG0, 0x4380E); W(SE_GENI_RX_PACKING_CFG1, 0xC3E0E);
    v = (R(SE_HW_PARAM_1) >> 16) & 0x3F; if (v < 4) v = 16;
    W(SE_GENI_RX_WATERMARK_REG, 2); W(SE_GENI_RX_RFR_WATERMARK_REG, v - 2);
    W(SE_UART_RX_STALE_CNT, 10 * 16);
    /* termios: 8N1, ignore CTS for the probe, RFR automatic */
    W(SE_UART_LOOPBACK_CFG, 0);
    W(SE_UART_TX_TRANS_CFG, (R(SE_UART_TX_TRANS_CFG) & ~1u) | (1u << 1));
    W(SE_UART_RX_TRANS_CFG, R(SE_UART_RX_TRANS_CFG) & ~(1u << 3));
    W(SE_UART_TX_PARITY_CFG, R(SE_UART_TX_PARITY_CFG) & ~1u);
    W(SE_UART_RX_PARITY_CFG, R(SE_UART_RX_PARITY_CFG) & ~1u);
    W(SE_UART_TX_WORD_LEN, 8); W(SE_UART_RX_WORD_LEN, 8); W(SE_UART_TX_STOP_BIT_LEN, 0);
    W(SE_UART_MANUAL_RFR, 0);
    W(SE_GENI_CLK_SEL, 0);
    W(GENI_SER_M_CLK_CFG, 1u | (div << 4)); W(GENI_SER_S_CLK_CFG, 1u | (div << 4));
    /* start the receiver */
    W(SE_GENI_S_CMD0, 1u << 27);
}

static VOID UartTx(PUCHAR se, const UCHAR *b, ULONG n)
{
    ULONG i, w, t;
    W(SE_GENI_M_IRQ_CLEAR, 0xFFFFFFFF);
    W(SE_UART_TX_TRANS_LEN, n);
    W(SE_GENI_M_CMD0, 1u << 27);
    for (i = 0; i < n; i += 4) {
        w = 0; for (t = 0; t < 4 && i + t < n; t++) w |= (ULONG)b[i + t] << (8 * t);
        W(SE_GENI_TX_FIFOn, w);
    }
    for (t = 0; t < 1000 && !(R(SE_GENI_M_IRQ_STATUS) & 1u); t++) KeStallExecutionProcessor(100);
    LOG("tx %lu bytes m_irq=%08X status=%08X ios=%08X", n, R(SE_GENI_M_IRQ_STATUS), R(SE_GENI_STATUS), R(SE_GENI_IOS));
    if (!(R(SE_GENI_M_IRQ_STATUS) & 1u)) { W(SE_GENI_M_CMD_CTRL_REG, 1u << 2); KeStallExecutionProcessor(1000); }
    W(SE_GENI_M_IRQ_CLEAR, 0xFFFFFFFF);
}

static ULONG UartRx(PUCHAR se, UCHAR *out, ULONG cap, ULONG ms)
{
    ULONG got = 0, el, st, wc, i, k, nb, w;
    for (el = 0; el < ms && got < cap; ) {
        st = R(SE_GENI_RX_FIFO_STATUS);
        wc = st & 0x1FFFFFF;
        if (!wc) { MsSleep(5); el += 5; continue; }
        for (i = 0; i < wc && got < cap; i++) {
            w = R(SE_GENI_RX_FIFOn);
            nb = 4;
            if (i == wc - 1 && (st & 0x80000000u)) { nb = (st >> 28) & 7; if (!nb) nb = 4; }
            for (k = 0; k < nb && got < cap; k++) out[got++] = (UCHAR)(w >> (8 * k));
        }
        W(SE_GENI_S_IRQ_CLEAR, 0xFFFFFFFF);
    }
    return got;
}

static VOID Dump(PCSTR what, const UCHAR *b, ULONG n)
{
    ULONG i;
    LOG("%s: %lu bytes", what, n);
    for (i = 0; i < n; i += 8)
        LOG("  %02X %02X %02X %02X %02X %02X %02X %02X", b[i], i+1<n?b[i+1]:0, i+2<n?b[i+2]:0, i+3<n?b[i+3]:0,
            i+4<n?b[i+4]:0, i+5<n?b[i+5]:0, i+6<n?b[i+6]:0, i+7<n?b[i+7]:0);
}

static VOID BtProbe(VOID)
{
    static const UCHAR reset[] = { 0x01, 0x03, 0x0C, 0x00 };
    static const UCHAR ver[] = { 0x01, 0x00, 0xFC, 0x01, 0x19 };
    PUCHAR gcc, rcg, wrap, se; ULONG qver, samp, div, n, fw;
    UCHAR rx[96];

    gcc = MapPhys(GCC_VOTE_PAGE, 0x1000);
    if (!gcc) { LOG("bt: map gcc failed"); return; }
    n = READ_REGISTER_ULONG((volatile ULONG *)(gcc + 0x8));
    WRITE_REGISTER_ULONG((volatile ULONG *)(gcc + 0x8), n | (1u << 16));   /* gcc_qupv3_wrap0_s6_clk */
    KeStallExecutionProcessor(100);
    LOG("bt: gcc vote 52008 %08X -> %08X", n, READ_REGISTER_ULONG((volatile ULONG *)(gcc + 0x8)));
    MmUnmapIoSpace(gcc, 0x1000);
    rcg = MapPhys(GCC_RCG_PAGE, 0x1000);
    if (rcg) {
        LOG("bt: s6 rcg cmd=%08X cfg=%08X dfs0=%08X dfs1=%08X", READ_REGISTER_ULONG((volatile ULONG *)(rcg + 0x730)),
            READ_REGISTER_ULONG((volatile ULONG *)(rcg + 0x734)), READ_REGISTER_ULONG((volatile ULONG *)(rcg + 0x74C)),
            READ_REGISTER_ULONG((volatile ULONG *)(rcg + 0x750)));
        MmUnmapIoSpace(rcg, 0x1000);
    }
    wrap = MapPhys(QUP0_WRAP, 0x1000);
    qver = wrap ? READ_REGISTER_ULONG((volatile ULONG *)(wrap + 0x4)) : 0;
    if (wrap) MmUnmapIoSpace(wrap, 0x1000);
    samp = (qver >= 0x20050000) ? 16 : 32;
    div = 7372800 / (115200 * samp);              /* DFS level 0 = 7.3728 MHz (gcc-sm8250 ftbl) */
    se = MapPhys(SE6_BASE, 0x4000);
    if (!se) { LOG("bt: map se6 failed"); return; }
    fw = R(GENI_FW_REVISION_RO);
    LOG("bt: qup hw ver %08X sampling %lu div %lu; se6 fw=%08X status=%08X ios=%08X", qver, samp, div, fw, R(SE_GENI_STATUS), R(SE_GENI_IOS));
    if (((fw >> 8) & 0xFF) != 2) { LOG("bt: SE6 is not UART firmware - abort"); MmUnmapIoSpace(se, 0x4000); return; }
    UartSetup(se, div);
    n = UartRx(se, rx, sizeof(rx), 50);           /* drain anything pending */
    if (n) Dump("stale rx", rx, n);
    UartTx(se, reset, sizeof(reset));
    n = UartRx(se, rx, sizeof(rx), 1000); Dump("hci_reset reply", rx, n);
    UartTx(se, ver, sizeof(ver));
    n = UartRx(se, rx, sizeof(rx), 1000); Dump("qca_version reply", rx, n);
    LOG("bt: done s_irq=%08X rxfifo=%08X ios=%08X", R(SE_GENI_S_IRQ_STATUS), R(SE_GENI_RX_FIFO_STATUS), R(SE_GENI_IOS));
    MmUnmapIoSpace(se, 0x4000);
}

NTSTATUS DriverEntry(PDRIVER_OBJECT drv, PUNICODE_STRING path)
{
    WDF_DRIVER_CONFIG cfg;
    WDF_DRIVER_CONFIG_INIT(&cfg, PwrDeviceAdd);
    return WdfDriverCreate(drv, path, WDF_NO_OBJECT_ATTRIBUTES, &cfg, WDF_NO_HANDLE);
}

NTSTATUS PwrDeviceAdd(WDFDRIVER drv, PWDFDEVICE_INIT init)
{
    WDF_PNPPOWER_EVENT_CALLBACKS pnp; WDFDEVICE dev;
    UNREFERENCED_PARAMETER(drv);
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnp);
    pnp.EvtDeviceD0Entry = PwrD0Entry;
    WdfDeviceInitSetPnpPowerEventCallbacks(init, &pnp);
    return WdfDeviceCreate(&init, WDF_NO_OBJECT_ATTRIBUTES, &dev);
}

NTSTATUS PwrD0Entry(WDFDEVICE dev, WDF_POWER_DEVICE_STATE prev)
{
    ULONG i;
    UNREFERENCED_PARAMETER(dev);
    LOG("D0 entry (from D%d)", (int)prev - 1);
    Show("wlan_en", 20); Show("bt_en", 21);
    for (i = 16; i <= 19; i++) Show("uart6", i);
    for (i = 16; i <= 19; i++) SetFunc(i, 1 /* qup6 */, PULL_UP, 1 /* 4 mA */);
    SetOut(21, 1);                         /* BT_EN */
    KeStallExecutionProcessor(20000);
    SetOut(20, 1);                         /* WLAN_EN */
    KeStallExecutionProcessor(20000);
    Show("wlan_en", 20); Show("bt_en", 21);
    for (i = 16; i <= 19; i++) Show("uart6", i);
    /* BT_EN low->high so the QCA6390 BT core boots fresh, then talk to it */
    SetOut(21, 0); MsSleep(50); SetOut(21, 1); MsSleep(150);
    BtProbe();
    return STATUS_SUCCESS;
}
