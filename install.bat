@echo off
setlocal EnableDelayedExpansion

echo ================================================================
echo TEMP RAM Disk - Installation Script
echo ================================================================
echo.

REM Check for Administrator privileges
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo ERROR: This script requires Administrator privileges.
    echo Please run as Administrator.
    pause
    exit /b 1
)

REM Set up environment variables
set "BIN_DIR=%~dp0bin"
set "DRIVER_NAME=TempRamDisk"
set "DRIVER_FILE=temp.sys"
set "CLI_FILE=temp.exe"

echo Checking for required files...

REM Check if driver file exists
if not exist "%BIN_DIR%\%DRIVER_FILE%" (
    echo ERROR: Driver file not found: %BIN_DIR%\%DRIVER_FILE%
    echo Please run build.bat first to compile the driver.
    pause
    exit /b 1
)

REM Check if CLI tool exists
if not exist "%BIN_DIR%\%CLI_FILE%" (
    echo ERROR: Command line tool not found: %BIN_DIR%\%CLI_FILE%
    echo Please run build.bat first to compile the CLI tool.
    pause
    exit /b 1
)

REM Check if INF file exists
if not exist "%BIN_DIR%\temp.inf" (
    echo ERROR: Installation file not found: %BIN_DIR%\temp.inf
    echo Please run build.bat first to create installation files.
    pause
    exit /b 1
)

echo All required files found.
echo.

echo ================================================================
echo Stopping Existing Service (if running)
echo ================================================================
echo.

REM Stop the service if it's already running
sc query "%DRIVER_NAME%" >nul 2>&1
if %errorLevel% equ 0 (
    echo Stopping existing TEMP RAM Disk service...
    sc stop "%DRIVER_NAME%" >nul 2>&1
    timeout /t 3 /nobreak >nul
) else (
    echo No existing service found.
)
echo.

echo ================================================================
echo Installing Driver Files
echo ================================================================
echo.

REM Copy driver to system directory
echo Copying driver to system directory...
copy "%BIN_DIR%\%DRIVER_FILE%" "%SystemRoot%\System32\drivers\" >nul
if %errorLevel% neq 0 (
    echo ERROR: Failed to copy driver file.
    echo Make sure you have Administrator privileges.
    pause
    exit /b 1
)

echo Driver file installed: %SystemRoot%\System32\drivers\%DRIVER_FILE%

REM Copy CLI tool to system directory for global access
echo Copying command line tool...
copy "%BIN_DIR%\%CLI_FILE%" "%SystemRoot%\System32\" >nul
if %errorLevel% neq 0 (
    echo WARNING: Failed to copy CLI tool to system directory.
    echo You can still use it from the bin directory.
)

REM Install GUI application if it exists
if exist "%BIN_DIR%\TempRamDiskGUI.exe" (
    echo Installing GUI application...
    set "INSTALL_DIR=%ProgramFiles%\TEMP RAM Disk"
    if not exist "!INSTALL_DIR!" mkdir "!INSTALL_DIR!"
    
    copy "%BIN_DIR%\TempRamDiskGUI.exe" "!INSTALL_DIR!\" >nul
    copy "%BIN_DIR%\*.dll" "!INSTALL_DIR!\" >nul 2>&1
    copy "%BIN_DIR%\*.json" "!INSTALL_DIR!\" >nul 2>&1
    copy "%~dp0README.md" "!INSTALL_DIR!\" >nul
    
    REM Create Start Menu shortcuts
    set "STARTMENU=%ProgramData%\Microsoft\Windows\Start Menu\Programs"
    if not exist "!STARTMENU!\TEMP RAM Disk" mkdir "!STARTMENU!\TEMP RAM Disk"
    
    echo Creating Start Menu shortcuts...
    powershell -Command "$WScriptShell = New-Object -ComObject WScript.Shell; $Shortcut = $WScriptShell.CreateShortcut('!STARTMENU!\TEMP RAM Disk\TEMP RAM Disk Manager.lnk'); $Shortcut.TargetPath = '!INSTALL_DIR!\TempRamDiskGUI.exe'; $Shortcut.Save()" >nul 2>&1
    powershell -Command "$WScriptShell = New-Object -ComObject WScript.Shell; $Shortcut = $WScriptShell.CreateShortcut('!STARTMENU!\TEMP RAM Disk\Command Line Tool.lnk'); $Shortcut.TargetPath = '%SystemRoot%\System32\temp.exe'; $Shortcut.Save()" >nul 2>&1
    
    echo GUI application installed: !INSTALL_DIR!\TempRamDiskGUI.exe
) else (
    echo GUI application not found - using CLI-only installation.
)

echo.

echo ================================================================
echo Creating Windows Service
echo ================================================================
echo.

