#include <ntddk.h>
#include <wdf.h>

#define DEVICE_NAME         L"\\Device\\ExampleDevice"
#define SYMLINK_NAME        L"\\DosDevices\\ExampleDevice"

#define IOCTL_ONE           CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_TWO           CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_THREE         CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_FOUR          CTL_CODE(FILE_DEVICE_UNKNOWN, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_FIVE          CTL_CODE(FILE_DEVICE_UNKNOWN, 0x804, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SIX           CTL_CODE(FILE_DEVICE_UNKNOWN, 0x805, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SEVEN         CTL_CODE(FILE_DEVICE_UNKNOWN, 0x806, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_EIGHT         CTL_CODE(FILE_DEVICE_UNKNOWN, 0x807, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_NINE          CTL_CODE(FILE_DEVICE_UNKNOWN, 0x808, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_TEN           CTL_CODE(FILE_DEVICE_UNKNOWN, 0x809, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_ELEVEN        CTL_CODE(FILE_DEVICE_UNKNOWN, 0x80A, METHOD_BUFFERED, FILE_ANY_ACCESS)

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD EvtDeviceAdd;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL EvtIoDeviceControl;
EVT_WDF_OBJECT_CONTEXT_CLEANUP EvtDriverContextCleanup;

VOID
EvtIoDeviceControl(
    WDFQUEUE Queue,
    WDFREQUEST Request,
    size_t OutputBufferLength,
    size_t InputBufferLength,
    ULONG IoControlCode
)
{
    UNREFERENCED_PARAMETER(Queue);
    PUCHAR buffer = NULL;
    NTSTATUS status = STATUS_INVALID_DEVICE_REQUEST;

    WdfRequestRetrieveInputBuffer(Request, 0, (PVOID*)&buffer, NULL);

    switch (IoControlCode) {
    case IOCTL_ONE:
        if (InputBufferLength >= 4) {
            KdPrint(("WDF: IOCTL_ONE OK\n"));
            status = STATUS_SUCCESS;
        }
        break;

    case IOCTL_TWO:
        if (OutputBufferLength == 0x10) {
            KdPrint(("WDF: IOCTL_TWO OK\n"));
            status = STATUS_SUCCESS;
        } else {
            status = STATUS_BUFFER_TOO_SMALL;
        }
        break;

    case IOCTL_THREE:
        if (InputBufferLength == 8 && OutputBufferLength == 8) {
            KdPrint(("WDF: IOCTL_THREE Perfect match\n"));
            status = STATUS_SUCCESS;
        } else {
            status = STATUS_INVALID_BUFFER_SIZE;
        }
        break;

    case IOCTL_FOUR:
        if (InputBufferLength >= 0x20 && OutputBufferLength >= 0x20) {
            KdPrint(("WDF: IOCTL_FOUR Large buffers\n"));
            status = STATUS_SUCCESS;
        } else {
            status = STATUS_BUFFER_TOO_SMALL;
        }
        break;

    case IOCTL_FIVE:
        if (InputBufferLength == 1 && buffer[0] == 'A') {
            KdPrint(("WDF: IOCTL_FIVE Match A\n"));
            status = STATUS_SUCCESS;
        } else {
            status = STATUS_INVALID_PARAMETER;
        }
        break;

    case IOCTL_SIX:
        if (InputBufferLength >= 2 && buffer[0] == 'B' && buffer[1] == 'C') {
            KdPrint(("WDF: IOCTL_SIX Starts with BC\n"));
            status = STATUS_SUCCESS;
        } else {
            status = STATUS_INVALID_PARAMETER;
        }
        break;

    case IOCTL_SEVEN:
        if (OutputBufferLength > 0x30) {
            KdPrint(("WDF: IOCTL_SEVEN Big output\n"));
            status = STATUS_SUCCESS;
        } else {
            status = STATUS_BUFFER_TOO_SMALL;
        }
        break;

    case IOCTL_EIGHT:
        if (InputBufferLength == 0x10 && OutputBufferLength == 0x40) {
            KdPrint(("WDF: IOCTL_EIGHT Complex match\n"));
            status = STATUS_SUCCESS;
        } else {
            status = STATUS_INVALID_PARAMETER;
        }
        break;

    case IOCTL_NINE:
        if (InputBufferLength >= 4 && buffer[3] == 'Z') {
            KdPrint(("WDF: IOCTL_NINE Z at offset 3\n"));
            status = STATUS_SUCCESS;
        } else {
            status = STATUS_INVALID_PARAMETER;
        }
        break;

    case IOCTL_TEN:
        KdPrint(("WDF: IOCTL_TEN Default OK\n"));
        status = STATUS_SUCCESS;
        break;

    case IOCTL_ELEVEN:
        if (InputBufferLength >= 2 && buffer[0] == 'X' && buffer[1] == 'Y') {
            KdPrint(("WDF: IOCTL_ELEVEN XY match\n"));
            status = STATUS_SUCCESS;
        } else {
            status = STATUS_INVALID_PARAMETER;
        }
        break;

    default:
        KdPrint(("WDF: Unknown IOCTL 0x%x\n", IoControlCode));
        status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

    WdfRequestComplete(Request, status);
}

NTSTATUS
EvtDeviceAdd(
    WDFDRIVER Driver,
    PWDFDEVICE_INIT DeviceInit
)
{
    UNREFERENCED_PARAMETER(Driver);
    NTSTATUS status;
    WDFDEVICE device;
    WDF_IO_QUEUE_CONFIG queueConfig;
    UNICODE_STRING symlink = RTL_CONSTANT_STRING(SYMLINK_NAME);

    WdfDeviceInitSetDeviceType(DeviceInit, FILE_DEVICE_UNKNOWN);
    status = WdfDeviceCreate(&DeviceInit, WDF_NO_OBJECT_ATTRIBUTES, &device);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    WdfDeviceCreateSymbolicLink(device, &symlink);

    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig, WdfIoQueueDispatchSequential);
    queueConfig.EvtIoDeviceControl = EvtIoDeviceControl;

    status = WdfIoQueueCreate(device, &queueConfig, WDF_NO_OBJECT_ATTRIBUTES, NULL);
    return status;
}

VOID
EvtDriverContextCleanup(
    WDFOBJECT DriverObject
)
{
    UNREFERENCED_PARAMETER(DriverObject);
    KdPrint(("WDF: Driver cleanup\n"));
}

NTSTATUS
DriverEntry(
    PDRIVER_OBJECT DriverObject,
    PUNICODE_STRING RegistryPath
)
{
    WDF_DRIVER_CONFIG config;
    NTSTATUS status;

    WDF_DRIVER_CONFIG_INIT(&config, EvtDeviceAdd);
    config.EvtDriverUnload = NULL;

    status = WdfDriverCreate(DriverObject, RegistryPath, WDF_NO_OBJECT_ATTRIBUTES, &config, WDF_NO_HANDLE);
    if (!NT_SUCCESS(status)) {
        KdPrint(("WDF: DriverEntry failed 0x%x\n", status));
    }

    return status;
}
