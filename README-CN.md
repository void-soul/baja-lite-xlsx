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
// => [ { name: '张三', 金额: '1200' },
//      { name: '李四', 金额: '980' } ]
const rows = readTableAsJSON('data.xlsx', {
  sheetName: 'Sheet1',           // 不传则读取第一个工作表
  headerRow: 0,                  // 表头行（从 0 开始）
  skipRows: [1, 2],              // 跳过的行
  headerMap: { '名称': 'name' }  // 表头重命名
});

// Buffer，返回结构同上
const rows2 = readTableAsJSON(fs.readFileSync('data.xlsx'));

// base64（建议显式声明；启发式判定要求解码后是 ZIP 头）
const rows3 = readTableAsJSON(base64String, { inputEncoding: 'base64' });

// 非致命诊断信息
// => { rows: [ { ... } ], warnings: [ "Sheet 'Sheet1' truncated to 100000 of 500000 rows (maxRows)" ] }
const { rows: rows4, warnings } = readTableAsJSON('data.xlsx', { includeWarnings: true });

// 只读需要的列：表头文字或 Excel 列标（"B"、"C:E"），其余列根本不会被读取
// => [ { 金额: '1200' }, { 金额: '980' } ]
const amounts = readTableAsJSON('big.xlsx', { columns: ['金额'] });

// 异步：解析不在事件循环上执行，返回结构同上
const rows5 = await readTableAsJSONAsync('big.xlsx', { maxRows: 100000 });

// 只要数值时跳过整条图片处理链路（不再二次遍历压缩包、不解压媒体）
const rows6 = await readTableAsJSONAsync('big.xlsx', { includeImages: false });

// 百万行表：分批流式处理，内存保持恒定
// => { rowCount: 1250000, warnings: [] }
const { rowCount } = await readTableAsJSONAsync('huge.xlsx', {
  batchSize: 50000,
  onBatch(rows) {
    writeToDatabase(rows);
  }
});
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
| `columns` | `string[]` | `[]` | 只读这些列：表头文字（`"金额"`）或 Excel 列标（`"B"`、`"C:E"`）；空数组表示全部列 |
| `includeImages` | `boolean` | `true` | 为 `false` 时跳过整条图片处理链路（不二次遍历压缩包、不解压媒体） |
| `includeWarnings` | `boolean` | `false` | 返回 `{ rows, warnings }` 而非仅行数组 |

**返回** `Array<Object>`：每个数据行一个普通对象，键为应用 `headerMap` 之后的表头
文字，值全部为字符串；单元格内是图片时则为 `ImageDataObject`：

```javascript
// data.xlsx：| 名称 | 金额 | 照片 |
[
  { 名称: '张三', 金额: '1200', 照片: { data: <Buffer ...>, name: 'image1.png', type: 'image/png' } },
  { 名称: '李四', 金额: '980', 照片: { data: <Buffer ...>, name: 'image2.png', type: 'image/png' } }
]
```

`includeWarnings: true` 时返回结构变为：

```javascript
{
  rows: [ { 名称: '张三', 金额: '1200' } ],
  warnings: [ "Sheet 'Sheet1' truncated to 2 of 900 rows (maxRows)" ]
}
```

同一单元格挂载多张图片时值为 `ImageDataObject` 数组；未包含在 `columns` 中的列不会
出现在结果里。

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

## 写入

### 用 JSON 新建工作簿

```javascript
const { writeTableAsJSON, updateCells } = require('baja-lite-xlsx');

// 返回 Buffer
const bytes = writeTableAsJSON(rows, { sheetName: 'Data' });
fs.writeFileSync('out.xlsx', bytes);

// 或交给原生层直接落盘：=> { bytes, rowCount, sheetName }
writeTableAsJSON(rows, {
  output: 'out.xlsx',
  columns: { amount: { header: '金额', numberFormat: '#,##0.00', width: 14 } }
});
```

数字写为数值、布尔写为布尔、`Date` 写为带自动日期格式的真实日期——在 Excel 里
仍可继续计算，而不会退化成文本。

### 写入已有工作簿

```javascript
// 覆盖目标 sheet 的数据，其余部分原样保留（其他 sheet、图片、主题、样式按字节搬运）
const buffer = writeTableAsJSON(rows, { template: 'template.xlsx' });

// 修改指定单元格：返回 Buffer；传 output 时返回 { bytes, cells }
const patched = updateCells({
  template: 'book.xlsx',
  updates: [
    { cell: 'B7', value: 1234.5, numberFormat: '#,##0.00' },
    { cell: 'C7', value: '文本' },
    { sheet: 'Summary', cell: 'A1', value: new Date(2026, 0, 15) }
  ]
});
```

`updateCells` 是就地打补丁：未传 `numberFormat` 时单元格保留原有样式；被改写单元格
里残留的公式会被清除而不是留下过期值；不存在单元格/行会按列序、行序正确插入。未被
列出的内容保持逐字节不变。

### 模板渲染

```javascript
const { renderTemplate } = require('baja-lite-xlsx');

// 模板单元格内容示例："报表 ${title}"、"{{#each items}}" / "{{/each}}"、"${name}"
const buffer = renderTemplate(
  { title: '一季度', items: [{ name: '甲', amount: 1 }, { name: '乙', amount: 2 }] },
  { template: 'report-template.xlsx' }
);
```

单元格文本里支持两种标记：

