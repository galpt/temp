#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winioctl.h>
#ifndef SIMPLIFIED_BUILD
#include "../core/temp_core.h"
#endif

// Command line options
typedef enum
{
    CMD_CREATE,
    CMD_REMOVE,
    CMD_LIST,
    CMD_STATS,
    CMD_VERSION,
    CMD_HELP,
    CMD_INVALID
} COMMAND_TYPE;

typedef struct
{
    COMMAND_TYPE Command;
    ULONG DeviceNumber;
    ULONG64 DiskSize;
    ULONG SectorSize;
    WCHAR DriveLetter;
    BOOLEAN RemovableMedia;
    BOOLEAN CdRomType;
    BOOLEAN ShowHelp;
} COMMAND_OPTIONS;

// Version information
#define TEMP_CLI_VERSION "1.0.0"

#ifdef SIMPLIFIED_BUILD
// Simplified structures for builds without full driver support
typedef struct
{
    ULONG DeviceNumber;
    ULONG64 DiskSize;
    ULONG SectorSize;
    WCHAR DriveLetter;
    BOOLEAN RemovableMedia;
    BOOLEAN CdRomType;
    WCHAR FileName[MAX_PATH];
} TEMP_CREATE_DATA_SIMPLE;

typedef struct
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
} TEMP_STATISTICS_SIMPLE;

#define TEMP_CREATE_DATA TEMP_CREATE_DATA_SIMPLE
#define TEMP_STATISTICS TEMP_STATISTICS_SIMPLE
#define PTEMP_CREATE_DATA TEMP_CREATE_DATA_SIMPLE *
#define PTEMP_STATISTICS TEMP_STATISTICS_SIMPLE *

// IOCTLs for simplified build
#define TEMP_IOCTL_CREATE_DEVICE 0x83000800
#define TEMP_IOCTL_REMOVE_DEVICE 0x83000801
#define TEMP_IOCTL_LIST_DEVICES 0x83000802
#define TEMP_IOCTL_GET_VERSION 0x83000803
#define TEMP_IOCTL_GET_STATISTICS 0x83000804
#endif

// Function prototypes
COMMAND_TYPE ParseCommand(int argc, char *argv[], COMMAND_OPTIONS *options);
void ShowHelp(const char *programName);
void ShowVersion(void);
NTSTATUS CreateRamDisk(const COMMAND_OPTIONS *options);
NTSTATUS RemoveRamDisk(ULONG deviceNumber);
NTSTATUS ListRamDisks(void);
NTSTATUS ShowStatistics(ULONG deviceNumber);
ULONG64 ParseSize(const char *sizeStr);
HANDLE OpenControlDevice(void);

int main(int argc, char *argv[])
{
    COMMAND_OPTIONS options = {0};
    NTSTATUS status = STATUS_SUCCESS;

    // Parse command line
    COMMAND_TYPE command = ParseCommand(argc, argv, &options);

    switch (command)
    {
    case CMD_CREATE:
        status = CreateRamDisk(&options);
        break;

    case CMD_REMOVE:
        status = RemoveRamDisk(options.DeviceNumber);
        break;

    case CMD_LIST:
        status = ListRamDisks();
        break;

    case CMD_STATS:
        status = ShowStatistics(options.DeviceNumber);
        break;

    case CMD_VERSION:
        ShowVersion();
        break;

    case CMD_HELP:
        ShowHelp(argv[0]);
        break;

    case CMD_INVALID:
    default:
        printf("Invalid command. Use --help for usage information.\n");
        return 1;
    }

    if (!NT_SUCCESS(status))
    {
        printf("Operation failed with error: 0x%08X\n", status);
        return 1;
    }

    return 0;
}

