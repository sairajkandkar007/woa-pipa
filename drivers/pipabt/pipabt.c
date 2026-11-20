/*
 * pipabt.c - stage 1 probe for the QCA6390 Bluetooth on Xiaomi Pad 6 (pipa).
 * Binds \_SB.BTH0 (ACPI\QCOM2571) instead of Qualcomm's qcbtfmuart (which only knows
 * WCN399x/ROME firmware). Opens the UART (uart6 via qcuart/SerCx2), pulses BT_EN (gpio21),
 * then sends H4 HCI_Reset and the Qualcomm "get version" vendor command and logs replies
 * (DbgPrint + device registry key: BtRxReset, BtRxVer, BtStatus).
 * Protocol: Linux drivers/bluetooth/btqca.c (EDL_PATCH_VER_REQ_CMD 0x19 via opcode 0xFC00).
 */
#include <ntddk.h>
#include <wdf.h>
#include <ntddser.h>
#define RESHUB_USE_HELPER_ROUTINES
#include <reshub.h>

#define LOG(fmt, ...) DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "pipabt: " fmt "\n", __VA_ARGS__)
#define TLMM_NORTH 0x0F900000ull
#define BT_EN 21

typedef struct _DEV_CTX { LARGE_INTEGER conn; WDFIOTARGET uart; } DEV_CTX, *PDEV_CTX;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEV_CTX, GetCtx)

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD BtAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE BtPrepare;
EVT_WDF_DEVICE_RELEASE_HARDWARE BtRelease;
EVT_WDF_DEVICE_D0_ENTRY BtD0;

static VOID Sleep(ULONG ms) { LARGE_INTEGER t; t.QuadPart = -10000LL * ms; KeDelayExecutionThread(KernelMode, FALSE, &t); }

static VOID BtEn(ULONG v)
{
    PHYSICAL_ADDRESS a; PUCHAR p; a.QuadPart = TLMM_NORTH + 0x1000ull * BT_EN;
    p = (PUCHAR)MmMapIoSpaceEx(a, 0x1000, PAGE_READWRITE | PAGE_NOCACHE);
    if (!p) { LOG("map tlmm failed"); return; }
    WRITE_REGISTER_ULONG((volatile ULONG *)(p + 4), v ? 2u : 0u);
    LOG("bt_en=%lu ctl=%08X io=%08X", v, READ_REGISTER_ULONG((volatile ULONG *)p), READ_REGISTER_ULONG((volatile ULONG *)(p + 4)));
    MmUnmapIoSpace(p, 0x1000);
}

static VOID Save(WDFDEVICE d, PCWSTR name, PVOID buf, ULONG len, ULONG type)
{
    WDFKEY k; UNICODE_STRING n;
    if (!NT_SUCCESS(WdfDeviceOpenRegistryKey(d, PLUGPLAY_REGKEY_DEVICE, KEY_WRITE, WDF_NO_OBJECT_ATTRIBUTES, &k))) return;
    RtlInitUnicodeString(&n, name);
    WdfRegistryAssignValue(k, &n, type, len, buf);
    WdfRegistryClose(k);
}

static NTSTATUS Ioctl(PDEV_CTX c, ULONG code, PVOID in, ULONG inLen)
{
    WDF_MEMORY_DESCRIPTOR m; WDF_REQUEST_SEND_OPTIONS o;
    WDF_REQUEST_SEND_OPTIONS_INIT(&o, WDF_REQUEST_SEND_OPTION_TIMEOUT);
    o.Timeout = WDF_REL_TIMEOUT_IN_MS(1000);
    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&m, in, inLen);
    return WdfIoTargetSendIoctlSynchronously(c->uart, NULL, code, inLen ? &m : NULL, NULL, &o, NULL);
}

