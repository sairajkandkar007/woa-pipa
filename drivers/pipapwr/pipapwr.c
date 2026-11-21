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
#include <bthxddi.h>

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
#define SE_GENI_M_IRQ_EN 0x614
#define SE_GENI_M_IRQ_CLEAR 0x618
#define SE_GENI_S_CMD0  0x630
#define SE_GENI_S_CMD_CTRL_REG 0x634
#define SE_GENI_S_IRQ_STATUS 0x640
#define SE_GENI_S_IRQ_EN 0x644
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

static BOOLEAN g_verbose = FALSE;
static UCHAR g_nvmBaud;   /* QCA baud code written into the NVM HCI tag: 0 = 115200, 0x0E = 3000000 */

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
    W(SE_IRQ_EN, R(SE_IRQ_EN) | 0xC);           /* GENI M/S irq to the top level ... */
    W(SE_GENI_M_IRQ_EN, 0); W(SE_GENI_S_IRQ_EN, 0);  /* ... but nothing enabled while we poll */
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
        for (t = 0; t < 4000 && (R(SE_GENI_TX_FIFO_STATUS) & 0x0FFFFFFF) >= 14; t++) KeStallExecutionProcessor(50);
        W(SE_GENI_TX_FIFOn, w);
    }
    for (t = 0; t < 2000 && !(R(SE_GENI_M_IRQ_STATUS) & 1u); t++) KeStallExecutionProcessor(100);
    if (g_verbose) LOG("tx %lu bytes m_irq=%08X status=%08X ios=%08X", n, R(SE_GENI_M_IRQ_STATUS), R(SE_GENI_STATUS), R(SE_GENI_IOS));
    if (!(R(SE_GENI_M_IRQ_STATUS) & 1u)) { W(SE_GENI_M_CMD_CTRL_REG, 1u << 2); KeStallExecutionProcessor(1000); }
    W(SE_GENI_M_IRQ_CLEAR, 0xFFFFFFFF);
}


/* IBS bytes (FD/FC) must go out even while the sleeping controller holds our CTS off: send them
   with CTS ignored. A blocked TX here used to spin under the TX spinlock until the controller woke. */
static VOID UartTxIbs(PUCHAR se, UCHAR b)
{
    ULONG cfg = R(SE_UART_TX_TRANS_CFG);
    W(SE_UART_TX_TRANS_CFG, cfg | (1u << 1));
    UartTx(se, &b, 1);
    W(SE_UART_TX_TRANS_CFG, cfg);
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
    do {
        if (!RxByte(se, &t, ms)) return -1;
        if (t == 0xFD) { static const UCHAR ack = 0xFC; UartTx(se, &ack, 1); }   /* IBS wake-ind -> ack */
    } while (t != 0x04);
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
        if (tag == 17 && tlen >= 3) { LOG("bt: nvm HCI tag %02X %02X %02X -> baud code %02X", v[0], v[1], v[2], g_nvmBaud); v[1] = g_nvmBaud; }
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
    LOG("bt: %ws sent in %lu segments", nvm ? L"NVM" : L"patch", nseg);
    ExFreePool(d);
    return TRUE;
}

static BOOLEAN BtFirmware(PUCHAR se, UCHAR rom, const UCHAR *bdaddr)
{
    WCHAR p1[80], p2[80]; UCHAR r[16]; ULONG st;
    static const UCHAR nolog[] = { 0x14, 0x00 };
    LARGE_INTEGER t0, t1;
    RtlStringCchPrintfW(p1, 80, L"\\SystemRoot\\System32\\drivers\\htbtfw%02x.tlv", rom);
    RtlStringCchPrintfW(p2, 80, L"\\SystemRoot\\System32\\drivers\\htnv%02x.bin", rom);
    /* honor CTS during the download so the controller can throttle us */
    W(SE_UART_TX_TRANS_CFG, R(SE_UART_TX_TRANS_CFG) & ~(1u << 1));
    KeQuerySystemTimePrecise(&t0);
    if (!Download(se, p1, FALSE)) return FALSE;
    MsSleep(10);
    if (!Download(se, p2, TRUE)) return FALSE;
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
    if (bdaddr) {     /* EDL_WRITE_BD_ADDR (btqca.c qca_set_bdaddr): 6 bytes, little endian */
        st = HciCmd(se, 0xFC14, bdaddr, 6, NULL, 0, 1000);
        LOG("bt: write bd_addr st=%lX", st);
        RtlZeroMemory(r, sizeof(r));
        st = HciCmd(se, 0x1009, NULL, 0, r, sizeof(r), 1000);
        LOG("bt: bd_addr now st=%lX %02X:%02X:%02X:%02X:%02X:%02X", st, r[9], r[8], r[7], r[6], r[5], r[4]);
    }
    return TRUE;
}

/* gcc_qupv3_wrap0_s6_clk_src RCG @0x117730 (CMD,CFG,M,N,D; DFS enable = +0x14 bit0). Boot leaves it on
 * XO bypass. Program an ftbl entry from GPLL0_OUT_EVEN (300 MHz, src 6) /1 * M/N, dual-edge MND, the way
 * clk-rcg2.c does: 7.3728 MHz = 384/15625 (115200 x16), 48 MHz = 4/25 (3 Mbaud x16). Returns the rate or 0. */
