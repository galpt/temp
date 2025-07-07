@echo off
setlocal EnableDelayedExpansion

echo ================================================================
echo TEMP RAM Disk - Build Script
echo ================================================================
echo.

REM Note: Administrator privileges are only required for driver installation, not compilation
echo Note: Building driver and applications (Administrator privileges only needed for installation)

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

echo Windows Driver Kit (WDK) not found. Attempting auto-installation...
echo.

REM Try to install WDK via winget
winget install Microsoft.WindowsWDK --silent --accept-package-agreements --accept-source-agreements >nul 2>&1

if !errorLevel! equ 0 (
    echo WDK installed successfully. Please run build.bat again.
    pause
    exit /b 1
) else (
    echo.
    echo ERROR: Could not auto-install WDK.
    echo Please manually install Windows Driver Kit from:
    echo https://docs.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk
    echo.
    echo Alternatively, for a lightweight build, we can try using Visual Studio SDK only...
    set /p USE_VS_ONLY="Continue with Visual Studio SDK only? (y/n): "
    if /i "!USE_VS_ONLY!"=="y" (
        goto try_vs_sdk
    ) else (
        pause
        exit /b 1
    )
)

:wdk_found
echo.
goto continue_build

:try_vs_sdk
echo.
echo Attempting to build with Visual Studio SDK only (simplified driver)...

REM Try to find Visual Studio SDK paths
set "WDK_PATH="
for %%P in (
    "C:\Program Files (x86)\Windows Kits\10"
    "C:\Program Files\Windows Kits\10"
    "C:\Program Files (x86)\Microsoft SDKs\Windows\v10.0"
    "C:\Program Files\Microsoft SDKs\Windows\v10.0"
) do (
    if exist "%%~P" (
        set "WDK_PATH=%%~P"
        echo Found Windows SDK at: %%~P
        goto continue_build
    )
)

echo ERROR: No suitable SDK found.
echo Please install Visual Studio with Windows SDK component.
pause
exit /b 1

:continue_build
echo.

REM Find the latest Windows 10 SDK version
echo Searching for Windows 10 SDK...
set "SDK_VERSION="

REM Try common Windows 10 SDK versions
for %%V in (
    "10.0.26100.0"
    "10.0.22621.0"
    "10.0.22000.0"
    "10.0.20348.0"
    "10.0.19041.0"
    "10.0.18362.0"
    "10.0.17763.0"
    "10.0.17134.0"
    "10.0.16299.0"
    "10.0.15063.0"
    "10.0.14393.0"
    "10.0.10586.0"
    "10.0.10240.0"
) do (
    if exist "%WDK_PATH%\Include\%%~V" (
        if "!SDK_VERSION!"=="" (
            set "SDK_VERSION=%%~V"
            echo Found Windows 10 SDK: %%~V
        )
    )
)

REM If no standard version found, try to find any 10.0.* version
if "%SDK_VERSION%"=="" (
    for /f "delims=" %%i in ('dir "%WDK_PATH%\Include\10.0.*" /b /ad 2^>nul') do (
        if "!SDK_VERSION!"=="" (
            set "SDK_VERSION=%%i"
            echo Found Windows 10 SDK: %%i
        )
    )
)

if "%SDK_VERSION%"=="" (
    echo ERROR: No Windows 10 SDK found in WDK installation.
    echo.
    echo Attempting to auto-download Windows 10 SDK...
    
    REM Try to install via winget if available
    winget install Microsoft.WindowsSDK.10 --silent --accept-package-agreements --accept-source-agreements >nul 2>&1
    
    if !errorLevel! equ 0 (
        echo Windows 10 SDK installed successfully. Please run build.bat again.
        pause
        exit /b 1
    ) else (
        echo.
        echo Please manually install Windows 10 SDK from:
        echo https://developer.microsoft.com/en-us/windows/downloads/windows-sdk/
        echo.
        echo Or install Visual Studio with "Windows 10 SDK" component.
        pause
        exit /b 1
    )
)

echo Using SDK Version: %SDK_VERSION%
echo.

REM Set up build environment for kernel mode (driver compilation)
set "KERNEL_INCLUDE=%WDK_PATH%\Include\%SDK_VERSION%\km;%WDK_PATH%\Include\%SDK_VERSION%\shared;%WDK_PATH%\Include\%SDK_VERSION%\km\crt"
set "USER_INCLUDE=%WDK_PATH%\Include\%SDK_VERSION%\um;%WDK_PATH%\Include\%SDK_VERSION%\shared"

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
    echo Visual Studio compiler not found. Attempting auto-installation...
    
    REM Try to install Visual Studio Build Tools
    winget install Microsoft.VisualStudio.2022.BuildTools --silent --accept-package-agreements --accept-source-agreements >nul 2>&1
    
    if !errorLevel! equ 0 (
        echo Visual Studio Build Tools installed. Please run build.bat again.
        pause
        exit /b 1
    ) else (
        echo.
        echo ERROR: Could not auto-install Visual Studio.
        echo Please manually install Visual Studio 2017 or later with C++ support from:
        echo https://visualstudio.microsoft.com/downloads/
        pause
        exit /b 1
    )
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

REM Compile driver source files (KERNEL MODE ONLY - no user-mode headers)
echo Compiling memory management module...
"%CL_PATH%\cl.exe" /c /nologo /W3 /O2 /Gz /D "_WIN64" /D "_AMD64_" /D "AMD64" /D "_KERNEL_MODE" /D "POOL_NX_OPTIN=1" /I "%SRC_DIR%\core" /I "%WDK_PATH%\Include\%SDK_VERSION%\km" /I "%WDK_PATH%\Include\%SDK_VERSION%\shared" /I "%WDK_PATH%\Include\%SDK_VERSION%\km\crt" /Fo"%BUILD_DIR%\temp_memory.obj" "%SRC_DIR%\core\temp_memory.c"
if %errorLevel% neq 0 (
    echo ERROR: Failed to compile memory management module.
    pause
    exit /b 1
)