COMMAND_TYPE ParseCommand(int argc, char *argv[], COMMAND_OPTIONS *options)
{
    if (argc < 2)
    {
        return CMD_HELP;
    }

    // Initialize defaults
    options->DeviceNumber = 0;
    options->DiskSize = 64 * 1024 * 1024; // 64MB default
    options->SectorSize = TEMP_DEFAULT_SECTOR_SIZE;
    options->DriveLetter = 0;
    options->RemovableMedia = FALSE;
    options->CdRomType = FALSE;
    options->ShowHelp = FALSE;

    // Parse main command
    if (strcmp(argv[1], "create") == 0)
    {
        options->Command = CMD_CREATE;

        // Parse create options
        for (int i = 2; i < argc; i++)
        {
            if (strcmp(argv[i], "--size") == 0 && i + 1 < argc)
            {
                options->DiskSize = ParseSize(argv[++i]);
            }
            else if (strcmp(argv[i], "--drive") == 0 && i + 1 < argc)
            {
                char *driveStr = argv[++i];
                if (strlen(driveStr) == 1 && driveStr[0] >= 'A' && driveStr[0] <= 'Z')
                {
                    options->DriveLetter = (WCHAR)driveStr[0];
                }
                else if (strlen(driveStr) == 1 && driveStr[0] >= 'a' && driveStr[0] <= 'z')
                {
                    options->DriveLetter = (WCHAR)(driveStr[0] - 'a' + 'A');
                }
            }
            else if (strcmp(argv[i], "--device") == 0 && i + 1 < argc)
            {
                options->DeviceNumber = atoi(argv[++i]);
            }
            else if (strcmp(argv[i], "--sector-size") == 0 && i + 1 < argc)
            {
                options->SectorSize = atoi(argv[++i]);
            }
            else if (strcmp(argv[i], "--removable") == 0)
            {
                options->RemovableMedia = TRUE;
            }
            else if (strcmp(argv[i], "--cdrom") == 0)
            {
                options->CdRomType = TRUE;
            }
        }

        if (options->DeviceNumber >= TEMP_MAX_DEVICES)
        {
            printf("Error: Device number must be between 0 and %d\n", TEMP_MAX_DEVICES - 1);
            return CMD_INVALID;
        }

        return CMD_CREATE;
    }
    else if (strcmp(argv[1], "remove") == 0)
    {
        options->Command = CMD_REMOVE;

        if (argc >= 3)
        {
            options->DeviceNumber = atoi(argv[2]);
        }
        else
        {
            printf("Error: Device number required for remove command\n");
            return CMD_INVALID;
        }

        return CMD_REMOVE;
    }
    else if (strcmp(argv[1], "list") == 0)
    {
        return CMD_LIST;
    }
    else if (strcmp(argv[1], "stats") == 0)
    {
        options->Command = CMD_STATS;

        if (argc >= 3)
        {
            options->DeviceNumber = atoi(argv[2]);
        }
        else
        {
            printf("Error: Device number required for stats command\n");
            return CMD_INVALID;
        }

        return CMD_STATS;
    }
    else if (strcmp(argv[1], "version") == 0 || strcmp(argv[1], "--version") == 0)
    {
        return CMD_VERSION;
    }
    else if (strcmp(argv[1], "help") == 0 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)
    {
        return CMD_HELP;
    }
    else
    {
        return CMD_INVALID;
    }
}

void ShowHelp(const char *programName)
{
    printf("TEMP - High-Performance RAM Disk for Windows\n");
    printf("Version %d.%d.%d\n\n", TEMP_VERSION_MAJOR, TEMP_VERSION_MINOR, TEMP_VERSION_BUILD);

    printf("Usage: %s <command> [options]\n\n", programName);

    printf("Commands:\n");
    printf("  create          Create a new RAM disk\n");
    printf("  remove <num>    Remove RAM disk by device number\n");
    printf("  list            List all RAM disks\n");
    printf("  stats <num>     Show statistics for device number\n");
    printf("  version         Show version information\n");
    printf("  help            Show this help message\n\n");

    printf("Create Options:\n");
    printf("  --size <size>        Disk size (e.g., 128M, 1G, 2048K)\n");
    printf("  --drive <letter>     Drive letter (A-Z)\n");
    printf("  --device <num>       Device number (0-%d)\n", TEMP_MAX_DEVICES - 1);
    printf("  --sector-size <size> Sector size in bytes (default: 512)\n");
    printf("  --removable          Mark as removable media\n");
    printf("  --cdrom              Emulate CD-ROM drive\n\n");

    printf("Size Examples:\n");
    printf("  64M     64 megabytes\n");
    printf("  1G      1 gigabyte\n");
    printf("  512K    512 kilobytes\n");
    printf("  1048576 1048576 bytes\n\n");

    printf("Examples:\n");
    printf("  %s create --size 256M --drive R\n", programName);
    printf("  %s create --size 1G --device 1 --removable\n", programName);
    printf("  %s remove 0\n", programName);
    printf("  %s list\n", programName);
    printf("  %s stats 0\n", programName);
}