static ULONG SetS6Clock(ULONG m, ULONG n, ULONG hz)
{
    PUCHAR rcg = MapPhys(GCC_RCG_PAGE, 0x1000); ULONG ok = 0;
    if (rcg) {
        volatile ULONG *r = (volatile ULONG *)(rcg + 0x730);
        ULONG dfs = READ_REGISTER_ULONG(r + 5), cfg0 = READ_REGISTER_ULONG(r + 1), t;
        if (!(dfs & 1)) {
            WRITE_REGISTER_ULONG(r + 2, m);
            WRITE_REGISTER_ULONG(r + 3, (~(n - m)) & 0xFFFF);
            WRITE_REGISTER_ULONG(r + 4, (~n) & 0xFFFF);
            WRITE_REGISTER_ULONG(r + 1, (cfg0 & ~0x37FFu) | 1u | (6u << 8) | 0x2000u);
            WRITE_REGISTER_ULONG(r, READ_REGISTER_ULONG(r) | 1u);
            for (t = 0; t < 500 && (READ_REGISTER_ULONG(r) & 1u); t++) KeStallExecutionProcessor(1);
            if (!(READ_REGISTER_ULONG(r) & 1u)) ok = hz;
        }
        LOG("bt: s6 clock %lu Hz: cfg %08X -> %08X dfs %08X %s", hz, cfg0, READ_REGISTER_ULONG(r + 1), dfs, ok ? "ok" : "FAILED");
        MmUnmapIoSpace(rcg, 0x1000);
    }
    return ok;
}

static BOOLEAN ReadVersion(PUCHAR se, UCHAR *rom)
{
    static const UCHAR ver[] = { 0x00 + 0x19 };
    UCHAR r[24]; ULONG st;
    RtlZeroMemory(r, sizeof(r));
    st = HciCmd(se, 0xFC00, ver, 1, r, sizeof(r), 1000);
    /* CC payload: ncmd, op(2), status, rtype 0x19, len 0x0C, product(4), patch(2), rom(2), soc(4) */
    if (st != 0 || r[4] != 0x19) { LOG("bt: version request failed st=%lX rtype=%02X", st, r[4]); return FALSE; }
    {
        ULONG socid = r[14] | (r[15] << 8) | (r[16] << 16) | ((ULONG)r[17] << 24);
        ULONG romv = r[12] | (r[13] << 8);
        ULONG socver = (socid << 16) | (romv & 0xFFFF);
        *rom = (UCHAR)(((socver & 0xF00) >> 4) | (socver & 0xF));
        LOG("bt: soc_id=%08X rom_ver=%04X patch=%04X -> fw rom %02X", socid, romv, r[10] | (r[11] << 8), *rom);
    }
    return TRUE;
}


static PUCHAR BtProbe(const UCHAR *bdaddr, BOOLEAN fast)
{
    PUCHAR gcc, wrap, se; ULONG qver, samp, n, fw, sclk; UCHAR rom;

    gcc = MapPhys(GCC_VOTE_PAGE, 0x1000);
    if (!gcc) { LOG("bt: map gcc failed"); return NULL; }
    n = READ_REGISTER_ULONG((volatile ULONG *)(gcc + 0x8));
    WRITE_REGISTER_ULONG((volatile ULONG *)(gcc + 0x8), n | (1u << 16));   /* gcc_qupv3_wrap0_s6_clk */
    KeStallExecutionProcessor(100);
    MmUnmapIoSpace(gcc, 0x1000);
    wrap = MapPhys(QUP0_WRAP, 0x1000);
    qver = wrap ? READ_REGISTER_ULONG((volatile ULONG *)(wrap + 0x4)) : 0;
    if (wrap) MmUnmapIoSpace(wrap, 0x1000);
    samp = (qver >= 0x20050000) ? 16 : 32;
    sclk = SetS6Clock(384, 15625, 7372800);
    if (!sclk) return NULL;
    se = MapPhys(SE6_BASE, 0x4000);
    if (!se) { LOG("bt: map se6 failed"); return NULL; }
    fw = R(GENI_FW_REVISION_RO);
    if (((fw >> 8) & 0xFF) != 2) { LOG("bt: SE6 is not UART firmware (%08X) - abort", fw); MmUnmapIoSpace(se, 0x4000); return NULL; }
    UartSetup(se, sclk / (115200 * samp));
    g_rxn = g_rxp = 0;
    UartRx(se, g_rxb, sizeof(g_rxb), 50);           /* drain anything pending */
    g_rxn = g_rxp = 0;
    n = HciCmd(se, 0x0C03, NULL, 0, NULL, 0, 1000);
    LOG("bt: HCI_Reset @115200 st=%lX", n);
    if (n != 0 || !ReadVersion(se, &rom)) goto fail;
    g_nvmBaud = 0;
    if (fast) {
        /* hci_qca.c qca_set_speed() for QCA6390: EDL baud command, give it 300 ms, then follow on the host */
        static const UCHAR baud[] = { 0x01, 0x48, 0xFC, 0x01, 0x0E };
        UartTx(se, baud, sizeof(baud));
        MsSleep(300);
        if (SetS6Clock(4, 25, 48000000)) {
            UartSetup(se, 48000000 / (3000000 * samp));
            MsSleep(10);
            g_rxn = g_rxp = 0;
            UartRx(se, g_rxb, sizeof(g_rxb), 20);      /* drop the (garbled) baud response */
            g_rxn = g_rxp = 0;
            if (ReadVersion(se, &rom)) { g_nvmBaud = 0x0E; LOG("bt: running at 3 Mbaud"); }
            else { LOG("bt: no answer at 3 Mbaud"); goto fail; }
        } else goto fail;
    }
    if (BtFirmware(se, rom, bdaddr)) {
        LOG("bt: controller ready");
        return se;                       /* stays mapped: the HCI transport uses it */
    }
fail:
    LOG("bt: setup failed s_irq=%08X rxfifo=%08X ios=%08X", R(SE_GENI_S_IRQ_STATUS), R(SE_GENI_RX_FIFO_STATUS), R(SE_GENI_IOS));
    MmUnmapIoSpace(se, 0x4000);
    return NULL;
}


