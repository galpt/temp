@echo off
setlocal EnableDelayedExpansion

echo ================================================================
echo TEMP RAM Disk - Uninstallation Script
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
set "DRIVER_NAME=TempRamDisk"
set "DRIVER_FILE=temp.sys"
set "CLI_FILE=temp.exe"

echo WARNING: This will completely remove TEMP RAM Disk from your system.
echo All active RAM disks will be destroyed and their data will be lost.
echo.
set /p CONFIRM="Are you sure you want to continue? (yes/no): "

if /i not "%CONFIRM%"=="yes" (
    echo Uninstallation cancelled.
    pause
    exit /b 0
)

echo.

echo ================================================================
echo Removing Active RAM Disks
echo ================================================================
echo.

REM Try to remove all active RAM disks
echo Checking for active RAM disks...
where temp.exe >nul 2>&1
if %errorLevel% equ 0 (
    temp.exe list >nul 2>&1
    if %errorLevel% equ 0 (
        echo Attempting to remove all active RAM disks...
        for /L %%i in (0,1,31) do (
            temp.exe remove %%i >nul 2>&1
        )
        echo Active RAM disks removed.
    ) else (
        echo No active RAM disks found or driver not responding.
    )
) else (
    echo CLI tool not found - skipping RAM disk cleanup.
)

echo.

echo ================================================================
echo Stopping and Removing Service
echo ================================================================
echo.

REM Check if service exists
sc query "%DRIVER_NAME%" >nul 2>&1
if %errorLevel% neq 0 (
    echo TEMP RAM Disk service not found.
) else (
    echo Stopping TEMP RAM Disk service...
    sc stop "%DRIVER_NAME%" >nul 2>&1
    
    REM Wait for service to stop
    echo Waiting for service to stop...
    timeout /t 5 /nobreak >nul
    
    REM Force stop if still running
    sc query "%DRIVER_NAME%" | findstr "RUNNING" >nul
    if %errorLevel% equ 0 (
        echo Force stopping service...
        taskkill /f /im temp.sys >nul 2>&1
        timeout /t 2 /nobreak >nul
    )
    
    echo Removing service...
    sc delete "%DRIVER_NAME%" >nul 2>&1
    if %errorLevel% equ 0 (
        echo Service removed successfully.
    ) else (
        echo WARNING: Failed to remove service. It may require manual removal.
    )
)

echo.

echo ================================================================
echo Removing Driver Files
echo ================================================================
echo.

REM Remove driver file
if exist "%SystemRoot%\System32\drivers\%DRIVER_FILE%" (
    echo Removing driver file...
    del "%SystemRoot%\System32\drivers\%DRIVER_FILE%" >nul 2>&1
    if %errorLevel% equ 0 (
        echo Driver file removed: %SystemRoot%\System32\drivers\%DRIVER_FILE%
    ) else (
        echo WARNING: Could not remove driver file. It may be in use.
        echo You may need to restart and run this script again.
    )
) else (
    echo Driver file not found in system directory.
)

REM Remove CLI tool from system directory
if exist "%SystemRoot%\System32\%CLI_FILE%" (
    echo Removing command line tool...
    del "%SystemRoot%\System32\%CLI_FILE%" >nul 2>&1
    if %errorLevel% equ 0 (
        echo CLI tool removed: %SystemRoot%\System32\%CLI_FILE%
    ) else (
        echo WARNING: Could not remove CLI tool from system directory.
    )
) else (
    echo CLI tool not found in system directory.
)

echo.

echo ================================================================
echo Cleaning Registry Entries
echo ================================================================
echo.

REM Remove registry entries
echo Removing registry entries...

REM Remove service registry entries
reg delete "HKLM\SYSTEM\CurrentControlSet\Services\%DRIVER_NAME%" /f >nul 2>&1
if %errorLevel% equ 0 (
    echo Service registry entries removed.
) else (
    echo Service registry entries not found or already removed.
)

