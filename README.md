# TEMP - High-Performance RAM Disk for Windows

**TEMP** is a production-ready RAM disk implementation that creates virtual disk drives from your computer's RAM memory. This provides ultra-fast storage perfect for gaming, development, and everyday computing.

## What is a RAM Disk?

A RAM disk creates virtual drives (like R: or Z:) using your computer's RAM memory instead of traditional storage. Since RAM is thousands of times faster than hard drives or SSDs, this provides exceptional performance benefits:

**For Gamers:**
- Reduce game loading times by 10-50x
- Eliminate texture streaming delays
- Speed up level transitions and save/load operations
- Perfect for large games like GTA V, Cyberpunk 2077, or modern flight simulators

**For Developers:**
- Dramatically speed up compilation and build processes
- Accelerate IDE performance and IntelliSense
- Speed up test execution and deployment
- Ideal for Node.js projects, Docker builds, and large codebases

**For Everyone:**
- Accelerate browser cache and downloads
- Speed up file extraction and compression
- Perfect for temporary files and scratch space
- Boost video editing and rendering workflows

**Example**: If your computer has drive C: and D:, TEMP can create additional drives like R: that exist entirely in RAM, appearing in Windows Explorer just like any other drive.

**TEMP** combines the best features of ImDisk with fastcache-inspired memory management optimizations for superior performance and reliability on modern Windows systems.

## Features

### Core Capabilities
- **High-Performance Memory Management**: Bucket-based caching system with 512 concurrent buckets for optimal scalability
- **Thread-Safe Operations**: Full thread safety with per-bucket locking for maximum concurrency
- **Memory Efficient**: 64KB chunk-based storage system minimizing memory fragmentation
- **Modern Windows Support**: Built for Windows 10/11 with proper driver architecture
- **Production Ready**: Designed for mission-critical workloads with robust error handling

### Technical Features
- **Multiple RAM Disks**: Support for up to 32 concurrent RAM disk devices
- **Flexible Sizing**: RAM disks from kilobytes to terabytes (hardware permitting)
- **Drive Letter Assignment**: Automatic drive letter assignment (A-Z)
- **Device Type Emulation**: Support for fixed disks, removable media, and CD-ROM emulation
- **Real-Time Statistics**: Comprehensive I/O and cache performance monitoring
- **LRU Eviction**: Least Recently Used cache eviction for optimal memory utilization

### User Experience
- **Simple CLI Interface**: Easy-to-use command-line tool for device management
- **Automated Installation**: Complete build, install, and uninstall automation
- **Comprehensive Help**: Built-in documentation and troubleshooting guides
- **Status Monitoring**: Real-time device statistics and health monitoring

## Architecture

### Driver Architecture
The TEMP driver follows Windows kernel driver best practices:

- **Layered Design**: Separate core, driver, and CLI components for maintainability
- **Windows Driver Model**: Proper IRP handling and device object management
- **Memory Pool Management**: Proper kernel memory allocation with pool tags
- **Service Integration**: Windows service integration for automatic startup

### Memory Management
Inspired by high-performance caching systems:

- **Bucket-Based Sharding**: 512 independent buckets to minimize lock contention
- **Hash-Based Lookup**: Fast O(1) sector address resolution
- **Generation-Based LRU**: Efficient cache eviction without linked lists
- **Reference Counting**: Safe memory management with proper cleanup

### Performance Characteristics
- **Scalable Concurrency**: Performance scales with CPU core count
- **Low Latency**: Direct memory access without filesystem overhead
- **High Throughput**: Optimized for both small and large I/O operations
- **Memory Efficient**: Minimal overhead per stored byte

## System Requirements

### Operating System
- Windows 10 (version 1903 or later)
- Windows 11 (all versions)
- Windows Server 2019/2022

### Development Requirements
- Windows Driver Kit (WDK) 10
- Visual Studio 2017 or later with C++ support
- .NET 6.0 SDK or later (for GUI application)
- Administrator privileges for installation

### Runtime Requirements
- Minimum 1GB RAM (plus desired RAM disk size)
- Administrator privileges for creating/removing RAM disks
- NTFS or FAT32 filesystem support

## Installation

### Quick Start
1. **Download** the latest release from GitHub
2. **Extract** the archive to a local directory
3. **Run as Administrator**: `build.bat`
4. **Run as Administrator**: `install.bat`
5. **Launch**: Start Menu → TEMP RAM Disk → TEMP RAM Disk Manager

**Note**: The installer automatically creates both command-line and graphical interfaces. The GUI requires .NET 6.0 (automatically detected during build).

### Detailed Installation Steps

#### 1. Build the Driver
```batch
# Run as Administrator
build.bat
```
This will:
- Detect Windows Driver Kit installation
- Find Visual Studio compiler
- Compile the kernel driver
- Build the command-line interface
- Create installation files

#### 2. Install the Driver
```batch
# Run as Administrator
install.bat
```
This will:
- Copy driver files to system directories
- Create Windows service
- Start the driver service
- Install CLI tool globally
- Test the installation

