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
 * Stage 2: NT36532 is flash-less. At D0 the driver loads the touch firmware from
 * \SystemRoot\System32\drivers\nt36532_{csot,tianma}.bin (copied there by phase10, never shipped
 * in the repo), downloads it (port of pipa-mainline nt36xxx_fw_update.c, hw-CRC + cascade path),
 * waits for the FW reset state, reads FW info, then polls the event buffer every 20 ms and logs
 * finger reports ("nt365: touch ..."). No HID yet.
 */
#include <ntddk.h>
#include <wdf.h>
#include <hidport.h>

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
#define SPI_TX_ONLY       1u
#define TX_FIFO_WC_MSK    0x0FFFFFFFu
#define FIFO_DEPTH        16u
#define NVT_CHUNK         1024u          /* bytes per page-set when writing SRAM */
#define MAX_RD            128u

typedef struct _DEV_CTX {
    PUCHAR se;
    ULONG seLen;
    BOOLEAN inited;
    BOOLEAN fwOk;
    volatile BOOLEAN polling;
    WDFTIMER poll;
    ULONG lastFingers;
    ULONG logBudget;
    USHORT absX, absY;
    WDFQUEUE readQ;
    USHORT scan;
    BOOLEAN sentUp;
    UCHAR lastCid[10];
} DEV_CTX, *PDEV_CTX;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEV_CTX, GetCtx)

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD NtDeviceAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE NtPrepare;
EVT_WDF_DEVICE_RELEASE_HARDWARE NtRelease;
EVT_WDF_DEVICE_D0_ENTRY NtD0Entry;
EVT_WDF_DEVICE_D0_EXIT NtD0Exit;
EVT_WDF_TIMER NtPoll;
EVT_WDF_IO_QUEUE_IO_INTERNAL_DEVICE_CONTROL NtIoctl;

static __forceinline ULONG rd(PDEV_CTX c, ULONG o) { return READ_REGISTER_ULONG((volatile ULONG *)(c->se + o)); }

static __forceinline VOID wr(PDEV_CTX c, ULONG o, ULONG v) { WRITE_REGISTER_ULONG((volatile ULONG *)(c->se + o), v); }
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
    /* clock: DFS level 0 (XO 19.2 MHz), divider 2 */
    wr(c, GENI_CLK_SEL, 0);
    wr(c, GENI_SER_M_CLK_CFG, (2u << 4) | 1u);   /* 19.2 MHz / 2 = 9.6 MHz (Android uses 9.6) */
    wr(c, GENI_SER_S_CLK_CFG, (2u << 4) | 1u);
    LOG("SE init done: m_clk=%08X trans_cfg=%08X word_len=%08X", rd(c, GENI_SER_M_CLK_CFG), rd(c, SE_SPI_TRANS_CFG), rd(c, SE_SPI_WORD_LEN));
    return STATUS_SUCCESS;
}

/* Streaming polled transfer of any length. rx == NULL -> TX only. CS held for the whole transfer. */
static NTSTATUS GeniXfer(PDEV_CTX c, const UCHAR *tx, UCHAR *rx, ULONG len)
{
    ULONG txw = 0, words = (len + 3) / 4, got = 0, st, spin = 0;
    if (len == 0) return STATUS_INVALID_PARAMETER;
    wr(c, SE_GENI_M_IRQ_CLEAR, 0xFFFFFFFF);
    wr(c, SE_SPI_TX_TRANS_LEN, len);
    wr(c, SE_SPI_RX_TRANS_LEN, rx ? len : 0);
    wr(c, SE_GENI_M_CMD0, ((rx ? SPI_FULL_DUPLEX : SPI_TX_ONLY) << 27));
    for (;;) {
        /* top up TX FIFO */
        while (txw < words && (rd(c, SE_GENI_TX_FIFO_STATUS) & TX_FIFO_WC_MSK) < FIFO_DEPTH - 1) {
            ULONG w = 0, b;
            for (b = 0; b < 4 && txw * 4 + b < len; b++) w |= (ULONG)tx[txw * 4 + b] << (8 * b);
            wr(c, SE_GENI_TX_FIFOn, w); txw++;
        }
        st = rd(c, SE_GENI_M_IRQ_STATUS);
        if (rx) {
            ULONG wc = rd(c, SE_GENI_RX_FIFO_STATUS) & RX_FIFO_WC_MSK, k;
            for (k = 0; k < wc; k++) {
                ULONG w = rd(c, SE_GENI_RX_FIFOn), b;
                for (b = 0; b < 4 && got < len; b++, got++) rx[got] = (UCHAR)(w >> (8 * b));
            }
            if (st & (M_RX_FIFO_WM | M_RX_FIFO_LAST)) wr(c, SE_GENI_M_IRQ_CLEAR, st & (M_RX_FIFO_WM | M_RX_FIFO_LAST));
        }
        if (st & M_TX_FIFO_WM) wr(c, SE_GENI_M_IRQ_CLEAR, M_TX_FIFO_WM);
        if (st & M_CMD_ERR_MASK) { LOG("xfer error m_irq=%08X len=%lu", st, len); wr(c, SE_GENI_M_IRQ_CLEAR, st); return STATUS_IO_DEVICE_ERROR; }
        if ((st & M_CMD_DONE) && txw >= words && (!rx || got >= len)) { wr(c, SE_GENI_M_IRQ_CLEAR, 0xFFFFFFFF); return STATUS_SUCCESS; }
        if (++spin > 2000000) break;
        KeStallExecutionProcessor(1);
    }
    LOG("xfer timeout len=%lu txw=%lu got=%lu m_irq=%08X rx=%08X tx=%08X", len, txw, got,
        rd(c, SE_GENI_M_IRQ_STATUS), rd(c, SE_GENI_RX_FIFO_STATUS), rd(c, SE_GENI_TX_FIFO_STATUS));
    wr(c, SE_GENI_M_CMD_CTRL_REG, 1u << 2);
    KeStallExecutionProcessor(100);
    wr(c, SE_GENI_M_IRQ_CLEAR, 0xFFFFFFFF);
    return STATUS_IO_TIMEOUT;
}

