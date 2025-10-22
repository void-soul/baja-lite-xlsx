@echo off
chcp 65001 > nul
echo ============================================
echo 创建预编译包（包含 DLL）
echo ============================================
echo.

REM 检查 vcpkg 路径
if exist "E:\vcpkg\vcpkg.exe" (
    set VCPKG_ROOT=E:\vcpkg
) else if exist "C:\vcpkg\vcpkg.exe" (
    set VCPKG_ROOT=C:\vcpkg
) else (
    echo [✗] 未找到 vcpkg
    pause
    exit /b 1
)

echo VCPKG_ROOT=%VCPKG_ROOT%
echo.

REM 回到项目目录
cd /d %~dp0\..

echo [1/4] 清理旧的预编译包...
if exist prebuilds (
    rmdir /s /q prebuilds
    echo ✓ 已删除旧的预编译包
)
echo.

echo [2/4] 编译原生模块...
call npm run build
if %errorlevel% neq 0 (
    echo [✗] 编译失败
    pause
    exit /b 1
)
echo ✓ 编译成功
echo.

echo [3/4] 复制 DLL 文件...
call npm run copy-dlls
if %errorlevel% neq 0 (
    echo [✗] DLL 复制失败
    pause
    exit /b 1
)
echo.

echo [4/4] 创建预编译包...
echo.

echo 📦 创建 N-API v8 预编译包...
call npx prebuild --runtime napi --target 8 --strip
if %errorlevel% neq 0 (
    echo [✗] N-API 预编译包创建失败
    pause
    exit /b 1
)
echo.

echo 📦 创建 Electron v34 预编译包...
call npx prebuild --runtime electron --target 34.0.0 --strip
if %errorlevel% neq 0 (
    echo [✗] Electron 预编译包创建失败
    pause
    exit /b 1
)
echo.

echo ============================================
echo [✅] 预编译包创建成功！
echo ============================================
echo.

echo 预编译包位置: prebuilds\
dir /b prebuilds
echo.

echo 接下来可以:
echo   1. 测试预编译包: npm run test:prebuild
echo   2. 发布到 GitHub: git tag v1.0.13 ^&^& git push --tags
echo.
pause