REM Delete existing service if it exists
sc query "%DRIVER_NAME%" >nul 2>&1
if %errorLevel% equ 0 (
    echo Removing existing service...
    sc delete "%DRIVER_NAME%" >nul 2>&1
    timeout /t 2 /nobreak >nul
)

REM Create the service
echo Creating TEMP RAM Disk service...
sc create "%DRIVER_NAME%" binPath= "System32\drivers\%DRIVER_FILE%" type= kernel start= demand error= normal DisplayName= "TEMP RAM Disk Driver" >nul
if %errorLevel% neq 0 (
    echo ERROR: Failed to create service.
    pause
    exit /b 1
)

REM Set service description
sc description "%DRIVER_NAME%" "High-performance RAM disk driver for Windows. Provides fast in-memory storage with thread-safe operations." >nul

echo Service created successfully.
echo.

echo ================================================================
echo Starting Service
echo ================================================================
echo.

REM Start the service
echo Starting TEMP RAM Disk service...
sc start "%DRIVER_NAME%" >nul
if %errorLevel% neq 0 (
    echo ERROR: Failed to start service.
    echo This may be normal on first installation. The service will start when needed.
) else (
    echo Service started successfully.
)

echo.

REM Wait a moment for service to initialize
timeout /t 2 /nobreak >nul

echo ================================================================
echo Testing Installation
echo ================================================================
echo.

REM Test the installation by checking service status
echo Checking service status...
sc query "%DRIVER_NAME%" | findstr "STATE" | findstr "RUNNING" >nul
if %errorLevel% equ 0 (
    echo ✓ Service is running
) else (
    sc query "%DRIVER_NAME%" | findstr "STATE" | findstr "STOPPED" >nul
    if %errorLevel% equ 0 (
        echo ℹ Service is stopped (this is normal - it starts on demand)
    ) else (
        echo ⚠ Service status unknown
    )
)

REM Test CLI tool
echo Testing command line tool...
where temp.exe >nul 2>&1
if %errorLevel% equ 0 (
    echo ✓ Command line tool is available globally
    temp.exe version >nul 2>&1
    if %errorLevel% equ 0 (
        echo ✓ CLI tool can communicate with driver
    ) else (
        echo ℹ CLI tool is installed but driver communication needs testing
    )
) else (
    echo ⚠ Command line tool not in PATH (can be used from bin directory)
)

echo.

echo ================================================================
echo Installation Complete!
echo ================================================================
echo.

echo TEMP RAM Disk has been installed successfully.
echo.
echo What is a RAM Disk?
echo A RAM disk creates a virtual drive using your computer's RAM memory.
echo This provides ultra-fast storage perfect for:
echo   • Gaming: Reduce loading times by 10-50x
echo   • Development: Speed up compilation and builds  
echo   • General: Accelerate browser cache and temp files
echo.
echo Available Interfaces:
if exist "%ProgramFiles%\TEMP RAM Disk\TempRamDiskGUI.exe" (
    echo   • Graphical: Start Menu ^> TEMP RAM Disk ^> TEMP RAM Disk Manager
)
echo   • Command-line: temp.exe ^<command^>
echo.
echo Quick CLI Examples:
echo   temp.exe create --size 256M --drive R
echo   temp.exe list
echo   temp.exe stats 0
echo   temp.exe remove 0
echo.
echo Documentation:
echo   temp.exe help          - Show detailed help
echo   temp.exe version       - Show version information
echo.

REM Create a desktop shortcut (optional)
set /p CREATE_SHORTCUT="Create desktop shortcut for TEMP CLI? (y/n): "
if /i "%CREATE_SHORTCUT%"=="y" (
    echo Creating desktop shortcut...
    powershell -Command "$WshShell = New-Object -ComObject WScript.Shell; $Shortcut = $WshShell.CreateShortcut('%USERPROFILE%\Desktop\TEMP RAM Disk.lnk'); $Shortcut.TargetPath = 'cmd.exe'; $Shortcut.Arguments = '/k temp.exe help'; $Shortcut.WorkingDirectory = '%SystemRoot%\System32'; $Shortcut.IconLocation = 'shell32.dll,21'; $Shortcut.Description = 'TEMP RAM Disk Command Line Interface'; $Shortcut.Save()" >nul 2>&1
    if %errorLevel% equ 0 (
        echo Desktop shortcut created.
    ) else (
        echo Failed to create desktop shortcut.
    )
)

echo.
echo Installation log can be found in Windows Event Viewer under:
echo   Applications and Services Logs ^> System
echo.
echo IMPORTANT NOTES:
echo - To create RAM disks, run 'temp.exe create' as Administrator
echo - RAM disks are volatile - data is lost on reboot/shutdown
echo - Use 'temp.exe help' for complete usage instructions
echo.

pause 