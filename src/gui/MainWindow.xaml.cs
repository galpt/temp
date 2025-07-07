using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Management;
using System.Runtime.InteropServices;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Threading;

namespace TempRamDiskGUI
{
    public partial class MainWindow : Window, INotifyPropertyChanged
    {
        // P/Invoke declarations for driver communication
        [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Auto)]
        private static extern IntPtr CreateFile(
            string lpFileName,
            uint dwDesiredAccess,
            uint dwShareMode,
            IntPtr lpSecurityAttributes,
            uint dwCreationDisposition,
            uint dwFlagsAndAttributes,
            IntPtr hTemplateFile);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool DeviceIoControl(
            IntPtr hDevice,
            uint dwIoControlCode,
            IntPtr lpInBuffer,
            uint nInBufferSize,
            IntPtr lpOutBuffer,
            uint nOutBufferSize,
            out uint lpBytesReturned,
            IntPtr lpOverlapped);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool CloseHandle(IntPtr hObject);

        [DllImport("kernel32.dll")]
        private static extern bool GlobalMemoryStatusEx(ref MEMORYSTATUSEX lpBuffer);

        // Constants
        private const uint GENERIC_READ = 0x80000000;
        private const uint GENERIC_WRITE = 0x40000000;
        private const uint FILE_SHARE_READ = 0x00000001;
        private const uint FILE_SHARE_WRITE = 0x00000002;
        private const uint OPEN_EXISTING = 3;
        private const uint TEMP_IOCTL_CREATE_DEVICE = 0x83000800;
        private const uint TEMP_IOCTL_REMOVE_DEVICE = 0x83000801;
        private const uint TEMP_IOCTL_GET_STATISTICS = 0x83000804;
        private const IntPtr INVALID_HANDLE_VALUE = (IntPtr)(-1);

        // Memory status structure
        [StructLayout(LayoutKind.Sequential)]
        public struct MEMORYSTATUSEX
        {
            public uint dwLength;
            public uint dwMemoryLoad;
            public ulong ullTotalPhys;
            public ulong ullAvailPhys;
            public ulong ullTotalPageFile;
            public ulong ullAvailPageFile;
            public ulong ullTotalVirtual;
            public ulong ullAvailVirtual;
            public ulong ullAvailExtendedVirtual;
        }

