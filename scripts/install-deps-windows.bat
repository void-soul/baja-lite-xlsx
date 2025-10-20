@echo off
echo ============================================
echo Baja-XLSX Dependencies Installation (Windows)
echo ============================================
echo.

REM Check if vcpkg exists
if not exist "C:\vcpkg\vcpkg.exe" (
    echo [ERROR] vcpkg not found at C:\vcpkg
    echo Please install vcpkg first:
    echo.
    echo   cd C:\
    echo   git clone https://github.com/Microsoft/vcpkg.git
    echo   cd vcpkg
    echo   .\bootstrap-vcpkg.bat
    echo.
    pause
    exit /b 1
)

echo [1/3] Installing xlnt...
cd C:\vcpkg
vcpkg install xlnt:x64-windows

if %errorlevel% neq 0 (
    echo [ERROR] Failed to install xlnt
    pause
    exit /b 1
)

echo.
echo [2/3] Installing libzip...
vcpkg install libzip:x64-windows

if %errorlevel% neq 0 (
    echo [ERROR] Failed to install libzip
    pause
    exit /b 1
)

echo.
echo [3/3] Integrating vcpkg with Visual Studio...
vcpkg integrate install

echo.
echo ============================================
echo Dependencies installed successfully!
echo ============================================
echo.
echo You can now build the module with:
echo   npm install
echo.
pause


