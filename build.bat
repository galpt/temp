@echo off
setlocal EnableDelayedExpansion

echo ================================================================
echo TEMP RAM Disk - Build Script
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
set "BUILD_DIR=%~dp0build"
set "SRC_DIR=%~dp0src"
set "BIN_DIR=%~dp0bin"

REM Create directories
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
if not exist "%BIN_DIR%" mkdir "%BIN_DIR%"

echo Creating build directories...
echo   Build Directory: %BUILD_DIR%
echo   Binary Directory: %BIN_DIR%
echo.

REM Check for Windows Driver Kit
echo Checking for Windows Driver Kit (WDK)...
set "WDK_PATH="

REM Try common WDK installation paths
for %%P in (
    "C:\Program Files (x86)\Windows Kits\10"
    "C:\Program Files\Windows Kits\10"
    "C:\WinDDK"
) do (
    if exist "%%~P" (
        set "WDK_PATH=%%~P"
        echo Found WDK at: %%~P
        goto wdk_found
    )
)

echo ERROR: Windows Driver Kit (WDK) not found.
echo Please install the Windows Driver Kit and ensure it's in the default location.
echo You can download it from: https://docs.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk
pause
exit /b 1

:wdk_found
echo.

REM Find the latest Windows 10 SDK version
echo Searching for Windows 10 SDK...
set "SDK_VERSION="
for /f "delims=" %%i in ('dir "%WDK_PATH%\Include" /b /ad /o-n 2^>nul') do (
    if "!SDK_VERSION!"=="" (
        set "SDK_VERSION=%%i"
    )
)

if "%SDK_VERSION%"=="" (
    echo ERROR: No Windows 10 SDK found in WDK installation.
    pause
    exit /b 1
)

echo Using SDK Version: %SDK_VERSION%
echo.

REM Set up build environment
set "INCLUDE=%WDK_PATH%\Include\%SDK_VERSION%\km;%WDK_PATH%\Include\%SDK_VERSION%\shared;%WDK_PATH%\Include\%SDK_VERSION%\km\crt"

REM Determine architecture
if "%PROCESSOR_ARCHITECTURE%"=="AMD64" (
    set "ARCH=x64"
    set "LIB_PATH=%WDK_PATH%\Lib\%SDK_VERSION%\km\x64"
) else (
    set "ARCH=x86"
    set "LIB_PATH=%WDK_PATH%\Lib\%SDK_VERSION%\km\x86"
)

echo Building for architecture: %ARCH%
echo Library Path: %LIB_PATH%
echo.

REM Find Visual Studio compiler
echo Searching for Visual Studio compiler...
set "VS_PATH="
set "CL_PATH="

REM Try to find Visual Studio installations
for %%Y in (2022 2019 2017) do (
    for %%E in (Enterprise Professional Community BuildTools) do (
        if exist "C:\Program Files\Microsoft Visual Studio\%%Y\%%E\VC\Tools\MSVC" (
            for /f "delims=" %%V in ('dir "C:\Program Files\Microsoft Visual Studio\%%Y\%%E\VC\Tools\MSVC" /b /ad /o-n 2^>nul') do (
                if "!VS_PATH!"=="" (
                    set "VS_PATH=C:\Program Files\Microsoft Visual Studio\%%Y\%%E\VC\Tools\MSVC\%%V"
                    set "CL_PATH=!VS_PATH!\bin\Hostx64\%ARCH%"
                )
            )
        )
    )
)

if "%CL_PATH%"=="" (
    echo ERROR: Visual Studio compiler not found.
    echo Please install Visual Studio 2017 or later with C++ support.
    pause
    exit /b 1
)

echo Found Visual Studio at: %VS_PATH%
echo Compiler path: %CL_PATH%
echo.

REM Set up compiler environment
set "PATH=%CL_PATH%;%PATH%"
set "LIB=%LIB_PATH%;%VS_PATH%\lib\%ARCH%"

echo ================================================================
echo Building Driver
echo ================================================================
echo.

REM Compile driver source files
echo Compiling memory management module...
"%CL_PATH%\cl.exe" /c /nologo /W3 /WX /O2 /D "_WIN64" /D "_AMD64_" /D "AMD64" /D "_KERNEL_MODE" /I "%SRC_DIR%\core" /I "%INCLUDE%" /Fo"%BUILD_DIR%\temp_memory.obj" "%SRC_DIR%\core\temp_memory.c"
if %errorLevel% neq 0 (
    echo ERROR: Failed to compile memory management module.
    pause
    exit /b 1
)

echo Compiling driver module...
"%CL_PATH%\cl.exe" /c /nologo /W3 /WX /O2 /D "_WIN64" /D "_AMD64_" /D "AMD64" /D "_KERNEL_MODE" /I "%SRC_DIR%\core" /I "%INCLUDE%" /Fo"%BUILD_DIR%\temp_driver.obj" "%SRC_DIR%\driver\temp_driver.c"
if %errorLevel% neq 0 (
    echo ERROR: Failed to compile driver module.
    pause
    exit /b 1
)