/* ---------------- Novatek SPI protocol (pipa-mainline nt36xxx.c) ---------------- */
static NTSTATUS NvtSetPage(PDEV_CTX c, ULONG addr)
{
    UCHAR tx[3] = { 0xFF | 0x80, (UCHAR)((addr >> 15) & 0xFF), (UCHAR)((addr >> 7) & 0xFF) };
    return GeniXfer(c, tx, NULL, 3);
}
/* write n bytes at the current page, offset (addr & 0x7F) */
static NTSTATUS NvtWriteRaw(PDEV_CTX c, ULONG addr, const UCHAR *d, ULONG n, UCHAR *scratch)
{
    scratch[0] = (UCHAR)((addr & 0x7F) | 0x80);
    RtlCopyMemory(scratch + 1, d, n);
    return GeniXfer(c, scratch, NULL, n + 1);
}
static NTSTATUS NvtWriteAddr(PDEV_CTX c, ULONG addr, UCHAR v)
{
    UCHAR s[2]; NTSTATUS st = NvtSetPage(c, addr);
    return NT_SUCCESS(st) ? NvtWriteRaw(c, addr, &v, 1, s) : st;
}
/* read n bytes at the current page: cmd, dummy, data */
static NTSTATUS NvtReadRaw(PDEV_CTX c, ULONG addr, UCHAR *out, ULONG n)
{
    UCHAR tx[MAX_RD + 2] = { 0 }, rx[MAX_RD + 2] = { 0 }; NTSTATUS st;
    if (n > MAX_RD) return STATUS_INVALID_PARAMETER;
    tx[0] = (UCHAR)(addr & 0x7F);
    st = GeniXfer(c, tx, rx, n + 2);
    if (NT_SUCCESS(st)) RtlCopyMemory(out, rx + 2, n);
    return st;
}
static NTSTATUS NvtRead(PDEV_CTX c, ULONG addr, UCHAR *out, ULONG n)
{
    NTSTATUS st = NvtSetPage(c, addr);
    return NT_SUCCESS(st) ? NvtReadRaw(c, addr, out, n) : st;
}

/* NT36532 cascade map + hw info (nt36xxx_mem_map.h) */
#define SWRST_N8_ADDR        0x03F0FE      /* pipa DT novatek,swrst-n8-addr */
#define EVENT_BUF_ADDR       0x125800
#define EV_HOST_CMD          0x50
#define EV_RESET_COMPLETE    0x60
#define EV_FWINFO            0x78
#define BOOT_RDY_ADDR        0x1FB50D
#define TX_AUTO_COPY_EN      0x1FC925
#define ILM_LENGTH_ADDR      0x1FB518
#define DLM_LENGTH_ADDR      0x1FB530
#define ILM_DES_ADDR         0x1FB528
#define DLM_DES_ADDR         0x1FB52C
#define G_ILM_CHECKSUM_ADDR  0x1FB500
#define G_DLM_CHECKSUM_ADDR  0x1FB504
#define R_ILM_CHECKSUM_ADDR  0x1FB520
#define R_DLM_CHECKSUM_ADDR  0x1FB524
#define BLD_ILM_DLM_CRC_ADDR 0x1FB533
#define RESET_STATE_INIT     0xA0
#define RESET_STATE_REK      0xA1
#define RESET_STATE_MAX      0xAF

