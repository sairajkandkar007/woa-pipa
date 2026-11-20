/*
 * ktz8866.c - minimal KMDF SPB client for the Kinetic KTZ8866 backlight on Xiaomi Pad 6 (pipa).
 * Bus: pipa i2c11 (\_SB.IC10, QUP1 SE3), 7-bit address 0x11 (from the stock DTB).
 *
 * Stage 1 goal: prove I2C transfers reach real chips. At D0 the driver reads and logs the
 * chip registers (DbgPrint + device registry key "Ktz*"). It does NOT re-initialise the chip:
 * the bootloader/UEFI already enabled it. Brightness is written only if the device key has a
 * DWORD "Brightness" (0..2047); change it and restart the device to apply.
 * Register map: Linux drivers/video/backlight/ktz8866.c.
 */
#include <ntddk.h>
#include <wdf.h>
#include <ntstrsafe.h>
#include <spb.h>
#define RESHUB_USE_HELPER_ROUTINES
#include <reshub.h>

#define KTZ_DEVICE_ID   0x01
#define KTZ_BL_CFG1     0x02
#define KTZ_BL_CFG2     0x03
#define KTZ_BRT_LSB     0x04
#define KTZ_BRT_MSB     0x05
#define KTZ_BL_EN       0x08
#define KTZ_BIAS_CFG1   0x09
#define KTZ_FLAG        0x0F
#define KTZ_MAX_BRT     2047

#define LOG(fmt, ...) DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "ktz8866: " fmt "\n", __VA_ARGS__)

typedef struct _DEV_CTX {
    LARGE_INTEGER conn;
    BOOLEAN haveConn;
    WDFIOTARGET spb;
} DEV_CTX, *PDEV_CTX;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEV_CTX, GetCtx)

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD KtzDeviceAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE KtzPrepare;
EVT_WDF_DEVICE_RELEASE_HARDWARE KtzRelease;
EVT_WDF_DEVICE_D0_ENTRY KtzD0Entry;

typedef struct { SPB_TRANSFER_LIST list; SPB_TRANSFER_LIST_ENTRY second; } SEQ2;

static NTSTATUS KtzRead(PDEV_CTX c, UCHAR reg, UCHAR *val)
{
    SEQ2 s; WDF_MEMORY_DESCRIPTOR in; ULONG_PTR got = 0; NTSTATUS st;
    RtlZeroMemory(&s, sizeof(s));
    SPB_TRANSFER_LIST_INIT(&s.list, 2);
    s.list.Transfers[0] = SPB_TRANSFER_LIST_ENTRY_INIT_SIMPLE(SpbTransferDirectionToDevice, 0, &reg, 1);
    s.list.Transfers[1] = SPB_TRANSFER_LIST_ENTRY_INIT_SIMPLE(SpbTransferDirectionFromDevice, 0, val, 1);
    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&in, &s, sizeof(s));
    st = WdfIoTargetSendIoctlSynchronously(c->spb, NULL, IOCTL_SPB_EXECUTE_SEQUENCE, &in, NULL, NULL, &got);
    return st;
}

static NTSTATUS KtzWrite(PDEV_CTX c, UCHAR reg, UCHAR val)
{
    UCHAR b[2] = { reg, val }; WDF_MEMORY_DESCRIPTOR m; ULONG_PTR n = 0;
    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&m, b, sizeof(b));
    return WdfIoTargetSendWriteSynchronously(c->spb, NULL, &m, NULL, NULL, &n);
}

static VOID KtzSaveDword(WDFDEVICE d, PCWSTR name, ULONG v)
{
    WDFKEY k; UNICODE_STRING n;
    if (!NT_SUCCESS(WdfDeviceOpenRegistryKey(d, PLUGPLAY_REGKEY_DEVICE, KEY_WRITE, WDF_NO_OBJECT_ATTRIBUTES, &k))) return;
    RtlInitUnicodeString(&n, name);
    WdfRegistryAssignULong(k, &n, v);
    WdfRegistryClose(k);
}

NTSTATUS DriverEntry(PDRIVER_OBJECT drv, PUNICODE_STRING path)
{
    WDF_DRIVER_CONFIG cfg;
    WDF_DRIVER_CONFIG_INIT(&cfg, KtzDeviceAdd);
    return WdfDriverCreate(drv, path, WDF_NO_OBJECT_ATTRIBUTES, &cfg, WDF_NO_HANDLE);
}

NTSTATUS KtzDeviceAdd(WDFDRIVER drv, PWDFDEVICE_INIT init)
{
    WDF_PNPPOWER_EVENT_CALLBACKS pnp; WDF_OBJECT_ATTRIBUTES a; WDFDEVICE dev;
    UNREFERENCED_PARAMETER(drv);
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnp);
    pnp.EvtDevicePrepareHardware = KtzPrepare;
    pnp.EvtDeviceReleaseHardware = KtzRelease;
    pnp.EvtDeviceD0Entry = KtzD0Entry;
    WdfDeviceInitSetPnpPowerEventCallbacks(init, &pnp);
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&a, DEV_CTX);
    return WdfDeviceCreate(&init, &a, &dev);
}