void ShowVersion(void)
{
    HANDLE hDevice = OpenControlDevice();
    if (hDevice != INVALID_HANDLE_VALUE)
    {
        ULONG version = 0;
        DWORD bytesReturned = 0;

        if (DeviceIoControl(hDevice, TEMP_IOCTL_GET_VERSION, NULL, 0,
                            &version, sizeof(version), &bytesReturned, NULL))
        {
            ULONG major = (version >> 16) & 0xFF;
            ULONG minor = (version >> 8) & 0xFF;
            ULONG build = version & 0xFF;

            printf("TEMP RAM Disk Driver Version: %d.%d.%d\n", major, minor, build);
        }
        else
        {
            printf("TEMP RAM Disk Driver: Version information unavailable\n");
        }

        CloseHandle(hDevice);
    }
    else
    {
        printf("TEMP RAM Disk Driver: Not installed or not running\n");
    }

    printf("Command Line Tool Version: %d.%d.%d\n",
           TEMP_VERSION_MAJOR, TEMP_VERSION_MINOR, TEMP_VERSION_BUILD);
}

NTSTATUS CreateRamDisk(const COMMAND_OPTIONS *options)
{
    HANDLE hDevice = OpenControlDevice();
    if (hDevice == INVALID_HANDLE_VALUE)
    {
        printf("Error: Cannot open control device. Driver may not be installed.\n");
        return STATUS_DEVICE_NOT_READY;
    }

    TEMP_CREATE_DATA createData = {0};
    createData.DeviceNumber = options->DeviceNumber;
    createData.DiskSize = options->DiskSize;
    createData.SectorSize = options->SectorSize;
    createData.DriveLetter = options->DriveLetter;
    createData.RemovableMedia = options->RemovableMedia;
    createData.CdRomType = options->CdRomType;

    DWORD bytesReturned = 0;
    BOOL success = DeviceIoControl(
        hDevice,
        TEMP_IOCTL_CREATE_DEVICE,
        &createData,
        sizeof(createData),
        NULL,
        0,
        &bytesReturned,
        NULL);

    CloseHandle(hDevice);

    if (success)
    {
        printf("RAM disk created successfully:\n");
        printf("  Device Number: %d\n", options->DeviceNumber);
        printf("  Size: %llu bytes (%.2f MB)\n", options->DiskSize,
               (double)options->DiskSize / (1024.0 * 1024.0));
        printf("  Sector Size: %d bytes\n", options->SectorSize);

        if (options->DriveLetter)
        {
            printf("  Drive Letter: %C:\n", options->DriveLetter);
        }

        if (options->RemovableMedia)
        {
            printf("  Type: Removable Media\n");
        }
        else if (options->CdRomType)
        {
            printf("  Type: CD-ROM\n");
        }
        else
        {
            printf("  Type: Fixed Disk\n");
        }

        return STATUS_SUCCESS;
    }
    else
    {
        DWORD error = GetLastError();
        printf("Failed to create RAM disk. Windows error: %d\n", error);
        return STATUS_UNSUCCESSFUL;
    }
}

NTSTATUS RemoveRamDisk(ULONG deviceNumber)
{
    HANDLE hDevice = OpenControlDevice();
    if (hDevice == INVALID_HANDLE_VALUE)
    {
        printf("Error: Cannot open control device. Driver may not be installed.\n");
        return STATUS_DEVICE_NOT_READY;
    }

    DWORD bytesReturned = 0;
    BOOL success = DeviceIoControl(
        hDevice,
        TEMP_IOCTL_REMOVE_DEVICE,
        &deviceNumber,
        sizeof(deviceNumber),
        NULL,
        0,
        &bytesReturned,
        NULL);

    CloseHandle(hDevice);

    if (success)
    {
        printf("RAM disk %d removed successfully.\n", deviceNumber);
        return STATUS_SUCCESS;
    }
    else
    {
        DWORD error = GetLastError();
        printf("Failed to remove RAM disk %d. Windows error: %d\n", deviceNumber, error);
        return STATUS_UNSUCCESSFUL;
    }
}