| 标记 | 含义 |
|------|------|
| `${path}` | 取 `path` 处的值（`${user.name}`、`${items.0.amount}`）。优先在当前 `{{#each}}` 项内查找，其次从根查找；`../name` 跳出循环，`${@index}` 是当前 0 起的循环下标。 |
| `{{#each path}}` … `{{/each}}` | 两个标记之间的行按 `path` 数组逐项重复。 |

重复行是**复制模板行的 XML**生成的，因此样式、数字格式、行高、合并单元格、条件格式
都原样保留。只有文本含标记的单元格会被重写（且保留其样式），不含标记的工作表整表
按字节搬运。

标记既可以写在单元格里，也可以位于 `sharedStrings`（Excel 存储单元格文本的方式），
因此在 Excel 里手工制作的模板可以直接使用。

只含标记的行视为分隔行；若 `{{#each}}` 所在行还有数据单元格，则该行就是第一条被
重复的行。找不到值的标记会抛 `TEMPLATE_ERROR`（传 `strict: false` 则写为空串），
`{{#each}}` 未闭合一定报错。

#### 复用模板：`cache: true`

```javascript
// 报表服务反复渲染同一个模板
const filled = await renderTemplateAsync(values, { template, cache: true });
```

解析后的模板结构（行布局、标记位置、sharedStrings 表）会留在进程内**有界缓存**中
（最多 8 个模板 / 64 MB，LRU 淘汰），重复渲染因而完全跳过模板的读取与扫描。缓存键为
模板身份（压缩包中央目录的条目名、大小与 CRC）加表名过滤，因此模板被改写一定会被
识别。默认 `false`。

### 异步写入

三个写入函数都有异步孪生，返回值完全一致：

```javascript
const {
  writeTableAsJSONAsync, updateCellsAsync, renderTemplateAsync
} = require('baja-lite-xlsx');

await writeTableAsJSONAsync(rows, { output: 'report.xlsx' }); // { bytes, rowCount, sheetName }
const patched = await updateCellsAsync({ template, updates });
const filled = await renderTemplateAsync(values, { template });
```

行 / 更新 / 值会先在调用线程上拷入一份紧凑的原生快照（数据在 JS 里，这一步无法
移出），随后所有重活（工作表 XML、deflate、压缩包装配、写文件）都在 libuv 线程池
执行。服务端或 Electron 主进程在生成大工作簿时仍能继续处理请求；失败时 reject 的
错误码与同步调用抛出的完全一致。

### 流式写入

`rows` 不必是数组。任意可迭代对象（异步版本还支持异步可迭代对象）都会**分批**写入，
这正是数据库游标、生成器、文件解析器需要的形态：

```javascript
const { writeTableAsJSONAsync } = require('baja-lite-xlsx');

async function* fromDatabase() {
  for await (const batch of cursor) yield* batch;
}

// => { bytes, rowCount, sheetName }
await writeTableAsJSONAsync(fromDatabase(), {
  sheetName: 'Report',
  columns: { id: {}, name: {}, amount: { numberFormat: '#,##0.00' } },
  output: 'report.xlsx'
});
```

行会以每批 20000 行的粒度折进与异步路径相同的**紧凑原生快照**（每单元格 16 字节，
重复字符串只存一份），因此峰值内存是"一批 + 快照"，而不是整张表的一堆 JS 对象。
同步的 `writeTableAsJSON` 同样接受可迭代对象，只是在调用线程上排空。两种情况下
都必须显式给出 `options.columns`（无法从首行推断）。

## 性能

读取过程只做你要求的那部分工作：

- **只读单个工作表**：`sheetName` 在原生层解析，其他工作表不会被物化。
- **不为空格分配单元格**：取值前先探测，稀疏表不再为每个空坐标创建单元格。
- **图片按需解析**：包内没有 `xl/media`、`xl/drawings`、`xl/cellimages` 时整条
  图片链路直接跳过；`includeImages: false` 则无条件跳过。
- **列投影**：`columns` 在产出任何单元格值之前就收窄读取范围。

大文件实践建议：

1. 只需要少数几列时传 `columns`；
2. 只要数值时传 `includeImages: false`；
3. 用 `maxRows` / `maxCols` 限制探查读取的规模；
4. 服务端与 Electron 用 `readTableAsJSONAsync`，多次读取可在 libuv 线程池并行
   （由 `UV_THREADPOOL_SIZE` 控制并发度）；
5. 表太大不宜整体物化时用 `onBatch` + `batchSize`。

### 用 `onBatch` 流式读取

```javascript
const { rowCount, warnings } = readTableAsJSON('huge.xlsx', {
  batchSize: 50000,                 // 每批行数，默认 50000
  onBatch(rows, meta) {             // meta: { startIndex, count }
    appendToCsv(rows);
  }
});
```

行在解析过程中边产出边回调，峰值内存只有一个批次而非整张表；返回值变为
`{ rowCount, warnings }`，不会累积行。其余选项（`columns`、`includeImages`、
`skipRows`、`headerMap` 等）行为与一次性读取完全一致，图片同样按行归属后返回。

### 基准测试

```bash
npm run bench                     # 默认用 examples/sample.xlsx
npm run bench -- huge.xlsx        # 指定自己的文件
npm run bench -- huge.xlsx --iterations 3 --batch 20000
```

会打印一次性读取、跳过图片、列投影、流式四类场景的最优/中位耗时、行数与驻留内存。
大文件上流式那一行内存恒定，而一次性读取会随表规模增长——这是最直观的验证方式。

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
