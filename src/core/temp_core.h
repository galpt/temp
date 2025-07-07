#ifndef TEMP_CORE_H
#define TEMP_CORE_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <windows.h>
#include <winioctl.h>
#include <ntddk.h>

// Version information
#define TEMP_VERSION_MAJOR 1
#define TEMP_VERSION_MINOR 0
#define TEMP_VERSION_BUILD 0
#define TEMP_VERSION_STRING L"1.0.0"

// Configuration constants
#define TEMP_MAX_DEVICES 32
#define TEMP_BUCKET_COUNT 512
#define TEMP_CHUNK_SIZE (64 * 1024) // 64KB chunks like fastcache
#define TEMP_DEFAULT_SECTOR_SIZE 512
#define TEMP_MAX_DISK_SIZE (1ULL << 40) // 1TB max

// Device name and symbolic link prefixes
#define TEMP_DEVICE_NAME_PREFIX L"\\Device\\TempRamDisk"
#define TEMP_SYMLINK_PREFIX L"\\DosDevices\\"
#define TEMP_CTL_DEVICE_NAME L"\\Device\\TempRamDiskControl"
#define TEMP_CTL_SYMLINK_NAME L"\\DosDevices\\TempRamDiskControl"

// IOCTLs
#define TEMP_IOCTL_BASE 0x83000000
#define TEMP_IOCTL_CREATE_DEVICE CTL_CODE(FILE_DEVICE_DISK, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define TEMP_IOCTL_REMOVE_DEVICE CTL_CODE(FILE_DEVICE_DISK, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define TEMP_IOCTL_LIST_DEVICES CTL_CODE(FILE_DEVICE_DISK, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define TEMP_IOCTL_GET_VERSION CTL_CODE(FILE_DEVICE_DISK, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define TEMP_IOCTL_GET_STATISTICS CTL_CODE(FILE_DEVICE_DISK, 0x804, METHOD_BUFFERED, FILE_ANY_ACCESS)

    // Forward declarations
    typedef struct _TEMP_DEVICE_EXTENSION TEMP_DEVICE_EXTENSION, *PTEMP_DEVICE_EXTENSION;
    typedef struct _TEMP_BUCKET TEMP_BUCKET, *PTEMP_BUCKET;
    typedef struct _TEMP_MEMORY_MANAGER TEMP_MEMORY_MANAGER, *PTEMP_MEMORY_MANAGER;

    // Memory chunk structure (inspired by fastcache)
    typedef struct _TEMP_CHUNK
    {
        UCHAR Data[TEMP_CHUNK_SIZE];
        volatile LONG64 Generation;
        volatile LONG RefCount;
    } TEMP_CHUNK, *PTEMP_CHUNK;

    // Hash table entry for fast lookup
    typedef struct _TEMP_HASH_ENTRY
    {
        ULONG64 Key;        // Hash of the sector address
        ULONG64 ChunkIndex; // Index into bucket's chunk array
        ULONG64 Offset;     // Offset within the chunk
    } TEMP_HASH_ENTRY, *PTEMP_HASH_ENTRY;

    // Bucket structure for scalable memory management
    typedef struct _TEMP_BUCKET
    {
        KSPIN_LOCK Lock;            // Per-bucket lock for scalability
        PTEMP_CHUNK *Chunks;        // Array of chunk pointers
        ULONG ChunkCount;           // Current number of chunks
        ULONG MaxChunks;            // Maximum allowed chunks
        PTEMP_HASH_ENTRY HashTable; // Hash table for O(1) lookups
        ULONG HashTableSize;        // Size of hash table
        volatile LONG64 Generation; // Current generation for eviction

        // Statistics
        volatile LONG64 HitCount;
        volatile LONG64 MissCount;
        volatile LONG64 EvictionCount;
    } TEMP_BUCKET, *PTEMP_BUCKET;

    // Memory manager structure
    typedef struct _TEMP_MEMORY_MANAGER
    {
        TEMP_BUCKET Buckets[TEMP_BUCKET_COUNT];
        ULONG64 TotalSize; // Total allocated memory
        ULONG64 MaxSize;   // Maximum allowed memory
        volatile LONG64 TotalReads;
        volatile LONG64 TotalWrites;
        volatile LONG64 TotalHits;
        volatile LONG64 TotalMisses;
    } TEMP_MEMORY_MANAGER, *PTEMP_MEMORY_MANAGER;

    // Device creation parameters
    typedef struct _TEMP_CREATE_DATA
    {
        ULONG DeviceNumber;
        ULONG64 DiskSize;
        ULONG SectorSize;
        WCHAR DriveLetter;
        BOOLEAN RemovableMedia;
        BOOLEAN CdRomType;
        WCHAR FileName[MAX_PATH]; // Optional backing file
    } TEMP_CREATE_DATA, *PTEMP_CREATE_DATA;

    // Device extension structure
    typedef struct _TEMP_DEVICE_EXTENSION
    {
        ULONG DeviceNumber;
        ULONG64 DiskSize;
        ULONG SectorSize;
        WCHAR DriveLetter;
        BOOLEAN RemovableMedia;
        BOOLEAN CdRomType;

        PTEMP_MEMORY_MANAGER MemoryManager;
        PDEVICE_OBJECT DeviceObject;
        PDEVICE_OBJECT PhysicalDeviceObject;

        UNICODE_STRING DeviceName;
        UNICODE_STRING SymbolicLinkName;

        volatile LONG ReferenceCount;
        KEVENT RemoveEvent;

        // I/O Statistics
        volatile LONG64 BytesRead;
        volatile LONG64 BytesWritten;
        volatile LONG64 ReadRequests;
        volatile LONG64 WriteRequests;
    } TEMP_DEVICE_EXTENSION, *PTEMP_DEVICE_EXTENSION;

    // Statistics structure for user mode
    typedef struct _TEMP_STATISTICS
    {
        ULONG DeviceNumber;
        ULONG64 DiskSize;
        ULONG64 MemoryUsed;
        ULONG64 TotalReads;
        ULONG64 TotalWrites;
        ULONG64 BytesRead;
        ULONG64 BytesWritten;
        ULONG64 CacheHits;
        ULONG64 CacheMisses;
        ULONG64 EvictionCount;
    } TEMP_STATISTICS, *PTEMP_STATISTICS;

    // Function declarations
    NTSTATUS TempInitializeMemoryManager(PTEMP_MEMORY_MANAGER MemoryManager, ULONG64 MaxSize);
    VOID TempCleanupMemoryManager(PTEMP_MEMORY_MANAGER MemoryManager);
    NTSTATUS TempReadSectors(PTEMP_MEMORY_MANAGER MemoryManager, ULONG64 StartSector, ULONG SectorCount, PVOID Buffer, ULONG SectorSize);
    NTSTATUS TempWriteSectors(PTEMP_MEMORY_MANAGER MemoryManager, ULONG64 StartSector, ULONG SectorCount, PVOID Buffer, ULONG SectorSize);
    NTSTATUS TempFormatDisk(PTEMP_MEMORY_MANAGER MemoryManager, ULONG64 DiskSize, ULONG SectorSize);

    // Utility functions
    ULONG64 TempHashFunction(ULONG64 SectorAddress);
    ULONG TempGetBucketIndex(ULONG64 Hash);
    NTSTATUS TempAllocateChunk(PTEMP_BUCKET Bucket, PTEMP_CHUNK *Chunk);
    VOID TempReleaseChunk(PTEMP_BUCKET Bucket, PTEMP_CHUNK Chunk);

    // Driver entry points
    NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath);
    VOID TempUnloadDriver(PDRIVER_OBJECT DriverObject);
    NTSTATUS TempCreateDevice(PDRIVER_OBJECT DriverObject, PTEMP_CREATE_DATA CreateData);
    NTSTATUS TempRemoveDevice(ULONG DeviceNumber);

    // IRP handlers
    NTSTATUS TempDispatchCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp);
    NTSTATUS TempDispatchReadWrite(PDEVICE_OBJECT DeviceObject, PIRP Irp);
    NTSTATUS TempDispatchDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp);
    NTSTATUS TempDispatchPnP(PDEVICE_OBJECT DeviceObject, PIRP Irp);

#ifdef __cplusplus
}
#endif

#endif // TEMP_CORE_H