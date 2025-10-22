@echo off
chcp 65001
echo ============================================
echo Baja-XLSX 本地编译脚本
echo ============================================
echo.

REM 检查 vcpkg 路径
set VCPKG_FOUND=0

if exist "E:\vcpkg\vcpkg.exe" (
    set VCPKG_ROOT=E:\vcpkg
    set VCPKG_FOUND=1
    echo [✓] 找到 vcpkg: E:\vcpkg
)

if exist "C:\vcpkg\vcpkg.exe" (
    set VCPKG_ROOT=C:\vcpkg
    set VCPKG_FOUND=1
    echo [✓] 找到 vcpkg: C:\vcpkg
)

if %VCPKG_FOUND%==0 (
    echo [✗] 未找到 vcpkg
    echo.
    echo 请先安装 vcpkg 和 xlnt：
    echo   方法 1: 运行自动安装脚本
    echo     install-vcpkg-and-deps.bat
    echo.
    echo   方法 2: 手动安装
    echo     cd E:\
    echo     git clone https://github.com/Microsoft/vcpkg.git
    echo     cd vcpkg
    echo     .\bootstrap-vcpkg.bat
    echo     .\vcpkg install xlnt:x64-windows
    echo     .\vcpkg install libzip:x64-windows
    echo.
    pause
    exit /b 1
)

REM 检查 xlnt 是否已安装
if not exist "%VCPKG_ROOT%\installed\x64-windows\include\xlnt\xlnt.hpp" (
    echo [✗] xlnt 未安装
    echo.
    echo 正在安装 xlnt...
    cd /d %VCPKG_ROOT%
    call vcpkg install xlnt:x64-windows
    
    if %errorlevel% neq 0 (
        echo [✗] xlnt 安装失败
        pause
        exit /b 1
    )
    
    echo [✓] xlnt 安装成功
) else (
    echo [✓] xlnt 已安装
)

REM 检查 libzip 是否已安装
if not exist "%VCPKG_ROOT%\installed\x64-windows\include\zip.h" (
    echo [!] libzip 未安装，正在安装...
    cd /d %VCPKG_ROOT%
    call vcpkg install libzip:x64-windows
    
    if %errorlevel% neq 0 (
        echo [✗] libzip 安装失败
        pause
        exit /b 1
    )
    
    echo [✓] libzip 安装成功
) else (
    echo [✓] libzip 已安装
)

echo.
echo ============================================
echo 开始编译...
echo ============================================
echo.
echo VCPKG_ROOT=%VCPKG_ROOT%
echo.

REM 回到项目目录
cd /d %~dp0

REM 设置环境变量并编译
set VCPKG_ROOT=%VCPKG_ROOT%

echo [1/2] 清理旧的构建...
call npm run clean 2>nul

echo.
echo [2/2] 重新编译原生模块...
call npm run build

if %errorlevel% neq 0 (
    echo.
    echo [✗] 编译失败
    echo.
    echo 请检查:
    echo   1. Visual Studio Build Tools 是否已安装
    echo   2. Node.js 版本是否 ^>= 16
    echo   3. vcpkg 依赖是否正确安装
    echo.
    pause
    exit /b 1
)

echo.
echo ============================================
echo [✓] 编译成功！
echo ============================================
echo.

REM 复制 DLL
echo 正在复制依赖的 DLL...
call npm run copy-dlls

if %errorlevel% neq 0 (
    echo [!] DLL 复制可能失败，但不影响本地使用
)

echo.
echo [2/2] 重新编译预设模块...
call npm run prebuild
if %errorlevel% neq 0 (
    echo.
    echo [✗] 编译失败
    echo.
    echo 请检查:
    echo   1. Visual Studio Build Tools 是否已安装
    echo   2. Node.js 版本是否 ^>= 16
    echo   3. vcpkg 依赖是否正确安装
    echo.
    pause
    exit /b 1
)

echo.
echo ============================================
echo [✓] 预设模块编译成功！
echo ============================================
echo.

pause