typedef struct { ULONG bin, sram, size, crc; } BINMAP;
#define MAX_PART 32

static ULONG W32(const UCHAR *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((ULONG)p[3] << 24); }

/* nvt_bin_header_parser (hw_crc variant) */
static ULONG ParseHeader(const UCHAR *fw, ULONG fwsize, BINMAP *m, BOOLEAN *cascade)
{
    ULONG end = W32(fw), pos, info = 0, ovly = 0, parts, list, ilmdlm = 2;
    BOOLEAN findHdr = FALSE;
    *cascade = (fw[0x20] & 0x02) ? TRUE : FALSE;
    if (*cascade) { for (pos = 0x30; pos < end / 2; pos += 0x10) info++; info++; }
    else { for (pos = 0x30; pos < end; pos += 0x10) info++; }
    if (fw[0x28] & 0x10) ovly = fw[0x28] & 0x0F;
    parts = ilmdlm + ovly + info;
    LOG("fw header: end=0x%lX cascade=%d info=%lu ovly=%lu partitions=%lu", end, *cascade, info, ovly, parts);
    if (parts > MAX_PART) return 0;
    for (list = 0; list < parts; list++) {
        if (list < ilmdlm) {
            m[list].bin = W32(&fw[list * 12]); m[list].sram = W32(&fw[4 + list * 12]);
            m[list].size = W32(&fw[8 + list * 12]); m[list].crc = W32(&fw[0x18 + list * 4]);
        } else if (list < ilmdlm + info) {
            if (!findHdr) pos = 0x30 + 0x10 * (list - ilmdlm);
            else if (*cascade) pos = end - 0x10;
            m[list].sram = W32(&fw[pos]); m[list].size = W32(&fw[pos + 4]);
            m[list].bin = W32(&fw[pos + 8]); m[list].crc = W32(&fw[pos + 12]);
            if (m[list].bin < end && m[list].size != 0) findHdr = TRUE;
        } else {
            pos = m[1].bin + 0x10 * (list - ilmdlm - info);
            m[list].sram = W32(&fw[pos]); m[list].size = W32(&fw[pos + 4]);
            m[list].bin = W32(&fw[pos + 8]); m[list].crc = W32(&fw[pos + 12]);
        }
        if (m[list].bin + m[list].size > fwsize) { LOG("partition %lu out of range", list); return 0; }
        LOG("  part[%lu] sram=%06lX size=%06lX bin=%06lX crc=%08lX", list, m[list].sram, m[list].size, m[list].bin, m[list].crc);
    }
    return parts;
}

static VOID SetCrcBank(PDEV_CTX c, ULONG des, ULONG sram, ULONG lenA, ULONG size, ULONG gck, ULONG crc)
{
    UCHAR s[8], d[4];
    NvtSetPage(c, des);
    d[0] = (UCHAR)sram; d[1] = (UCHAR)(sram >> 8); d[2] = (UCHAR)(sram >> 16); NvtWriteRaw(c, des, d, 3, s);
    d[0] = (UCHAR)size; d[1] = (UCHAR)(size >> 8); d[2] = (UCHAR)(size >> 16); NvtWriteRaw(c, lenA, d, 3, s);
    d[0] = (UCHAR)crc; d[1] = (UCHAR)(crc >> 8); d[2] = (UCHAR)(crc >> 16); d[3] = (UCHAR)(crc >> 24); NvtWriteRaw(c, gck, d, 4, s);
}

static NTSTATUS WriteSram(PDEV_CTX c, const UCHAR *fw, ULONG sram, ULONG size, ULONG bin, UCHAR *scratch)
{
    NTSTATUS st = STATUS_SUCCESS;
    while (size) {
        ULONG n = size < NVT_CHUNK ? size : NVT_CHUNK;
        st = NvtSetPage(c, sram);
        if (NT_SUCCESS(st)) st = NvtWriteRaw(c, sram, fw + bin, n, scratch);
        if (!NT_SUCCESS(st)) return st;
        sram += n; bin += n; size -= n;
    }
    return st;
}

