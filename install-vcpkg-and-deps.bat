chcp 65001
@echo off
echo ========================================
echo Baja-XLSX 依赖安装脚本
echo ========================================
echo.

REM 检查是否已安装 vcpkg
if exist "E:\vcpkg\vcpkg.exe" (
    echo [信息] 检测到 vcpkg 已安装在 E:\vcpkg
    goto install_libs
)

echo [步骤 1/3] 安装 vcpkg...
echo.

REM 检查是否有 git
where git >nul 2>nul
if %errorlevel% neq 0 (
    echo [错误] 未找到 git，请先安装 Git for Windows
    echo 下载地址: https://git-scm.com/download/win
    pause
    exit /b 1
)

REM 克隆并安装 vcpkg
cd /d E:\
echo 正在克隆 vcpkg...
git clone https://github.com/Microsoft/vcpkg.git

if %errorlevel% neq 0 (
    echo [错误] vcpkg 克隆失败
    pause
    exit /b 1
)

cd vcpkg
echo.
echo 正在引导 vcpkg...
call bootstrap-vcpkg.bat

if %errorlevel% neq 0 (
    echo [错误] vcpkg 引导失败
    pause
    exit /b 1
)

echo [成功] vcpkg 已安装到 E:\vcpkg
echo.

:install_libs
echo [步骤 2/3] 安装 xlnt 库...
echo 这可能需要 10-20 分钟，请耐心等待...
echo.

cd /d E:\vcpkg
call vcpkg install xlnt:x64-windows

if %errorlevel% neq 0 (
    echo [错误] xlnt 安装失败
    pause
    exit /b 1
)

echo.
echo [步骤 3/3] 安装 libzip 库...
echo.

call vcpkg install libzip:x64-windows

if %errorlevel% neq 0 (
    echo [错误] libzip 安装失败
    pause
    exit /b 1
)

echo.
echo ========================================
echo 安装完成！
echo ========================================
echo.
echo xlnt 和 libzip 已成功安装到:
echo E:\vcpkg\installed\x64-windows\
echo.
echo 接下来请执行:
echo   cd E:\pro\mysdk\baja-lite-xlsx
echo   yarn install
echo 或
echo   npm install
echo.
pause