/* ======================================================================================
 * HCI transport for the Windows Bluetooth stack (BthMini).
 * This FDO (\_SB.WPWR) is a tiny bus driver: once the controller is up it adds a child PDO with
 * compatible ID MS_BTHX_BTHMINI, so the inbox BthMini/BthPort stack loads on it. The child forwards
 * the IOCTL_BTHX_* requests here (model: Windows-driver-samples bluetooth/serialhcibus).
 *  - TX: H4 frames written polled to the GENI TX FIFO (CTS honored), serialized by a wait lock.
 *  - RX: uart6 interrupt (GSIV 0x27F from WPWR _CRS): ISR drains the RX FIFO into a ring,
 *        DPC parses H4 (event / ACL) and completes READ_HCI requests or parks packets in a list.
 * ====================================================================================== */

#define RING_SIZE   8192
#define MAX_PKT     (4 + 1021)

typedef struct _PKT { LIST_ENTRY le; ULONG len; UCHAR type; UCHAR data[1]; } PKT, *PPKT;

typedef struct _FDO_CTX {
    WDFDEVICE dev;
    PUCHAR se;                      /* QUP0 SE6, set once the controller is ready (ISR data path) */
    PUCHAR seMask;                  /* QUP0 SE6 mapped at D0 entry, used to mask a stray interrupt */
    WDFINTERRUPT irq;
    BOOLEAN started, ready, childAdded;
    UCHAR bdaddr[6];
    /* RX ring (ISR producer, DPC consumer; synchronized with the interrupt lock) */
    UCHAR ring[RING_SIZE]; ULONG rhead, rtail, rdrop;
    /* H4 parser (DPC only) */
    UCHAR pk[1 + MAX_PKT]; ULONG pkn, pkneed; UCHAR pktype; BOOLEAN hdrDone;
    /* read side */
    WDFSPINLOCK pktLock;
    WDFQUEUE ioQueue, evtQueue, aclQueue;
    LIST_ENTRY evtList, aclList; ULONG evtCount, aclCount;
    WDFSPINLOCK txLock;    /* spinlock: the WAKE_ACK is sent straight from the RX DPC */
    BTHX_CAPABILITIES caps;
    ULONG cntTx, cntEvt, cntAcl, cntSco, isrCount, ioctlLogged, polls, rawLogged;
    volatile BOOLEAN ibsAwake; ULONG ibsWakeInd, ibsSleepInd, ibsWakeTimeout;
    KEVENT wakeAck; WDFWORKITEM ackWork;
    WDFSPINLOCK parseLock;
    WDFTIMER poll;
} FDO_CTX, *PFDO_CTX;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(FDO_CTX, FdoCtx)

typedef struct _PDO_CTX { WDFDEVICE parent; } PDO_CTX, *PPDO_CTX;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(PDO_CTX, PdoCtx)

EVT_WDF_INTERRUPT_ISR BtIsr;
EVT_WDF_INTERRUPT_DPC BtDpc;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL FdoIoctl;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL PdoIoctl;
EVT_WDF_IO_QUEUE_IO_INTERNAL_DEVICE_CONTROL PdoInternalIoctl;

#define RXBITS ((1u << 26) | (1u << 27))       /* RX_FIFO_WATERMARK | RX_FIFO_LAST */

/* move RX FIFO bytes into the ring; caller holds the interrupt lock (or is the ISR) */
static VOID DrainFifo(PFDO_CTX c)
{
    PUCHAR se = c->se; ULONG st, wc, i, k, nb, w;
    for (;;) {
        st = R(SE_GENI_RX_FIFO_STATUS); wc = st & 0x1FFFFFF;
        if (!wc) break;
        for (i = 0; i < wc; i++) {
            w = R(SE_GENI_RX_FIFOn);
            nb = 4;
            if (i == wc - 1 && (st & 0x80000000u)) { nb = (st >> 28) & 7; if (!nb) nb = 4; }
            for (k = 0; k < nb; k++) {
                ULONG nh = (c->rhead + 1) % RING_SIZE;
                if (nh == c->rtail) { c->rdrop++; continue; }
                c->ring[c->rhead] = (UCHAR)(w >> (8 * k)); c->rhead = nh;
            }
        }
    }
}

BOOLEAN BtIsr(WDFINTERRUPT irq, ULONG msgId)
{
    PFDO_CTX c = FdoCtx(WdfInterruptGetDevice(irq));
    PUCHAR se = c->se; ULONG m, s;
    UNREFERENCED_PARAMETER(msgId);
    if (!se) {      /* not ours yet: make sure the (level) line drops, or a single CPU livelocks */
        se = c->seMask;
        if (!se) return FALSE;
        W(SE_IRQ_EN, 0); W(SE_GENI_M_IRQ_EN, 0); W(SE_GENI_S_IRQ_EN, 0);
        W(SE_GENI_M_IRQ_CLEAR, 0xFFFFFFFF); W(SE_GENI_S_IRQ_CLEAR, 0xFFFFFFFF);
        c->rdrop++;
        return TRUE;
    }
    m = R(SE_GENI_M_IRQ_STATUS); s = R(SE_GENI_S_IRQ_STATUS);
    c->isrCount++;
    if (!((m & (RXBITS | 0x80000000u)) | s)) return FALSE;
    DrainFifo(c);
    W(SE_GENI_S_IRQ_CLEAR, s);
    W(SE_GENI_M_IRQ_CLEAR, m & ~1u);           /* leave M_CMD_DONE for the TX poller */
    WdfInterruptQueueDpcForIsr(irq);
    return TRUE;
}

