/* Copyright (c) 2026 Nika Okromtchedlishvili */
#include "driver.h"
#include "acpi.h"
#include "qmp.h"
#include "qmp_service.h"
#include "qmp_win.h"
#include "trace.h"
#include "qmp_win.tmh"

static const GUID qmp_guid = QMP_INTERFACE_GUID;
static const GUID ipcc_guid = {
    0x9efdff9f, 0x8f1b, 0x4e79, {0xa9, 0x13, 0x31, 0x71, 0xee, 0x9f, 0x9b, 0xd3}};

typedef struct ipcc_interface {
    INTERFACE header;
    ULONG (*open)(PVOID* handle, ULONG protocol);
    ULONG (*close)(PVOID* handle);
    PVOID reg;
    PVOID unreg;
    ULONG (*trigger)(PVOID handle, ULONG client, ULONG first, ULONG last);
    PVOID query[3];
} ipcc_interface_t;

typedef struct qmp_win {
    WDFDEVICE dev;
    KMUTEX lock;
    PHYSICAL_ADDRESS address;
    ULONG size;
    ULONG protocol;
    ULONG client;
    ULONG signal;
    ULONGLONG epoch;
    BOOLEAN configured;
    BOOLEAN available;
    BOOLEAN interface_held;
    BOOLEAN target_open;
    PUCHAR mapping;
    WDFIOTARGET target;
    PVOID handle;
    ipcc_interface_t ipcc;
#if defined(PEP_TARGET_8250)
    volatile ULONG* doorbell; /* SM8250: APCS IPC register (0x17C0000C), mapped in open_ipcc */
#endif
    qmp_t transport;
    qmp_interface_t interface;
} qmp_win_t;

C_ASSERT(sizeof(ipcc_interface_t) == 96);
C_ASSERT(FIELD_OFFSET(ipcc_interface_t, trigger) == 64);
C_ASSERT(FIELD_OFFSET(qmp_interface_t, send) == sizeof(INTERFACE));

static qmp_win_t* get_qmp(WDFDEVICE dev)
{
    return dev ? (qmp_win_t*)get_dev_ctx(dev)->qmp : NULL;
}

static VOID lock_qmp(qmp_win_t* ctx)
{
    (VOID) KeWaitForSingleObject(&ctx->lock, Executive, KernelMode, FALSE, NULL);
}

static VOID unlock_qmp(qmp_win_t* ctx)
{
    KeReleaseMutex(&ctx->lock, FALSE);
}

static uint32_t read_qmp(void* context, uint32_t offset)
{
    qmp_win_t* ctx = context;

    return READ_REGISTER_ULONG((PULONG)(ctx->mapping + offset));
}

static void write_qmp(void* context, uint32_t offset, uint32_t value)
{
    qmp_win_t* ctx = context;

    WRITE_REGISTER_ULONG((PULONG)(ctx->mapping + offset), value);
}

static int kick_qmp(void* context)
{
    qmp_win_t* ctx = context;

    KeMemoryBarrier();
#if defined(PEP_TARGET_8250)
    /* SM8250 has no IPCC: AOP is signalled through the APCS IPC register (Linux qcom_aoss
     * "mboxes = <&apss_shared 0>"). _DSM signal = bit in APCS IPC register 0x17C0000C. */
    if (!ctx->doorbell)
        return 1;
    WRITE_REGISTER_ULONG((PULONG)ctx->doorbell, 1u << (ctx->signal & 31u));
    return 0;
#else
    return ctx->ipcc.trigger(ctx->handle, ctx->client, ctx->signal, ctx->signal) != 0;
#endif
}

static void delay_qmp(void* context)
{
    LARGE_INTEGER delay;

    UNREFERENCED_PARAMETER(context);
    delay.QuadPart = -10000;
    (VOID) KeDelayExecutionThread(KernelMode, FALSE, &delay);
}

static uint64_t time_qmp(void* context)
{
    UNREFERENCED_PARAMETER(context);
    return KeQueryInterruptTime() / 10000u;
}

static VOID reference_qmp(void* context)
{
    WdfObjectReference(((qmp_win_t*)context)->dev);
}

static VOID dereference_qmp(void* context)
{
    WdfObjectDereference(((qmp_win_t*)context)->dev);
}