static NTSTATUS CheckResetState(PDEV_CTX c, UCHAR want, ULONG tries)
{
    UCHAR b[6]; ULONG i;
    for (i = 0; i < tries; i++) {
        NvtRead(c, EVENT_BUF_ADDR | EV_RESET_COMPLETE, b, 5);
        if (b[0] >= want && b[0] <= RESET_STATE_MAX) { LOG("reset state 0x%02X (want >= 0x%02X) after %lu", b[0], want, i); return STATUS_SUCCESS; }
        KeStallExecutionProcessor(10000);
    }
    LOG("reset state timeout: %02X %02X %02X %02X %02X (want 0x%02X)", b[0], b[1], b[2], b[3], b[4], want);
    return STATUS_IO_TIMEOUT;
}

/* nvt_download_firmware_hw_crc (cascade) */
static NTSTATUS Download(PDEV_CTX c, const UCHAR *fw, ULONG fwsize)
{
    BINMAP m[MAX_PART] = { 0 }; BOOLEAN casc; ULONG parts, i, tries; NTSTATUS st = STATUS_UNSUCCESSFUL;
    UCHAR *scratch; UCHAR b[8];
    LARGE_INTEGER t0, t1, f;
    parts = ParseHeader(fw, fwsize, m, &casc);
    if (!parts) return STATUS_INVALID_IMAGE_FORMAT;
    scratch = (UCHAR *)ExAllocatePool2(POOL_FLAG_NON_PAGED, NVT_CHUNK + 8, 'TN36');
    if (!scratch) return STATUS_INSUFFICIENT_RESOURCES;
    for (tries = 0; tries < 3; tries++) {
        t0 = KeQueryPerformanceCounter(&f);
        NvtWriteAddr(c, SWRST_N8_ADDR, 0x69);          /* bootloader reset */
        KeStallExecutionProcessor(5000);
        SetCrcBank(c, ILM_DES_ADDR, m[0].sram, ILM_LENGTH_ADDR, m[0].size, G_ILM_CHECKSUM_ADDR, m[0].crc);
        SetCrcBank(c, DLM_DES_ADDR, m[1].sram, DLM_LENGTH_ADDR, m[1].size, G_DLM_CHECKSUM_ADDR, m[1].crc);
        if (casc) NvtWriteAddr(c, TX_AUTO_COPY_EN, 0x56); /* CHECK_TX_AUTO_COPY_EN */
        for (i = 0, st = STATUS_SUCCESS; i < parts && NT_SUCCESS(st); i++)
            if (m[i].size) st = WriteSram(c, fw, m[i].sram, m[i].size + 1, m[i].bin, scratch);
        if (!NT_SUCCESS(st)) { LOG("write firmware failed %08X", st); continue; }
        if (casc) {
            for (i = 0; i < 200; i++) { NvtRead(c, TX_AUTO_COPY_EN, b, 1); if (b[0] == 0) break; KeStallExecutionProcessor(1000); }
            LOG("auto copy wait i=%lu val=%02X", i, b[0]);
        }
        /* nvt_fw_crc_enable + nvt_boot_ready */
        NvtSetPage(c, EVENT_BUF_ADDR);
        b[0] = 0x00; NvtWriteRaw(c, EV_RESET_COMPLETE, b, 1, scratch);
        b[0] = 0xAE; NvtWriteRaw(c, EV_HOST_CMD, b, 1, scratch);
        NvtWriteAddr(c, BOOT_RDY_ADDR, 1);
        KeStallExecutionProcessor(5000);
        t1 = KeQueryPerformanceCounter(NULL);
        LOG("download pass %lu took %lu ms", tries, (ULONG)((t1.QuadPart - t0.QuadPart) * 1000 / f.QuadPart));
        st = CheckResetState(c, RESET_STATE_INIT, 10);
        if (NT_SUCCESS(st)) break;
        {   /* nvt_read_bld_hw_crc */
            UCHAR g[4], r[4];
            NvtRead(c, BLD_ILM_DLM_CRC_ADDR, b, 1);
            NvtRead(c, G_ILM_CHECKSUM_ADDR, g, 4); NvtRead(c, R_ILM_CHECKSUM_ADDR, r, 4);
            LOG("crc flag=%02X ilm bin=%08lX golden=%08lX result=%08lX", b[0], m[0].crc, W32(g), W32(r));
            NvtRead(c, G_DLM_CHECKSUM_ADDR, g, 4); NvtRead(c, R_DLM_CHECKSUM_ADDR, r, 4);
            LOG("dlm bin=%08lX golden=%08lX result=%08lX", m[1].crc, W32(g), W32(r));
        }
    }
    ExFreePool(scratch);
    return st;
}

