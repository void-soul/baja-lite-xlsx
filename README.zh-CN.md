# baja-lite-xlsx

一个用于读取 Excel (.xlsx) 文件和提取图片的 Node.js 原生模块，使用 xlnt C++ 库实现。

[English](README.md) | 简体中文

## 特性

- 📊 高性能读取 Excel (.xlsx) 文件
- 🖼️ 提取 Excel 中的嵌入图片
- 📍 获取图片位置信息（工作表、行、列）
- 🚀 原生 C++ 实现，性能优异
- 💾 图片以 Node.js Buffer 返回，便于处理
- 🔧 支持 Windows、Linux、macOS

## 安装前准备

### Windows

1. **Node.js** (>= 16.0.0)
2. **Python 3.x** (node-gyp 需要)
3. **Visual Studio 2019 或更新版本**，包含 C++ 构建工具
4. **vcpkg** - C++ 包管理器
5. **xlnt 库**，通过 vcpkg 安装：

```powershell
# 安装 vcpkg（如果未安装）
git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg
cd C:\vcpkg
.\bootstrap-vcpkg.bat

# 安装 xlnt 和 libzip
.\vcpkg install xlnt:x64-windows
.\vcpkg install libzip:x64-windows
```

**注意**: 如果 vcpkg 不在 `C:\vcpkg`，请修改 `binding.gyp` 中的路径。

### Linux

```bash
# 安装构建工具（Ubuntu/Debian）
sudo apt-get install build-essential python3 cmake git libzip-dev

# 安装 xlnt（从源码构建）
git clone https://github.com/tfussell/xlnt.git
cd xlnt && mkdir build && cd build
cmake .. && make -j$(nproc)
sudo make install
sudo ldconfig
```

### macOS

```bash
# 安装 Xcode 命令行工具
xcode-select --install

# 通过 Homebrew 安装依赖
brew install xlnt libzip
```

## 安装

```bash
npm install
```

这会自动使用 node-gyp 构建原生模块。

如果遇到构建错误，尝试：

```bash
npm run clean
npm run build
```

## API 简介

baja-lite-xlsx 提供两套API：

1. **JSON API（推荐）** - 直接返回JSON数组，自动处理图片
2. **原始API** - 返回完整的Excel数据结构

## 使用方法

### JSON API（推荐）⭐

#### 快速开始

```javascript
const { readTableAsJSON, getSheetNames } = require('baja-lite-xlsx');

// 获取所有Sheet名称
const sheets = getSheetNames('./file.xlsx');
console.log(sheets); // ['Sheet1', 'Sheet2']

// 读取为JSON数组
const data = readTableAsJSON('./file.xlsx', {
  headerMap: {
    '姓名': 'name',
    '年龄': 'age',
    'Size': 'size'
  }
});

// 结果: [{ name: '张三', age: '25', size: 'M' }, ...]
```

#### 完整配置示例

```javascript
const data = readTableAsJSON('./file.xlsx', {
  sheetName: 'Sheet1',      // 可选，指定Sheet
  headerRow: 0,             // 可选，表头行索引（默认0）
  skipRows: [1, 2],         // 可选，跳过的行索引
  headerMap: {              // 可选，表头映射
    '姓名': 'name',
    '年龄': 'age',
    '照片': 'avatar'      // 映射列名
  }
});

// 图片会根据所在列的表头自动添加：
// Excel: | 姓名  | 年龄 | 照片  |
//        | 张三  | 25   | [图片]|
//
// 结果：
// {
//   name: '张三',
//   age: '25',
//   avatar: {               // 列名"照片"映射为"avatar"
//     data: Buffer,         // 图片数据
//     name: 'img1.png',     // 图片文件名
//     type: 'image/png'     // 图片类型
//   }
// }
```

#### 使用中文API

```javascript
const { 读取表格, 读取表格SheetName } = require('baja-lite-xlsx');

const sheets = 读取表格SheetName('./file.xlsx');
const data = 读取表格('./file.xlsx', { /* 配置 */ });
```

