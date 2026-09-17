@echo off
setlocal
echo ============================================
echo Creating prebuild archives (DLLs bundled)
echo ============================================
echo.

if not defined VCPKG_ROOT (
    if exist "C:\vcpkg\vcpkg.exe" set VCPKG_ROOT=C:\vcpkg
)
if not defined VCPKG_ROOT (
    echo [x] VCPKG_ROOT is not set and no vcpkg found at C:\vcpkg
    pause
    exit /b 1
)

echo VCPKG_ROOT=%VCPKG_ROOT%
echo.

cd /d "%~dp0.."

echo [1/5] Removing previous build and prebuild archives...
if exist build (
    rmdir /s /q build
    echo   - removed build\
)
if exist prebuilds (
    rmdir /s /q prebuilds
    echo   - removed prebuilds\
)
echo.

echo [2/5] Building the native addon...
call npm run build
if %errorlevel% neq 0 (
    echo [x] Build failed
    pause
    exit /b 1
)
echo   - build succeeded
echo.

echo [3/5] Copying runtime DLLs into build\Release...
call npm run copy-dlls
echo.

echo [4/5] Creating prebuild archives...
call npm run prebuild:napi
if %errorlevel% neq 0 (
    echo [x] N-API prebuild failed
    pause
    exit /b 1
)
call npm run prebuild:electron
if %errorlevel% neq 0 (
    echo [x] Electron prebuild failed
    pause
    exit /b 1
)
echo.

echo [5/5] Packing DLLs into the archives...
call npm run prebuild:pack-dlls
if %errorlevel% neq 0 (
    echo [x] DLL packing failed
    pause
    exit /b 1
)
echo.

echo ============================================
echo [ok] Prebuild archives created
echo ============================================
echo.
echo Location: prebuilds\
dir /b prebuilds
echo.
echo Next steps:
echo   1. Verify the archives: npm run verify:prebuild
echo   2. Publish a release:   git tag v1.0.16 ^&^& git push origin v1.0.16
echo.
pause