        // Data structures for driver communication
        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
        public struct TempCreateData
        {
            public uint DeviceNumber;
            public ulong DiskSize;
            public uint SectorSize;
            public char DriveLetter;
            public bool RemovableMedia;
            public bool CdRomType;
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 260)]
            public string FileName;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct TempStatistics
        {
            public uint DeviceNumber;
            public ulong DiskSize;
            public ulong MemoryUsed;
            public ulong TotalReads;
            public ulong TotalWrites;
            public ulong BytesRead;
            public ulong BytesWritten;
            public ulong CacheHits;
            public ulong CacheMisses;
            public ulong EvictionCount;
        }

        // RAM disk info class for data binding
        public class RamDiskInfo
        {
            public int DeviceNumber { get; set; }
            public string DriveLetter { get; set; }
            public string SizeFormatted { get; set; }
            public string DeviceType { get; set; }
            public string MemoryUsedFormatted { get; set; }
            public string CacheHitRatio { get; set; }
            public string ReadWriteInfo { get; set; }
            public ulong DiskSize { get; set; }
        }

        // Properties for data binding
        public ObservableCollection<RamDiskInfo> RamDisks { get; set; }
        
        private string _systemMemoryInfo;
        public string SystemMemoryInfo
        {
            get => _systemMemoryInfo;
            set { _systemMemoryInfo = value; OnPropertyChanged(nameof(SystemMemoryInfo)); }
        }

        private string _driverStatus;
        public string DriverStatus
        {
            get => _driverStatus;
            set { _driverStatus = value; OnPropertyChanged(nameof(DriverStatus)); }
        }

        private DispatcherTimer _refreshTimer;

        public MainWindow()
        {
            InitializeComponent();
            DataContext = this;
            RamDisks = new ObservableCollection<RamDiskInfo>();
            RamDisksDataGrid.ItemsSource = RamDisks;

            InitializeControls();
            UpdateSystemInfo();
            RefreshRamDisks();

            // Set up auto-refresh timer
            _refreshTimer = new DispatcherTimer
            {
                Interval = TimeSpan.FromSeconds(5)
            };
            _refreshTimer.Tick += (s, e) => RefreshRamDisks();
            _refreshTimer.Start();
        }

        private void InitializeControls()
        {
            // Populate drive letters
            DriveLetterComboBox.Items.Clear();
            DriveLetterComboBox.Items.Add("Auto");
            
            var usedLetters = DriveInfo.GetDrives().Select(d => d.Name[0]).ToHashSet();
            
            for (char c = 'D'; c <= 'Z'; c++)
            {
                if (!usedLetters.Contains(c))
                {
                    DriveLetterComboBox.Items.Add(c.ToString());
                }
            }
            
            DriveLetterComboBox.SelectedIndex = 0;
        }

        private void UpdateSystemInfo()
        {
            try
            {
                var memStatus = new MEMORYSTATUSEX { dwLength = (uint)Marshal.SizeOf<MEMORYSTATUSEX>() };
                if (GlobalMemoryStatusEx(ref memStatus))
                {
                    var totalGB = memStatus.ullTotalPhys / (1024.0 * 1024.0 * 1024.0);
                    var availGB = memStatus.ullAvailPhys / (1024.0 * 1024.0 * 1024.0);
                    SystemMemoryInfo = $"RAM: {availGB:F1} GB available of {totalGB:F1} GB";
                }
                else
                {
                    SystemMemoryInfo = "RAM: Information unavailable";
                }

                // Check driver status
                DriverStatus = CheckDriverStatus() ? "Driver: Online" : "Driver: Offline";
            }
            catch (Exception ex)
            {
                SystemMemoryInfo = "RAM: Error reading info";
                DriverStatus = "Driver: Error";
                StatusText.Text = $"Error updating system info: {ex.Message}";
            }
        }

        private bool CheckDriverStatus()
        {
            try
            {
                var handle = CreateFile(
                    @"\\.\TempRamDiskControl",
                    GENERIC_READ | GENERIC_WRITE,
                    FILE_SHARE_READ | FILE_SHARE_WRITE,
                    IntPtr.Zero,
                    OPEN_EXISTING,
                    0,
                    IntPtr.Zero);

                if (handle != INVALID_HANDLE_VALUE)
                {
                    CloseHandle(handle);
                    return true;
                }
            }
            catch { }
            
            return false;
        }

        private void RefreshRamDisks()
        {
            try
            {
                RamDisks.Clear();

                for (int i = 0; i < 32; i++)
                {
                    var ramDisk = GetRamDiskInfo(i);
                    if (ramDisk != null)
                    {
                        RamDisks.Add(ramDisk);
                    }
                }

                StatusText.Text = $"Found {RamDisks.Count} active RAM disks";
            }
            catch (Exception ex)
            {
                StatusText.Text = $"Error refreshing: {ex.Message}";
            }
        }

        private RamDiskInfo GetRamDiskInfo(int deviceNumber)
        {
            try
            {
                var devicePath = $@"\\.\TempRamDisk{deviceNumber}";
                var handle = CreateFile(
                    devicePath,
                    GENERIC_READ,
                    FILE_SHARE_READ | FILE_SHARE_WRITE,
                    IntPtr.Zero,
                    OPEN_EXISTING,
                    0,
                    IntPtr.Zero);

                if (handle == INVALID_HANDLE_VALUE)
                    return null;

                var stats = new TempStatistics();
                var size = Marshal.SizeOf<TempStatistics>();
                var ptr = Marshal.AllocHGlobal(size);

                try
                {
                    if (DeviceIoControl(handle, TEMP_IOCTL_GET_STATISTICS, IntPtr.Zero, 0, ptr, (uint)size, out uint bytesReturned, IntPtr.Zero))
                    {
                        stats = Marshal.PtrToStructure<TempStatistics>(ptr);
                        
                        var hitRatio = stats.CacheHits + stats.CacheMisses > 0 
                            ? (double)stats.CacheHits / (stats.CacheHits + stats.CacheMisses) * 100.0 
                            : 0.0;

                        return new RamDiskInfo
                        {
                            DeviceNumber = deviceNumber,
                            DriveLetter = GetDriveLetterForDevice(deviceNumber),
                            SizeFormatted = FormatBytes(stats.DiskSize),
                            DeviceType = "RAM Disk",
                            MemoryUsedFormatted = FormatBytes(stats.MemoryUsed),
                            CacheHitRatio = $"{hitRatio:F1}%",
                            ReadWriteInfo = $"{FormatBytes(stats.BytesRead)} / {FormatBytes(stats.BytesWritten)}",
                            DiskSize = stats.DiskSize
                        };
                    }
                }
                finally
                {
                    Marshal.FreeHGlobal(ptr);
                    CloseHandle(handle);
                }
            }
            catch { }

            return null;
        }

        private string GetDriveLetterForDevice(int deviceNumber)
        {
            try
            {
                var drives = DriveInfo.GetDrives();
                // This is a simplified approach - in reality, you'd need to map device numbers to drive letters
                return "N/A";
            }
            catch
            {
                return "N/A";
            }
        }

        private string FormatBytes(ulong bytes)
        {
            string[] sizes = { "B", "KB", "MB", "GB", "TB" };
            double len = bytes;
            int order = 0;
            while (len >= 1024 && order < sizes.Length - 1)
            {
                order++;
                len = len / 1024;
            }
            return $"{len:F1} {sizes[order]}";
        }

        private async void CreateButton_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                CreateButton.IsEnabled = false;
                StatusText.Text = "Creating RAM disk...";

                if (!double.TryParse(SizeTextBox.Text, out double size) || size <= 0)
                {
                    MessageBox.Show("Please enter a valid size value.", "Invalid Size", MessageBoxButton.OK, MessageBoxImage.Warning);
                    return;
                }

                var selectedUnit = ((ComboBoxItem)SizeUnitComboBox.SelectedItem).Content.ToString();
                ulong sizeInBytes = (ulong)size;
                
                switch (selectedUnit)
                {
                    case "KB": sizeInBytes *= 1024; break;
                    case "MB": sizeInBytes *= 1024 * 1024; break;
                    case "GB": sizeInBytes *= 1024 * 1024 * 1024; break;
                }

                var driveSelection = DriveLetterComboBox.SelectedItem.ToString();
                char driveLetter = driveSelection == "Auto" ? '\0' : driveSelection[0];

                var createData = new TempCreateData
                {
                    DeviceNumber = (uint)GetNextAvailableDeviceNumber(),
                    DiskSize = sizeInBytes,
                    SectorSize = 512,
                    DriveLetter = driveLetter,
                    RemovableMedia = RemovableCheckBox.IsChecked ?? false,
                    CdRomType = CdRomCheckBox.IsChecked ?? false,
                    FileName = ""
                };

                await Task.Run(() => CreateRamDisk(createData));
                
                RefreshRamDisks();
                StatusText.Text = "RAM disk created successfully";
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Failed to create RAM disk: {ex.Message}", "Error", MessageBoxButton.OK, MessageBoxImage.Error);
                StatusText.Text = "Failed to create RAM disk";
            }
            finally
            {
                CreateButton.IsEnabled = true;
            }
        }

        private int GetNextAvailableDeviceNumber()
        {
            for (int i = 0; i < 32; i++)
            {
                if (!RamDisks.Any(rd => rd.DeviceNumber == i))
                    return i;
            }
            return 0;
        }

        private void CreateRamDisk(TempCreateData createData)
        {
            var handle = CreateFile(
                @"\\.\TempRamDiskControl",
                GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                IntPtr.Zero,
                OPEN_EXISTING,
                0,
                IntPtr.Zero);

            if (handle == INVALID_HANDLE_VALUE)
                throw new Exception("Cannot open control device. Driver may not be installed.");

            try
            {
                var size = Marshal.SizeOf<TempCreateData>();
                var ptr = Marshal.AllocHGlobal(size);
                
                try
                {
                    Marshal.StructureToPtr(createData, ptr, false);
                    
                    if (!DeviceIoControl(handle, TEMP_IOCTL_CREATE_DEVICE, ptr, (uint)size, IntPtr.Zero, 0, out uint bytesReturned, IntPtr.Zero))
                    {
                        throw new Exception($"Failed to create RAM disk. Error: {Marshal.GetLastWin32Error()}");
                    }
                }
                finally
                {
                    Marshal.FreeHGlobal(ptr);
                }
            }
            finally
            {
                CloseHandle(handle);
            }
        }

        private void RemoveButton_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                var button = sender as Button;
                var deviceNumber = (int)button.Tag;

                var result = MessageBox.Show(
                    $"Are you sure you want to remove RAM disk {deviceNumber}?\nAll data will be lost!",
                    "Confirm Removal",
                    MessageBoxButton.YesNo,
                    MessageBoxImage.Warning);

                if (result == MessageBoxResult.Yes)
                {
                    RemoveRamDisk(deviceNumber);
                    RefreshRamDisks();
                    StatusText.Text = $"RAM disk {deviceNumber} removed";
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Failed to remove RAM disk: {ex.Message}", "Error", MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }

        private void RemoveRamDisk(int deviceNumber)
        {
            var handle = CreateFile(
                @"\\.\TempRamDiskControl",
                GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                IntPtr.Zero,
                OPEN_EXISTING,
                0,
                IntPtr.Zero);

            if (handle == INVALID_HANDLE_VALUE)
                throw new Exception("Cannot open control device");

            try
            {
                var deviceNum = (uint)deviceNumber;
                var ptr = Marshal.AllocHGlobal(sizeof(uint));
                
                try
                {
                    Marshal.WriteInt32(ptr, deviceNumber);
                    
                    if (!DeviceIoControl(handle, TEMP_IOCTL_REMOVE_DEVICE, ptr, sizeof(uint), IntPtr.Zero, 0, out uint bytesReturned, IntPtr.Zero))
                    {
                        throw new Exception($"Failed to remove RAM disk. Error: {Marshal.GetLastWin32Error()}");
                    }
                }
                finally
                {
                    Marshal.FreeHGlobal(ptr);
                }
            }
            finally
            {
                CloseHandle(handle);
            }
        }

        private void StatsButton_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                var button = sender as Button;
                var deviceNumber = (int)button.Tag;
                var statsWindow = new StatsWindow(deviceNumber);
                statsWindow.Owner = this;
                statsWindow.ShowDialog();
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Failed to show statistics: {ex.Message}", "Error", MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }

        private void OpenButton_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                var button = sender as Button;
                var driveLetter = button.Tag.ToString();
                
                if (driveLetter != "N/A")
                {
                    Process.Start("explorer.exe", $"{driveLetter}:\\");
                }
                else
                {
                    MessageBox.Show("Drive letter not available", "Info", MessageBoxButton.OK, MessageBoxImage.Information);
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Failed to open drive: {ex.Message}", "Error", MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }

        private void RefreshButton_Click(object sender, RoutedEventArgs e)
        {
            RefreshRamDisks();
            UpdateSystemInfo();
        }

        private void RemoveAllButton_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                if (RamDisks.Count == 0)
                {
                    MessageBox.Show("No RAM disks to remove", "Info", MessageBoxButton.OK, MessageBoxImage.Information);
                    return;
                }

                var result = MessageBox.Show(
                    $"Are you sure you want to remove all {RamDisks.Count} RAM disks?\nAll data will be lost!",
                    "Confirm Remove All",
                    MessageBoxButton.YesNo,
                    MessageBoxImage.Warning);

                if (result == MessageBoxResult.Yes)
                {
                    var devices = RamDisks.Select(rd => rd.DeviceNumber).ToList();
                    foreach (var deviceNum in devices)
                    {
                        try
                        {
                            RemoveRamDisk(deviceNum);
                        }
                        catch (Exception ex)
                        {
                            StatusText.Text = $"Error removing device {deviceNum}: {ex.Message}";
                        }
                    }
                    
                    RefreshRamDisks();
                    StatusText.Text = "All RAM disks removed";
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Failed to remove all RAM disks: {ex.Message}", "Error", MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }

        private void AboutButton_Click(object sender, RoutedEventArgs e)
        {
            var aboutWindow = new AboutWindow();
            aboutWindow.Owner = this;
            aboutWindow.ShowDialog();
        }

        public event PropertyChangedEventHandler PropertyChanged;
        protected virtual void OnPropertyChanged(string propertyName)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }

        protected override void OnClosed(EventArgs e)
        {
            _refreshTimer?.Stop();
            base.OnClosed(e);
        }
    }
} 