static NTSTATUS LoadFile(PCWSTR path, UCHAR **buf, ULONG *len)
{
    UNICODE_STRING us; OBJECT_ATTRIBUTES oa; IO_STATUS_BLOCK io; HANDLE h; NTSTATUS st;
    FILE_STANDARD_INFORMATION fsi;
    RtlInitUnicodeString(&us, path);
    InitializeObjectAttributes(&oa, &us, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, NULL, NULL);
    st = ZwCreateFile(&h, GENERIC_READ | SYNCHRONIZE, &oa, &io, NULL, FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ,
                      FILE_OPEN, FILE_SYNCHRONOUS_IO_NONALERT | FILE_NON_DIRECTORY_FILE, NULL, 0);
    if (!NT_SUCCESS(st)) return st;
    st = ZwQueryInformationFile(h, &io, &fsi, sizeof(fsi), FileStandardInformation);
    if (NT_SUCCESS(st) && (fsi.EndOfFile.QuadPart < 0x1000 || fsi.EndOfFile.QuadPart > 0x100000)) st = STATUS_INVALID_IMAGE_FORMAT;
    if (NT_SUCCESS(st)) {
        *len = fsi.EndOfFile.LowPart;
        *buf = (UCHAR *)ExAllocatePool2(POOL_FLAG_PAGED, *len, 'WF36');
        if (!*buf) st = STATUS_INSUFFICIENT_RESOURCES;
        else {
            st = ZwReadFile(h, NULL, NULL, NULL, &io, *buf, *len, NULL, NULL);
            if (!NT_SUCCESS(st)) { ExFreePool(*buf); *buf = NULL; }
        }
    }
    ZwClose(h);
    return st;
}

/* event buffer: 1 + 6*10 fingers (+pressure at 1+98+i), pen 14 bytes after 65 */
#define POINT_DATA_LEN 65
#define PEN_DATA_LEN   14

/* ---------------- HID multi-touch (mshidkmdf shim; we are its lower filter) ----------------
 * Parallel-mode touch screen: 10 contacts per report. Coordinates are the controller's native
 * portrait space (x 0..17999, y 0..28799 = 1800x2880 * 10). Panel ~14.8 x 23.7 cm. */
#define MAX_CONTACTS 10
#define RID_TOUCH    1
#define RID_MAXCOUNT 2
#define LX 17999
#define LY 28799
#define PX 1480     /* 0.01 cm */
#define PY 2370
#pragma pack(push, 1)
typedef struct { UCHAR tip; UCHAR cid; USHORT x; USHORT y; } CONTACT;
typedef struct { UCHAR rid; CONTACT c[MAX_CONTACTS]; USHORT scan; UCHAR count; } TOUCH_REPORT;
#pragma pack(pop)

#define FINGER_COLL \
    0x05,0x0D, 0x09,0x22, 0xA1,0x02, \
      0x09,0x42, 0x15,0x00, 0x25,0x01, 0x75,0x01, 0x95,0x01, 0x81,0x02, \
      0x95,0x07, 0x81,0x03, \
      0x09,0x51, 0x75,0x08, 0x95,0x01, 0x25,0x0F, 0x81,0x02, \
      0x05,0x01, 0x75,0x10, 0x55,0x0E, 0x65,0x11, \
      0x09,0x30, 0x26,(LX & 0xFF),(LX >> 8), 0x46,(PX & 0xFF),(PX >> 8), 0x81,0x02, \
      0x09,0x31, 0x26,(LY & 0xFF),(LY >> 8), 0x46,(PY & 0xFF),(PY >> 8), 0x81,0x02, \
      0x55,0x00, 0x65,0x00, 0x35,0x00, 0x45,0x00, \
    0xC0
static const UCHAR g_ReportDesc[] = {
    0x05,0x0D, 0x09,0x04, 0xA1,0x01, 0x85,RID_TOUCH,
    FINGER_COLL, FINGER_COLL, FINGER_COLL, FINGER_COLL, FINGER_COLL,
    FINGER_COLL, FINGER_COLL, FINGER_COLL, FINGER_COLL, FINGER_COLL,
    0x05,0x0D, 0x55,0x0C, 0x66,0x01,0x10, 0x47,0xFF,0xFF,0x00,0x00, 0x27,0xFF,0xFF,0x00,0x00,
    0x75,0x10, 0x95,0x01, 0x09,0x56, 0x81,0x02,                     /* scan time (100 us) */
    0x55,0x00, 0x66,0x00,0x00, 0x45,0x00,
    0x09,0x54, 0x15,0x00, 0x25,0x7F, 0x75,0x08, 0x95,0x01, 0x81,0x02, /* contact count */
    0x85,RID_MAXCOUNT, 0x09,0x55, 0x25,MAX_CONTACTS, 0x75,0x08, 0x95,0x01, 0xB1,0x02, /* contact count max */
    0xC0
};
static const HID_DESCRIPTOR g_HidDesc = {
    sizeof(HID_DESCRIPTOR), 0x21, 0x0100, 0x00, 0x01, { 0x22, sizeof(g_ReportDesc) }
};

