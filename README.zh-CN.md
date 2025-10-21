# baja-lite-xlsx

高性能的 Node.js 原生模块，用于读取 Excel 文件和提取嵌入图片，基于 xlnt C++ 库实现。

中文文档 | [English](./README.md)

## 特性

- ✅ **高性能** - 原生 C++ 实现，性能卓越
- ✅ **Excel 读取** - 读取 Excel 文件 (.xlsx) 数据
- ✅ **图片提取** - 提取嵌入的图片及其位置信息
- ✅ **多种输入方式** - 支持文件路径、Buffer、base64 字符串
- ✅ **多工作表支持** - 获取工作表名称并读取指定工作表
- ✅ **JSON 输出** - 易用的 JSON API，自动附加图片
- ✅ **TypeScript 支持** - 完整的 TypeScript 类型定义
- ✅ **预编译包** - Windows x64 为 Node.js 20+ 和 Electron 34+ 提供预编译包
- ✅ **Electron 支持** - 在 Electron 应用中无缝工作

## 安装

### Windows + Node.js 20 / Electron 34（推荐）

对于 Windows x64 + Node.js 20+ 或 Electron 34+ 环境，提供预编译包：

```bash
npm install baja-lite-xlsx
```

✅ **无需任何编译环境！** 预编译包会自动下载。

**支持的预编译平台：**
- Windows x64 + Node.js 20+
- Windows x64 + Electron 34+

### 其他环境

对于其他平台或 Node.js 版本，会自动从源码编译：

```bash
npm install baja-lite-xlsx
```

**源码编译要求：**

<details>
<summary><strong>Windows（Node 16/18 或其他版本）</strong></summary>

```bash
# 安装编译工具
npm install -g windows-build-tools

# 安装 vcpkg（如果还没有）
git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg
cd C:\vcpkg
.\bootstrap-vcpkg.bat

# 安装 xlnt 库
.\vcpkg install xlnt:x64-windows

# 设置环境变量
setx VCPKG_ROOT "C:\vcpkg"

# 现在安装包
npm install baja-lite-xlsx
```

</details>

<details>
<summary><strong>Linux</strong></summary>

**Ubuntu/Debian:**
```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake git
npm install baja-lite-xlsx
```

**CentOS/RHEL:**
```bash
sudo yum groupinstall "Development Tools"
sudo yum install cmake git
npm install baja-lite-xlsx
```

</details>

<details>
<summary><strong>macOS</strong></summary>

```bash
# 安装 Xcode Command Line Tools
xcode-select --install

# 安装包
npm install baja-lite-xlsx
```

</details>

## 快速开始

```javascript
const { readTableAsJSON, getSheetNames } = require('baja-lite-xlsx');

// 获取所有工作表名称
const sheets = getSheetNames('./data.xlsx');
console.log('工作表:', sheets);

// 读取 Excel 为 JSON（自动提取图片）
const data = readTableAsJSON('./data.xlsx', {
  headerRow: 0,           // 表头行索引（默认：0）
  headerMap: {            // 将中文表头映射为英文键名
    '姓名': 'name',
    '年龄': 'age',
    '照片': 'photo'
  }
});

console.log(data);
// 输出:
// [
//   {
//     name: '张三',
//     age: '25',
//     photo: {
//       data: Buffer,           // 图片二进制数据
//       name: 'photo1.png',     // 图片文件名
//       type: 'image/png'       // MIME 类型
//     }
//   },
//   ...
// ]
```

## API 文档

### `getSheetNames(input)`

获取 Excel 文件中的所有工作表名称。

**参数：**
- `input` (string | Buffer): Excel 文件路径、Buffer 或 base64 字符串

**返回：**
- `string[]`: 工作表名称数组

**示例：**
```javascript
const sheets = getSheetNames('./data.xlsx');
// ['Sheet1', 'Sheet2', 'Sheet3']
```

---

### `readTableAsJSON(input, options?)`

将 Excel 数据读取为 JSON 数组，自动附加图片。

**参数：**

- `input` (string | Buffer): Excel 文件路径、Buffer 或 base64 字符串
- `options` (object, 可选):
  - `sheetName` (string): 要读取的工作表名称（默认：第一个工作表）
  - `headerRow` (number): 表头行索引（默认：0）
  - `headerMap` (object): 将表头映射为属性名
  - `skipRows` (number[]): 要跳过的行索引

**返回：**
- `object[]`: 行对象数组

**示例：**
```javascript
const data = readTableAsJSON('./data.xlsx', {
  headerRow: 0,
  headerMap: {
    '名称': 'name',
    '年龄': 'age',
    '照片': 'photo'
  },
  skipRows: [1, 2]  // 跳过第 2 和 3 行
});
```

**图片附加：**

如果您的 Excel 有图片列（例如，列标题为 `photo1`），图片会自动附加到匹配的行：

```javascript
{
  name: '张三',
  age: '25',
  photo1: {
    data: Buffer,           // 图片二进制数据
    name: 'image1.png',     // 图片文件名
    type: 'image/png'       // MIME 类型（image/png, image/jpeg 等）
  }
}
```

