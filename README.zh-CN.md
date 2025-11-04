# baja-lite-xlsx

轻量级、高性能的 Node.js Excel 读取库，支持自动图片提取。使用原生 C++ 实现，性能卓越。

[English Documentation](./README.md)

## 特性

- ✅ **读取 Excel 表格为 JSON** - 简单的 API 将 Excel 转换为 JavaScript 对象
- ✅ **自动图片提取** - 支持浮动图片和嵌入式图片
- ✅ **WPS Excel 支持** - 完全兼容 WPS Office 嵌入式图片（DISPIMG 公式）
- ✅ **高性能** - 使用 xlnt 库的原生 C++ 实现
- ✅ **TypeScript 支持** - 包含完整的类型定义
- ✅ **多种输入格式** - 支持文件路径、Buffer 或 base64 字符串
- ✅ **跨平台** - 提供 Windows、Linux、macOS 预编译二进制文件

## 安装

```bash
npm install baja-lite-xlsx
```

该包包含预编译的二进制文件。如果您的平台没有可用的二进制文件，它会自动从源代码编译（需要构建工具）。

## 快速开始

```javascript
const { readTableAsJSON } = require('baja-lite-xlsx');
const fs = require('fs');

// 读取 Excel 文件为 JSON
const data = readTableAsJSON('./sample.xlsx', {
  headerRow: 0,
  headerMap: {
    '姓名': 'name',
    '年龄': 'age',
    '照片': 'photo'
  }
});

// 访问数据
data.forEach(row => {
  console.log(row.name, row.age);

  // 图片会自动提取为 { data: Buffer, name, type } 对象
  if (row.photo && row.photo.data) {
    fs.writeFileSync(`${row.name}.png`, row.photo.data);
  }
});
```

## API 参考

### `readTableAsJSON(input, options)`

读取 Excel 文件并返回对象数组。

**参数：**
- `input` (string | Buffer): Excel 文件路径、Buffer 或 base64 字符串
- `options` (object):
  - `sheetName` (string): 要读取的工作表名称（默认为第一个工作表）
  - `headerRow` (number): 表头行索引，从 0 开始（默认：0）
  - `skipRows` (number[]): 要跳过的行索引数组
  - `headerMap` (object): 将原始表头映射为新的属性名

**返回值：** `Array<Object>` - 每个对象代表一行数据

**图片支持：**
- 浮动图片 (twoCellAnchor) ✅
- 嵌入式图片 - 标准 Excel (oneCellAnchor) ✅
- 嵌入式图片 - WPS Excel (DISPIMG 公式) ✅

所有图片都会返回为：
```javascript
{
  data: Buffer,      // 图片二进制数据
  name: 'image.png', // 文件名
  type: 'image/png'  // MIME 类型
}
```

## TypeScript 使用

```typescript
import { readTableAsJSON, ImageDataObject } from 'baja-lite-xlsx';

interface Employee {
  name: string;
  age: string;
  photo: string | ImageDataObject;
}

const data = readTableAsJSON('./sample.xlsx', {
  headerMap: {
    '姓名': 'name',
    '年龄': 'age',
    '照片': 'photo'
  }
}) as Employee[];
```

## 从源代码编译

### 环境准备

#### Windows
```bash
# 1. 安装 Visual Studio Build Tools 2019+
# 下载地址: https://visualstudio.microsoft.com/downloads/

# 2. 安装 vcpkg
cd C:\
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat

# 3. 安装依赖
.\vcpkg install xlnt:x64-windows
.\vcpkg install libzip:x64-windows

# 4. 设置环境变量
set VCPKG_ROOT=C:\vcpkg
```

或使用自动化脚本：
```bash
scripts/check-vcpkg.bat
```

#### Linux
```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install build-essential
sudo apt-get install libxlnt-dev libzip-dev

# Fedora/RHEL
sudo dnf install gcc-c++ make
sudo dnf install xlnt-devel libzip-devel
```

#### macOS
```bash
# 安装 Xcode 命令行工具
xcode-select --install

# 通过 Homebrew 安装依赖
brew install xlnt libzip
```

### 编译

```bash
# 克隆仓库
git clone https://github.com/void-soul/baja-lite-xlsx.git
cd baja-lite-xlsx

# 安装依赖
npm install

# 编译原生模块
npm run build

# 或使用本地构建脚本（Windows）
scripts/build-local.bat
```

## 创建预编译包

对于想要创建预编译二进制文件的库维护者：

### Windows
```bash
# 运行预编译脚本
scripts/create-prebuilds.bat

# 该脚本会：
# 1. 清理旧的构建
# 2. 编译原生模块
# 3. 复制 DLL 文件
# 4. 为 N-API 和 Electron 创建 .tar.gz 包
# 5. 将 DLL 文件打包到压缩包中
```

预编译包将生成在 `prebuilds/` 目录中。

## 发布

### 发布到 npm

```bash
# 1. 更新 package.json 中的版本号
npm version patch  # 或 minor, major

# 2. 构建预编译包
scripts/create-prebuilds.bat

# 3. 发布到 npm
npm publish
```

### 发布到 GitHub Releases

该库使用 GitHub Releases 托管预编译二进制文件。工作流程：

1. **本地创建预编译包：**
   ```bash
   scripts/create-prebuilds.bat
   ```
2. **NPM发布**
   ```bash
   npm version 1.0.16
   ```

3. **创建 git 标签：**
   ```bash
   git tag v1.0.16
   git push origin v1.0.16
   ```

4. **等待 GitHub Actions 执行完毕**
  - 访问: https://github.com/void-soul/baja-lite-xlsx/actions
  - 查看任务状态

4. **上传到 GitHub Releases：**
   - 访问: https://github.com/void-soul/baja-lite-xlsx/releases
   - 点击 "Create a new release"
   - 选择您的标签（例如 v1.0.16）


## 示例

查看 `examples/` 目录获取更多使用示例：
- `basic.js` - 基本用法
- `json-api.js` - JSON API 与图片提取
- `advanced.js` - 高级功能
- `typescript-example.ts` - TypeScript 示例

## 许可证

MIT

## 作者

DEDEDE

## 仓库

https://github.com/void-soul/baja-lite-xlsx