static NTSTATUS result_status(enum qmp_result result)
{
    switch (result) {
        case QMP_OK:
            return STATUS_SUCCESS;
        case QMP_INVALID:
            return STATUS_DATA_ERROR;
        case QMP_TIMEOUT:
            return STATUS_IO_TIMEOUT;
        case QMP_BUSY:
            return STATUS_DEVICE_BUSY;
        case QMP_IO_ERROR:
            return STATUS_IO_DEVICE_ERROR;
        default:
            return STATUS_DEVICE_NOT_READY;
    }
}

static int32_t send_qmp(void* context, const char* message, uint32_t length)
{
    qmp_win_t* ctx = context;
    enum qmp_result result = QMP_NOT_READY;
    NTSTATUS status;

    if (!ctx || !message || !length || length > QMP_MESSAGE_SIZE)
        return (int32_t)STATUS_INVALID_PARAMETER;
    if (KeGetCurrentIrql() != PASSIVE_LEVEL)
        return (int32_t)STATUS_INVALID_DEVICE_STATE;

    /* acquire the power reference before the transport lock. */
    status = WdfDeviceStopIdle(ctx->dev, TRUE);
    if (!NT_SUCCESS(status))
        return (int32_t)status;

    lock_qmp(ctx);
    if (ctx->available)
        result = qmp_send(&ctx->transport, message, length);
    status = result_status(result);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_WARNING, TRACE_RESOURCE,
                    "qmp: transfer failed epoch=%llu result=%u status=%08X", ctx->epoch,
                    (unsigned)result, status);
    } else {
        TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_RESOURCE,
                    "qmp: transfer complete epoch=%llu bytes=%u", ctx->epoch, length);
    }
    unlock_qmp(ctx);
    WdfDeviceResumeIdle(ctx->dev);
    return (int32_t)status;
}

NTSTATUS qmp_win_init(WDFDEVICE dev)
{
    WDF_QUERY_INTERFACE_CONFIG config;
    qmp_win_t* ctx;
    NTSTATUS status;

    ctx = ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(*ctx), POOL_TAG);
    if (!ctx)
        return STATUS_INSUFFICIENT_RESOURCES;

    RtlZeroMemory(ctx, sizeof(*ctx));
    ctx->dev = dev;
    KeInitializeMutex(&ctx->lock, 0);
    ctx->interface.size = sizeof(ctx->interface);
    ctx->interface.version = QMP_INTERFACE_VERSION;
    ctx->interface.context = ctx;
    ctx->interface.reference = reference_qmp;
    ctx->interface.dereference = dereference_qmp;
    ctx->interface.send = send_qmp;
    get_dev_ctx(dev)->qmp = ctx;

    WDF_QUERY_INTERFACE_CONFIG_INIT(&config, (PINTERFACE)&ctx->interface, &qmp_guid, NULL);
    status = WdfDeviceAddQueryInterface(dev, &config);
    if (!NT_SUCCESS(status))
        return status;
    return WdfDeviceCreateDeviceInterface(dev, &qmp_guid, NULL);
}