NTSTATUS ListRamDisks(void)
{
    printf("Active RAM Disks:\n");
    printf("Device | Size      | Drive | Type\n");
    printf("-------|-----------|-------|------------------\n");

    BOOLEAN foundAny = FALSE;

    // Try to get statistics for each possible device number
    for (ULONG i = 0; i < TEMP_MAX_DEVICES; i++)
    {
        WCHAR devicePath[64];
        swprintf_s(devicePath, ARRAYSIZE(devicePath), L"\\\\.\\TempRamDisk%d", i);

        HANDLE hDevice = CreateFileW(
            devicePath,
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            0,
            NULL);

        if (hDevice != INVALID_HANDLE_VALUE)
        {
            TEMP_STATISTICS stats = {0};
            DWORD bytesReturned = 0;

            if (DeviceIoControl(hDevice, TEMP_IOCTL_GET_STATISTICS, NULL, 0,
                                &stats, sizeof(stats), &bytesReturned, NULL))
            {

                foundAny = TRUE;

                char sizeStr[32];
                if (stats.DiskSize >= 1024 * 1024 * 1024)
                {
                    sprintf_s(sizeStr, sizeof(sizeStr), "%.1f GB",
                              (double)stats.DiskSize / (1024.0 * 1024.0 * 1024.0));
                }
                else if (stats.DiskSize >= 1024 * 1024)
                {
                    sprintf_s(sizeStr, sizeof(sizeStr), "%.1f MB",
                              (double)stats.DiskSize / (1024.0 * 1024.0));
                }
                else
                {
                    sprintf_s(sizeStr, sizeof(sizeStr), "%.1f KB",
                              (double)stats.DiskSize / 1024.0);
                }

                printf("%-6d | %-9s | %-5s | %s\n",
                       i, sizeStr, "N/A", "RAM Disk");
            }

            CloseHandle(hDevice);
        }
    }

    if (!foundAny)
    {
        printf("No RAM disks found.\n");
    }

    return STATUS_SUCCESS;
}

NTSTATUS ShowStatistics(ULONG deviceNumber)
{
    WCHAR devicePath[64];
    swprintf_s(devicePath, ARRAYSIZE(devicePath), L"\\\\.\\TempRamDisk%d", deviceNumber);

    HANDLE hDevice = CreateFileW(
        devicePath,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        0,
        NULL);

    if (hDevice == INVALID_HANDLE_VALUE)
    {
        printf("Error: Cannot open device %d. Device may not exist.\n", deviceNumber);
        return STATUS_NO_SUCH_DEVICE;
    }

    TEMP_STATISTICS stats = {0};
    DWORD bytesReturned = 0;

    BOOL success = DeviceIoControl(
        hDevice,
        TEMP_IOCTL_GET_STATISTICS,
        NULL,
        0,
        &stats,
        sizeof(stats),
        &bytesReturned,
        NULL);

    CloseHandle(hDevice);

    if (success)
    {
        printf("Statistics for RAM Disk %d:\n", deviceNumber);
        printf("  Disk Size: %llu bytes (%.2f MB)\n", stats.DiskSize,
               (double)stats.DiskSize / (1024.0 * 1024.0));
        printf("  Memory Used: %llu bytes (%.2f MB)\n", stats.MemoryUsed,
               (double)stats.MemoryUsed / (1024.0 * 1024.0));
        printf("  Total Reads: %llu\n", stats.TotalReads);
        printf("  Total Writes: %llu\n", stats.TotalWrites);
        printf("  Bytes Read: %llu (%.2f MB)\n", stats.BytesRead,
               (double)stats.BytesRead / (1024.0 * 1024.0));
        printf("  Bytes Written: %llu (%.2f MB)\n", stats.BytesWritten,
               (double)stats.BytesWritten / (1024.0 * 1024.0));
        printf("  Cache Hits: %llu\n", stats.CacheHits);
        printf("  Cache Misses: %llu\n", stats.CacheMisses);

        if (stats.CacheHits + stats.CacheMisses > 0)
        {
            double hitRatio = (double)stats.CacheHits / (stats.CacheHits + stats.CacheMisses) * 100.0;
            printf("  Cache Hit Ratio: %.2f%%\n", hitRatio);
        }

        printf("  Evictions: %llu\n", stats.EvictionCount);

        return STATUS_SUCCESS;
    }
    else
    {
        DWORD error = GetLastError();
        printf("Failed to get statistics for device %d. Windows error: %d\n", deviceNumber, error);
        return STATUS_UNSUCCESSFUL;
    }
}

ULONG64 ParseSize(const char *sizeStr)
{
    if (!sizeStr)
    {
        return 0;
    }

    char *endPtr;
    ULONG64 value = _strtoui64(sizeStr, &endPtr, 10);

    if (endPtr && *endPtr)
    {
        char unit = toupper(*endPtr);
        switch (unit)
        {
        case 'K':
            value *= 1024;
            break;
        case 'M':
            value *= 1024 * 1024;
            break;
        case 'G':
            value *= 1024ULL * 1024 * 1024;
            break;
        case 'T':
            value *= 1024ULL * 1024 * 1024 * 1024;
            break;
        }
    }

    return value;
}

HANDLE OpenControlDevice(void)
{
    return CreateFileW(
        L"\\\\.\\TempRamDiskControl",
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        0,
        NULL);
}