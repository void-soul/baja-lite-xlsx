@echo off
setlocal
echo ============================================
echo Baja-Lite-XLSX local build script
echo ============================================
echo.

REM Locate vcpkg: VCPKG_ROOT wins, then the usual install locations.
if not defined VCPKG_ROOT (
    if exist "C:\vcpkg\vcpkg.exe" set VCPKG_ROOT=C:\vcpkg
)
if not defined VCPKG_ROOT (
    echo [x] vcpkg not found.
    echo.
    echo Install it first:
    echo   git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
    echo   cd /d C:\vcpkg
    echo   .\bootstrap-vcpkg.bat
    echo   .\vcpkg install xlnt:x64-windows
    echo   .\vcpkg install libzip:x64-windows
    echo.
    echo Then set:  set VCPKG_ROOT=C:\vcpkg
    echo.
    pause
    exit /b 1
)

echo Using VCPKG_ROOT=%VCPKG_ROOT%
echo.

if not exist "%VCPKG_ROOT%\installed\x64-windows\include\xlnt\xlnt.hpp" (
    echo [x] xlnt is not installed.
    echo     Run:  "%VCPKG_ROOT%\vcpkg.exe" install xlnt:x64-windows
    pause
    exit /b 1
)

if not exist "%VCPKG_ROOT%\installed\x64-windows\include\zip.h" (
    echo [x] libzip is not installed.
    echo     Run:  "%VCPKG_ROOT%\vcpkg.exe" install libzip:x64-windows
    pause
    exit /b 1
)

cd /d "%~dp0.."

echo [1/3] Cleaning previous build...
call npm run clean 2>nul

echo.
echo [2/3] Building the native addon...
call npm run build
if %errorlevel% neq 0 (
    echo.
    echo [x] Build failed. Check that:
    echo   1. Visual Studio Build Tools are installed
    echo   2. Node.js ^>= 16
    echo   3. vcpkg dependencies (xlnt, libzip) are installed
    echo.
    pause
    exit /b 1
)

echo.
echo [3/3] Copying runtime DLLs...
call npm run copy-dlls

echo.
echo ============================================
echo [ok] Local build finished
echo ============================================
echo.
echo Next steps:
echo   1. Run an example:  npm run example
echo   2. Run the tests:   npm test
echo.
pause
