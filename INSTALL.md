# 安装指南 / Installation Guide

## Windows 安装步骤

### 1. 安装依赖工具

#### 1.1 安装 Node.js
从 [Node.js官网](https://nodejs.org/) 下载并安装 Node.js 16.x 或更高版本。

#### 1.2 安装 Python
从 [Python官网](https://www.python.org/downloads/) 下载并安装 Python 3.x。

**重要**: 安装时勾选"Add Python to PATH"选项。

#### 1.3 安装 Visual Studio Build Tools

选项 A - 完整 Visual Studio（推荐）:
- 下载 [Visual Studio 2019 或 2022](https://visualstudio.microsoft.com/)
- 安装时选择"使用C++的桌面开发"工作负载

选项 B - 仅构建工具:
```powershell
npm install --global windows-build-tools
```

### 2. 安装 vcpkg

```powershell
# 克隆 vcpkg
cd C:\
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg

# 引导 vcpkg
.\bootstrap-vcpkg.bat

# 添加到环境变量（可选）
setx VCPKG_ROOT "C:\vcpkg"
```

### 3. 安装 C++ 库

```powershell
cd C:\vcpkg

# 安装 xlnt
.\vcpkg install xlnt:x64-windows

# 安装 libzip
.\vcpkg install libzip:x64-windows

# 集成到 Visual Studio
.\vcpkg integrate install
```

### 4. 配置 binding.gyp

如果你的 vcpkg 安装在其他位置，修改 `binding.gyp` 文件：

```json
"include_dirs": [
  "你的vcpkg路径/installed/x64-windows/include"
],
"libraries": [
  "你的vcpkg路径/installed/x64-windows/lib/xlnt.lib",
  "你的vcpkg路径/installed/x64-windows/lib/zip.lib"
]
```

### 5. 构建模块

```powershell
cd 项目目录
npm install
```

如果遇到错误，尝试：
```powershell
npm run clean
npm run build
```

### 6. 测试

```powershell
npm test
```

---

## Linux 安装步骤

### 1. 安装依赖工具

```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install build-essential python3 cmake git

# Fedora/RHEL
sudo dnf install gcc-c++ python3 cmake git

# Arch Linux
sudo pacman -S base-devel python cmake git
```

### 2. 安装 xlnt

```bash
# 克隆并构建 xlnt
git clone https://github.com/tfussell/xlnt.git
cd xlnt
mkdir build && cd build
cmake ..
make -j$(nproc)
sudo make install
sudo ldconfig
```

### 3. 安装 libzip

```bash
# Ubuntu/Debian
sudo apt-get install libzip-dev

# Fedora/RHEL
sudo dnf install libzip-devel

# Arch Linux
sudo pacman -S libzip

# 或从源码构建
git clone https://github.com/nih-at/libzip.git
cd libzip
mkdir build && cd build
cmake ..
make -j$(nproc)
sudo make install
```

### 4. 构建模块

```bash
cd 项目目录
npm install
```

### 5. 测试

```bash
npm test
```

---

## macOS 安装步骤

### 1. 安装 Xcode Command Line Tools

```bash
xcode-select --install
```

### 2. 安装 Homebrew（如果尚未安装）

```bash
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

### 3. 安装依赖库

```bash
# 安装 xlnt
brew install xlnt

# 安装 libzip
brew install libzip
```

### 4. 构建模块

```bash
cd 项目目录
npm install
```

### 5. 测试

```bash
npm test
```

---

## 常见问题排查

### Windows: 找不到 Python

确保 Python 已添加到 PATH：
```powershell
python --version
```

如果不行，手动设置：
```powershell
npm config set python "C:\Python310\python.exe"
```

### Windows: MSBuild 错误

确保安装了正确的 Visual Studio 版本：
```powershell
npm config set msvs_version 2019
```

### Linux: 找不到 xlnt 或 libzip

确保库已正确安装并在 ld 路径中：
```bash
sudo ldconfig
ldconfig -p | grep xlnt
ldconfig -p | grep zip
```

如果仍然找不到，更新 `binding.gyp` 中的路径。

### 通用: node-gyp 构建失败

清理并重试：
```bash
npm run clean
rm -rf node_modules package-lock.json
npm install
```

### vcpkg 库版本不匹配

如果遇到链接错误，尝试重新安装库：
```powershell
cd C:\vcpkg
.\vcpkg remove xlnt:x64-windows libzip:x64-windows
.\vcpkg install xlnt:x64-windows libzip:x64-windows
```

---

## 验证安装

创建测试文件 `test-install.js`:

```javascript
const { readExcel } = require('./index');
console.log('✓ Module loaded successfully!');
```

运行：
```bash
node test-install.js
```

如果看到 "Module loaded successfully!"，说明安装成功！


