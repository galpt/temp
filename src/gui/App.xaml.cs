using System.Windows;

namespace TempRamDiskGUI
{
    public partial class App : Application
    {
        protected override void OnStartup(StartupEventArgs e)
        {
            // Apply modern WPF theme
            ModernWpf.ThemeManager.Current.ApplicationTheme = ModernWpf.ApplicationTheme.Light;
            
            base.OnStartup(e);
        }
    }
} 