NTSTATUS qmp_win_prepare(WDFDEVICE dev, WDFCMRESLIST raw, WDFCMRESLIST translated)
{
    qmp_win_t* ctx = get_qmp(dev);
    ULONG values[6] = {0};
    ULONG count = 0;
    ULONG matches = 0;
    PHYSICAL_ADDRESS address = {0};
    NTSTATUS status;

    if (!ctx)
        return STATUS_NOT_SUPPORTED;
    if (!raw || !translated)
        return STATUS_INVALID_PARAMETER;

    lock_qmp(ctx);
    if (ctx->mapping || ctx->available) {
        unlock_qmp(ctx);
        return STATUS_DEVICE_BUSY;
    }
    ctx->configured = FALSE;
    unlock_qmp(ctx);

    status = acpi_eval_dsm_package_ulongs(dev, &qmp_guid, 1, 1, NULL, 0, values,
                                          RTL_NUMBER_OF(values), &count);
#if defined(PEP_TARGET_8250)
    /* Kona DSDT has no QMP _DSM; use the AOP MSGRAM from PEP0.INTR (0x0C300000, Linux
     * qcom_aoss uses the first 0x400 bytes) with APCS IPC bit 0 as doorbell. */
    if (!NT_SUCCESS(status)) {
        values[0] = 1; values[1] = 0x0C300000u; values[2] = 0x400u;
        values[3] = 0; values[4] = 0; values[5] = 0;
        count = RTL_NUMBER_OF(values);
        status = STATUS_SUCCESS;
    }
#endif
    if (!NT_SUCCESS(status))
        return status;
    if (count == 1 && values[0] == 0)
        return STATUS_NOT_SUPPORTED;
    if (count != RTL_NUMBER_OF(values) || values[0] != 1 || !values[1] || (values[1] & 3u) ||
        (values[2] & 3u) || values[2] < 0x80 || values[2] > 0x10000 || values[3] > 4 ||
        values[4] > 0xffff || values[5] > 0xffff)
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    if (WdfCmResourceListGetCount(raw) != WdfCmResourceListGetCount(translated))
        return STATUS_DEVICE_CONFIGURATION_ERROR;

    for (ULONG i = 0; i < WdfCmResourceListGetCount(raw); ++i) {
        PCM_PARTIAL_RESOURCE_DESCRIPTOR r = WdfCmResourceListGetDescriptor(raw, i);
        PCM_PARTIAL_RESOURCE_DESCRIPTOR t = WdfCmResourceListGetDescriptor(translated, i);

        if (!r || !t || r->Type != CmResourceTypeMemory || t->Type != CmResourceTypeMemory)
            continue;
        if (r->u.Memory.Start.QuadPart != values[1] || r->u.Memory.Length != values[2])
            continue;
        if (t->u.Memory.Length != values[2] || t->u.Memory.Start.QuadPart <= 0 ||
            (t->u.Memory.Start.QuadPart & 3) ||
            r->ShareDisposition != CmResourceShareDeviceExclusive ||
            t->ShareDisposition != CmResourceShareDeviceExclusive ||
            (r->Flags & (CM_RESOURCE_MEMORY_READ_ONLY | CM_RESOURCE_MEMORY_WRITE_ONLY)) ||
            (t->Flags & (CM_RESOURCE_MEMORY_READ_ONLY | CM_RESOURCE_MEMORY_WRITE_ONLY)))
            return STATUS_DEVICE_CONFIGURATION_ERROR;

        address = t->u.Memory.Start;
        ++matches;
    }
#if defined(PEP_TARGET_8250)
    /* PEP0 _CRS on Kona carries no memory resources: map the MSGRAM directly. */
    if (matches == 0) {
        address.QuadPart = (LONGLONG)values[1];
        matches = 1;
    }
#endif
    if (matches != 1)
        return STATUS_DEVICE_CONFIGURATION_ERROR;

    lock_qmp(ctx);
    if (ctx->mapping) {
        unlock_qmp(ctx);
        return STATUS_DEVICE_BUSY;
    }
    ctx->address = address;
    ctx->size = values[2];
    ctx->protocol = values[3];
    ctx->client = values[4];
    ctx->signal = values[5];
    ctx->configured = TRUE;
    unlock_qmp(ctx);
    return STATUS_SUCCESS;
}

static VOID close_ipcc(qmp_win_t* ctx)
{
#if defined(PEP_TARGET_8250)
    if (ctx->doorbell) {
        MmUnmapIoSpace((PVOID)ctx->doorbell, sizeof(ULONG));
        ctx->doorbell = NULL;
    }
    return;
#endif
    if (ctx->handle) {
        (VOID) ctx->ipcc.close(&ctx->handle);
        ctx->handle = NULL;
    }
    if (ctx->interface_held && ctx->ipcc.header.InterfaceDereference)
        ctx->ipcc.header.InterfaceDereference(ctx->ipcc.header.Context);
    ctx->interface_held = FALSE;
    RtlZeroMemory(&ctx->ipcc, sizeof(ctx->ipcc));
    if (ctx->target) {
        if (ctx->target_open)
            WdfIoTargetClose(ctx->target);
        WdfObjectDelete(ctx->target);
        ctx->target = NULL;
    }
    ctx->target_open = FALSE;
}