**保存图片：**
```javascript
const fs = require('fs');
const data = readTableAsJSON('./data.xlsx');

data.forEach((row, i) => {
  if (row.photo && row.photo.data) {
    fs.writeFileSync(`./output/photo_${i}.png`, row.photo.data);
  }
});
```

---

### 输入类型

所有函数都支持三种输入类型：

**1. 文件路径：**
```javascript
readTableAsJSON('./data.xlsx');
```

**2. Buffer：**
```javascript
const fs = require('fs');
const buffer = fs.readFileSync('./data.xlsx');
readTableAsJSON(buffer);
```

**3. Base64 字符串：**
```javascript
const base64 = buffer.toString('base64');
readTableAsJSON(base64);
```

## 示例

查看 [examples](./examples) 目录获取更多详细用法：

- [basic.js](./examples/basic.js) - 基本用法
- [json-api.js](./examples/json-api.js) - JSON API 完整功能
- [advanced.js](./examples/advanced.js) - 高级用法
- [typescript-example.ts](./examples/typescript-example.ts) - TypeScript 用法
- [electron-example.js](./examples/electron-example.js) - Electron 应用用法

运行示例：
```bash
npm run example          # 基本示例
npm run example:json     # JSON API 示例
npm run example:advanced # 高级示例
```

## 开发者指南

### 本地开发

**1. 克隆仓库：**
```bash
git clone https://github.com/void-soul/baja-lite-xlsx.git
cd baja-lite-xlsx
```

**2. 安装依赖：**
```bash
npm install
```

**3. 设置编译环境：**

参见 [安装 - 其他环境](#其他环境) 了解各平台的编译工具要求。

**4. 编译模块：**
```bash
npm run build
```

**5. 运行测试：**
```bash
npm test
```

### 发布到 npm

**1. 更新版本：**
```bash
npm version patch  # 或 minor, major
```

**2. 提交并推送：**
```bash
git add .
git commit -m "chore: release v1.0.x"
git push origin master
```

**3. 创建并推送标签：**
```bash
git tag v1.0.x
git push origin v1.0.x
```

**4. 等待 GitHub Actions：**

GitHub Actions 工作流会自动：
- ✅ 为 Windows x64 + Node 20 构建预编译包
- ✅ 为 Windows x64 + Electron 34 构建预编译包
- ✅ 创建 GitHub Release
- ✅ 上传预编译包到 Release

**5. 发布到 npm：**
```bash
npm publish
```

**注意：** 预编译包托管在 GitHub Releases。匹配环境的用户在 `npm install` 时会自动下载。

### GitHub Actions 工作流

项目使用 GitHub Actions 进行自动化构建：

- **触发条件：** 推送匹配 `v*` 的标签（如 `v1.0.8`）
- **平台：** Windows x64
- **运行时：** Node.js 20, Electron 34
- **输出：** 预编译包上传到 GitHub Releases

**工作流文件：** [`.github/workflows/prebuild.yml`](./.github/workflows/prebuild.yml)

## 技术细节

**核心依赖：**
- [xlnt](https://github.com/tfussell/xlnt) - Excel 文件操作 C++ 库
- [libzip](https://libzip.org/) - ZIP 文件处理（Excel 文件是 ZIP 压缩包）
- [node-addon-api](https://github.com/nodejs/node-addon-api) - N-API C++ 封装

**构建系统：**
- `node-gyp` - 原生插件构建工具
- `prebuild` - 预编译包创建
- `prebuild-install` - 自动安装预编译包

**支持的平台：**
- Windows (x64)
- Linux (x64, ARM64)
- macOS (x64, ARM64)

**支持的运行时：**
- Node.js 16.x+
- Node.js 18.x+
- Node.js 20.x+（Windows x64 提供预编译包）
- Electron 34.x+（Windows x64 提供预编译包）

## 故障排查

<details>
<summary><strong>Windows 上编译失败</strong></summary>

1. 确保安装了 Visual Studio 2019+ 或 Build Tools for Visual Studio
2. 检查 `vcpkg` 是否正确安装，并设置了 `VCPKG_ROOT` 环境变量
3. 验证 xlnt 已安装：`C:\vcpkg\vcpkg list`
4. 尝试重新编译：`npm run clean && npm run build`

</details>

<details>
<summary><strong>安装后找不到模块</strong></summary>

1. 检查模块是否已安装：`npm list baja-lite-xlsx`
2. 尝试重新安装：`npm uninstall baja-lite-xlsx && npm install baja-lite-xlsx`
3. 清除 npm 缓存：`npm cache clean --force`

</details>

<details>
<summary><strong>未找到预编译包</strong></summary>

这在非 Windows 或非 Node 20 环境中是正常的。模块会自动回退到源码编译。确保您已安装必要的编译工具。

</details>

## 贡献

欢迎贡献！请随时提交 Pull Request。

## 许可证

MIT License - 详见 [LICENSE](./LICENSE) 文件。

## 作者

DEDEDE

## 链接

- [GitHub 仓库](https://github.com/void-soul/baja-lite-xlsx)
- [npm 包](https://www.npmjs.com/package/baja-lite-xlsx)
- [问题反馈](https://github.com/void-soul/baja-lite-xlsx/issues)
- [更新日志](./CHANGELOG.md)