static VOID CompleteRead(WDFREQUEST req, UCHAR type, const UCHAR *data, ULONG len)
{
    PBTHX_HCI_READ_WRITE_CONTEXT h; size_t cap; NTSTATUS st;
    ULONG need = (ULONG)FIELD_OFFSET(BTHX_HCI_READ_WRITE_CONTEXT, Data) + len;
    st = WdfRequestRetrieveOutputBuffer(req, need, (PVOID *)&h, &cap);
    if (!NT_SUCCESS(st)) { WdfRequestComplete(req, st); return; }
    h->Type = type; h->DataLen = len;
    RtlCopyMemory(h->Data, data, len);
    WdfRequestCompleteWithInformation(req, STATUS_SUCCESS, need);
}

/* a complete packet from the controller: hand to a waiting READ_HCI or park it */
static VOID Deliver(PFDO_CTX c, UCHAR type, const UCHAR *data, ULONG len)
{
    WDFQUEUE q = (type == HciPacketEvent) ? c->evtQueue : c->aclQueue;
    PLIST_ENTRY list = (type == HciPacketEvent) ? &c->evtList : &c->aclList;
    PULONG cnt = (type == HciPacketEvent) ? &c->evtCount : &c->aclCount;
    WDFREQUEST req; NTSTATUS st; PPKT p;
    if (type == HciPacketEvent) c->cntEvt++; else c->cntAcl++;
    WdfSpinLockAcquire(c->pktLock);
    st = WdfIoQueueRetrieveNextRequest(q, &req);
    if (NT_SUCCESS(st)) {
        WdfSpinLockRelease(c->pktLock);
        CompleteRead(req, type, data, len);
        return;
    }
    p = (PPKT)ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(PKT) + len, 'kPtB');
    if (p && *cnt < 512) {
        p->len = len; p->type = type; RtlCopyMemory(p->data, data, len);
        InsertTailList(list, &p->le); (*cnt)++;
        p = NULL;
    }
    WdfSpinLockRelease(c->pktLock);
    if (p) ExFreePool(p);
}

static VOID ParseByte(PFDO_CTX c, UCHAR b)
{
    if (c->pkn == 0) {
        /* QCA in-band sleep (hci_qca.c): FD = controller wants to send (needs our WAKE_ACK FC),
         * FC = ack of our wake, FE = controller going to sleep */
        if (b == 0xFD) {   /* answer at once from the DPC (a work item added visible input lag) */
            static const UCHAR ack = 0xFC;
            c->ibsWakeInd++;
            WdfSpinLockAcquire(c->txLock); UartTxIbs(c->se, ack); c->ibsAwake = TRUE; WdfSpinLockRelease(c->txLock);
            return;
        }
        if (b == 0xFC) { c->ibsAwake = TRUE; KeSetEvent(&c->wakeAck, 0, FALSE); return; }
        if (b == 0xFE) {   /* controller wants to sleep: wake it straight back up (sleep/wake round trips lag HID input) */
            static const UCHAR wake = 0xFD;
            c->ibsAwake = FALSE; c->ibsSleepInd++;
            WdfSpinLockAcquire(c->txLock); UartTxIbs(c->se, wake); WdfSpinLockRelease(c->txLock);
            return;
        }
        if (b != 0x04 && b != 0x02 && b != 0x03) return;   /* wait for an event, ACL or SCO type byte */
        c->pktype = b; c->pkneed = (b == 0x02) ? 5 : 3; c->hdrDone = FALSE;   /* evt: 1+2, sco: 1+2+1 -> see below */
        if (b == 0x03) c->pkneed = 4;
    }
    c->pk[c->pkn++] = b;
    if (c->pkn < c->pkneed) return;
    if (!c->hdrDone) {                                 /* header complete: add the payload length */
        ULONG pl = (c->pktype == 0x04) ? c->pk[2] : (c->pktype == 0x03) ? c->pk[3] : (ULONG)(c->pk[3] | (c->pk[4] << 8));
        c->hdrDone = TRUE;
        c->pkneed += pl;
        if (c->pkneed > sizeof(c->pk)) { c->pkn = 0; return; }   /* garbage - resync */
        if (pl) return;
    }
    if (c->pktype == 0x03) c->cntSco++;                /* SCO over HCI is not supported: drop, but stay in sync */
    else Deliver(c, c->pktype == 0x04 ? (UCHAR)HciPacketEvent : (UCHAR)HciPacketAclData, c->pk + 1, c->pkn - 1);
    c->pkn = 0;
}

static VOID ProcessRing(PFDO_CTX c, WDFINTERRUPT irq)
{
    UCHAR tmp[256]; ULONG n, i;
    WdfSpinLockAcquire(c->parseLock);
    for (;;) {
        n = 0;
        WdfInterruptAcquireLock(irq);
        while (c->rtail != c->rhead && n < sizeof(tmp)) { tmp[n++] = c->ring[c->rtail]; c->rtail = (c->rtail + 1) % RING_SIZE; }
        WdfInterruptReleaseLock(irq);
        if (!n) break;
        for (i = 0; i < n; i++) ParseByte(c, tmp[i]);
    }
    WdfSpinLockRelease(c->parseLock);
}