#### 3. Verify Installation
```cmd
temp.exe version
```

## Usage

### Graphical Interface (Recommended)

The easiest way to use TEMP is through the beautiful graphical interface:

1. **Launch**: Start Menu → TEMP RAM Disk → TEMP RAM Disk Manager
2. **Create**: Enter size (e.g., 256), select unit (MB/GB), choose drive letter, click "⭐ Create RAM Disk"
3. **Monitor**: View real-time statistics, cache hit ratios, and memory usage
4. **Manage**: Use built-in buttons to view stats, open drives, or remove RAM disks

**Features of the GUI:**
- Real-time system memory monitoring
- Easy drag-and-drop style interface
- Live performance statistics with charts
- One-click drive opening in Windows Explorer
- Automatic drive letter detection
- No command-line knowledge required

### Command Line Interface

For advanced users and automation:

#### Basic Usage
```cmd
# Create 256MB RAM disk with drive letter R:
temp.exe create --size 256M --drive R

# Create 1GB RAM disk on device 1
temp.exe create --size 1G --device 1

# Create removable 512MB RAM disk
temp.exe create --size 512M --drive S --removable
```

#### Size Specifications
- **Bytes**: `1048576` (exact byte count)
- **Kilobytes**: `512K` or `512KB`
- **Megabytes**: `256M` or `256MB`
- **Gigabytes**: `4G` or `4GB`
- **Terabytes**: `1T` or `1TB`

#### Advanced Options
```cmd
# CD-ROM emulation
temp.exe create --size 700M --drive D --cdrom

# Custom sector size
temp.exe create --size 128M --drive T --sector-size 4096

# Specific device number
temp.exe create --size 64M --device 5 --drive U
```

### Managing RAM Disks

#### List Active Devices
```cmd
temp.exe list
```

#### View Statistics
```cmd
# Statistics for device 0
temp.exe stats 0
```

#### Remove RAM Disks
```cmd
# Remove device 0
temp.exe remove 0

# Remove all devices (in uninstall.bat)
```

### Command Reference

| Command | Description | Example |
|---------|-------------|---------|
| `create` | Create new RAM disk | `temp.exe create --size 256M --drive R` |
| `remove` | Remove RAM disk | `temp.exe remove 0` |
| `list` | List active RAM disks | `temp.exe list` |
| `stats` | Show device statistics | `temp.exe stats 0` |
| `version` | Show version info | `temp.exe version` |
| `help` | Show detailed help | `temp.exe help` |

## Configuration

### Default Settings
- **Bucket Count**: 512 (optimized for multi-core systems)
- **Chunk Size**: 64KB (balance of memory efficiency and performance)
- **Sector Size**: 512 bytes (standard disk sector size)
- **Max Devices**: 32 concurrent RAM disks
- **Max Disk Size**: 1TB per device

### Customization
The driver can be customized by modifying constants in `src/core/temp_core.h`:

```c
#define TEMP_BUCKET_COUNT 512        // Number of memory buckets
#define TEMP_CHUNK_SIZE (64 * 1024)  // Chunk size in bytes
#define TEMP_MAX_DEVICES 32          // Maximum concurrent devices
```

## Monitoring and Statistics

### Available Metrics
- **I/O Operations**: Read/write request counts
- **Throughput**: Bytes read/written
- **Cache Performance**: Hit/miss ratios
- **Memory Usage**: Current memory utilization
- **Eviction Statistics**: Cache eviction events

### Statistics Example
```
Statistics for RAM Disk 0:
  Disk Size: 268435456 bytes (256.00 MB)
  Memory Used: 134217728 bytes (128.00 MB)
  Total Reads: 1024
  Total Writes: 512
  Bytes Read: 524288000 (500.00 MB)
  Bytes Written: 262144000 (250.00 MB)
  Cache Hits: 896
  Cache Misses: 128
  Cache Hit Ratio: 87.50%
  Evictions: 0
```

## Troubleshooting

### Common Issues

#### Driver Won't Start
**Symptoms**: Service fails to start, error in Event Viewer
**Solutions**:
1. Verify Windows Driver Kit installation
2. Check for conflicting drivers
3. Run `install.bat` as Administrator
4. Check Windows Event Viewer for detailed errors

#### Permission Denied
**Symptoms**: Access denied when creating RAM disks
**Solutions**:
1. Run command prompt as Administrator
2. Verify user has "Create symbolic links" privilege
3. Check UAC settings

#### Drive Letter Conflicts
**Symptoms**: Cannot assign requested drive letter
**Solutions**:
1. Use `temp.exe list` to check existing assignments
2. Choose different drive letter
3. Disconnect conflicting drives/devices

#### Performance Issues
**Symptoms**: Slower than expected performance
**Solutions**:
1. Check available system memory
2. Verify adequate free memory for caching
3. Monitor cache hit ratios with `temp.exe stats`
4. Consider reducing number of concurrent RAM disks

