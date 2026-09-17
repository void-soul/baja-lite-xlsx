# Baja-Lite-XLSX

[![npm version](https://img.shields.io/npm/v/baja-lite-xlsx.svg)](https://www.npmjs.com/package/baja-lite-xlsx)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

[English](./README.md) | **简体中文**

基于 [xlnt](https://github.com/tfussell/xlnt) 与 libzip 的高性能 Node.js 原生模块
（N-API），用于读取 Excel `.xlsx` 表格并提取其中的图片。

## 特性

- 一行调用即可把工作表读成 JSON 数组：`readTableAsJSON` / `readTableAsJSONAsync`
- 输入支持文件路径、`Buffer` 或 base64 字符串
- 图片提取：浮动图片（`twoCellAnchor`）、嵌入式图片（`oneCellAnchor`）以及
  WPS 的 `DISPIMG` / `cellimages.xml` 图片
- 对真实文件足够健壮：带私有关系类型或反斜杠 ZIP 条目名的 WPS 工作簿会自动
  通过“清洗副本”加载
- 异步版本在线程池执行，不阻塞事件循环
- 读取上限（`maxRows` / `maxCols`）与单条目大小限制，抵御恶意文件
- 错误码（`err.code`）与非致命诊断信息（`warnings`）
- Windows / Linux / macOS 预编译二进制，采用 **N-API v8**：每个平台+架构只需
  一个二进制，即可覆盖所有 Node.js ≥ 16 与所有 Electron 版本

> 所有单元格值都以**字符串**返回（数字、日期同理；日期格式为
> `YYYY-MM-DD[ HH:MM:SS]`）。含图片的单元格返回图片对象。

## 安装

```bash
npm install baja-lite-xlsx
```

安装时会自动下载匹配的预编译二进制；若无对应平台的预编译包，则回退到源码编译
（见[从源码编译](#从源码编译)）。Windows 预编译包已内置运行时 DLL。

## 快速开始

```javascript
const { readTableAsJSON, readTableAsJSONAsync } = require('baja-lite-xlsx');
const fs = require('fs');

// 文件路径
const rows = readTableAsJSON('data.xlsx', {
  sheetName: 'Sheet1',           // 不传则读取第一个工作表
  headerRow: 0,                  // 表头行（从 0 开始）
  skipRows: [1, 2],              // 跳过的行
  headerMap: { '名称': 'name' }  // 表头重命名
});

// Buffer
const rows2 = readTableAsJSON(fs.readFileSync('data.xlsx'));

// base64（建议显式声明；启发式判定要求解码后是 ZIP 头）
const rows3 = readTableAsJSON(base64String, { inputEncoding: 'base64' });

// 非致命诊断信息（图片未挂载、工作表被截断等）
const { rows: rows4, warnings } = readTableAsJSON('data.xlsx', { includeWarnings: true });

// 异步：解析不在事件循环上执行
const rows5 = await readTableAsJSONAsync('big.xlsx', { maxRows: 100000 });
```

## API

### readTableAsJSON(input, options?)

同步接口，解析期间会阻塞调用线程——大文件、服务端或 Electron 界面请优先使用
`readTableAsJSONAsync`。

| 选项 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `sheetName` | `string` | 第一个工作表 | 要读取的工作表 |
| `headerRow` | `number` | `0` | 表头行索引（从 0 开始，非负整数） |
| `skipRows` | `number[]` | `[]` | 需要跳过的行索引（从 0 开始） |
| `headerMap` | `Record<string,string>` | `{}` | 输出对象的表头重命名映射 |
| `inputEncoding` | `'base64'` | – | 强制按 base64 解析字符串输入 |
| `maxRows` | `number` | `0` | 每个工作表读取行数上限（0 表示不限制）；超出部分截断并通过 `warnings` 上报 |
| `maxCols` | `number` | `0` | 每个工作表读取列数上限（0 表示不限制） |
| `includeWarnings` | `boolean` | `false` | 返回 `{ rows, warnings }` 而非仅行数组 |

返回 `Array<Record<string, string | ImageDataObject | ImageDataObject[]>>`。

图片单元格为 `{ data: Buffer, name: string, type: string }`；同一单元格挂载多张
图片时返回它们的数组。

### readTableAsJSONAsync(input, options?)

参数与返回结构完全一致，返回 `Promise`；解析在 libuv 线程池执行，保持事件循环
（以及 Electron 界面）响应。

### 图片归属规则

- **WPS 嵌入图片**（`=DISPIMG("ID_...")` 公式）：按图片 ID 归属；
- **标准嵌入式图片**（`oneCellAnchor`）：归属到锚点单元格；
- **浮动图片**（`twoCellAnchor`）：归属到锚点区域左上角单元格。

无法归属的情况（媒体缺失、关系 ID 未知、drawing 无法映射到工作表）不会让读取
失败，而是记录在 `warnings` 中（当 `includeWarnings` 为 `true` 时返回）。

### 错误码

所有抛出的错误都带可编程判断的 `code`：

| 错误码 | 含义 |
|--------|------|
| `ADDON_LOAD_FAILED` / `ADDON_NOT_FOUND` | 原生模块缺失，或它依赖的运行时 DLL 缺失 |
| `FILE_NOT_FOUND` | 输入路径不存在 |
| `FILE_OPEN_FAILED` | 文件存在但无法按工作簿解析 |
| `INVALID_INPUT` | 输入类型不支持，或 base64 缺少 ZIP 头 |
| `INVALID_OPTIONS` | 选项校验失败（如 `headerRow` 为负） |
| `NO_SHEETS` | 工作簿中没有工作表 |
| `SHEET_NOT_FOUND` | `options.sheetName` 未匹配到工作表 |
| `HEADER_ROW_OUT_OF_RANGE` | `headerRow` 超出可用行范围 |
| `PARSE_ERROR` / `READ_FAILED` | 工作簿解析失败 |

### 资源限制

- 未声明大小或超过 256 MB 的 ZIP 条目会被拒绝；媒体条目上限 128 MB（防 zip 炸弹）；
- `maxRows` / `maxCols` 限制工作表物化规模；
- 截断始终以 warning 上报，绝不静默发生。

## 从源码编译

各平台统一使用 [vcpkg](https://github.com/microsoft/vcpkg) 的 `xlnt` 与 `libzip`
端口，并要求设置 `VCPKG_ROOT` 环境变量（未设置时构建会给出明确报错）：

```bash
# 1. 安装依赖（按平台选择 triplet）
vcpkg install xlnt:x64-windows libzip:x64-windows   # Windows x64
vcpkg install xlnt:x64-linux   libzip:x64-linux     # Linux x64
vcpkg install xlnt:arm64-osx   libzip:arm64-osx     # macOS（Apple Silicon）

# 2. 指向 vcpkg
#    Windows:  set VCPKG_ROOT=C:\vcpkg
#    bash:     export VCPKG_ROOT=~/vcpkg

# 3. 编译（Windows 会额外复制运行时 DLL）
npm run build:dev     # 或者：npm run build

# 4. 运行测试
npm test
```

## CI 与发布

- 推送到 `master` / PR：lint + 类型检查、原生构建、`npm test`；
- 推送 `v*` 标签（需与 `package.json` 的 `version` 一致）后，发布流水线会：
  1. 构建 **win32-x64、linux-x64、linux-arm64、darwin-arm64、darwin-x64** 的
     N-API 预编译包（Windows 包内附带运行时 DLL）；
  2. 运行测试，并校验每个压缩包确实包含 `.node` 模块；
  3. 把压缩包与 `checksums.txt` 发布到对应的 GitHub Release；
  4. 发布 npm 包并附带 provenance（`npm publish --provenance`）。

每个平台只产出 N-API v8 一个二进制：ABI 稳定，无需按 Node/Electron 版本重复构建。

### 发布新版本

```bash
# 1. 升版本（需与后续 tag 去掉前缀 v 后一致）
npm version patch --no-git-tag-version     # 或 minor / major
# 2. 更新 CHANGELOG.md，用英文提交并推送
git commit -am "chore: release v1.0.17"
git push origin master
# 3. 打标签 -> 自动构建、发布 GitHub Release 与 npm 包
git tag -a v1.0.17 -m "v1.0.17" && git push origin v1.0.17
```

### npm 发布配置（一次性）

流水线使用 npm **Trusted Publishing（OIDC）**，仓库中不保存 `NPM_TOKEN`：

1. 打开 <https://www.npmjs.com/package/baja-lite-xlsx> → *Settings* →
   *Trusted Publisher* → *GitHub Actions*；
2. 填写：Organization/user `void-soul`、Repository `baja-lite-xlsx`、
   Workflow filename `prebuild.yml`、Environment 留空；
3. 保存后，下次推送 `v*` 标签即可免 token 发布。

如需改用 token：在仓库添加 secret `NPM_TOKEN`，并把发布步骤替换为：

```yaml
      - run: npm publish --access public
        env:
          NODE_AUTH_TOKEN: ${{ secrets.NPM_TOKEN }}
```

npm 包内只包含 `index.js`、`index.d.ts`、`src/`、`scripts/`、`binding.gyp`
与文档（见 `package.json` 的 `files`）：二进制从不打进 npm 包，而是从 GitHub
Release 下载，包体小且始终与发布资产一致。

## 故障排除（Windows）

模块加载失败时：

1. 安装 [VC++ Redistributable](https://aka.ms/vs/17/release/vc_redist.x64.exe)；
2. 把 `%VCPKG_ROOT%\installed\x64-windows\bin` 下的 DLL 复制到 `baja_xlsx.node`
   旁边（`node scripts/package-dlls.js` 可自动完成）；
3. 设置 `BAJA_XLSX_TEST_LOAD=1` 后重跑安装步骤，可看到具体的加载错误。

## 安全说明

- 本库解析不可信文件：请保持升级，并在处理第三方工作簿时使用 `maxRows` /
  `maxCols` 限制；
- 预编译包提供 SHA-256 校验（`npm run checksums` 生成
  `prebuilds/checksums.txt`），消费发布资产时建议核对。

## 许可证

MIT —— 见 [LICENSE](LICENSE)。