VOID BtDpc(WDFINTERRUPT irq, WDFOBJECT obj)
{
    UNREFERENCED_PARAMETER(obj);
    ProcessRing(FdoCtx(WdfInterruptGetDevice(irq)), irq);
}

/* safety-net RX poll (50 ms): normally the uart6 interrupt does the work */
EVT_WDF_TIMER RxPoll;
VOID RxPoll(WDFTIMER t)
{
    PFDO_CTX c = FdoCtx((WDFDEVICE)WdfTimerGetParentObject(t));
    if (!c->ready) return;
    WdfInterruptAcquireLock(c->irq);
    DrainFifo(c);
    { PUCHAR se = c->se; W(SE_GENI_S_IRQ_CLEAR, R(SE_GENI_S_IRQ_STATUS) & RXBITS); }
    WdfInterruptReleaseLock(c->irq);
    ProcessRing(c, c->irq);
    c->polls++;
}

EVT_WDF_WORKITEM AckWork;
VOID AckWork(WDFWORKITEM wi)
{
    PFDO_CTX c = FdoCtx((WDFDEVICE)WdfWorkItemGetParentObject(wi));
    static const UCHAR ack = 0xFC;
    WdfSpinLockAcquire(c->txLock);
    UartTxIbs(c->se, ack);
    c->ibsAwake = TRUE;
    WdfSpinLockRelease(c->txLock);
}

static NTSTATUS WriteHci(PFDO_CTX c, WDFREQUEST req, size_t inLen)
{
    PBTHX_HCI_READ_WRITE_CONTEXT h; size_t len; NTSTATUS st; UCHAR *buf;
    st = WdfRequestRetrieveInputBuffer(req, sizeof(BTHX_HCI_READ_WRITE_CONTEXT), (PVOID *)&h, &len);
    if (!NT_SUCCESS(st)) return st;
    UNREFERENCED_PARAMETER(inLen);
    if (h->DataLen > MAX_PKT || FIELD_OFFSET(BTHX_HCI_READ_WRITE_CONTEXT, Data) + h->DataLen > len) return STATUS_INVALID_PARAMETER;
    if (h->Type != HciPacketCommand && h->Type != HciPacketAclData) return STATUS_INVALID_PARAMETER;
    if (!c->ready) return STATUS_DEVICE_NOT_READY;
    buf = (UCHAR *)ExAllocatePool2(POOL_FLAG_NON_PAGED, 1 + h->DataLen, 'xTtB');
    if (!buf) return STATUS_INSUFFICIENT_RESOURCES;
    buf[0] = h->Type; RtlCopyMemory(buf + 1, h->Data, h->DataLen);
    if (!c->ibsAwake) {        /* HCI_IBS_WAKE_IND, wait for WAKE_ACK (no lock held while waiting) */
        static const UCHAR wake = 0xFD; ULONG tries; LARGE_INTEGER to; to.QuadPart = -10000LL * 20;
        for (tries = 0; tries < 10 && !c->ibsAwake; tries++) {
            KeClearEvent(&c->wakeAck);
            WdfSpinLockAcquire(c->txLock); UartTxIbs(c->se, wake); WdfSpinLockRelease(c->txLock);
            KeWaitForSingleObject(&c->wakeAck, Executive, KernelMode, FALSE, &to);
        }
        if (!c->ibsAwake) { c->ibsWakeTimeout++; c->ibsAwake = TRUE; LOG("bthx: no WAKE_ACK from controller, sending anyway"); }
    }
    WdfSpinLockAcquire(c->txLock);
    UartTx(c->se, buf, 1 + h->DataLen);
    WdfSpinLockRelease(c->txLock);
    ExFreePool(buf);
    c->cntTx++;
    WdfRequestCompleteWithInformation(req, STATUS_SUCCESS, h->DataLen);
    return STATUS_PENDING;   /* already completed */
}

VOID FdoIoctl(WDFQUEUE q, WDFREQUEST req, size_t outLen, size_t inLen, ULONG code)
{
    PFDO_CTX c = FdoCtx(WdfIoQueueGetDevice(q));
    NTSTATUS st = STATUS_SUCCESS; ULONG_PTR info = 0; PVOID p;
    switch (code) {
    case IOCTL_BTHX_GET_VERSION:
        st = WdfRequestRetrieveOutputBuffer(req, sizeof(BTHX_VERSION), &p, NULL);
        if (NT_SUCCESS(st)) { RtlCopyMemory(p, &Microsoft_BTHX_DDI_Version, sizeof(BTHX_VERSION)); info = sizeof(BTHX_VERSION); }
        break;
    case IOCTL_BTHX_SET_VERSION:
        st = WdfRequestRetrieveInputBuffer(req, sizeof(BTHX_VERSION), &p, NULL);
        if (NT_SUCCESS(st)) LOG("bthx: set version %lx", ((BTHX_VERSION *)p)->Version);
        break;
    case IOCTL_BTHX_QUERY_CAPABILITIES:
        st = WdfRequestRetrieveOutputBuffer(req, sizeof(BTHX_CAPABILITIES), &p, NULL);
        if (NT_SUCCESS(st)) { RtlCopyMemory(p, &c->caps, sizeof(BTHX_CAPABILITIES)); info = sizeof(BTHX_CAPABILITIES); }
        break;
    case IOCTL_BTHX_WRITE_HCI:
        st = WriteHci(c, req, inLen);
        if (st == STATUS_PENDING) return;
        break;
    case IOCTL_BTHX_READ_HCI: {
        BTHX_HCI_PACKET_TYPE *pt; PPKT pk = NULL; WDFQUEUE tq; PLIST_ENTRY list; PULONG cnt;
        st = WdfRequestRetrieveInputBuffer(req, sizeof(BTHX_HCI_PACKET_TYPE), (PVOID *)&pt, NULL);
        if (!NT_SUCCESS(st)) break;
        if (*pt == HciPacketEvent) { tq = c->evtQueue; list = &c->evtList; cnt = &c->evtCount; }
        else if (*pt == HciPacketAclData) { tq = c->aclQueue; list = &c->aclList; cnt = &c->aclCount; }
        else { st = STATUS_INVALID_PARAMETER; break; }
        WdfSpinLockAcquire(c->pktLock);
        if (!IsListEmpty(list)) { pk = CONTAINING_RECORD(RemoveHeadList(list), PKT, le); (*cnt)--; }
        else st = WdfRequestForwardToIoQueue(req, tq);
        WdfSpinLockRelease(c->pktLock);
        if (pk) { CompleteRead(req, pk->type, pk->data, pk->len); ExFreePool(pk); return; }
        if (NT_SUCCESS(st)) return;
        break; }
    default:
        UNREFERENCED_PARAMETER(outLen);
        st = STATUS_NOT_SUPPORTED;
        LOG("bthx: unknown ioctl %08lX", code);
    }
    WdfRequestCompleteWithInformation(req, st, info);
}