NTSTATUS KtzPrepare(WDFDEVICE dev, WDFCMRESLIST raw, WDFCMRESLIST xl)
{
    PDEV_CTX c = GetCtx(dev); ULONG i; NTSTATUS st;
    WDF_OBJECT_ATTRIBUTES a; WDF_IO_TARGET_OPEN_PARAMS op; UNICODE_STRING path;
    DECLARE_UNICODE_STRING_SIZE(buf, RESOURCE_HUB_PATH_SIZE);
    UNREFERENCED_PARAMETER(raw);
    for (i = 0; i < WdfCmResourceListGetCount(xl); i++) {
        PCM_PARTIAL_RESOURCE_DESCRIPTOR r = WdfCmResourceListGetDescriptor(xl, i);
        if (r->Type == CmResourceTypeConnection && r->u.Connection.Class == CM_RESOURCE_CONNECTION_CLASS_SERIAL &&
            r->u.Connection.Type == CM_RESOURCE_CONNECTION_TYPE_SERIAL_I2C) {
            c->conn.LowPart = r->u.Connection.IdLowPart; c->conn.HighPart = r->u.Connection.IdHighPart;
            c->haveConn = TRUE; break;
        }
    }
    if (!c->haveConn) { LOG("no I2C connection resource"); return STATUS_INSUFFICIENT_RESOURCES; }
    st = RESOURCE_HUB_CREATE_PATH_FROM_ID(&buf, c->conn.LowPart, c->conn.HighPart);
    if (!NT_SUCCESS(st)) return st;
    path = buf;
    WDF_OBJECT_ATTRIBUTES_INIT(&a); a.ParentObject = dev;
    st = WdfIoTargetCreate(dev, &a, &c->spb);
    if (!NT_SUCCESS(st)) return st;
    WDF_IO_TARGET_OPEN_PARAMS_INIT_OPEN_BY_NAME(&op, &path, GENERIC_READ | GENERIC_WRITE);
    st = WdfIoTargetOpen(c->spb, &op);
    LOG("open SPB target st=0x%08X", st);
    return st;
}

NTSTATUS KtzRelease(WDFDEVICE dev, WDFCMRESLIST xl)
{
    PDEV_CTX c = GetCtx(dev); UNREFERENCED_PARAMETER(xl);
    if (c->spb) { WdfIoTargetClose(c->spb); WdfObjectDelete(c->spb); c->spb = NULL; }
    return STATUS_SUCCESS;
}

NTSTATUS KtzD0Entry(WDFDEVICE dev, WDF_POWER_DEVICE_STATE prev)
{
    PDEV_CTX c = GetCtx(dev);
    static const UCHAR regs[] = { KTZ_DEVICE_ID, KTZ_BL_CFG1, KTZ_BL_CFG2, KTZ_BRT_LSB, KTZ_BRT_MSB, KTZ_BL_EN, KTZ_BIAS_CFG1, KTZ_FLAG };
    UCHAR v[sizeof(regs)] = { 0 }; ULONG i, okmask = 0, packed = 0; NTSTATUS st, first = STATUS_SUCCESS;
    WDFKEY k; UNICODE_STRING n; ULONG want;
    UNREFERENCED_PARAMETER(prev);

    for (i = 0; i < sizeof(regs); i++) {
        st = KtzRead(c, regs[i], &v[i]);
        if (NT_SUCCESS(st)) okmask |= 1u << i; else if (NT_SUCCESS(first)) first = st;
        LOG("reg 0x%02X = 0x%02X st=0x%08X", regs[i], v[i], st);
    }
    KtzSaveDword(dev, L"KtzReadOkMask", okmask);
    KtzSaveDword(dev, L"KtzFirstError", (ULONG)first);
    KtzSaveDword(dev, L"KtzDeviceId", v[0]);
    KtzSaveDword(dev, L"KtzBlEn", v[5]);
    packed = ((ULONG)v[4] << 3) | (v[3] & 7);
    KtzSaveDword(dev, L"KtzBrightnessNow", packed);
    LOG("id=0x%02X bl_en=0x%02X brightness=%lu okmask=0x%02X", v[0], v[5], packed, okmask);

    if (NT_SUCCESS(WdfDeviceOpenRegistryKey(dev, PLUGPLAY_REGKEY_DEVICE, KEY_READ, WDF_NO_OBJECT_ATTRIBUTES, &k))) {
        RtlInitUnicodeString(&n, L"Brightness");
        if (NT_SUCCESS(WdfRegistryQueryULong(k, &n, &want))) {
            if (want > KTZ_MAX_BRT) want = KTZ_MAX_BRT;
            if (want == 0) want = 1;  /* never blank the panel from here */
            st = KtzWrite(c, KTZ_BRT_LSB, (UCHAR)(want & 7));
            if (NT_SUCCESS(st)) st = KtzWrite(c, KTZ_BRT_MSB, (UCHAR)((want >> 3) & 0xFF));
            LOG("set brightness %lu st=0x%08X", want, st);
            KtzSaveDword(dev, L"KtzLastSetStatus", (ULONG)st);
        }
        WdfRegistryClose(k);
    }
    return STATUS_SUCCESS;  /* never fail start: a dead I2C read must not take the device down */
}
