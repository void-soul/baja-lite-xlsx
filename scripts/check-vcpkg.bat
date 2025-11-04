@echo off
echo ======================================
echo 检查 vcpkg 依赖
echo ======================================
echo.

set VCPKG_ROOT=E:\vcpkg
echo VCPKG_ROOT=%VCPKG_ROOT%
echo.

echo 检查 vcpkg 是否存在...
if exist "%VCPKG_ROOT%\vcpkg.exe" (
    echo ✓ vcpkg.exe 存在
) else (
    echo ✗ vcpkg.exe 不存在于 %VCPKG_ROOT%
    echo 请确认 vcpkg 路径是否正确
    pause
    exit /b 1
)
echo.

echo 检查已安装的包...
call "%VCPKG_ROOT%\vcpkg.exe" list
echo.

echo ======================================
echo 检查必需的头文件和库...
echo ======================================

set VCPKG_INSTALLED=%VCPKG_ROOT%\installed\x64-windows

echo.
echo 检查 xlnt...
if exist "%VCPKG_INSTALLED%\include\xlnt\xlnt.hpp" (
    echo ✓ xlnt 头文件存在
) else (
    echo ✗ xlnt 头文件不存在
    echo 路径: %VCPKG_INSTALLED%\include\xlnt\xlnt.hpp
)

if exist "%VCPKG_INSTALLED%\lib\xlnt.lib" (
    echo ✓ xlnt 库文件存在
) else (
    echo ✗ xlnt 库文件不存在
)

echo.
echo 检查 libzip...
if exist "%VCPKG_INSTALLED%\include\zip.h" (
    echo ✓ libzip 头文件存在
) else (
    echo ✗ libzip 头文件不存在
)

if exist "%VCPKG_INSTALLED%\lib\zip.lib" (
    echo ✓ libzip 库文件存在
) else (
    echo ✗ libzip 库文件不存在
)

echo.
echo ======================================
echo 如果缺少依赖，请运行：
echo   cd %VCPKG_ROOT%
echo   vcpkg install xlnt:x64-windows
echo   vcpkg install libzip:x64-windows
echo   vcpkg install zlib:x64-windows
echo   vcpkg install bzip2:x64-windows
echo   vcpkg install fmt:x64-windows
echo ======================================

pause