/* ---------------- child PDO for BthMini ---------------- */
VOID PdoIoctl(WDFQUEUE q, WDFREQUEST req, size_t outLen, size_t inLen, ULONG code)
{
    PPDO_CTX pc = PdoCtx(WdfIoQueueGetDevice(q));
    WDF_REQUEST_FORWARD_OPTIONS fo; NTSTATUS st;
    UNREFERENCED_PARAMETER(outLen); UNREFERENCED_PARAMETER(inLen); UNREFERENCED_PARAMETER(code);
    WDF_REQUEST_FORWARD_OPTIONS_INIT(&fo);
    st = WdfRequestForwardToParentDeviceIoQueue(req, FdoCtx(pc->parent)->ioQueue, &fo);
    if (!NT_SUCCESS(st)) WdfRequestComplete(req, st);
}
VOID PdoInternalIoctl(WDFQUEUE q, WDFREQUEST req, size_t outLen, size_t inLen, ULONG code)
{
    PdoIoctl(q, req, outLen, inLen, code);
}

static NTSTATUS AddChild(PFDO_CTX c)
{
    PWDFDEVICE_INIT init; NTSTATUS st; WDFDEVICE child; WDF_OBJECT_ATTRIBUTES a; WDF_IO_QUEUE_CONFIG qc;
    WDF_DEVICE_PNP_CAPABILITIES caps;
    DECLARE_CONST_UNICODE_STRING(hwid, L"PIPA\\QCA6390_UART");
    DECLARE_CONST_UNICODE_STRING(compat, L"MS_BTHX_BTHMINI");
    DECLARE_CONST_UNICODE_STRING(inst, L"00");
    DECLARE_CONST_UNICODE_STRING(text, L"Qualcomm QCA6390 Bluetooth (pipa)");
    DECLARE_CONST_UNICODE_STRING(loc, L"pipa uart6");
    init = WdfPdoInitAllocate(c->dev);
    if (!init) return STATUS_INSUFFICIENT_RESOURCES;
    st = WdfPdoInitAssignDeviceID(init, &hwid);
    if (NT_SUCCESS(st)) st = WdfPdoInitAddHardwareID(init, &hwid);
    if (NT_SUCCESS(st)) st = WdfPdoInitAddCompatibleID(init, &compat);
    if (NT_SUCCESS(st)) st = WdfPdoInitAssignInstanceID(init, &inst);
    if (NT_SUCCESS(st)) st = WdfPdoInitAddDeviceText(init, &text, &loc, 0x409);
    if (!NT_SUCCESS(st)) { WdfDeviceInitFree(init); return st; }
    WdfPdoInitSetDefaultLocale(init, 0x409);
    WdfPdoInitAllowForwardingRequestToParent(init);
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&a, PDO_CTX);
    st = WdfDeviceCreate(&init, &a, &child);
    if (!NT_SUCCESS(st)) { WdfDeviceInitFree(init); return st; }
    PdoCtx(child)->parent = c->dev;
    WDF_DEVICE_PNP_CAPABILITIES_INIT(&caps);
    caps.Removable = WdfFalse; caps.SurpriseRemovalOK = WdfTrue; caps.UniqueID = WdfTrue;
    WdfDeviceSetPnpCapabilities(child, &caps);
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&qc, WdfIoQueueDispatchParallel);
    qc.EvtIoDeviceControl = PdoIoctl;
    qc.EvtIoInternalDeviceControl = PdoInternalIoctl;
    qc.PowerManaged = WdfFalse;   /* BthMini sends IOCTLs from its AddDevice, before this PDO is in D0 */
    st = WdfIoQueueCreate(child, &qc, WDF_NO_OBJECT_ATTRIBUTES, NULL);
    if (!NT_SUCCESS(st)) { WdfObjectDelete(child); return st; }
    WdfFdoLockStaticChildListForIteration(c->dev);
    st = WdfFdoAddStaticChild(c->dev, child);
    WdfFdoUnlockStaticChildListFromIteration(c->dev);
    if (!NT_SUCCESS(st)) WdfObjectDelete(child);
    return st;
}