REM Remove any additional registry entries that might have been created
reg delete "HKLM\SYSTEM\CurrentControlSet\Enum\Root\LEGACY_TEMPRAMDISK" /f >nul 2>&1
reg delete "HKLM\SYSTEM\ControlSet001\Services\TempRamDisk" /f >nul 2>&1
reg delete "HKLM\SYSTEM\ControlSet002\Services\TempRamDisk" /f >nul 2>&1

echo Registry cleanup completed.
echo.

echo ================================================================
echo Removing Desktop Shortcuts and Shortcuts
echo ================================================================
echo.

REM Remove desktop shortcut if it exists
if exist "%USERPROFILE%\Desktop\TEMP RAM Disk.lnk" (
    echo Removing desktop shortcut...
    del "%USERPROFILE%\Desktop\TEMP RAM Disk.lnk" >nul 2>&1
    if %errorLevel% equ 0 (
        echo Desktop shortcut removed.
    )
) else (
    echo No desktop shortcut found.
)

REM Remove any start menu shortcuts
if exist "%ProgramData%\Microsoft\Windows\Start Menu\Programs\TEMP RAM Disk.lnk" (
    del "%ProgramData%\Microsoft\Windows\Start Menu\Programs\TEMP RAM Disk.lnk" >nul 2>&1
)

REM Remove GUI application and folders
if exist "%ProgramFiles%\TEMP RAM Disk" (
    echo Removing GUI application...
    rd /s /q "%ProgramFiles%\TEMP RAM Disk" >nul 2>&1
    if exist "%ProgramFiles%\TEMP RAM Disk" (
        echo WARNING: Could not completely remove GUI application directory.
        echo Please manually delete: %ProgramFiles%\TEMP RAM Disk
    ) else (
        echo GUI application removed.
    )
)

REM Remove Start Menu folder
if exist "%ProgramData%\Microsoft\Windows\Start Menu\Programs\TEMP RAM Disk" (
    echo Removing Start Menu folder...
    rd /s /q "%ProgramData%\Microsoft\Windows\Start Menu\Programs\TEMP RAM Disk" >nul 2>&1
    if exist "%ProgramData%\Microsoft\Windows\Start Menu\Programs\TEMP RAM Disk" (
        echo WARNING: Could not remove Start Menu folder.
    ) else (
        echo Start Menu folder removed.
    )
)

echo.

echo ================================================================
echo Verifying Removal
echo ================================================================
echo.

REM Verify service is gone
sc query "%DRIVER_NAME%" >nul 2>&1
if %errorLevel% neq 0 (
    echo ✓ Service successfully removed
) else (
    echo ⚠ Service may still exist
)

REM Verify driver file is gone
if not exist "%SystemRoot%\System32\drivers\%DRIVER_FILE%" (
    echo ✓ Driver file successfully removed
) else (
    echo ⚠ Driver file still exists: %SystemRoot%\System32\drivers\%DRIVER_FILE%
)

REM Verify CLI tool is gone from system directory
if not exist "%SystemRoot%\System32\%CLI_FILE%" (
    echo ✓ CLI tool successfully removed from system directory
) else (
    echo ⚠ CLI tool still exists: %SystemRoot%\System32\%CLI_FILE%
)

echo.

echo ================================================================
echo Uninstallation Complete!
echo ================================================================
echo.

echo TEMP RAM Disk has been removed from your system.
echo.

REM Check if restart is recommended
if exist "%SystemRoot%\System32\drivers\%DRIVER_FILE%" (
    echo IMPORTANT: Some files could not be removed because they are in use.
    echo A system restart is recommended to complete the removal process.
    echo.
    set /p RESTART="Restart computer now? (y/n): "
    if /i "!RESTART!"=="y" (
        echo Restarting computer in 10 seconds...
        echo Press Ctrl+C to cancel.
        timeout /t 10
        shutdown /r /t 0
    )
) else (
    echo All components have been successfully removed.
    echo No restart is required.
)

echo.
echo If you experience any issues, you can:
echo 1. Restart your computer to ensure all components are unloaded
echo 2. Manually check Device Manager for any remaining devices
echo 3. Use 'sc query TempRamDisk' to verify service removal
echo.

echo Thank you for using TEMP RAM Disk!
echo.
pause 