static NTSTATUS open_ipcc(qmp_win_t* ctx)
{
#if defined(PEP_TARGET_8250)
    PHYSICAL_ADDRESS bell;
    /* only the APCS shared IPC register is accepted as a doorbell on this target. */
    /* _DSM protocol/client are ignored; signal = APCS IPC bit (0 for AOP on SM8250). */
    if (ctx->signal > 31u)
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    bell.QuadPart = 0x17C0000CLL; /* APCS_ALIAS0_IPC_INTERRUPT (Linux apss_shared + 0xc) */
    ctx->doorbell = (volatile ULONG*)MmMapIoSpaceEx(bell, sizeof(ULONG), PAGE_READWRITE | PAGE_NOCACHE);
    return ctx->doorbell ? STATUS_SUCCESS : STATUS_INSUFFICIENT_RESOURCES;
#else
    WDF_OBJECT_ATTRIBUTES attrs;
    WDF_IO_TARGET_OPEN_PARAMS params;
    UNICODE_STRING name = RTL_CONSTANT_STRING(L"\\Device\\IPCC");
    NTSTATUS status;

    WDF_OBJECT_ATTRIBUTES_INIT(&attrs);
    attrs.ParentObject = ctx->dev;
    status = WdfIoTargetCreate(ctx->dev, &attrs, &ctx->target);
    if (!NT_SUCCESS(status))
        return status;
    WDF_IO_TARGET_OPEN_PARAMS_INIT_OPEN_BY_NAME(&params, &name, GENERIC_READ | GENERIC_WRITE);
    params.ShareAccess = FILE_SHARE_READ | FILE_SHARE_WRITE;
    status = WdfIoTargetOpen(ctx->target, &params);
    if (!NT_SUCCESS(status))
        goto fail;
    ctx->target_open = TRUE;
    status = WdfIoTargetQueryForInterface(ctx->target, &ipcc_guid, (PINTERFACE)&ctx->ipcc,
                                          sizeof(ctx->ipcc), 2, NULL);
    if (!NT_SUCCESS(status))
        goto fail;
    ctx->interface_held = TRUE;
    if (ctx->ipcc.header.Size != sizeof(ctx->ipcc) || ctx->ipcc.header.Version != 2 ||
        !ctx->ipcc.open || !ctx->ipcc.close || !ctx->ipcc.trigger) {
        status = STATUS_NOINTERFACE;
        goto fail;
    }
    if (ctx->ipcc.open(&ctx->handle, ctx->protocol) != 0 || !ctx->handle) {
        status = STATUS_DEVICE_NOT_READY;
        goto fail;
    }
    return STATUS_SUCCESS;

fail:
    close_ipcc(ctx);
    return status;
}
#endif

void qmp_win_start(WDFDEVICE dev)
{
    static const qmp_io_t io = {read_qmp, write_qmp, kick_qmp, time_qmp, delay_qmp};
    qmp_win_t* ctx = get_qmp(dev);
    NTSTATUS status;

    if (!ctx)
        return;
    lock_qmp(ctx);
    if (!ctx->configured || ctx->available) {
        unlock_qmp(ctx);
        return;
    }
    status = open_ipcc(ctx);
    if (!NT_SUCCESS(status))
        goto fail;
    ctx->mapping = MmMapIoSpaceEx(ctx->address, ctx->size, PAGE_READWRITE | PAGE_NOCACHE);
    if (!ctx->mapping) {
        status = STATUS_INSUFFICIENT_RESOURCES;
        close_ipcc(ctx);
        goto fail;
    }
    qmp_init(&ctx->transport, &io, ctx, ctx->size);
    ctx->available = TRUE;
    ++ctx->epoch;
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_RESOURCE, "qmp: mailbox mapped epoch=%llu",
                ctx->epoch);
    unlock_qmp(ctx);
    return;

fail:
    TraceEvents(TRACE_LEVEL_WARNING, TRACE_RESOURCE, "qmp: provider unavailable status=%08X",
                status);
    unlock_qmp(ctx);
}

void qmp_win_stop(WDFDEVICE dev)
{
    qmp_win_t* ctx = get_qmp(dev);

    if (!ctx)
        return;
    lock_qmp(ctx);
    ctx->available = FALSE;
    if (ctx->mapping) {
        qmp_close(&ctx->transport);
        MmUnmapIoSpace(ctx->mapping, ctx->size);
        ctx->mapping = NULL;
    }
    close_ipcc(ctx);
    unlock_qmp(ctx);
}

void qmp_win_release(WDFDEVICE dev)
{
    qmp_win_t* ctx = get_qmp(dev);

    if (!ctx)
        return;
    qmp_win_stop(dev);
    lock_qmp(ctx);
    ctx->configured = FALSE;
    ctx->size = 0;
    unlock_qmp(ctx);
}

void qmp_win_cleanup(WDFDEVICE dev)
{
    qmp_win_t* ctx = get_qmp(dev);

    if (!ctx)
        return;
    qmp_win_release(dev);
    get_dev_ctx(dev)->qmp = NULL;
    ExFreePoolWithTag(ctx, POOL_TAG);
}
