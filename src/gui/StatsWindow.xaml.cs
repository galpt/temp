using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Threading;

namespace TempRamDiskGUI
{
    public partial class StatsWindow : Window, INotifyPropertyChanged
    {
        private int _deviceNumber;
        private DispatcherTimer _refreshTimer;

        // P/Invoke declarations
        [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Auto)]
        private static extern IntPtr CreateFile(
            string lpFileName, uint dwDesiredAccess, uint dwShareMode,
            IntPtr lpSecurityAttributes, uint dwCreationDisposition,
            uint dwFlagsAndAttributes, IntPtr hTemplateFile);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool DeviceIoControl(
            IntPtr hDevice, uint dwIoControlCode, IntPtr lpInBuffer,
            uint nInBufferSize, IntPtr lpOutBuffer, uint nOutBufferSize,
            out uint lpBytesReturned, IntPtr lpOverlapped);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool CloseHandle(IntPtr hObject);

        private const uint GENERIC_READ = 0x80000000;
        private const uint FILE_SHARE_READ = 0x00000001;
        private const uint FILE_SHARE_WRITE = 0x00000002;
        private const uint OPEN_EXISTING = 3;
        private const uint TEMP_IOCTL_GET_STATISTICS = 0x83000804;
        private const IntPtr INVALID_HANDLE_VALUE = (IntPtr)(-1);

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

        // Properties for binding
        public string Title => $"Statistics for RAM Disk {_deviceNumber}";
        public string DeviceNumber => _deviceNumber.ToString();
        public string DiskSizeFormatted { get; private set; }
        public string MemoryUsedFormatted { get; private set; }
        public string TotalReads { get; private set; }
        public string TotalWrites { get; private set; }
        public string BytesReadFormatted { get; private set; }
        public string BytesWrittenFormatted { get; private set; }
        public string CacheHits { get; private set; }
        public string CacheMisses { get; private set; }
        public string HitRatio { get; private set; }
        public string EvictionCount { get; private set; }

        public StatsWindow(int deviceNumber)
        {
            InitializeComponent();
            DataContext = this;
            _deviceNumber = deviceNumber;

            RefreshStats();

            // Auto-refresh every 2 seconds
            _refreshTimer = new DispatcherTimer
            {
                Interval = TimeSpan.FromSeconds(2)
            };
            _refreshTimer.Tick += (s, e) => RefreshStats();
            _refreshTimer.Start();
        }

        private void RefreshStats()
        {
            try
            {
                var devicePath = $@"\\.\TempRamDisk{_deviceNumber}";
                var handle = CreateFile(devicePath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                    IntPtr.Zero, OPEN_EXISTING, 0, IntPtr.Zero);

                if (handle == INVALID_HANDLE_VALUE)
                {
                    MessageBox.Show("Cannot open device for statistics", "Error", MessageBoxButton.OK, MessageBoxImage.Error);
                    return;
                }

                var size = Marshal.SizeOf<TempStatistics>();
                var ptr = Marshal.AllocHGlobal(size);

                try
                {
                    if (DeviceIoControl(handle, TEMP_IOCTL_GET_STATISTICS, IntPtr.Zero, 0, ptr, (uint)size, out uint bytesReturned, IntPtr.Zero))
                    {
                        var stats = Marshal.PtrToStructure<TempStatistics>(ptr);
                        UpdateProperties(stats);
                    }
                }
                finally
                {
                    Marshal.FreeHGlobal(ptr);
                    CloseHandle(handle);
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Error refreshing statistics: {ex.Message}", "Error", MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }

        private void UpdateProperties(TempStatistics stats)
        {
            DiskSizeFormatted = FormatBytes(stats.DiskSize);
            MemoryUsedFormatted = FormatBytes(stats.MemoryUsed);
            TotalReads = stats.TotalReads.ToString("N0");
            TotalWrites = stats.TotalWrites.ToString("N0");
            BytesReadFormatted = FormatBytes(stats.BytesRead);
            BytesWrittenFormatted = FormatBytes(stats.BytesWritten);
            CacheHits = stats.CacheHits.ToString("N0");
            CacheMisses = stats.CacheMisses.ToString("N0");
            EvictionCount = stats.EvictionCount.ToString("N0");

            var totalAccess = stats.CacheHits + stats.CacheMisses;
            if (totalAccess > 0)
            {
                var ratio = (double)stats.CacheHits / totalAccess * 100.0;
                HitRatio = $"{ratio:F2}%";
            }
            else
            {
                HitRatio = "N/A";
            }

            OnPropertyChanged(string.Empty); // Refresh all properties
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

        private void RefreshButton_Click(object sender, RoutedEventArgs e)
        {
            RefreshStats();
        }

        private void CloseButton_Click(object sender, RoutedEventArgs e)
        {
            Close();
        }

        protected override void OnClosed(EventArgs e)
        {
            _refreshTimer?.Stop();
            base.OnClosed(e);
        }

        public event PropertyChangedEventHandler PropertyChanged;
        protected virtual void OnPropertyChanged(string propertyName)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }
    }
} 