# 快速入门 / Quick Start Guide

## 最简单的安装方法 (Windows)

### 第一步：安装必需工具

```powershell
# 1. 下载并安装 Node.js (v16+)
# 访问: https://nodejs.org/

# 2. 下载并安装 Python 3.x
# 访问: https://www.python.org/downloads/
# 注意: 安装时勾选 "Add Python to PATH"

# 3. 安装 Visual Studio 2019 或 2022
# 访问: https://visualstudio.microsoft.com/
# 选择 "使用C++的桌面开发" 工作负载
```

### 第二步：安装 vcpkg 和依赖库

```powershell
# 克隆 vcpkg
cd C:\
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat

# 安装依赖
.\vcpkg install xlnt:x64-windows
.\vcpkg install libzip:x64-windows
```

### 第三步：构建模块

```powershell
# 进入项目目录
cd E:\pro\mysdk\baja-lite-xlsx

# 安装并构建
npm install
```

如果构建成功，你会看到：
```
✓ baja_xlsx.node compiled successfully
```

### 第四步：测试

创建测试文件 `test.js`:

```javascript
const { readExcel } = require('baja-lite-xlsx');

// 读取 Excel 文件
const data = readExcel('./your-file.xlsx');

// 打印工作表名称
console.log('Sheets:', data.sheets.map(s => s.name));

// 打印第一行数据
if (data.sheets.length > 0 && data.sheets[0].data.length > 0) {
  console.log('First row:', data.sheets[0].data[0]);
}

// 打印图片数量
console.log('Images found:', data.images.length);
```

运行:
```powershell
node test.js
```

## 常见问题

### Q: Python 找不到？
A: 运行 `python --version` 检查。如果不行：
```powershell
npm config set python "C:\Python310\python.exe"
```

### Q: MSBuild 错误？
A: 设置正确的 Visual Studio 版本：
```powershell
npm config set msvs_version 2019
```

### Q: vcpkg 路径不对？
A: 修改 `binding.gyp` 中的路径：
```json
"include_dirs": ["你的vcpkg路径/installed/x64-windows/include"]
```

### Q: 构建失败？
A: 清理后重试：
```powershell
npm run clean
npm install
```

## 下一步

- 查看 [完整文档](README.md)
- 查看 [安装指南](INSTALL.md)
- 查看 [使用示例](examples/)

---

## Simplest Installation (Linux/macOS)

### Linux (Ubuntu/Debian)

```bash
# Install dependencies
sudo apt-get update
sudo apt-get install build-essential python3 cmake git libzip-dev

# Install xlnt
git clone https://github.com/tfussell/xlnt.git
cd xlnt && mkdir build && cd build
cmake .. && make -j$(nproc)
sudo make install
sudo ldconfig

# Build module
cd /path/to/baja-lite-xlsx
npm install
```

### macOS

```bash
# Install Homebrew (if not installed)
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install dependencies
brew install xlnt libzip

# Build module
cd /path/to/baja-lite-xlsx
npm install
```

## Usage Example

```javascript
const { readExcel, extractImages } = require('baja-lite-xlsx');

// Read Excel file
const data = readExcel('./sample.xlsx');

// Access sheets
data.sheets.forEach(sheet => {
  console.log(`Sheet: ${sheet.name}`);
  console.log(`Rows: ${sheet.data.length}`);
});

// Extract images
const images = extractImages('./sample.xlsx');
images.forEach(img => {
  console.log(`Image: ${img.name}, Size: ${img.data.length} bytes`);
});
```

## Need Help?

- Check [INSTALL.md](INSTALL.md) for detailed installation instructions
- Check [README.md](README.md) for complete API documentation
- Check [examples/](examples/) for more usage examples


