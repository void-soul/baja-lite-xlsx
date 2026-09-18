# Baja-Lite-XLSX

[![npm version](https://img.shields.io/npm/v/baja-lite-xlsx.svg)](https://www.npmjs.com/package/baja-lite-xlsx)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

[English](./README.md) | **简体中文**

基于 [xlnt](https://github.com/tfussell/xlnt) 与 libzip 的高性能 Node.js 原生模块
（N-API），用于读取 Excel `.xlsx` 表格并提取其中的图片。

## 特性

- **全部 API 均为异步**：只有四个函数，全部返回 Promise；重活（解析、XML 生成、
  deflate、压缩包装配、写文件）都在 libuv 线程池执行，服务端与 Electron UI 始终
  响应
- 读取：`readTableAsJSON`，一行调用把工作表读成 JSON 数组
- 写入：`writeTableAsJSON` —— 可向任意 sheet **追加**行，并通过在返回的
  `Buffer` 上链式调用构建多 sheet 工作簿
- 单元格级修补：`updateCells`
- 模板渲染：`renderTemplate`，支持 ejsExcel 风格的 `<%...%>` 标记（行循环、动态
  公式、合并单元格、图片、二维码）以及简单的原生 `${...}` 标记
- 输入输出均支持文件路径或 `Buffer` —— 写入结果可直接作为下一次调用的
  `sourceFile` 链式使用
- 读取时可提取图片：浮动图片（`twoCellAnchor`）、嵌入式图片（`oneCellAnchor`）
  以及 WPS 的 `DISPIMG` / `cellimages.xml` 图片
- 类型化读取（`values: 'typed'`）：数字、布尔、日期返回真实 JS 值
- 对真实文件足够健壮：带私有关系类型或反斜杠 ZIP 条目名的 WPS 工作簿会自动
  通过“清洗副本”加载
- 读取上限（`maxRows` / `maxCols`）与单条目大小限制，抵御恶意文件
- 错误码（`err.code`）与非致命诊断信息（`warnings`）
- Windows / Linux / macOS 预编译二进制，采用 **N-API v8**：每个平台+架构只需
  一个二进制，即可覆盖所有 Node.js ≥ 16 与所有 Electron 版本

> 默认所有单元格值都以**字符串**返回（数字、日期同理；日期格式为
> `YYYY-MM-DD[ HH:MM:SS]`）。含图片的单元格返回图片对象；传 `values: 'typed'`
> 可获得真实类型。

## 安装

```bash
npm install baja-lite-xlsx
```

安装时会自动下载匹配的预编译二进制；若无对应平台的预编译包，则回退到源码编译
（见[从源码编译](#从源码编译)）。Windows 预编译包已内置运行时 DLL。

## 快速开始

```javascript
const { readTableAsJSON, writeTableAsJSON, updateCells, renderTemplate } =
  require('baja-lite-xlsx');

// 读取：=> [ { 姓名: '张三', 金额: '1200' }, { 姓名: '李四', 金额: '980' } ]
const rows = await readTableAsJSON('data.xlsx', {
  sheetName: 'Sheet1',       // 不传则读取第一个工作表
  columns: ['金额'],          // 只读这些列，其余列根本不会被读取
  values: 'typed',           // 配合 engine: 'xml' 返回真实类型
  engine: 'xml'              // 直读引擎：大文件更快
});

// 写入：=> Buffer
const bytes = await writeTableAsJSON(rows2, { sheetName: 'Data' });

// 链式：向同一 sheet 追加行，再新增第二个 sheet。
// sourceFile 之外的内容保持原样。
const withMore = await writeTableAsJSON(rows3, { sourceFile: bytes, sheetName: 'Data' });
const multiSheet = await writeTableAsJSON(summaryRows, { sourceFile: withMore, sheetName: 'Summary' });

// 修补单元格
const patched = await updateCells({
  sourceFile: multiSheet,
  updates: [{ cell: 'B7', value: 1234.5, numberFormat: '#,##0.00' }]
});

// 渲染模板（单元格内为 ejsExcel 语法标记）
const report = await renderTemplate(
  { title: '一季度', items: [{ name: '甲', amount: 1 }, { name: '乙', amount: 2 }] },
  { template: 'report-template.xlsx' }
);

// 把任意结果写到磁盘
await writeTableAsJSON(rows2, { output: 'out.xlsx' }); // => { bytes, rowCount, sheetName }
```

## API

### readTableAsJSON(input, options?)

读取一个工作表，解析为 JSON 数组（每个数据行一个对象）。解析在 libuv 线程池执行，
事件循环（以及 Electron 界面）保持响应。参数不合法时**同步抛出**；解析失败时
reject。

| 参数 | 类型 | 是否必传 | 默认值 | 说明 |
|------|------|----------|--------|------|
| `input` | `string \| Buffer` | **必传** | – | 文件路径、工作簿字节或 base64 字符串 |
| `options` | `object` | 否 | `{}` | 选项，见下表 |

**选项**

| 选项 | 类型 | 是否必传 | 默认值 | 说明 |
|------|------|----------|--------|------|
| `sheetName` | `string` | 否 | 第一个工作表 | 要读取的工作表 |
| `headerRow` | `number` | 否 | `0` | 表头行索引（从 0 开始） |
| `skipRows` | `number[]` | 否 | `[]` | 需要跳过的行索引（从 0 开始） |
| `headerMap` | `object` | 否 | `{}` | 表头重命名：`{ 原表头: '新表头' }` |
| `inputEncoding` | `'base64'` | 否 | – | 强制按 base64 解析字符串输入 |
| `maxRows` | `number` | 否 | `0` | 读取行数上限（0 不限制）；截断通过 `warnings` 上报 |
| `maxCols` | `number` | 否 | `0` | 读取列数上限（0 不限制） |
| `columns` | `string[]` | 否 | `[]` | 只读这些列：表头文字（`"金额"`）或 Excel 列标（`"B"`、`"C:E"`）；未选中的列根本不会被读取 |
| `engine` | `'xlnt' \| 'xml'` | 否 | `'xlnt'` | `'xml'` 直接从压缩包读取工作表（投影/流式场景显著更快）；文件需要完整模型时自动回退 |
| `values` | `'string' \| 'typed'` | 否 | `'string'` | `'typed'` 返回真实的数字、布尔与 `Date`（需 `engine: 'xml'`） |
| `includeImages` | `boolean` | 否 | `true` | 为 `false` 时跳过整条图片处理链路 |
| `includeWarnings` | `boolean` | 否 | `false` | 返回 `{ rows, warnings }` 而非仅行数组 |
| `onBatch` | `function` | 否 | – | 分批流式回调；此时返回 `{ rowCount, warnings }` |
| `batchSize` | `number` | 否 | `50000` | `onBatch` 每批行数 |

**返回** `Promise<Array<Object>>`：每个数据行一个对象，键为应用 `headerMap` 之后的
表头文字，值全部为字符串；单元格内是图片时则为图片对象。传 `values: 'typed'` 时
值为真实的数字、布尔与 `Date`。

**示例**

```javascript
const rows = await readTableAsJSON('data.xlsx', { sheetName: 'Sheet1' });
// => [ { name: '张三', 金额: '1200' }, { name: '李四', 金额: '980' } ]

// 类型化：金额为 number，日期为 Date
const typed = await readTableAsJSON('big.xlsx', { engine: 'xml', values: 'typed' });

// 百万行表分批流式处理，内存恒定
const { rowCount } = await readTableAsJSON('huge.xlsx', {
  batchSize: 50000,
  onBatch(batch, meta) { writeToDatabase(batch); }   // meta: { startIndex, count }
});
```

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

### writeTableAsJSON(rows, options?)

把行数据写进工作表，resolve 为 `.xlsx` 字节（传 `output` 时为摘要对象）。数字写为
数值、布尔写为布尔、`Date` 写为真实日期——在 Excel 里仍可继续计算，而不会退化成
文本。

传 `sourceFile`（文件路径，或任意写入调用返回的 `Buffer`）时，行会**追加**到指定
sheet——sheet 不存在时会自动创建，这正是通过在返回的 `Buffer` 上链式调用构建多
sheet 工作簿的方式。传 `append: false` 则替换该 sheet 的数据；两种情况下工作簿其余
部分（其他 sheet、图片、样式）都按字节搬运。

| 参数 | 类型 | 是否必传 | 默认值 | 说明 |
|------|------|----------|--------|------|
| `rows` | `Array \| Iterable \| AsyncIterable` | **必传** | – | 数据行：对象、数组，或任意（异步）可迭代对象（生成器、数据库游标等） |
| `options` | `object` | 否 | `{}` | 选项，见下表 |

**选项**

| 选项 | 类型 | 是否必传 | 默认值 | 说明 |
|------|------|----------|--------|------|
| `sheetName` | `string` | 否 | `'Sheet1'` | 工作表名；传 `sourceFile` 时为追加目标 sheet（不存在则创建） |
| `sourceFile` | `string \| Buffer` | 否 | – | 写入目标工作簿：路径，或上一次 write / update / render 返回的 `Buffer` |
| `append` | `boolean` | 否 | `true` | 追加到该 sheet 已有行之后；`false` 替换该 sheet 数据（仅在传 `sourceFile` 时有意义） |
| `includeHeader` | `boolean` | 否 | `true`；传 `sourceFile` 时为 auto | 是否写表头行；"auto" 仅在 sheet 为新建或原本为空时写表头，链式追加不会重复表头 |
| `freezeHeader` | `boolean` | 否 | `false` | 冻结表头行（仅新建 sheet） |
| `columns` | `object \| array` | 否 | 取自首行 | 列定义：`{ 属性: { header, numberFormat, align, width } }` 或 `[{ key \| index, header, ... }]`。`rows` 为可迭代对象时**必传**（无法从首行推断） |
| `output` | `string` | 否 | – | 由原生层直接写文件，而非返回 Buffer |
| `compression` | `number` | 否 | `6` | 0（仅存储）.. 9（最大压缩） |

**返回** `Promise<Buffer>` —— 完整工作簿；传 `output` 时为
`Promise<{ bytes, rowCount, sheetName }>`。

**示例**

```javascript
// 新建工作簿：=> Buffer
const bytes = await writeTableAsJSON(rows, { sheetName: 'Data' });

// 向同一 sheet 追加行（不会重复表头），再新增第二个 sheet——
// 多 sheet 工作簿通过在返回的 Buffer 上链式调用构建：
const more = await writeTableAsJSON(rows2, { sourceFile: bytes, sheetName: 'Data' });
const multi = await writeTableAsJSON(summary, { sourceFile: more, sheetName: 'Summary' });

// 替换 sheet 数据而不是追加
const replaced = await writeTableAsJSON(rows3, {
  sourceFile: multi, sheetName: 'Data', append: false
});

// 从数据库游标流式写入，内存恒定（此时 columns 必传）
await writeTableAsJSON(fromDatabase(), {
  sheetName: 'Report',
  columns: { id: {}, name: {}, amount: { numberFormat: '#,##0.00' } },
  output: 'report.xlsx'   // => { bytes, rowCount, sheetName }
});
```

### updateCells(options)

重写既有工作簿中的指定单元格。只有受影响的工作表会重新生成，压缩包其余部分按字节
搬运，因此未触碰的 sheet、图片与样式原样保留。未传 `numberFormat` 时单元格保留
原有样式；被改写单元格里残留的公式会被清除而不是留下过期值；不存在的单元格/行会按
列序、行序正确插入。

| 参数 | 类型 | 是否必传 | 默认值 | 说明 |
|------|------|----------|--------|------|
| `options` | `object` | **必传** | – | 见下表 |

**选项**

| 选项 | 类型 | 是否必传 | 默认值 | 说明 |
|------|------|----------|--------|------|
| `sourceFile` | `string \| Buffer` | **必传** | – | 要修补的工作簿：路径，或上一次 write / update / render 返回的 `Buffer` |
| `updates` | `array` | **必传** | – | `{ sheet?, cell, value?, numberFormat? }` 条目；`cell` 为 A1 引用（`"B7"`），`sheet` 默认第一个工作表，`value` 可为字符串、数字、布尔、`Date` 或 `null`（清空） |
| `output` | `string` | 否 | – | 由原生层直接写文件，而非返回 Buffer |
| `compression` | `number` | 否 | `6` | 0（仅存储）.. 9（最大压缩） |

**返回** `Promise<Buffer>` —— 修补后的工作簿；传 `output` 时为
`Promise<{ bytes, cells }>`。

**示例**

```javascript
const patched = await updateCells({
  sourceFile: 'book.xlsx',
  updates: [
    { cell: 'B7', value: 1234.5, numberFormat: '#,##0.00' },
    { cell: 'C7', value: '文本' },
    { sheet: 'Summary', cell: 'A1', value: new Date(2026, 0, 15) }
  ]
});
```

### renderTemplate(values, options?)

渲染模板工作簿：填充单元格文本中的标记，resolve 为成品工作簿。重复行是**复制模板行
的 XML**生成的，因此样式、数字格式、行高、合并单元格、条件格式都原样保留。只有文本
含标记的单元格会被重写（且保留其样式），不含标记的工作表整表按字节搬运。标记既可以
写在单元格里，也可以位于 `sharedStrings`，因此在 Excel 里手工制作的模板可以直接
使用。

单元格文本支持两种标记风格，按工作表自动识别：

**1. ejsExcel 语法** —— `<%...%>` 标记按真实 JavaScript 求值。`_data_` 即传入的
values；当它是数组时，`_data_[i]` 为第 i 个工作表的数据（按工作簿顺序）。每个含标记
的工作表都会被渲染。

| 标记 | 含义 |
|------|------|
| `<%=expr%>` | 把表达式的值作为单元格文本输出 |
| `<%~expr%>` | 输出数字 / `Date`，使单元格的数字格式生效（日期转为 Excel 序列值） |
| `<%#expr%>` | 动态公式：表达式求值为公式字符串（`"=SUM(A1,A2)"`）。配合 `<%~结果%>` 可同时写入预计算的缓存值——这正是 WPS 打开时公式显示 0、需双击才重算的解法 |
| `<%forRow item,i in expr%>` | 标记所在行按 `expr` 逐项重复；循环变量（`item`、`i`）在整行范围内可用 |
| `<%forRBegin item,i in expr%>` … `<%forREnd%>` | 两个标记之间的行按 `expr` 逐项重复 |
| `<%forCell key in expr%>` | 标记所在单元格横向重复，每项一次 |
| `<%ifCBegin cond%>` … `<%ifCEnd%>` | 两者之间的行仅在 `cond` 为真时输出 |
| `_row` / `_col` / `_rc` | 当前输出行号（`12`）、列标（`F`）、单元格名（`F12`） |
| `_charPlus_(col, n)` / `_charToNum_(col)` | 列运算：`"F"+3 → "I"`、`"F" → 6` |
| `_mergeCellFn_(range)` | 合并单元格，如 `_mergeCellFn_("C"+_row+":E"+_row)` |
| `_outlineLevel_(n)` | 给当前输出行设置分组层级 |
| `_dataValidation_({sqref, formula1})` | 给指定区域提供下拉校验 |
| `_img_({imgPh, cellNumAdd, rowNumAdd})` | 在当前单元格插入图片。`imgPh` 支持 **http(s) 链接、`Buffer`、base64 字符串、data: URI 或文件路径**；锚点横跨 `cellNumAdd` 列 × `rowNumAdd` 行。要求模板中至少已有一张图片（在其 drawing 结构上扩展） |
| `_qrcode_({text, size, cellNumAdd, rowNumAdd})` | 为 `text` 生成二维码（需 `npm install qrcode`） |

**2. 原生标记** —— 在工作线程上渲染，无需 JS 求值：

| 标记 | 含义 |
|------|------|
| `${path}` | 取 `path` 处的值（`${user.name}`、`${items.0.amount}`）。优先在当前 `{{#each}}` 项内查找；`../name` 跳出循环，`${@index}` 为循环下标。未知标记抛 `TEMPLATE_ERROR`（`strict: false` 则写空串） |
| `{{#each path}}` … `{{/each}}` | 两个标记之间的行按 `path` 数组逐项重复 |

| 参数 | 类型 | 是否必传 | 默认值 | 说明 |
|------|------|----------|--------|------|
| `values` | `object \| array` | **必传** | – | 标记取值；`<%...%>` 语法下同时充当 `_data_`（数组 = 每个 sheet 的数据） |
| `options` | `object` | **必传** | – | 见下表 |

**选项**

| 选项 | 类型 | 是否必传 | 默认值 | 说明 |
|------|------|----------|--------|------|
| `template` | `string \| Buffer` | **必传** | – | 模板工作簿：路径或字节 |
| `sheetName` | `string` | 否 | 全部 sheet | 只渲染该工作表 |
| `strict` | `boolean` | 否 | `true` | 仅原生 `${...}` 标记：未知标记时抛错而不是写空串 |
| `cache` | `boolean` | 否 | `false` | 把解析后的模板结构留在进程内有界缓存中（重复渲染完全跳过模板读取与扫描；模板被改写会被自动识别） |
| `output` | `string` | 否 | – | 直接写文件，而非返回 Buffer |
| `compression` | `number` | 否 | `6` | 0（仅存储）.. 9（最大压缩） |

**返回** `Promise<Buffer>` —— 渲染后的工作簿；传 `output` 时为
`Promise<{ bytes, sheets }>`（`sheets` 列出被渲染的工作表名）。

**示例**

```javascript
// 模板单元格："报表 ${title}"                       （原生标记）
//                "<%forRow it,i in _data_.items%>" / "<%=it.name%>" / "<%~it.qty%>"
//                "<%#\"=SUM(C2:C99)\"%><%~99%>"     （动态公式 + 缓存值）
//                "<%_img_({imgPh:\"https://cdn.example.com/logo.png\", cellNumAdd:3})%>"
const buffer = await renderTemplate(
  { title: '一季度', items: [{ name: '甲', qty: 1 }, { name: '乙', qty: 2 }] },
  { template: 'report-template.xlsx' }
);

// 报表服务反复渲染同一个模板：
const filled = await renderTemplate(values, { template, cache: true });
```

`forRBegin` / `ifCBegin` 未闭合，或标记表达式求值出错，都会抛出带工作表名与单元格名
的 `TEMPLATE_ERROR`。

### 读取引擎与类型化读取

默认（`engine: 'xlnt'`）会先把整个工作簿解析成模型，再从模型里读目标工作表。
`engine: 'xml'` 跳过模型：直接从压缩包里读目标工作表（共享字符串、样式表、工作表
XML），并在扫描过程中完成列投影、上限截断与表头解析。两个引擎返回的行**完全一致**；
遇到直读器未建模的内容会自动回退到 `'xlnt'`。

传 `values: 'typed'`（需 `engine: 'xml'`）时，读取器返回真实的 JavaScript 值而不是
格式化字符串：

```javascript
const rows = await readTableAsJSON('big.xlsx', {
  engine: 'xml', values: 'typed', columns: ['金额', '日期']
});
// => [ { 金额: 1200, 日期: Date 2026-01-15 }, ... ]
```

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
4. 读取本身就在 libuv 线程池执行，多次读取可并行
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
