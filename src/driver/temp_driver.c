#include "../core/temp_core.h"
#include <ntstrsafe.h>

// Pool tag for memory allocation tracking
#define TEMP_POOL_TAG 'pmeT' // 'Temp' backwards

// Global variables
PDRIVER_OBJECT g_DriverObject = NULL;
PDEVICE_OBJECT g_ControlDeviceObject = NULL;
KSPIN_LOCK g_DeviceListLock;
PTEMP_DEVICE_EXTENSION g_DeviceList[TEMP_MAX_DEVICES] = {NULL};

// Function prototypes
NTSTATUS TempCreateControlDevice(PDRIVER_OBJECT DriverObject);
VOID TempDeleteControlDevice(VOID);
PTEMP_DEVICE_EXTENSION TempFindDevice(ULONG DeviceNumber);
NTSTATUS TempCompleteRequest(PIRP Irp, NTSTATUS Status, ULONG_PTR Information);

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    NTSTATUS status;

    UNREFERENCED_PARAMETER(RegistryPath);

    // Initialize global variables
    g_DriverObject = DriverObject;
    KeInitializeSpinLock(&g_DeviceListLock);

    // Set up driver dispatch routines
    DriverObject->DriverUnload = TempUnloadDriver;
    DriverObject->MajorFunction[IRP_MJ_CREATE] = TempDispatchCreateClose;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = TempDispatchCreateClose;
    DriverObject->MajorFunction[IRP_MJ_READ] = TempDispatchReadWrite;
    DriverObject->MajorFunction[IRP_MJ_WRITE] = TempDispatchReadWrite;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = TempDispatchDeviceControl;
    DriverObject->MajorFunction[IRP_MJ_PNP] = TempDispatchPnP;

    // Create control device
    status = TempCreateControlDevice(DriverObject);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    return STATUS_SUCCESS;
}

VOID TempUnloadDriver(PDRIVER_OBJECT DriverObject)
{
    UNREFERENCED_PARAMETER(DriverObject);

    // Remove all devices
    KIRQL oldIrql;
    KeAcquireSpinLock(&g_DeviceListLock, &oldIrql);

    for (ULONG i = 0; i < TEMP_MAX_DEVICES; i++)
    {
        if (g_DeviceList[i] != NULL)
        {
            PTEMP_DEVICE_EXTENSION deviceExtension = g_DeviceList[i];
            g_DeviceList[i] = NULL;
            KeReleaseSpinLock(&g_DeviceListLock, oldIrql);

            // Remove device
            TempRemoveDevice(deviceExtension->DeviceNumber);

            KeAcquireSpinLock(&g_DeviceListLock, &oldIrql);
        }
    }

    KeReleaseSpinLock(&g_DeviceListLock, oldIrql);

    // Delete control device
    TempDeleteControlDevice();
}

NTSTATUS TempCreateControlDevice(PDRIVER_OBJECT DriverObject)
{
    NTSTATUS status;
    UNICODE_STRING deviceName;
    UNICODE_STRING symbolicLink;

    RtlInitUnicodeString(&deviceName, TEMP_CTL_DEVICE_NAME);

    status = IoCreateDevice(
        DriverObject,
        0, // No device extension for control device
        &deviceName,
        FILE_DEVICE_DISK,
        FILE_DEVICE_SECURE_OPEN,
        FALSE,
        &g_ControlDeviceObject);

    if (!NT_SUCCESS(status))
    {
        return status;
    }

    RtlInitUnicodeString(&symbolicLink, TEMP_CTL_SYMLINK_NAME);

    status = IoCreateSymbolicLink(&symbolicLink, &deviceName);
    if (!NT_SUCCESS(status))
    {
        IoDeleteDevice(g_ControlDeviceObject);
        g_ControlDeviceObject = NULL;
        return status;
    }

    g_ControlDeviceObject->Flags |= DO_DIRECT_IO;
    g_ControlDeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

    return STATUS_SUCCESS;
}