/* ---------------- bring-up thread ---------------- */
static VOID GetBdAddr(PFDO_CTX c)
{
    WDFKEY k; UNICODE_STRING n; ULONG lo = 0, hi = 0; BOOLEAN have = FALSE;
    RtlInitUnicodeString(&n, L"BdAddrLo");
    if (NT_SUCCESS(WdfDeviceOpenRegistryKey(c->dev, PLUGPLAY_REGKEY_DEVICE, KEY_READ | KEY_WRITE, WDF_NO_OBJECT_ATTRIBUTES, &k))) {
        UNICODE_STRING n2; RtlInitUnicodeString(&n2, L"BdAddrHi");
        if (NT_SUCCESS(WdfRegistryQueryULong(k, &n, &lo)) && NT_SUCCESS(WdfRegistryQueryULong(k, &n2, &hi))) have = TRUE;
        if (!have) {   /* first boot: make one up and keep it (Xiaomi-style OUI-less, but stable per install) */
            LARGE_INTEGER t; ULONG seed;
            KeQuerySystemTimePrecise(&t); seed = t.LowPart ^ (ULONG)(t.QuadPart >> 32) ^ (ULONG)(ULONG_PTR)c;
            seed = seed * 1103515245u + 12345u; lo = seed ^ (seed >> 13);
            seed = seed * 1103515245u + 12345u; hi = 0x2200 | ((seed >> 16) & 0xFF);   /* 22:xx:... */
            WdfRegistryAssignULong(k, &n, lo); WdfRegistryAssignULong(k, &n2, hi);
            LOG("bt: generated bd_addr");
        }
        WdfRegistryClose(k);
    }
    c->bdaddr[0] = (UCHAR)lo; c->bdaddr[1] = (UCHAR)(lo >> 8); c->bdaddr[2] = (UCHAR)(lo >> 16);
    c->bdaddr[3] = (UCHAR)(lo >> 24); c->bdaddr[4] = (UCHAR)hi; c->bdaddr[5] = (UCHAR)(hi >> 8);
}

static KSTART_ROUTINE BtThread;
static VOID BtThread(PVOID ctx)
{
    PFDO_CTX c = (PFDO_CTX)ctx; PUCHAR se; NTSTATUS st;
    GetBdAddr(c);
    /* BT_EN low->high so the QCA6390 BT core boots fresh, then talk to it */
    SetOut(21, 0); MsSleep(50); SetOut(21, 1); MsSleep(150);
    se = BtProbe(c->bdaddr, TRUE);
    if (!se) {      /* retry the proven 115200 path from a fresh controller boot */
        SetOut(21, 0); MsSleep(50); SetOut(21, 1); MsSleep(150);
        se = BtProbe(c->bdaddr, FALSE);
    }
    if (se) {
        /* switch RX to interrupt mode */
        W(SE_GENI_S_IRQ_CLEAR, 0xFFFFFFFF); W(SE_GENI_M_IRQ_CLEAR, 0xFFFFFFFF);
        c->se = se;
        W(SE_GENI_S_IRQ_EN, RXBITS);
        W(SE_GENI_M_IRQ_EN, RXBITS);
        c->ready = TRUE;
        g_verbose = FALSE;
        WdfTimerStart(c->poll, WDF_REL_TIMEOUT_IN_MS(50));
        st = AddChild(c);
        c->childAdded = NT_SUCCESS(st);
        LOG("bt: HCI transport up, child PDO st=%08X (stray irqs before ready: %lu)", st, c->rdrop);
    }
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
    WDF_PNPPOWER_EVENT_CALLBACKS pnp; WDFDEVICE dev; WDF_OBJECT_ATTRIBUTES a; NTSTATUS st;
    WDF_INTERRUPT_CONFIG ic; WDF_IO_QUEUE_CONFIG qc; PFDO_CTX c;
    UNREFERENCED_PARAMETER(drv);
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnp);
    pnp.EvtDeviceD0Entry = PwrD0Entry;
    WdfDeviceInitSetPnpPowerEventCallbacks(init, &pnp);
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&a, FDO_CTX);
    st = WdfDeviceCreate(&init, &a, &dev);
    if (!NT_SUCCESS(st)) return st;
    c = FdoCtx(dev); c->dev = dev;
    InitializeListHead(&c->evtList); InitializeListHead(&c->aclList);
    c->caps.MaxAclTransferInSize = MAX_PKT;
    c->caps.ScoSupport = ScoSupportHCIBypass;
    c->caps.MaxScoChannels = 1;
    c->caps.IsDeviceIdleCapable = FALSE;
    c->caps.IsDeviceWakeCapable = FALSE;
    WDF_OBJECT_ATTRIBUTES_INIT(&a); a.ParentObject = dev;
    st = WdfSpinLockCreate(&a, &c->pktLock); if (!NT_SUCCESS(st)) return st;
    st = WdfSpinLockCreate(&a, &c->txLock); if (!NT_SUCCESS(st)) return st;
    st = WdfSpinLockCreate(&a, &c->parseLock); if (!NT_SUCCESS(st)) return st;
    KeInitializeEvent(&c->wakeAck, NotificationEvent, FALSE);
    c->ibsAwake = TRUE;
    {
        WDF_WORKITEM_CONFIG wc; WDF_OBJECT_ATTRIBUTES wa;
        WDF_WORKITEM_CONFIG_INIT(&wc, AckWork); wc.AutomaticSerialization = FALSE;
        WDF_OBJECT_ATTRIBUTES_INIT(&wa); wa.ParentObject = dev;
        st = WdfWorkItemCreate(&wc, &wa, &c->ackWork); if (!NT_SUCCESS(st)) return st;
    }
    {
        WDF_TIMER_CONFIG tc; WDF_OBJECT_ATTRIBUTES ta;
        WDF_TIMER_CONFIG_INIT_PERIODIC(&tc, RxPoll, 50);
        tc.AutomaticSerialization = FALSE;
        WDF_OBJECT_ATTRIBUTES_INIT(&ta); ta.ParentObject = dev;
        st = WdfTimerCreate(&tc, &ta, &c->poll); if (!NT_SUCCESS(st)) return st;
    }
    WDF_INTERRUPT_CONFIG_INIT(&ic, BtIsr, BtDpc);
    st = WdfInterruptCreate(dev, &ic, WDF_NO_OBJECT_ATTRIBUTES, &c->irq);
    LOG("interrupt create st=%08X", st);
    if (!NT_SUCCESS(st)) return st;
    /* forwarded BTHX requests; passive because WRITE_HCI does polled TX under a wait lock */
    WDF_IO_QUEUE_CONFIG_INIT(&qc, WdfIoQueueDispatchParallel);
    qc.EvtIoDeviceControl = FdoIoctl;
    qc.EvtIoInternalDeviceControl = FdoIoctl;
    qc.PowerManaged = WdfFalse;
    WDF_OBJECT_ATTRIBUTES_INIT(&a); a.ExecutionLevel = WdfExecutionLevelPassive;
    st = WdfIoQueueCreate(dev, &qc, &a, &c->ioQueue); if (!NT_SUCCESS(st)) return st;
    WDF_IO_QUEUE_CONFIG_INIT(&qc, WdfIoQueueDispatchManual);
    qc.PowerManaged = WdfFalse;
    st = WdfIoQueueCreate(dev, &qc, WDF_NO_OBJECT_ATTRIBUTES, &c->evtQueue); if (!NT_SUCCESS(st)) return st;
    st = WdfIoQueueCreate(dev, &qc, WDF_NO_OBJECT_ATTRIBUTES, &c->aclQueue);
    return st;
}