static NTSTATUS CopyOut(WDFREQUEST r, const VOID *src, size_t len)
{
    WDFMEMORY m; size_t cap; NTSTATUS st = WdfRequestRetrieveOutputMemory(r, &m);
    if (!NT_SUCCESS(st)) return st;
    WdfMemoryGetBuffer(m, &cap);
    if (cap < len) return STATUS_INVALID_BUFFER_SIZE;
    st = WdfMemoryCopyFromBuffer(m, 0, (PVOID)src, len);
    if (NT_SUCCESS(st)) WdfRequestSetInformation(r, len);
    return st;
}

VOID NtIoctl(WDFQUEUE q, WDFREQUEST r, size_t outLen, size_t inLen, ULONG code)
{
    PDEV_CTX c = GetCtx(WdfIoQueueGetDevice(q)); NTSTATUS st = STATUS_NOT_SUPPORTED;
    static LONG nlog;
    if (code != IOCTL_HID_READ_REPORT || InterlockedIncrement(&nlog) <= 4)
        LOG("ioctl %08X (fn %lu) in=%Iu out=%Iu", code, (code >> 2) & 0xFFF, inLen, outLen);
    switch (code) {
    case IOCTL_HID_GET_DEVICE_DESCRIPTOR: st = CopyOut(r, &g_HidDesc, sizeof(g_HidDesc)); break;
    case IOCTL_HID_GET_REPORT_DESCRIPTOR: st = CopyOut(r, g_ReportDesc, sizeof(g_ReportDesc)); break;
    case IOCTL_HID_GET_DEVICE_ATTRIBUTES: {
        HID_DEVICE_ATTRIBUTES a = { sizeof(a), 0x0603 /* Novatek */, 0x6532, 0x0001 };
        st = CopyOut(r, &a, sizeof(a)); break; }
    case IOCTL_HID_READ_REPORT:
        st = WdfRequestForwardToIoQueue(r, c->readQ);
        if (NT_SUCCESS(st)) return;
        break;
    case IOCTL_HID_GET_FEATURE: {
        /* via mshidkmdf this arrives METHOD_OUT_DIRECT with no input buffer: the caller puts the
         * report ID in byte 0 of the output buffer (HidD_GetFeature convention). */
        WDFMEMORY om; UCHAR *ob; size_t ol;
        st = WdfRequestRetrieveOutputMemory(r, &om);
        if (NT_SUCCESS(st)) {
            ob = (UCHAR *)WdfMemoryGetBuffer(om, &ol);
            LOG("get feature id=%u len=%Iu", ol ? ob[0] : 0xFF, ol);
            if (ol >= 2 && ob[0] == RID_MAXCOUNT) { ob[1] = MAX_CONTACTS; WdfRequestSetInformation(r, 2); st = STATUS_SUCCESS; }
            else st = STATUS_INVALID_PARAMETER;
        }
        break; }
    case IOCTL_HID_GET_STRING: {
        static const WCHAR name[] = L"pipa Touch Screen (NT36532)";
        st = CopyOut(r, name, sizeof(name));
        break; }
    case IOCTL_HID_SET_FEATURE:
    case IOCTL_HID_ACTIVATE_DEVICE:
    case IOCTL_HID_DEACTIVATE_DEVICE:
        st = STATUS_SUCCESS; break;
    case IOCTL_HID_SEND_IDLE_NOTIFICATION_REQUEST:
        /* no selective suspend: completing this immediately lets hidclass drop us to Dx,
         * which stopped polling. The controller has no wake source wired up yet. */
        st = STATUS_NOT_SUPPORTED; break;
    default:
        st = STATUS_NOT_SUPPORTED; break;
    }
    WdfRequestComplete(r, st);
}

static VOID SendReport(PDEV_CTX c, const TOUCH_REPORT *rep)
{
    WDFREQUEST r;
    if (!c->readQ) return;
    static LONG ndrop, nsent;
    NTSTATUS st;
    if (!NT_SUCCESS(WdfIoQueueRetrieveNextRequest(c->readQ, &r))) {   /* nobody reading: drop */
        if (InterlockedIncrement(&ndrop) <= 5) LOG("report dropped: no pending READ_REPORT");
        return;
    }
    st = CopyOut(r, rep, sizeof(*rep));
    if (InterlockedIncrement(&nsent) <= 5 || !NT_SUCCESS(st)) LOG("report sent count=%u st=%08X", rep->count, st);
    WdfRequestComplete(r, st);
}