echo Compiling driver module...
"%CL_PATH%\cl.exe" /c /nologo /W3 /O2 /Gz /D "_WIN64" /D "_AMD64_" /D "AMD64" /D "_KERNEL_MODE" /D "POOL_NX_OPTIN=1" /I "%SRC_DIR%\core" /I "%WDK_PATH%\Include\%SDK_VERSION%\km" /I "%WDK_PATH%\Include\%SDK_VERSION%\shared" /I "%WDK_PATH%\Include\%SDK_VERSION%\km\crt" /Fo"%BUILD_DIR%\temp_driver.obj" "%SRC_DIR%\driver\temp_driver.c"
if %errorLevel% neq 0 (
    echo ERROR: Failed to compile driver module.
    pause
    exit /b 1
)

REM Link driver
echo Linking driver...
"%CL_PATH%\link.exe" /nologo /DRIVER /NODEFAULTLIB /SUBSYSTEM:NATIVE /MACHINE:%ARCH% /ENTRY:DriverEntry /OUT:"%BIN_DIR%\temp.sys" /LIBPATH:"%LIB_PATH%" "%BUILD_DIR%\temp_memory.obj" "%BUILD_DIR%\temp_driver.obj" ntoskrnl.lib hal.lib BufferOverflowK.lib
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

REM Compile CLI tool (USER MODE ONLY - no kernel headers)
echo Compiling command line interface...
"%CL_PATH%\cl.exe" /nologo /W3 /O2 /D "WIN32" /D "_WIN64" /D "_CONSOLE" /I "%SRC_DIR%\core" /I "%WDK_PATH%\Include\%SDK_VERSION%\um" /I "%WDK_PATH%\Include\%SDK_VERSION%\shared" /I "%WDK_PATH%\Include\%SDK_VERSION%\ucrt" /I "%VS_PATH%\include" /Fe"%BIN_DIR%\temp.exe" "%SRC_DIR%\cli\temp_cli.c" /link /LIBPATH:"%WDK_PATH%\Lib\%SDK_VERSION%\um\%ARCH%" /LIBPATH:"%WDK_PATH%\Lib\%SDK_VERSION%\ucrt\%ARCH%" kernel32.lib user32.lib
if %errorLevel% neq 0 (
    echo WARNING: Driver-based CLI failed. Trying simplified version...
    
    REM Try to compile a simplified version without driver dependencies
    "%CL_PATH%\cl.exe" /nologo /W3 /O2 /D "WIN32" /D "_WIN64" /D "_CONSOLE" /D "SIMPLIFIED_BUILD" /I "%WDK_PATH%\Include\%SDK_VERSION%\um" /I "%WDK_PATH%\Include\%SDK_VERSION%\shared" /I "%WDK_PATH%\Include\%SDK_VERSION%\ucrt" /I "%VS_PATH%\include" /Fe"%BIN_DIR%\temp.exe" "%SRC_DIR%\cli\temp_cli.c" /link /LIBPATH:"%WDK_PATH%\Lib\%SDK_VERSION%\um\%ARCH%" /LIBPATH:"%WDK_PATH%\Lib\%SDK_VERSION%\ucrt\%ARCH%" kernel32.lib user32.lib
    if !errorLevel! neq 0 (
        echo ERROR: Failed to compile command line tool.
        pause
        exit /b 1
    ) else (
        echo Simplified CLI tool built successfully.
    )
) else (
    echo Command line tool built successfully: %BIN_DIR%\temp.exe
)

echo.

echo ================================================================
echo Building GUI Application
echo ================================================================
echo.

REM Check for .NET SDK
echo Checking for .NET SDK...
dotnet --version >nul 2>&1
if %errorLevel% neq 0 (
    echo .NET SDK not found. Attempting auto-installation...
    
    REM Try to install .NET via winget
    winget install Microsoft.DotNet.SDK.6 --silent --accept-package-agreements --accept-source-agreements >nul 2>&1
    
    if !errorLevel! equ 0 (
        echo .NET SDK installed successfully. Checking again...
        dotnet --version >nul 2>&1
        if !errorLevel! equ 0 (
            echo .NET SDK is now available. GUI will be built.
        ) else (
            echo .NET SDK installation completed but not immediately available.
            echo WARNING: Skipping GUI build. Please restart command prompt and run build.bat again.
            goto skip_gui
        )
    ) else (
        echo WARNING: Could not auto-install .NET SDK. Skipping GUI build.
        echo To build GUI manually, install .NET 6.0 or later from: https://dotnet.microsoft.com/download
        goto skip_gui
    )
) else (
    for /f "tokens=*" %%i in ('dotnet --version 2^>nul') do set "DOTNET_VERSION=%%i"
    echo .NET SDK found: %DOTNET_VERSION%. GUI will be built.
)

echo Building TEMP RAM Disk Manager GUI...
cd "%SRC_DIR%\gui"

dotnet restore TempRamDiskGUI.csproj --no-cache
dotnet build TempRamDiskGUI.csproj -c Release -o "%BIN_DIR%\gui" --no-restore
if %errorLevel% neq 0 (
    echo WARNING: GUI build failed. Continuing with CLI-only build.
    echo Note: GUI requires .NET 6.0 or later SDK
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