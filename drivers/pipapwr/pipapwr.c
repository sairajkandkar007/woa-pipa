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
#include <ntstrsafe.h>

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

static BOOLEAN g_verbose = TRUE;

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
        for (t = 0; t < 20000 && (R(SE_GENI_TX_FIFO_STATUS) & 0x0FFFFFFF) >= 14; t++) KeStallExecutionProcessor(50);
        W(SE_GENI_TX_FIFOn, w);
    }
    for (t = 0; t < 20000 && !(R(SE_GENI_M_IRQ_STATUS) & 1u); t++) KeStallExecutionProcessor(100);
    if (g_verbose) LOG("tx %lu bytes m_irq=%08X status=%08X ios=%08X", n, R(SE_GENI_M_IRQ_STATUS), R(SE_GENI_STATUS), R(SE_GENI_IOS));
    if (!(R(SE_GENI_M_IRQ_STATUS) & 1u)) { W(SE_GENI_M_CMD_CTRL_REG, 1u << 2); KeStallExecutionProcessor(1000); }
    W(SE_GENI_M_IRQ_CLEAR, 0xFFFFFFFF);
}

static ULONG UartRx(PUCHAR se, UCHAR *out, ULONG cap, ULONG ms)
{
    ULONG got = 0, el, st, wc, i, k, nb, w;
    for (el = 0; el < ms && got < cap; ) {
        st = R(SE_GENI_RX_FIFO_STATUS);
        wc = st & 0x1FFFFFF;
        if (!wc) { if (got) { KeStallExecutionProcessor(3000); if (!(R(SE_GENI_RX_FIFO_STATUS) & 0x1FFFFFF)) break; continue; } MsSleep(2); el += 2; continue; }
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


/* ---------------- QCA6390 firmware download (Linux drivers/bluetooth/btqca.c) ---------------- */
static ULONG g_rxn, g_rxp; static UCHAR g_rxb[512];

static BOOLEAN RxByte(PUCHAR se, UCHAR *b, ULONG ms)
{
    if (g_rxp >= g_rxn) { g_rxp = 0; g_rxn = UartRx(se, g_rxb, sizeof(g_rxb), ms); if (!g_rxn) return FALSE; }
    *b = g_rxb[g_rxp++]; return TRUE;
}
/* read one H4 event (04 evt len payload); returns payload length or -1 */
static LONG ReadEvent(PUCHAR se, UCHAR *evt, UCHAR *p, ULONG cap, ULONG ms)
{
    UCHAR t = 0, len, i;
    do { if (!RxByte(se, &t, ms)) return -1; } while (t != 0x04);
    if (!RxByte(se, evt, ms) || !RxByte(se, &len, ms)) return -1;
    for (i = 0; i < len; i++) { UCHAR c; if (!RxByte(se, &c, ms)) return -1; if (i < cap) p[i] = c; }
    return len;
}
/* send H4 command, wait for Command Complete for that opcode; returns status byte or 0xFFFF */
static ULONG HciCmd(PUCHAR se, USHORT op, const UCHAR *par, UCHAR plen, UCHAR *ret, ULONG cap, ULONG ms)
{
    UCHAR pkt[4 + 255], evt, p[64]; LONG n; ULONG tries;
    pkt[0] = 1; pkt[1] = (UCHAR)op; pkt[2] = (UCHAR)(op >> 8); pkt[3] = plen;
    if (plen) RtlCopyMemory(pkt + 4, par, plen);
    UartTx(se, pkt, 4u + plen);
    for (tries = 0; tries < 8; tries++) {
        n = ReadEvent(se, &evt, p, sizeof(p), ms);
        if (n < 0) return 0xFFFF;
        if (evt == 0x0E && n >= 4 && p[1] == (UCHAR)op && p[2] == (UCHAR)(op >> 8)) {
            if (ret) RtlCopyMemory(ret, p, min((ULONG)n, cap));
            return p[3];
        }
        if (g_verbose) LOG("bt: op %04X: other event %02X len %ld", op, evt, n);
    }
    return 0xFFFF;
}

static NTSTATUS LoadBtFile(PCWSTR path, UCHAR **buf, ULONG *len)
{
    UNICODE_STRING us; OBJECT_ATTRIBUTES oa; IO_STATUS_BLOCK io; HANDLE h; NTSTATUS st;
    FILE_STANDARD_INFORMATION fsi;
    RtlInitUnicodeString(&us, path);
    InitializeObjectAttributes(&oa, &us, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, NULL, NULL);
    st = ZwCreateFile(&h, GENERIC_READ | SYNCHRONIZE, &oa, &io, NULL, FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ,
                      FILE_OPEN, FILE_SYNCHRONOUS_IO_NONALERT | FILE_NON_DIRECTORY_FILE, NULL, 0);
    if (!NT_SUCCESS(st)) return st;
    st = ZwQueryInformationFile(h, &io, &fsi, sizeof(fsi), FileStandardInformation);
    if (NT_SUCCESS(st) && (fsi.EndOfFile.QuadPart < 16 || fsi.EndOfFile.QuadPart > 0x100000)) st = STATUS_INVALID_IMAGE_FORMAT;
    if (NT_SUCCESS(st)) {
        *len = fsi.EndOfFile.LowPart;
        *buf = (UCHAR *)ExAllocatePool2(POOL_FLAG_PAGED, *len, 'tbPP');
        if (!*buf) st = STATUS_INSUFFICIENT_RESOURCES;
        else { st = ZwReadFile(h, NULL, NULL, NULL, &io, *buf, *len, NULL, NULL); if (!NT_SUCCESS(st)) { ExFreePool(*buf); *buf = NULL; } }
    }
    ZwClose(h);
    return st;
}

/* NVM tweak: keep the controller at 115200 and do NOT enable in-band sleep (we have no IBS handling) */
static VOID NvmFixup(UCHAR *d, ULONG size)
{
    ULONG tl, length, type, idx; UCHAR *data;
    if (size < 4) return;
    tl = d[0] | (d[1] << 8) | (d[2] << 16) | ((ULONG)d[3] << 24); length = tl >> 8; type = tl & 0xFF;
    data = d + 4;
    if (type == 4 && size >= 8) { tl = d[4] | (d[5] << 8) | (d[6] << 16) | ((ULONG)d[7] << 24); length = tl >> 8; type = tl & 0xFF; data = d + 8; }
    if (type != 2 || (ULONG)(data - d) + length > size) { LOG("bt: nvm type %lu - not patched", type); return; }
    for (idx = 0; idx + 12 <= length; ) {
        USHORT tag = data[idx] | (data[idx + 1] << 8), tlen = data[idx + 2] | (data[idx + 3] << 8);
        UCHAR *v = data + idx + 12;
        if (idx + 12 + tlen > length) break;
        if (tag == 17 && tlen >= 3) { LOG("bt: nvm HCI tag %02X %02X %02X -> baud 115200, IBS off", v[0], v[1], v[2]); v[0] &= 0x7F; v[1] = 0; }
        if (tag == 27 && tlen >= 1) { LOG("bt: nvm deep-sleep tag %02X -> off", v[0]); v[0] &= 0xFE; }
        if (tag == 2 && tlen == 6) LOG("bt: nvm bd_addr %02X:%02X:%02X:%02X:%02X:%02X", v[5], v[4], v[3], v[2], v[1], v[0]);
        idx += 12 + tlen;
    }
}

static BOOLEAN Download(PUCHAR se, PCWSTR path, BOOLEAN nvm)
{
    UCHAR *d = NULL, seg[2 + 243]; ULONG size = 0, off, segsz, nseg = 0, mode = 0, st;
    NTSTATUS ns = LoadBtFile(path, &d, &size);
    LOG("bt: load %ws st=%08X size=%lu", path, ns, size);
    if (!NT_SUCCESS(ns)) return FALSE;
    if (nvm) NvmFixup(d, size);
    else if (size >= 4 + 12) { mode = d[4 + 10]; LOG("bt: patch total=%lu data=%lu dl_mode=%lu rom_build=%04X patch=%04X", 
        d[4] | (d[5] << 8) | (d[6] << 16), d[8] | (d[9] << 8) | (d[10] << 16), mode, d[18] | (d[19] << 8), d[20] | (d[21] << 8)); }
    g_verbose = FALSE;
    for (off = 0; off < size; off += segsz) {
        BOOLEAN last;
        segsz = min(243u, size - off);
        last = (off + segsz >= size) || segsz < 243;
        seg[0] = 0x1E; seg[1] = (UCHAR)segsz; RtlCopyMemory(seg + 2, d + off, segsz);
        if (!last && (mode == 1 || mode == 3)) {          /* QCA_SKIP_EVT_VSE / VSE_CC: no reply expected */
            UCHAR pkt[4 + 245]; pkt[0] = 1; pkt[1] = 0x00; pkt[2] = 0xFC; pkt[3] = (UCHAR)(segsz + 2);
            RtlCopyMemory(pkt + 4, seg, segsz + 2); UartTx(se, pkt, 6 + segsz);
        } else {
            UCHAR r[8];
            st = HciCmd(se, 0xFC00, seg, (UCHAR)(segsz + 2), r, sizeof(r), 2000);
            if (st != 0 || r[4] != 0x1E) { g_verbose = TRUE; LOG("bt: segment %lu at %lu failed st=%lX rtype=%02X", nseg, off, st, r[4]); ExFreePool(d); return FALSE; }
        }
        nseg++;
    }
    g_verbose = TRUE;
    LOG("bt: %ws sent in %lu segments", nvm ? L"NVM" : L"patch", nseg);
    ExFreePool(d);
    return TRUE;
}

static VOID BtFirmware(PUCHAR se, UCHAR rom)
{
    WCHAR p1[80], p2[80]; UCHAR r[16]; ULONG st;
    static const UCHAR nolog[] = { 0x14, 0x00 };
    LARGE_INTEGER t0, t1;
    RtlStringCchPrintfW(p1, 80, L"\\SystemRoot\\System32\\drivers\\htbtfw%02x.tlv", rom);
    RtlStringCchPrintfW(p2, 80, L"\\SystemRoot\\System32\\drivers\\htnv%02x.bin", rom);
    /* honor CTS during the download so the controller can throttle us */
    W(SE_UART_TX_TRANS_CFG, R(SE_UART_TX_TRANS_CFG) & ~(1u << 1));
    KeQuerySystemTimePrecise(&t0);
    if (!Download(se, p1, FALSE)) return;
    MsSleep(10);
    if (!Download(se, p2, TRUE)) return;
    KeQuerySystemTimePrecise(&t1);
    LOG("bt: firmware download took %lu ms", (ULONG)((t1.QuadPart - t0.QuadPart) / 10000));
    st = HciCmd(se, 0xFC17, nolog, 2, NULL, 0, 1000);
    LOG("bt: disable soc logging st=%lX", st);
    st = HciCmd(se, 0x0C03, NULL, 0, NULL, 0, 2000);
    LOG("bt: HCI_Reset after fw st=%lX", st);
    RtlZeroMemory(r, sizeof(r));
    st = HciCmd(se, 0x1001, NULL, 0, r, sizeof(r), 1000);       /* Read_Local_Version_Information */
    LOG("bt: local version st=%lX hci_ver=%02X hci_rev=%04X lmp_ver=%02X manuf=%04X lmp_sub=%04X", st, r[4], r[5] | (r[6] << 8), r[7], r[8] | (r[9] << 8), r[10] | (r[11] << 8));
    RtlZeroMemory(r, sizeof(r));
    st = HciCmd(se, 0x1009, NULL, 0, r, sizeof(r), 1000);       /* Read_BD_ADDR */
    LOG("bt: bd_addr st=%lX %02X:%02X:%02X:%02X:%02X:%02X", st, r[9], r[8], r[7], r[6], r[5], r[4]);
}

static VOID BtProbe(VOID)
{
    static const UCHAR reset[] = { 0x01, 0x03, 0x0C, 0x00 };
    static const UCHAR ver[] = { 0x01, 0x00, 0xFC, 0x01, 0x19 };
    PUCHAR gcc, rcg, wrap, se; ULONG qver, samp, div, n, fw, sclk;
    UCHAR rx[96];

    gcc = MapPhys(GCC_VOTE_PAGE, 0x1000);
    if (!gcc) { LOG("bt: map gcc failed"); return; }
    n = READ_REGISTER_ULONG((volatile ULONG *)(gcc + 0x8));
    WRITE_REGISTER_ULONG((volatile ULONG *)(gcc + 0x8), n | (1u << 16));   /* gcc_qupv3_wrap0_s6_clk */
    KeStallExecutionProcessor(100);
    LOG("bt: gcc vote 52008 %08X -> %08X", n, READ_REGISTER_ULONG((volatile ULONG *)(gcc + 0x8)));
    MmUnmapIoSpace(gcc, 0x1000);
    /* gcc_qupv3_wrap0_s6_clk_src RCG @0x117730 (CMD,CFG,M,N,D; DFS enable = +0x14 bit0).
     * Boot left it on XO 19.2 MHz bypass, which can't make 115200 exactly. If DFS is off, program the
     * gcc-sm8250 ftbl entry 7.3728 MHz = GPLL0_OUT_EVEN(src 6) /1 * 384/15625 (MND dual edge), as clk-rcg2.c does. */
    rcg = MapPhys(GCC_RCG_PAGE, 0x1000);
    sclk = 19200000;
    if (rcg) {
        volatile ULONG *r = (volatile ULONG *)(rcg + 0x730);
        ULONG dfs = READ_REGISTER_ULONG(r + 5), cfg0 = READ_REGISTER_ULONG(r + 1), t;
        LOG("bt: s6 rcg cmd=%08X cfg=%08X m=%08X n=%08X d=%08X dfs_en=%08X", READ_REGISTER_ULONG(r), cfg0,
            READ_REGISTER_ULONG(r + 2), READ_REGISTER_ULONG(r + 3), READ_REGISTER_ULONG(r + 4), dfs);
        if (!(dfs & 1) && (cfg0 & 0x7FF) == 0) {
            WRITE_REGISTER_ULONG(r + 2, 384);
            WRITE_REGISTER_ULONG(r + 3, (~(15625u - 384u)) & 0xFFFF);
            WRITE_REGISTER_ULONG(r + 4, (~15625u) & 0xFFFF);
            WRITE_REGISTER_ULONG(r + 1, (cfg0 & ~0x37FFu) | 1u | (6u << 8) | 0x2000u);
            WRITE_REGISTER_ULONG(r, READ_REGISTER_ULONG(r) | 1u);
            for (t = 0; t < 500 && (READ_REGISTER_ULONG(r) & 1u); t++) KeStallExecutionProcessor(1);
            LOG("bt: s6 rcg -> 7.3728 MHz cmd=%08X cfg=%08X (update %s)", READ_REGISTER_ULONG(r), READ_REGISTER_ULONG(r + 1),
                (READ_REGISTER_ULONG(r) & 1u) ? "STUCK" : "ok");
            if (!(READ_REGISTER_ULONG(r) & 1u)) sclk = 7372800;
        }
        MmUnmapIoSpace(rcg, 0x1000);
    }
    wrap = MapPhys(QUP0_WRAP, 0x1000);
    qver = wrap ? READ_REGISTER_ULONG((volatile ULONG *)(wrap + 0x4)) : 0;
    if (wrap) MmUnmapIoSpace(wrap, 0x1000);
    samp = (qver >= 0x20050000) ? 16 : 32;
    div = (sclk + 115200 * samp / 2) / (115200 * samp);   /* 7.3728M/16 -> 4 exact; 19.2M -> 10 (+4%) */
    se = MapPhys(SE6_BASE, 0x4000);
    if (!se) { LOG("bt: map se6 failed"); return; }
    fw = R(GENI_FW_REVISION_RO);
    LOG("bt: sclk %lu qup hw ver %08X sampling %lu div %lu; se6 fw=%08X status=%08X ios=%08X", sclk, qver, samp, div, fw, R(SE_GENI_STATUS), R(SE_GENI_IOS));
    if (((fw >> 8) & 0xFF) != 2) { LOG("bt: SE6 is not UART firmware - abort"); MmUnmapIoSpace(se, 0x4000); return; }
    UartSetup(se, div);
    n = UartRx(se, rx, sizeof(rx), 50);           /* drain anything pending */
    if (n) Dump("stale rx", rx, n);
    UartTx(se, reset, sizeof(reset));
    n = UartRx(se, rx, sizeof(rx), 1000); Dump("hci_reset reply", rx, n);
    UartTx(se, ver, sizeof(ver));
    n = UartRx(se, rx, sizeof(rx), 1000); Dump("qca_version reply", rx, n);
    if (n >= 21 && rx[0] == 4 && rx[1] == 0x0E && rx[4] == 0x00 && rx[5] == 0xFC && rx[6] == 0 && rx[7] == 0x19) {
        ULONG socid = rx[17] | (rx[18] << 8) | (rx[19] << 16) | ((ULONG)rx[20] << 24);
        ULONG romv = rx[15] | (rx[16] << 8);
        ULONG socver = (socid << 16) | (romv & 0xFFFF);
        UCHAR rom = (UCHAR)(((socver & 0xF00) >> 4) | (socver & 0xF));
        LOG("bt: soc_id=%08X rom_ver=%04X patch=%04X -> fw rom %02X", socid, romv, rx[13] | (rx[14] << 8), rom);
        BtFirmware(se, rom);
    } else LOG("bt: version reply not understood - no firmware download");
    LOG("bt: done s_irq=%08X rxfifo=%08X ios=%08X", R(SE_GENI_S_IRQ_STATUS), R(SE_GENI_RX_FIFO_STATUS), R(SE_GENI_IOS));
    MmUnmapIoSpace(se, 0x4000);
}

static KSTART_ROUTINE BtThread;
static VOID BtThread(PVOID ctx)
{
    UNREFERENCED_PARAMETER(ctx);
    /* BT_EN low->high so the QCA6390 BT core boots fresh, then talk to it */
    SetOut(21, 0); MsSleep(50); SetOut(21, 1); MsSleep(150);
    BtProbe();
    PsTerminateSystemThread(STATUS_SUCCESS);
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
    {
        HANDLE th; NTSTATUS st = PsCreateSystemThread(&th, THREAD_ALL_ACCESS, NULL, NULL, NULL, BtThread, NULL);
        LOG("bt thread start st=%08X", st);
        if (NT_SUCCESS(st)) ZwClose(th);
    }
    return STATUS_SUCCESS;
}