VOID TempDeleteControlDevice(VOID)
{
    if (g_ControlDeviceObject != NULL)
    {
        UNICODE_STRING symbolicLink;
        RtlInitUnicodeString(&symbolicLink, TEMP_CTL_SYMLINK_NAME);
        IoDeleteSymbolicLink(&symbolicLink);
        IoDeleteDevice(g_ControlDeviceObject);
        g_ControlDeviceObject = NULL;
    }
}

NTSTATUS TempCreateDevice(PDRIVER_OBJECT DriverObject, PTEMP_CREATE_DATA CreateData)
{
    NTSTATUS status;
    PDEVICE_OBJECT deviceObject = NULL;
    PTEMP_DEVICE_EXTENSION deviceExtension = NULL;
    UNICODE_STRING deviceName;
    UNICODE_STRING symbolicLink;
    WCHAR deviceNameBuffer[64];
    WCHAR symbolicLinkBuffer[64];

    if (!CreateData || CreateData->DeviceNumber >= TEMP_MAX_DEVICES)
    {
        return STATUS_INVALID_PARAMETER;
    }

    // Check if device already exists
    KIRQL oldIrql;
    KeAcquireSpinLock(&g_DeviceListLock, &oldIrql);

    if (g_DeviceList[CreateData->DeviceNumber] != NULL)
    {
        KeReleaseSpinLock(&g_DeviceListLock, oldIrql);
        return STATUS_OBJECT_NAME_COLLISION;
    }

    KeReleaseSpinLock(&g_DeviceListLock, oldIrql);

    // Create device name
    status = RtlStringCchPrintfW(
        deviceNameBuffer,
        ARRAYSIZE(deviceNameBuffer),
        L"%s%u",
        TEMP_DEVICE_NAME_PREFIX,
        CreateData->DeviceNumber);

    if (!NT_SUCCESS(status))
    {
        return status;
    }

    RtlInitUnicodeString(&deviceName, deviceNameBuffer);

    // Create device object
    status = IoCreateDevice(
        DriverObject,
        sizeof(TEMP_DEVICE_EXTENSION),
        &deviceName,
        CreateData->CdRomType ? FILE_DEVICE_CD_ROM : FILE_DEVICE_DISK,
        CreateData->RemovableMedia ? FILE_REMOVABLE_MEDIA : 0,
        FALSE,
        &deviceObject);

    if (!NT_SUCCESS(status))
    {
        return status;
    }

    deviceExtension = (PTEMP_DEVICE_EXTENSION)deviceObject->DeviceExtension;
    RtlZeroMemory(deviceExtension, sizeof(TEMP_DEVICE_EXTENSION));

    // Initialize device extension
    deviceExtension->DeviceNumber = CreateData->DeviceNumber;
    deviceExtension->DiskSize = CreateData->DiskSize;
    deviceExtension->SectorSize = CreateData->SectorSize;
    deviceExtension->DriveLetter = CreateData->DriveLetter;
    deviceExtension->RemovableMedia = CreateData->RemovableMedia;
    deviceExtension->CdRomType = CreateData->CdRomType;
    deviceExtension->DeviceObject = deviceObject;
    deviceExtension->ReferenceCount = 1;

    KeInitializeEvent(&deviceExtension->RemoveEvent, NotificationEvent, FALSE);

    // Allocate memory manager
    deviceExtension->MemoryManager = (PTEMP_MEMORY_MANAGER)ExAllocatePool2(
        POOL_FLAG_NON_PAGED,
        sizeof(TEMP_MEMORY_MANAGER),
        TEMP_POOL_TAG);

    if (!deviceExtension->MemoryManager)
    {
        IoDeleteDevice(deviceObject);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    // Initialize memory manager
    status = TempInitializeMemoryManager(deviceExtension->MemoryManager, CreateData->DiskSize);
    if (!NT_SUCCESS(status))
    {
        ExFreePool(deviceExtension->MemoryManager);
        IoDeleteDevice(deviceObject);
        return status;
    }

    // Copy device and symbolic link names
    deviceExtension->DeviceName.Length = deviceName.Length;
    deviceExtension->DeviceName.MaximumLength = deviceName.MaximumLength;
    deviceExtension->DeviceName.Buffer = (PWSTR)ExAllocatePool2(
        POOL_FLAG_NON_PAGED,
        deviceName.MaximumLength,
        TEMP_POOL_TAG);

    if (!deviceExtension->DeviceName.Buffer)
    {
        TempCleanupMemoryManager(deviceExtension->MemoryManager);
        ExFreePool(deviceExtension->MemoryManager);
        IoDeleteDevice(deviceObject);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlCopyUnicodeString(&deviceExtension->DeviceName, &deviceName);

    // Create symbolic link if drive letter is specified
    if (CreateData->DriveLetter >= L'A' && CreateData->DriveLetter <= L'Z')
    {
        status = RtlStringCchPrintfW(
            symbolicLinkBuffer,
            ARRAYSIZE(symbolicLinkBuffer),
            L"%s%c:",
            TEMP_SYMLINK_PREFIX,
            CreateData->DriveLetter);

        if (NT_SUCCESS(status))
        {
            RtlInitUnicodeString(&symbolicLink, symbolicLinkBuffer);

            deviceExtension->SymbolicLinkName.Length = symbolicLink.Length;
            deviceExtension->SymbolicLinkName.MaximumLength = symbolicLink.MaximumLength;
            deviceExtension->SymbolicLinkName.Buffer = (PWSTR)ExAllocatePool2(
                POOL_FLAG_NON_PAGED,
                symbolicLink.MaximumLength,
                TEMP_POOL_TAG);

            if (deviceExtension->SymbolicLinkName.Buffer)
            {
                RtlCopyUnicodeString(&deviceExtension->SymbolicLinkName, &symbolicLink);
                IoCreateSymbolicLink(&symbolicLink, &deviceName);
            }
        }
    }

    // Set device characteristics
    deviceObject->Characteristics |= FILE_READ_ONLY_DEVICE;

    if (CreateData->RemovableMedia)
    {
        deviceObject->Characteristics |= FILE_REMOVABLE_MEDIA;
    }

    deviceObject->Flags |= DO_DIRECT_IO;
    deviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

    // Add to device list
    KeAcquireSpinLock(&g_DeviceListLock, &oldIrql);
    g_DeviceList[CreateData->DeviceNumber] = deviceExtension;
    KeReleaseSpinLock(&g_DeviceListLock, oldIrql);

    return STATUS_SUCCESS;
}

NTSTATUS TempRemoveDevice(ULONG DeviceNumber)
{
    if (DeviceNumber >= TEMP_MAX_DEVICES)
    {
        return STATUS_INVALID_PARAMETER;
    }

    KIRQL oldIrql;
    KeAcquireSpinLock(&g_DeviceListLock, &oldIrql);

    PTEMP_DEVICE_EXTENSION deviceExtension = g_DeviceList[DeviceNumber];
    if (!deviceExtension)
    {
        KeReleaseSpinLock(&g_DeviceListLock, oldIrql);
        return STATUS_NO_SUCH_DEVICE;
    }

    g_DeviceList[DeviceNumber] = NULL;
    KeReleaseSpinLock(&g_DeviceListLock, oldIrql);

    // Signal removal and wait for reference count to reach zero
    KeSetEvent(&deviceExtension->RemoveEvent, IO_NO_INCREMENT, FALSE);

    // Wait for all references to be released
    while (InterlockedCompareExchange(&deviceExtension->ReferenceCount, 0, 0) > 0)
    {
        LARGE_INTEGER interval;
        interval.QuadPart = -100000; // 10ms
        KeDelayExecutionThread(KernelMode, FALSE, &interval);
    }

    // Delete symbolic link
    if (deviceExtension->SymbolicLinkName.Buffer)
    {
        IoDeleteSymbolicLink(&deviceExtension->SymbolicLinkName);
        ExFreePool(deviceExtension->SymbolicLinkName.Buffer);
    }

    // Cleanup memory manager
    if (deviceExtension->MemoryManager)
    {
        TempCleanupMemoryManager(deviceExtension->MemoryManager);
        ExFreePool(deviceExtension->MemoryManager);
    }

    // Free device name
    if (deviceExtension->DeviceName.Buffer)
    {
        ExFreePool(deviceExtension->DeviceName.Buffer);
    }

    // Delete device object
    IoDeleteDevice(deviceExtension->DeviceObject);

    return STATUS_SUCCESS;
}

PTEMP_DEVICE_EXTENSION TempFindDevice(ULONG DeviceNumber)
{
    if (DeviceNumber >= TEMP_MAX_DEVICES)
    {
        return NULL;
    }

    KIRQL oldIrql;
    KeAcquireSpinLock(&g_DeviceListLock, &oldIrql);

    PTEMP_DEVICE_EXTENSION deviceExtension = g_DeviceList[DeviceNumber];
    if (deviceExtension)
    {
        InterlockedIncrement(&deviceExtension->ReferenceCount);
    }

    KeReleaseSpinLock(&g_DeviceListLock, oldIrql);

    return deviceExtension;
}

NTSTATUS TempCompleteRequest(PIRP Irp, NTSTATUS Status, ULONG_PTR Information)
{
    Irp->IoStatus.Status = Status;
    Irp->IoStatus.Information = Information;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}

NTSTATUS TempDispatchCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);
    return TempCompleteRequest(Irp, STATUS_SUCCESS, 0);
}