static NTSTATUS Config(PDEV_CTX c, ULONG baud, BOOLEAN flow)
{
    SERIAL_BAUD_RATE b = { baud };
    SERIAL_LINE_CONTROL lc = { STOP_BIT_1, NO_PARITY, 8 };
    SERIAL_HANDFLOW hf = { 0 };
    SERIAL_TIMEOUTS to = { 0 };
    NTSTATUS s1, s2, s3, s4;
    if (flow) { hf.ControlHandShake = SERIAL_CTS_HANDSHAKE; hf.FlowReplace = SERIAL_RTS_HANDSHAKE; }
    to.ReadIntervalTimeout = 30; to.ReadTotalTimeoutConstant = 600;
    to.WriteTotalTimeoutConstant = 500;
    s1 = Ioctl(c, IOCTL_SERIAL_SET_BAUD_RATE, &b, sizeof(b));
    s2 = Ioctl(c, IOCTL_SERIAL_SET_LINE_CONTROL, &lc, sizeof(lc));
    s3 = Ioctl(c, IOCTL_SERIAL_SET_HANDFLOW, &hf, sizeof(hf));
    s4 = Ioctl(c, IOCTL_SERIAL_SET_TIMEOUTS, &to, sizeof(to));
    LOG("config baud=%lu flow=%u st=%08X %08X %08X %08X", baud, flow, s1, s2, s3, s4);
    return NT_SUCCESS(s1) ? s4 : s1;
}

/* send cmd, collect reply bytes for up to ~1.5 s; returns byte count */
static ULONG Xfer(PDEV_CTX c, const UCHAR *cmd, ULONG len, UCHAR *rx, ULONG cap, PCSTR what)
{
    WDF_MEMORY_DESCRIPTOR m; WDF_REQUEST_SEND_OPTIONS o; ULONG_PTR n = 0; ULONG got = 0, i, tries; NTSTATUS st;
    UCHAR tmp[64];
    WDF_REQUEST_SEND_OPTIONS_INIT(&o, WDF_REQUEST_SEND_OPTION_TIMEOUT);
    o.Timeout = WDF_REL_TIMEOUT_IN_MS(1000);
    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&m, (PVOID)cmd, len);
    st = WdfIoTargetSendWriteSynchronously(c->uart, NULL, &m, NULL, &o, &n);
    LOG("%s: write st=%08X n=%Iu", what, st, n);
    for (tries = 0; tries < 3 && got < cap; tries++) {
        n = 0;
        WDF_REQUEST_SEND_OPTIONS_INIT(&o, WDF_REQUEST_SEND_OPTION_TIMEOUT);
        o.Timeout = WDF_REL_TIMEOUT_IN_MS(1500);
        WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&m, tmp, min(sizeof(tmp), cap - got));
        st = WdfIoTargetSendReadSynchronously(c->uart, NULL, &m, NULL, &o, &n);
        for (i = 0; i < n; i++) rx[got++] = tmp[i];
        if (n == 0 && got) break;
        if (!NT_SUCCESS(st) && st != STATUS_TIMEOUT && st != STATUS_IO_TIMEOUT) break;
    }
    LOG("%s: read st=%08X total=%lu", what, st, got);
    for (i = 0; i < got; i += 8)
        LOG("  %02X %02X %02X %02X %02X %02X %02X %02X", rx[i], i+1<got?rx[i+1]:0, i+2<got?rx[i+2]:0, i+3<got?rx[i+3]:0,
            i+4<got?rx[i+4]:0, i+5<got?rx[i+5]:0, i+6<got?rx[i+6]:0, i+7<got?rx[i+7]:0);
    return got;
}

NTSTATUS DriverEntry(PDRIVER_OBJECT drv, PUNICODE_STRING path)
{
    WDF_DRIVER_CONFIG cfg; WDF_DRIVER_CONFIG_INIT(&cfg, BtAdd);
    return WdfDriverCreate(drv, path, WDF_NO_OBJECT_ATTRIBUTES, &cfg, WDF_NO_HANDLE);
}

NTSTATUS BtAdd(WDFDRIVER drv, PWDFDEVICE_INIT init)
{
    WDF_PNPPOWER_EVENT_CALLBACKS pnp; WDF_OBJECT_ATTRIBUTES a; WDFDEVICE dev;
    UNREFERENCED_PARAMETER(drv);
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnp);
    pnp.EvtDevicePrepareHardware = BtPrepare; pnp.EvtDeviceReleaseHardware = BtRelease; pnp.EvtDeviceD0Entry = BtD0;
    WdfDeviceInitSetPnpPowerEventCallbacks(init, &pnp);
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&a, DEV_CTX);
    return WdfDeviceCreate(&init, &a, &dev);
}