VOID NtPoll(WDFTIMER t)
{
    PDEV_CTX c = GetCtx(WdfTimerGetParentObject(t));
    UCHAR d[POINT_DATA_LEN + PEN_DATA_LEN + 1] = { 0 };
    TOUCH_REPORT rep;
    ULONG i, n = 0;
    if (!c->fwOk || !c->se) return;
    if (!NT_SUCCESS(NvtRead(c, EVENT_BUF_ADDR, d, POINT_DATA_LEN + PEN_DATA_LEN))) goto rearm;
    RtlZeroMemory(&rep, sizeof(rep));
    rep.rid = RID_TOUCH;
    for (i = 0; i < 10; i++) {
        ULONG p = 6 * i, id = d[p] >> 3, stt = d[p] & 7;
        if (id == 0 || id > 10) continue;
        if (stt == 1 || stt == 2) {
            ULONG x = (d[p + 1] << 8) | d[p + 2], y = (d[p + 3] << 8) | d[p + 4];
            if (x > LX || y > LY) continue;
            rep.c[n].tip = 1; rep.c[n].cid = (UCHAR)(id - 1); rep.c[n].x = (USHORT)x; rep.c[n].y = (USHORT)y;
            n++;
            if (c->logBudget) { c->logBudget--; LOG("touch id=%lu x=%lu y=%lu w=%u", id, x, y, d[p + 5]); }
        }
    }
    if (n != c->lastFingers) { LOG("fingers %lu -> %lu", c->lastFingers, n); if (n) c->logBudget = 6; }
    if (n || c->lastFingers) {         /* report while touching, plus one all-up report */
        if (!n) {                       /* lift: send the previous contacts with tip=0 */
            for (i = 0; i < c->lastFingers && i < MAX_CONTACTS; i++) { rep.c[i].cid = c->lastCid[i]; }
        }
        c->scan = (USHORT)(c->scan + 200);   /* 20 ms in 100 us units */
        rep.scan = c->scan;
        rep.count = (UCHAR)(n ? n : c->lastFingers);
        SendReport(c, &rep);
    }
    for (i = 0; i < n; i++) c->lastCid[i] = rep.c[i].cid;
    c->lastFingers = n;
rearm:
    if (c->polling) WdfTimerStart(t, WDF_REL_TIMEOUT_IN_MS(n ? 10 : 20));
}

NTSTATUS DriverEntry(PDRIVER_OBJECT drv, PUNICODE_STRING path)
{
    WDF_DRIVER_CONFIG cfg;
    WDF_DRIVER_CONFIG_INIT(&cfg, NtDeviceAdd);
    return WdfDriverCreate(drv, path, WDF_NO_OBJECT_ATTRIBUTES, &cfg, WDF_NO_HANDLE);
}

NTSTATUS NtDeviceAdd(WDFDRIVER drv, PWDFDEVICE_INIT init)
{
    WDF_PNPPOWER_EVENT_CALLBACKS pnp; WDF_OBJECT_ATTRIBUTES a; WDFDEVICE dev; NTSTATUS st;
    WDF_TIMER_CONFIG tc; WDF_OBJECT_ATTRIBUTES ta;
    UNREFERENCED_PARAMETER(drv);
    WdfFdoInitSetFilter(init);         /* lower filter below mshidkmdf */
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnp);
    pnp.EvtDevicePrepareHardware = NtPrepare;
    pnp.EvtDeviceReleaseHardware = NtRelease;
    pnp.EvtDeviceD0Entry = NtD0Entry;
    pnp.EvtDeviceD0Exit = NtD0Exit;
    WdfDeviceInitSetPnpPowerEventCallbacks(init, &pnp);
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&a, DEV_CTX);
    st = WdfDeviceCreate(&init, &a, &dev);
    if (!NT_SUCCESS(st)) return st;
    WDF_TIMER_CONFIG_INIT(&tc, NtPoll);   /* passive timers cannot be periodic: one-shot, re-armed in NtPoll */
    tc.AutomaticSerialization = FALSE;
    WDF_OBJECT_ATTRIBUTES_INIT(&ta);
    ta.ParentObject = dev;
    ta.ExecutionLevel = WdfExecutionLevelPassive;
    st = WdfTimerCreate(&tc, &ta, &GetCtx(dev)->poll);
    LOG("poll timer create st=%08X", st);
    {
        WDF_IO_QUEUE_CONFIG qc; WDFQUEUE q;
        WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&qc, WdfIoQueueDispatchParallel);
        qc.EvtIoInternalDeviceControl = NtIoctl;
        st = WdfIoQueueCreate(dev, &qc, WDF_NO_OBJECT_ATTRIBUTES, &q);
        if (!NT_SUCCESS(st)) { LOG("default queue st=%08X", st); return st; }
        WDF_IO_QUEUE_CONFIG_INIT(&qc, WdfIoQueueDispatchManual);
        qc.PowerManaged = WdfFalse;
        st = WdfIoQueueCreate(dev, &qc, WDF_NO_OBJECT_ATTRIBUTES, &GetCtx(dev)->readQ);
        if (!NT_SUCCESS(st)) { LOG("read queue st=%08X", st); return st; }
    }
    return STATUS_SUCCESS;
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
    return STATUS_SUCCESS;
}

