#include <ntddk.h>

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

NTSTATUS DispatchDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
    ULONG code = stack->Parameters.DeviceIoControl.IoControlCode;
    ULONG inLen = stack->Parameters.DeviceIoControl.InputBufferLength;
    ULONG outLen = stack->Parameters.DeviceIoControl.OutputBufferLength;
    PUCHAR buffer = (PUCHAR)Irp->AssociatedIrp.SystemBuffer;

    Irp->IoStatus.Information = 0;
    Irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;

    switch (code)
    {
    case IOCTL_ONE:
        if (inLen >= 4) {
            DbgPrint("[IOCTL_ONE] OK\n");
            Irp->IoStatus.Status = STATUS_SUCCESS;
        }
        break;

    case IOCTL_TWO:
        if (outLen == 0x10) {
            DbgPrint("[IOCTL_TWO] OK\n");
            Irp->IoStatus.Status = STATUS_SUCCESS;
        } else {
            Irp->IoStatus.Status = STATUS_BUFFER_TOO_SMALL;
        }
        break;

    case IOCTL_THREE:
        if (inLen == 8 && outLen == 8) {
            DbgPrint("[IOCTL_THREE] Perfect match\n");
            Irp->IoStatus.Status = STATUS_SUCCESS;
        } else {
            Irp->IoStatus.Status = STATUS_INVALID_BUFFER_SIZE;
        }
        break;

    case IOCTL_FOUR:
        if (inLen >= 0x20 && outLen >= 0x20) {
            DbgPrint("[IOCTL_FOUR] Large buffers\n");
            Irp->IoStatus.Status = STATUS_SUCCESS;
        } else {
            Irp->IoStatus.Status = STATUS_BUFFER_TOO_SMALL;
        }
        break;

    case IOCTL_FIVE:
        if (inLen == 1 && buffer[0] == 'A') {
            DbgPrint("[IOCTL_FIVE] Match A\n");
            Irp->IoStatus.Status = STATUS_SUCCESS;
        } else {
            Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
        }
        break;

    case IOCTL_SIX:
        if (inLen >= 2 && buffer[0] == 'B' && buffer[1] == 'C') {
            DbgPrint("[IOCTL_SIX] Starts with BC\n");
            Irp->IoStatus.Status = STATUS_SUCCESS;
        } else {
            Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
        }
        break;

    case IOCTL_SEVEN:
        if (outLen > 0x30) {
            DbgPrint("[IOCTL_SEVEN] Big output\n");
            Irp->IoStatus.Status = STATUS_SUCCESS;
        } else {
            Irp->IoStatus.Status = STATUS_BUFFER_TOO_SMALL;
        }
        break;

    case IOCTL_EIGHT:
        if (inLen == 0x10 && outLen == 0x40) {
            DbgPrint("[IOCTL_EIGHT] Complex match\n");
            Irp->IoStatus.Status = STATUS_SUCCESS;
        } else {
            Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
        }
        break;

    case IOCTL_NINE:
        if (inLen >= 4 && buffer[3] == 'Z') {
            DbgPrint("[IOCTL_NINE] Z at offset 3\n");
            Irp->IoStatus.Status = STATUS_SUCCESS;
        } else {
            Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
        }
        break;

    case IOCTL_TEN:
        DbgPrint("[IOCTL_TEN] Default OK\n");
        Irp->IoStatus.Status = STATUS_SUCCESS;
        break;

    case IOCTL_ELEVEN:
        if (inLen >= 2 && buffer[0] == 'X' && buffer[1] == 'Y') {
            DbgPrint("[IOCTL_ELEVEN] XY match\n");
            Irp->IoStatus.Status = STATUS_SUCCESS;
        } else {
            Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
        }
        break;

    default:
        DbgPrint("[UNKNOWN IOCTL] Not handled\n");
        Irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Irp->IoStatus.Status;
}

NTSTATUS DispatchCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

void DriverUnload(PDRIVER_OBJECT DriverObject)
{
    UNICODE_STRING symlink = RTL_CONSTANT_STRING(SYMLINK_NAME);
    IoDeleteSymbolicLink(&symlink);
    IoDeleteDevice(DriverObject->DeviceObject);
    DbgPrint("[Driver] Unloaded\n");
}

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    UNREFERENCED_PARAMETER(RegistryPath);

    UNICODE_STRING deviceName = RTL_CONSTANT_STRING(DEVICE_NAME);
    UNICODE_STRING symlink = RTL_CONSTANT_STRING(SYMLINK_NAME);
    PDEVICE_OBJECT deviceObject = NULL;

    NTSTATUS status = IoCreateDevice(
        DriverObject,
        0,
        &deviceName,
        FILE_DEVICE_UNKNOWN,
        0,
        FALSE,
        &deviceObject
    );

    if (!NT_SUCCESS(status)) {
        return status;
    }

    IoCreateSymbolicLink(&symlink, &deviceName);

    for (int i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++) {
        DriverObject->MajorFunction[i] = DispatchCreateClose;
    }

    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DispatchDeviceControl;
    DriverObject->DriverUnload = DriverUnload;

    DbgPrint("[Driver] Loaded\n");
    return STATUS_SUCCESS;
}