NTSTATUS TempDispatchReadWrite(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    PIO_STACK_LOCATION ioStack = IoGetCurrentIrpStackLocation(Irp);
    PTEMP_DEVICE_EXTENSION deviceExtension = (PTEMP_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
    NTSTATUS status = STATUS_SUCCESS;
    ULONG_PTR bytesTransferred = 0;

    if (DeviceObject == g_ControlDeviceObject)
    {
        return TempCompleteRequest(Irp, STATUS_INVALID_DEVICE_REQUEST, 0);
    }

    if (!deviceExtension || !deviceExtension->MemoryManager)
    {
        return TempCompleteRequest(Irp, STATUS_NO_SUCH_DEVICE, 0);
    }

    ULONG64 startOffset = ioStack->Parameters.Read.ByteOffset.QuadPart;
    ULONG length = ioStack->Parameters.Read.Length;
    PVOID buffer = NULL;

    if (Irp->MdlAddress)
    {
        buffer = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
    }

    if (!buffer)
    {
        return TempCompleteRequest(Irp, STATUS_INSUFFICIENT_RESOURCES, 0);
    }

    // Check bounds
    if (startOffset + length > deviceExtension->DiskSize)
    {
        return TempCompleteRequest(Irp, STATUS_INVALID_PARAMETER, 0);
    }

    ULONG64 startSector = startOffset / deviceExtension->SectorSize;
    ULONG sectorCount = (length + deviceExtension->SectorSize - 1) / deviceExtension->SectorSize;

    if (ioStack->MajorFunction == IRP_MJ_READ)
    {
        InterlockedIncrement64(&deviceExtension->ReadRequests);

        status = TempReadSectors(
            deviceExtension->MemoryManager,
            startSector,
            sectorCount,
            buffer,
            deviceExtension->SectorSize);

        if (NT_SUCCESS(status))
        {
            bytesTransferred = length;
            InterlockedAdd64(&deviceExtension->BytesRead, bytesTransferred);
        }
    }
    else if (ioStack->MajorFunction == IRP_MJ_WRITE)
    {
        InterlockedIncrement64(&deviceExtension->WriteRequests);

        status = TempWriteSectors(
            deviceExtension->MemoryManager,
            startSector,
            sectorCount,
            buffer,
            deviceExtension->SectorSize);

        if (NT_SUCCESS(status))
        {
            bytesTransferred = length;
            InterlockedAdd64(&deviceExtension->BytesWritten, bytesTransferred);
        }
    }

    return TempCompleteRequest(Irp, status, bytesTransferred);
}

NTSTATUS TempDispatchDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    PIO_STACK_LOCATION ioStack = IoGetCurrentIrpStackLocation(Irp);
    ULONG ioControlCode = ioStack->Parameters.DeviceIoControl.IoControlCode;
    NTSTATUS status = STATUS_INVALID_DEVICE_REQUEST;
    ULONG_PTR information = 0;

    switch (ioControlCode)
    {
    case TEMP_IOCTL_CREATE_DEVICE:
    {
        if (DeviceObject == g_ControlDeviceObject &&
            ioStack->Parameters.DeviceIoControl.InputBufferLength >= sizeof(TEMP_CREATE_DATA))
        {

            PTEMP_CREATE_DATA createData = (PTEMP_CREATE_DATA)Irp->AssociatedIrp.SystemBuffer;
            status = TempCreateDevice(g_DriverObject, createData);
        }
        break;
    }

    case TEMP_IOCTL_REMOVE_DEVICE:
    {
        if (DeviceObject == g_ControlDeviceObject &&
            ioStack->Parameters.DeviceIoControl.InputBufferLength >= sizeof(ULONG))
        {

            PULONG deviceNumber = (PULONG)Irp->AssociatedIrp.SystemBuffer;
            status = TempRemoveDevice(*deviceNumber);
        }
        break;
    }

    case TEMP_IOCTL_GET_VERSION:
    {
        if (ioStack->Parameters.DeviceIoControl.OutputBufferLength >= sizeof(ULONG))
        {
            PULONG version = (PULONG)Irp->AssociatedIrp.SystemBuffer;
            *version = (TEMP_VERSION_MAJOR << 16) | (TEMP_VERSION_MINOR << 8) | TEMP_VERSION_BUILD;
            information = sizeof(ULONG);
            status = STATUS_SUCCESS;
        }
        break;
    }

    case TEMP_IOCTL_GET_STATISTICS:
    {
        if (DeviceObject != g_ControlDeviceObject &&
            ioStack->Parameters.DeviceIoControl.OutputBufferLength >= sizeof(TEMP_STATISTICS))
        {

            PTEMP_DEVICE_EXTENSION deviceExtension = (PTEMP_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
            PTEMP_STATISTICS stats = (PTEMP_STATISTICS)Irp->AssociatedIrp.SystemBuffer;

            if (deviceExtension && deviceExtension->MemoryManager)
            {
                RtlZeroMemory(stats, sizeof(TEMP_STATISTICS));
                stats->DeviceNumber = deviceExtension->DeviceNumber;
                stats->DiskSize = deviceExtension->DiskSize;
                stats->BytesRead = deviceExtension->BytesRead;
                stats->BytesWritten = deviceExtension->BytesWritten;
                stats->TotalReads = deviceExtension->MemoryManager->TotalReads;
                stats->TotalWrites = deviceExtension->MemoryManager->TotalWrites;
                stats->CacheHits = deviceExtension->MemoryManager->TotalHits;
                stats->CacheMisses = deviceExtension->MemoryManager->TotalMisses;

                information = sizeof(TEMP_STATISTICS);
                status = STATUS_SUCCESS;
            }
        }
        break;
    }

    default:
        status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

    return TempCompleteRequest(Irp, status, information);
}

NTSTATUS TempDispatchPnP(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);
    return TempCompleteRequest(Irp, STATUS_NOT_SUPPORTED, 0);
}