NTSTATUS NtRelease(WDFDEVICE dev, WDFCMRESLIST xl)
{
    PDEV_CTX c = GetCtx(dev); UNREFERENCED_PARAMETER(xl);
    c->polling = FALSE;
    if (c->poll) WdfTimerStop(c->poll, TRUE);
    if (c->se) { MmUnmapIoSpace(c->se, c->seLen); c->se = NULL; }
    return STATUS_SUCCESS;
}

NTSTATUS NtD0Exit(WDFDEVICE dev, WDF_POWER_DEVICE_STATE target)
{
    PDEV_CTX c = GetCtx(dev);
    LOG("D0 exit -> D%d (polling stops)", (int)target - 1);
    c->polling = FALSE;
    if (c->poll) WdfTimerStop(c->poll, TRUE);
    return STATUS_SUCCESS;
}

NTSTATUS NtD0Entry(WDFDEVICE dev, WDF_POWER_DEVICE_STATE prev)
{
    PDEV_CTX c = GetCtx(dev);
    static const PCWSTR files[] = { L"\\SystemRoot\\System32\\drivers\\nt36532_csot.bin", L"\\SystemRoot\\System32\\drivers\\nt36532_tianma.bin" };
    UCHAR b[40]; ULONG i; NTSTATUS st;
    LOG("D0 entry (from D%d)", (int)prev - 1);
    if (!c->se) return STATUS_SUCCESS;
    if (!NT_SUCCESS(GeniInit(c))) return STATUS_SUCCESS;   /* never fail start during bring-up */
    c->inited = TRUE;
    RtlZeroMemory(b, sizeof(b));
    st = NvtRead(c, 0x1FB104, b, 7);
    LOG("chip id @1FB104 st=%08X: %02X %02X %02X %02X %02X %02X %02X", st, b[0], b[1], b[2], b[3], b[4], b[5], b[6]);
    if (b[3] != 0x32 || b[4] != 0x65 || b[5] != 0x03) { LOG("not an NT36532 - stop"); return STATUS_SUCCESS; }

    for (i = 0; i < RTL_NUMBER_OF(files) && !c->fwOk; i++) {
        UCHAR *fw = NULL; ULONG len = 0;
        st = LoadFile(files[i], &fw, &len);
        LOG("firmware %ws st=%08X len=%lu", files[i], st, len);
        if (!NT_SUCCESS(st)) continue;
        st = Download(c, fw, len);
        ExFreePool(fw);
        LOG("download %ws -> %08X", files[i], st);
        if (NT_SUCCESS(st)) c->fwOk = TRUE;
    }
    if (!c->fwOk) return STATUS_SUCCESS;

    CheckResetState(c, RESET_STATE_REK, 50);
    RtlZeroMemory(b, sizeof(b));
    NvtRead(c, EVENT_BUF_ADDR | EV_FWINFO, b, 38);
    c->absX = (USHORT)((b[4] << 8) | b[5]); c->absY = (USHORT)((b[6] << 8) | b[7]);
    LOG("fw info: ver=%02X ~ver=%02X x_num=%u y_num=%u abs_x=%u abs_y=%u buttons=%u cascade=%u",
        b[0], b[1], b[2], b[3], c->absX, c->absY, b[10], b[33] & 1);
    c->lastFingers = 0; c->logBudget = 0;
    c->polling = TRUE;
    if (c->poll) WdfTimerStart(c->poll, WDF_REL_TIMEOUT_IN_MS(200));
    LOG("polling started - touch the screen");
    return STATUS_SUCCESS;
}