NTSTATUS BtPrepare(WDFDEVICE dev, WDFCMRESLIST raw, WDFCMRESLIST xl)
{
    PDEV_CTX c = GetCtx(dev); ULONG i; NTSTATUS st; BOOLEAN have = FALSE;
    WDF_OBJECT_ATTRIBUTES a; WDF_IO_TARGET_OPEN_PARAMS op; UNICODE_STRING path;
    DECLARE_UNICODE_STRING_SIZE(buf, RESOURCE_HUB_PATH_SIZE);
    UNREFERENCED_PARAMETER(raw);
    for (i = 0; i < WdfCmResourceListGetCount(xl); i++) {
        PCM_PARTIAL_RESOURCE_DESCRIPTOR r = WdfCmResourceListGetDescriptor(xl, i);
        if (r->Type == CmResourceTypeConnection && r->u.Connection.Class == CM_RESOURCE_CONNECTION_CLASS_SERIAL &&
            r->u.Connection.Type == CM_RESOURCE_CONNECTION_TYPE_SERIAL_UART) {
            c->conn.LowPart = r->u.Connection.IdLowPart; c->conn.HighPart = r->u.Connection.IdHighPart; have = TRUE; break;
        }
    }
    if (!have) { LOG("no UART connection resource"); return STATUS_INSUFFICIENT_RESOURCES; }
    st = RESOURCE_HUB_CREATE_PATH_FROM_ID(&buf, c->conn.LowPart, c->conn.HighPart);
    if (!NT_SUCCESS(st)) return st;
    path = buf;
    WDF_OBJECT_ATTRIBUTES_INIT(&a); a.ParentObject = dev;
    st = WdfIoTargetCreate(dev, &a, &c->uart);
    if (!NT_SUCCESS(st)) return st;
    WDF_IO_TARGET_OPEN_PARAMS_INIT_OPEN_BY_NAME(&op, &path, GENERIC_READ | GENERIC_WRITE);
    st = WdfIoTargetOpen(c->uart, &op);
    LOG("open UART target st=0x%08X", st);
    return st;
}

NTSTATUS BtRelease(WDFDEVICE dev, WDFCMRESLIST xl)
{
    PDEV_CTX c = GetCtx(dev); UNREFERENCED_PARAMETER(xl);
    if (c->uart) { WdfIoTargetClose(c->uart); WdfObjectDelete(c->uart); c->uart = NULL; }
    return STATUS_SUCCESS;
}

NTSTATUS BtD0(WDFDEVICE dev, WDF_POWER_DEVICE_STATE prev)
{
    static const UCHAR reset[] = { 0x01, 0x03, 0x0C, 0x00 };
    static const UCHAR ver[] = { 0x01, 0x00, 0xFC, 0x01, 0x19 };
    PDEV_CTX c = GetCtx(dev); UCHAR rx[128]; ULONG n, status = 0;
    UNREFERENCED_PARAMETER(prev);
    Config(c, 115200, TRUE);
    BtEn(0); Sleep(100); BtEn(1); Sleep(300);   /* QCA6390 boot: ~100 ms after BT_EN */
    n = Xfer(c, reset, sizeof(reset), rx, sizeof(rx), "hci_reset");
    Save(dev, L"BtRxReset", rx, n, REG_BINARY);
    if (n) status |= 1;
    if (n == 0) {   /* retry without flow control in case CTS never asserts */
        Config(c, 115200, FALSE);
        n = Xfer(c, reset, sizeof(reset), rx, sizeof(rx), "hci_reset_noflow");
        Save(dev, L"BtRxResetNoFlow", rx, n, REG_BINARY);
        if (n) status |= 2;
    }
    n = Xfer(c, ver, sizeof(ver), rx, sizeof(rx), "qca_version");
    Save(dev, L"BtRxVer", rx, n, REG_BINARY);
    if (n) status |= 4;
    Save(dev, L"BtStatus", &status, sizeof(status), REG_DWORD);
    LOG("probe done status=0x%lx", status);
    return STATUS_SUCCESS;   /* never fail start: this is a probe */
}