### Diagnostic Commands

```cmd
# Check driver status
sc query TempRamDisk

# View driver logs (Event Viewer)
eventvwr.msc

# Test basic functionality
temp.exe create --size 64M --drive T
echo "test" > T:\test.txt
type T:\test.txt
temp.exe remove 0
```

### Log Files
- **Windows Event Viewer**: System logs for driver events
- **Service Logs**: `Applications and Services Logs > System`
- **Installation Logs**: Console output from `install.bat`

## Uninstallation

### Complete Removal
```batch
# Run as Administrator
uninstall.bat
```

This will:
1. Remove all active RAM disks
2. Stop and remove the Windows service
3. Delete driver files from system directories
4. Clean registry entries
5. Remove shortcuts and CLI tools

### Manual Cleanup
If automatic uninstallation fails:

```cmd
# Stop service manually
sc stop TempRamDisk
sc delete TempRamDisk

# Remove driver file
del %SystemRoot%\System32\drivers\temp.sys

# Remove CLI tool
del %SystemRoot%\System32\temp.exe
```

## Development

### Building from Source

#### Prerequisites
1. Install Windows Driver Kit (WDK) 10
2. Install Visual Studio 2017+ with C++ support
3. Clone this repository

#### Build Process
```batch
# Navigate to project directory
cd temp-ramdisk

# Build driver and CLI
build.bat
```

#### Project Structure
```
temp-ramdisk/
├── src/
│   ├── core/           # Core data structures and memory management
│   ├── driver/         # Windows kernel driver implementation
│   └── cli/            # Command-line interface
├── build.bat           # Automated build script
├── install.bat         # Installation script
├── uninstall.bat       # Uninstallation script
├── ForGitHub.bat       # Package for distribution
└── README.md           # This file
```

### Contributing

1. **Fork** the repository
2. **Create** a feature branch
3. **Make** your changes
4. **Test** thoroughly on target Windows versions
5. **Submit** a pull request

### Code Style
- Follow Windows driver development guidelines
- Use proper error handling with NTSTATUS codes
- Include comprehensive comments
- Test on both debug and release builds

## Compatibility

### Windows Versions
- ✅ Windows 10 (1903+)
- ✅ Windows 11 (all versions)
- ✅ Windows Server 2019
- ✅ Windows Server 2022
- ⚠️ Windows 10 (older versions) - may work but not tested
- ❌ Windows 8.1 and earlier - not supported

### Architecture Support
- ✅ x64 (AMD64)
- ⚠️ x86 (32-bit) - compile-time support, limited testing
- ❌ ARM/ARM64 - not supported in current version

### Filesystem Compatibility
RAM disks can be formatted with any Windows-supported filesystem:
- ✅ NTFS (recommended)
- ✅ FAT32
- ✅ exFAT
- ✅ ReFS (Windows Server)

## Security Considerations

### Privileges Required
- **Installation**: Administrator privileges required
- **Driver Operations**: Kernel mode execution
- **RAM Disk Creation**: Administrator or elevated user
- **RAM Disk Access**: Standard user (once created)

### Data Security
- **Volatile Storage**: All data lost on shutdown/restart
- **Memory Clearing**: Memory is not explicitly cleared on removal
- **Access Control**: Standard Windows filesystem permissions apply
- **Encryption**: Use BitLocker or EFS for encryption if needed

### Network Security
- **Local Only**: Driver operates locally, no network functionality
- **Service Account**: Runs under LocalSystem account
- **Attack Surface**: Minimal attack surface, standard driver security model

## License

This project is licensed under the GNU General Public License v2.0 (GPL-2.0).

See the [LICENSE](LICENSE) file for full license text.

### License Summary
- ✅ Commercial use allowed
- ✅ Modification allowed
- ✅ Distribution allowed
- ✅ Private use allowed
- ❗ Source code must be made available when distributing
- ❗ Same license must be used for derivatives
- ❗ Changes must be documented

## Support

### Getting Help
1. **Documentation**: Read this README thoroughly
2. **Troubleshooting**: Check troubleshooting section above
3. **Issues**: Create GitHub issue with detailed information
4. **Discussions**: Use GitHub Discussions for questions

### Reporting Issues
When reporting issues, please include:
- Windows version and build number
- Complete error messages
- Steps to reproduce
- Output of `temp.exe version`
- Relevant Event Viewer logs

### Feature Requests
Feature requests are welcome via GitHub Issues. Please include:
- Use case description
- Expected behavior
- Potential implementation approach
- Impact on existing functionality

## Acknowledgments

This project was inspired by and learned from:
- **ImDisk Virtual Disk Driver**: Windows driver architecture patterns
- **FastCache**: High-performance memory management techniques
- **Windows Driver Kit Documentation**: Microsoft's driver development guidelines
- **VictoriaMetrics**: Bucket-based caching implementation strategies

---

**TEMP RAM Disk** - High-performance, production-ready RAM disk solution for Windows.
