@echo off
setlocal
echo ======================================
echo Checking vcpkg dependencies
echo ======================================
echo.

if not defined VCPKG_ROOT (
    if exist "C:\vcpkg\vcpkg.exe" set VCPKG_ROOT=C:\vcpkg
)
if not defined VCPKG_ROOT (
    echo [x] VCPKG_ROOT is not set and no vcpkg found at C:\vcpkg
    echo     Set it first, e.g.:  set VCPKG_ROOT=C:\vcpkg
    pause
    exit /b 1
)

echo VCPKG_ROOT=%VCPKG_ROOT%
echo.

echo Checking vcpkg installation...
if exist "%VCPKG_ROOT%\vcpkg.exe" (
    echo [ok] vcpkg.exe found
) else (
    echo [x] vcpkg.exe NOT found in %VCPKG_ROOT%
    pause
    exit /b 1
)
echo.

echo Installed packages:
call "%VCPKG_ROOT%\vcpkg.exe" list
echo.

echo ======================================
echo Checking required headers and libraries
echo ======================================

set VCPKG_INSTALLED=%VCPKG_ROOT%\installed\x64-windows

echo xlnt:
if exist "%VCPKG_INSTALLED%\include\xlnt\xlnt.hpp" (
    echo   [ok] header found
) else (
    echo   [x]  header missing: %VCPKG_INSTALLED%\include\xlnt\xlnt.hpp
)
if exist "%VCPKG_INSTALLED%\lib\xlnt.lib" (
    echo   [ok] library found
) else (
    echo   [x]  library missing
)

echo libzip:
if exist "%VCPKG_INSTALLED%\include\zip.h" (
    echo   [ok] header found
) else (
    echo   [x]  header missing
)
if exist "%VCPKG_INSTALLED%\lib\zip.lib" (
    echo   [ok] library found
) else (
    echo   [x]  library missing
)

echo.
echo ======================================
echo If anything is missing, run:
echo   cd /d "%VCPKG_ROOT%"
echo   vcpkg install xlnt:x64-windows
echo   vcpkg install libzip:x64-windows
echo ======================================

pause