NTSTATUS PwrD0Entry(WDFDEVICE dev, WDF_POWER_DEVICE_STATE prev)
{
    PFDO_CTX c = FdoCtx(dev); ULONG i;
    LOG("D0 entry (from D%d)", (int)prev - 1);
    if (c->started) {
        /* resume: BT_EN stays high, so the controller keeps its firmware; re-assert our SE6 clock vote and
         * the RX interrupt enables in case the QUP lost them (full re-init on loss is not implemented yet) */
        PUCHAR g = MapPhys(GCC_VOTE_PAGE, 0x1000), se = c->se;
        if (g) { WRITE_REGISTER_ULONG((volatile ULONG *)(g + 0x8), READ_REGISTER_ULONG((volatile ULONG *)(g + 0x8)) | (1u << 16)); MmUnmapIoSpace(g, 0x1000); }
        if (se && c->ready) {
            LOG("resume: se6 fw=%08X s_en=%08X status=%08X", R(GENI_FW_REVISION_RO), R(SE_GENI_S_IRQ_EN), R(SE_GENI_STATUS));
            W(SE_GENI_S_IRQ_EN, RXBITS); W(SE_GENI_M_IRQ_EN, RXBITS); W(SE_IRQ_EN, R(SE_IRQ_EN) | 0xC);
            if (!(R(SE_GENI_STATUS) & (1u << 12))) W(SE_GENI_S_CMD0, 1u << 27);   /* receiver stopped: restart */
        }
        return STATUS_SUCCESS;
    }
    c->started = TRUE;
    {   /* uart6's interrupt gets connected right after this callback: clock SE6 and mask it first */
        PUCHAR g = MapPhys(GCC_VOTE_PAGE, 0x1000), se;
        if (g) { WRITE_REGISTER_ULONG((volatile ULONG *)(g + 0x8), READ_REGISTER_ULONG((volatile ULONG *)(g + 0x8)) | (1u << 16)); MmUnmapIoSpace(g, 0x1000); }
        KeStallExecutionProcessor(100);
        se = c->seMask = MapPhys(SE6_BASE, 0x4000);
        if (se) {
            LOG("se6 before mask: se_irq_en=%08X m_en=%08X s_en=%08X m_st=%08X s_st=%08X", R(SE_IRQ_EN), R(SE_GENI_M_IRQ_EN),
                R(SE_GENI_S_IRQ_EN), R(SE_GENI_M_IRQ_STATUS), R(SE_GENI_S_IRQ_STATUS));
            W(SE_IRQ_EN, 0); W(SE_GENI_M_IRQ_EN, 0); W(SE_GENI_S_IRQ_EN, 0);
            W(SE_GENI_M_IRQ_CLEAR, 0xFFFFFFFF); W(SE_GENI_S_IRQ_CLEAR, 0xFFFFFFFF);
        }
    }
    for (i = 16; i <= 19; i++) SetFunc(i, 1 /* qup6 */, PULL_UP, 1 /* 4 mA */);
    SetOut(21, 1);                         /* BT_EN */
    KeStallExecutionProcessor(20000);
    SetOut(20, 1);                         /* WLAN_EN */
    KeStallExecutionProcessor(20000);
    Show("wlan_en", 20); Show("bt_en", 21);
    {
        HANDLE th; NTSTATUS st = PsCreateSystemThread(&th, THREAD_ALL_ACCESS, NULL, NULL, NULL, BtThread, c);
        LOG("bt thread start st=%08X", st);
        if (NT_SUCCESS(st)) ZwClose(th);
    }
    return STATUS_SUCCESS;
}
