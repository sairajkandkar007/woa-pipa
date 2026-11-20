/*
 * pipapwr.c - Xiaomi Pad 6 (pipa) radio power: QCA6390 enable lines + BT UART pins.
 * Binds to ACPI\PIPA6390 (\_SB.WPWR, appended by make_pipa_k.py --pipa-wpwr).
 *
 * From pipa-mainline sm8250-xiaomi-pipa.dts (qca6390-pmu):
 *   wlan-enable-gpios = <&tlmm 20 GPIO_ACTIVE_HIGH>;  bt-enable-gpios = <&tlmm 21 GPIO_ACTIVE_HIGH>;
 *   pinctrl: function gpio, drive-strength 16, bias-pull-up.
 * uart6 (BT HCI, QUP0 SE6 @0x998000): gpio16-19 function qup6.
 * PMU supply rails (S4A/S5A/S6A/S8C, L16A) are expected on from boot; not voted here.
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
    return STATUS_SUCCESS;
}