REM Link driver
echo Linking driver...
"%CL_PATH%\link.exe" /nologo /DRIVER /NODEFAULTLIB /SUBSYSTEM:NATIVE /MACHINE:%ARCH% /ENTRY:DriverEntry /OUT:"%BIN_DIR%\temp.sys" /LIBPATH:"%LIB_PATH%" "%BUILD_DIR%\temp_memory.obj" "%BUILD_DIR%\temp_driver.obj" ntoskrnl.lib hal.lib
if %errorLevel% neq 0 (
    echo ERROR: Failed to link driver.
    pause
    exit /b 1
)

echo Driver built successfully: %BIN_DIR%\temp.sys
echo.

echo ================================================================
echo Building Command Line Tool
echo ================================================================
echo.

REM Compile CLI tool
echo Compiling command line interface...
"%CL_PATH%\cl.exe" /nologo /W3 /O2 /D "WIN32" /D "_CONSOLE" /I "%SRC_DIR%\core" /Fe"%BIN_DIR%\temp.exe" "%SRC_DIR%\cli\temp_cli.c" kernel32.lib user32.lib
if %errorLevel% neq 0 (
    echo ERROR: Failed to compile command line tool.
    pause
    exit /b 1
)

echo Command line tool built successfully: %BIN_DIR%\temp.exe
echo.

echo ================================================================
echo Building GUI Application
echo ================================================================
echo.

REM Check for .NET SDK
dotnet --version >nul 2>&1
if %errorLevel% neq 0 (
    echo WARNING: .NET SDK not found. Skipping GUI build.
    echo To build GUI, install .NET 6.0 or later from: https://dotnet.microsoft.com/download
    goto skip_gui
)

echo Building TEMP RAM Disk Manager GUI...
cd "%SRC_DIR%\gui"

dotnet build TempRamDiskGUI.csproj -c Release -o "%BIN_DIR%\gui"
if %errorLevel% neq 0 (
    echo WARNING: GUI build failed. Continuing with CLI-only build.
    cd "%~dp0"
    goto skip_gui
)

echo Copying GUI files to bin directory...
copy "%BIN_DIR%\gui\TempRamDiskGUI.exe" "%BIN_DIR%\" >nul
copy "%BIN_DIR%\gui\*.dll" "%BIN_DIR%\" >nul 2>&1
copy "%BIN_DIR%\gui\*.json" "%BIN_DIR%\" >nul 2>&1

cd "%~dp0"
echo GUI application built successfully: %BIN_DIR%\TempRamDiskGUI.exe
echo.

:skip_gui

echo ================================================================
echo Creating Installation Files
echo ================================================================
echo.

REM Create INF file for driver installation
echo Creating driver installation file...
(
echo [Version]
echo Signature="$WINDOWS NT$"
echo Class=System
echo ClassGuid={4D36E97D-E325-11CE-BFC1-08002BE10318}
echo Provider=TempRamDisk
echo DriverVer=01/01/2024,1.0.0.0
echo CatalogFile=temp.cat
echo.
echo [DestinationDirs]
echo DefaultDestDir = 12
echo.
echo [DefaultInstall]
echo CopyFiles=DriverFiles
echo AddReg=ServiceReg
echo.
echo [DefaultUninstall]
echo DelFiles=DriverFiles
echo DelReg=ServiceReg
echo.
echo [DriverFiles]
echo temp.sys
echo.
echo [ServiceReg]
echo HKLM,System\CurrentControlSet\Services\TempRamDisk,Type,0x10001,1
echo HKLM,System\CurrentControlSet\Services\TempRamDisk,Start,0x10001,3
echo HKLM,System\CurrentControlSet\Services\TempRamDisk,ErrorControl,0x10001,1
echo HKLM,System\CurrentControlSet\Services\TempRamDisk,ImagePath,0x20000,System32\drivers\temp.sys
echo HKLM,System\CurrentControlSet\Services\TempRamDisk,DisplayName,,"TEMP RAM Disk Driver"
echo HKLM,System\CurrentControlSet\Services\TempRamDisk,Description,,"High-performance RAM disk driver for Windows"
) > "%BIN_DIR%\temp.inf"

echo Driver installation file created: %BIN_DIR%\temp.inf
echo.

REM Clean up build directory
echo Cleaning up temporary files...
del /q "%BUILD_DIR%\*.obj" 2>nul

echo ================================================================
echo Build Complete!
echo ================================================================
echo.
echo Built files:
echo   Driver: %BIN_DIR%\temp.sys
echo   CLI Tool: %BIN_DIR%\temp.exe
echo   Installation: %BIN_DIR%\temp.inf
echo.
echo Next steps:
echo   1. Run install.bat to install the driver
echo   2. Use temp.exe to manage RAM disks
echo.
pause 