**详细文档**: 查看 [API_USAGE_CN.md](API_USAGE_CN.md)

### 原始API

#### 读取完整的 Excel 文件

```javascript
const { readExcel } = require('baja-lite-xlsx');

const data = readExcel('./sample.xlsx');

// 访问工作表
console.log(data.sheets[0].name);     // 工作表名称
console.log(data.sheets[0].data);     // 单元格数据的二维数组

// 访问图片
console.log(data.images[0].name);     // 图片文件名
console.log(data.images[0].data);     // 图片数据（Buffer）
console.log(data.images[0].type);     // MIME 类型（如 'image/png'）

// 访问图片位置
console.log(data.imagePositions[0].sheet);    // 工作表名称
console.log(data.imagePositions[0].from);     // { col: 2, row: 5 }
console.log(data.imagePositions[0].to);       // { col: 4, row: 10 }
```

### 仅提取图片

```javascript
const { extractImages } = require('baja-lite-xlsx');
const fs = require('fs');

const images = extractImages('./sample.xlsx');

images.forEach(img => {
  fs.writeFileSync(img.name, img.data);
  console.log(`已保存 ${img.name}`);
});
```

## API 参考

### readExcel(filepath)

读取 Excel 文件并返回所有数据，包括工作表、图片和图片位置。

**参数:**
- `filepath` (字符串): .xlsx 文件的路径

**返回:**
```javascript
{
  sheets: [
    {
      name: string,           // 工作表名称
      data: string[][]        // 单元格值的二维数组
    }
  ],
  images: [
    {
      name: string,           // 图片文件名
      data: Buffer,           // 图片数据（Buffer）
      type: string            // MIME 类型（如 'image/png'）
    }
  ],
  imagePositions: [
    {
      image: string,          // 图片文件名
      sheet: string,          // 工作表名称
      from: {                 // 左上角位置
        col: number,
        row: number
      },
      to: {                   // 右下角位置
        col: number,
        row: number
      }
    }
  ]
}
```

### extractImages(filepath)

仅从 Excel 文件中提取图片。

**参数:**
- `filepath` (字符串): .xlsx 文件的路径

**返回:**
```javascript
[
  {
    name: string,             // 图片文件名
    data: Buffer,             // 图片数据（Buffer）
    type: string              // MIME 类型
  }
]
```

## 示例

查看 `examples/` 目录获取更多使用示例：

```bash
node examples/basic.js       # 基础用法
node examples/advanced.js    # 高级用法
```

## 测试

创建一个包含数据和图片的 `test/sample.xlsx` 文件，然后运行：

```bash
npm test
```

## 限制

- 目前仅支持 .xlsx 格式（不支持 .xls）
- 图片提取依赖于 xlnt 库的功能
- 某些高级 Excel 功能可能不被支持

**关于图片提取的说明**: xlnt 库对图片提取的内置支持有限。为了实现完整的图片提取和位置信息功能，本模块直接解析 .xlsx ZIP 结构和关系 XML 文件。

## 从源码构建

```bash
# 清理之前的构建
npm run clean

# 构建原生模块
npm run build

# 或者安装并构建
npm install
```

## 故障排除

### Windows 构建失败

1. 确保安装了 Visual Studio C++ 构建工具
2. 检查 `binding.gyp` 中的 vcpkg 路径是否正确
3. 验证 xlnt 已安装：`vcpkg list | findstr xlnt`

### Linux/macOS 构建失败

1. 确保 xlnt 已安装且在库路径中
2. 如需要，更新 `binding.gyp` 中的包含路径
3. 检查编译器是否支持 C++17

### 模块未找到错误

确保在使用模块前运行了 `npm install` 或 `npm run build`。

## 文档

- [完整安装指南](INSTALL.md)
- [快速入门](QUICKSTART.md)
- [项目总结](PROJECT_SUMMARY.md)
- [更新日志](CHANGELOG.md)

## 许可证

MIT

## 贡献

欢迎贡献！请随时提交 Pull Request。

## 作者

为 Node.js 应用程序的高性能 Excel 处理而创建。


