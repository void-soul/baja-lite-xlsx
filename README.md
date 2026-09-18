# Baja-Lite-XLSX

[![npm version](https://img.shields.io/npm/v/baja-lite-xlsx.svg)](https://www.npmjs.com/package/baja-lite-xlsx)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

**English** | [简体中文](./README-CN.md)

High-performance Node.js native module (N-API) for reading Excel `.xlsx` tables and
extracting embedded images, built on [xlnt](https://github.com/tfussell/xlnt) and libzip.

## Features

- **Everything is async.** Four functions, all returning Promises; heavy work
  (parsing, XML generation, deflate, package assembly, file writes) runs on the
  libuv thread pool, so servers and Electron UIs keep responding
- Read a sheet as JSON rows: `readTableAsJSON`
- Write JSON into a workbook: `writeTableAsJSON` — appends to any sheet and
  builds multi-sheet workbooks by chaining on the returned `Buffer`
- Patch individual cells: `updateCells`
- Render templates with ejsExcel-style `<%...%>` markers (loops, dynamic
  formulas, merges, images, QR codes) or simple native `${...}` markers:
  `renderTemplate`
- Input and output as file path or `Buffer` — write results chain directly into
  the next call as `sourceFile`
- Image extraction on read: floating images (`twoCellAnchor`), embedded images
  (`oneCellAnchor`) and WPS `DISPIMG` / `cellimages.xml` images
- Typed reads (`values: 'typed'`): numbers, booleans and dates as real JS values
- Robust against real-world files: WPS workbooks with proprietary relationship
  types or backslash ZIP entry names are loaded through a sanitized copy
- Read caps (`maxRows` / `maxCols`) and per-entry size limits against hostile files
- Coded errors (`err.code`) plus non-fatal diagnostics (`warnings`)
- Prebuilt binaries for Windows / Linux / macOS as **N-API v8**: one binary per
  platform+architecture covers every Node.js >= 16 and every Electron version

> By default every cell value is returned as a **string** (numbers and dates
> included; dates are formatted as `YYYY-MM-DD[ HH:MM:SS]`). Cells containing
> pictures return image objects instead; pass `values: 'typed'` for real types.

## Install

```bash
npm install baja-lite-xlsx
```

The matching prebuilt binary is downloaded during install. When no prebuild is
available for your platform, the package compiles from source (see
[Building from source](#building-from-source)); on Windows the prebuild ships
the required runtime DLLs.

## Quick start

```javascript
const { readTableAsJSON, writeTableAsJSON, updateCells, renderTemplate } =
  require('baja-lite-xlsx');

// Read: => [ { fullName: 'Ada', age: '36' }, { fullName: 'Alan', age: '41' } ]
const rows = await readTableAsJSON('data.xlsx', {
  sheetName: 'Sheet1',                                // default: first sheet
  columns: ['Amount'],                                // only these columns are read
  values: 'typed',                                    // engine: 'xml' -> real types
  engine: 'xml'                                       // direct reader: fastest on big files
});

// Write: => Buffer
const bytes = await writeTableAsJSON(rows2, { sheetName: 'Data' });

// Chain: append more rows to the same sheet, then add a second sheet.
// Everything after `sourceFile` keeps the rest of the workbook untouched.
const withMore = await writeTableAsJSON(rows3, { sourceFile: bytes, sheetName: 'Data' });
const multiSheet = await writeTableAsJSON(summaryRows, { sourceFile: withMore, sheetName: 'Summary' });

// Patch individual cells
const patched = await updateCells({
  sourceFile: multiSheet,
  updates: [{ cell: 'B7', value: 1234.5, numberFormat: '#,##0.00' }]
});

// Render a template (ejsExcel syntax in the cells)
const report = await renderTemplate(
  { title: 'Q1', items: [{ name: 'a', amount: 1 }, { name: 'b', amount: 2 }] },
  { template: 'report-template.xlsx' }
);

// Write any of the results to disk
await writeTableAsJSON(rows2, { output: 'out.xlsx' }); // => { bytes, rowCount, sheetName }
```

## API

### readTableAsJSON(input, options?)

Reads one worksheet and resolves to a JSON array with one object per data row.
Parsing runs on the libuv thread pool, so the event loop (and any Electron UI)
stays responsive. Invalid arguments throw synchronously; parse failures reject.

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `input` | `string \| Buffer` | **yes** | – | File path, workbook bytes, or base64 string |
| `options` | `object` | no | `{}` | Options, see the table below |

**Options**

| Option | Type | Required | Default | Description |
|--------|------|----------|---------|-------------|
| `sheetName` | `string` | no | first sheet | Sheet to read |
| `headerRow` | `number` | no | `0` | Header row index (0-based) |
| `skipRows` | `number[]` | no | `[]` | Row indices to skip (0-based) |
| `headerMap` | `object` | no | `{}` | Rename headers: `{ original: 'renamed' }` |
| `inputEncoding` | `'base64'` | no | – | Force base64 interpretation of string input |
| `maxRows` | `number` | no | `0` | Cap on rows (0 = no cap); truncation is reported via `warnings` |
| `maxCols` | `number` | no | `0` | Cap on columns (0 = no cap) |
| `columns` | `string[]` | no | `[]` | Read only these columns: header texts (`"Amount"`) or Excel references (`"B"`, `"C:E"`); unrequested columns are never read |
| `engine` | `'xlnt' \| 'xml'` | no | `'xlnt'` | `'xml'` reads the sheet straight out of the package (much faster on projected/streamed reads); falls back automatically when a file needs the full model |
| `values` | `'string' \| 'typed'` | no | `'string'` | `'typed'` returns numbers, booleans and `Date` objects instead of strings (requires `engine: 'xml'`) |
| `includeImages` | `boolean` | no | `true` | `false` skips the whole image pipeline |
| `includeWarnings` | `boolean` | no | `false` | Resolve to `{ rows, warnings }` instead of the rows array |
| `onBatch` | `function` | no | – | Stream rows in batches; resolves to `{ rowCount, warnings }` instead |
| `batchSize` | `number` | no | `50000` | Rows per batch when `onBatch` is used |

**Returns** `Promise<Array<Object>>` — one object per data row, keyed by the
header texts after `headerMap`. Every value is a `string`; a cell holding a
picture contains an image object instead. With `values: 'typed'` the values are
real numbers, booleans and `Date`s.

**Example**

```javascript
const rows = await readTableAsJSON('data.xlsx', { sheetName: 'Sheet1' });
// => [ { name: 'Ada', age: '36' }, { name: 'Alan', age: '41' } ]

// Typed values: amount is a number, when a Date
const typed = await readTableAsJSON('big.xlsx', { engine: 'xml', values: 'typed' });

// Stream a million rows with flat memory
const { rowCount } = await readTableAsJSON('huge.xlsx', {
  batchSize: 50000,
  onBatch(batch, meta) { writeToDatabase(batch); }   // meta: { startIndex, count }
});
```

### Image attachment rules

- **WPS embedded images** (`=DISPIMG("ID_...")` formulas): attached by image ID.
- **Embedded images** (standard Excel, `oneCellAnchor`): attached to the anchor cell.
- **Floating images** (`twoCellAnchor`): attached to the top-left cell of the anchor range.

Unresolvable cases (missing media, unknown relationship ids, drawings that
cannot be mapped to a sheet) never fail the read; they appear in `warnings`
when `includeWarnings` is enabled.

### Errors

Every thrown error carries a machine-readable `code`:

| Code | Meaning |
|------|---------|
| `ADDON_LOAD_FAILED` / `ADDON_NOT_FOUND` | Native binary missing or its runtime DLLs are missing |
| `FILE_NOT_FOUND` | Input path does not exist |
| `FILE_OPEN_FAILED` | File exists but could not be parsed as a workbook |
| `INVALID_INPUT` | Unsupported input type / base64 without ZIP magic |
| `INVALID_OPTIONS` | Options failed validation (e.g. negative `headerRow`) |
| `NO_SHEETS` | Workbook has no sheets |
| `SHEET_NOT_FOUND` | `options.sheetName` matches no sheet |
| `HEADER_ROW_OUT_OF_RANGE` | `headerRow` beyond the available rows |
| `PARSE_ERROR` / `READ_FAILED` | Workbook parsing failed |

### Resource limits

- ZIP entries without a declared size, or above 256 MB, are rejected; media
  entries are capped at 128 MB (protection against zip bombs).
- `maxRows` / `maxCols` limit how much of a sheet is materialized.
- Truncation is always reported as a warning, never silently applied.

## Writing

### writeTableAsJSON(rows, options?)

Writes rows into a worksheet and resolves to the `.xlsx` bytes — or to a summary
when `output` is given. Numbers stay numbers, booleans stay booleans and `Date`
values become real Excel dates, so everything stays computable in Excel.

With `sourceFile` (a path or a `Buffer` returned by any write call) the rows are
**appended** to the named sheet — creating the sheet when it does not exist yet,
which is how multi-sheet workbooks are built by chaining calls on the returned
`Buffer`. With `append: false` the sheet's data is replaced instead; the rest of
the workbook (other sheets, images, styles) is copied byte for byte either way.

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `rows` | `Array \| Iterable \| AsyncIterable` | **yes** | – | Data rows: objects, arrays, or any (async) iterable such as a generator or database cursor |
| `options` | `object` | no | `{}` | Options, see the table below |

**Options**

| Option | Type | Required | Default | Description |
|--------|------|----------|---------|-------------|
| `sheetName` | `string` | no | `'Sheet1'` | Worksheet name; with `sourceFile`, the sheet to append to (created when missing) |
| `sourceFile` | `string \| Buffer` | no | – | Workbook to write into: a path, or the `Buffer` from a previous write / update / render call |
| `append` | `boolean` | no | `true` | Append after the sheet's last row; `false` replaces the sheet's data (only meaningful with `sourceFile`) |
| `includeHeader` | `boolean` | no | `true`; with `sourceFile`: auto | Write a header row; "auto" writes it only when the sheet was created or had no rows, so chained appends never duplicate it |
| `freezeHeader` | `boolean` | no | `false` | Freeze the header row (fresh sheets) |
| `columns` | `object \| array` | no | from the first row | Column spec: `{ prop: { header, numberFormat, align, width } }` or `[{ key \| index, header, ... }]`. **Required** when `rows` is an iterable (it cannot be inferred) |
| `output` | `string` | no | – | Write the file natively instead of returning a Buffer |
| `compression` | `number` | no | `6` | 0 (store) .. 9 (maximum) |

**Returns** `Promise<Buffer>` — the finished workbook; or
`Promise<{ bytes, rowCount, sheetName }>` when `output` is given.

**Example**

```javascript
// New workbook from rows: => Buffer
const bytes = await writeTableAsJSON(rows, { sheetName: 'Data' });

// Append more rows to the same sheet (header is not duplicated), then add a
// second sheet — multi-sheet workbooks by chaining on the returned Buffer:
const more = await writeTableAsJSON(rows2, { sourceFile: bytes, sheetName: 'Data' });
const multi = await writeTableAsJSON(summary, { sourceFile: more, sheetName: 'Summary' });

// Replace a sheet's data instead of appending
const replaced = await writeTableAsJSON(rows3, {
  sourceFile: multi, sheetName: 'Data', append: false
});

// Stream from a database cursor with flat memory (columns are required)
await writeTableAsJSON(fromDatabase(), {
  sheetName: 'Report',
  columns: { id: {}, name: {}, amount: { numberFormat: '#,##0.00' } },
  output: 'report.xlsx'   // => { bytes, rowCount, sheetName }
});
```

### updateCells(options)

Rewrites individual cells of an existing workbook. Only the affected worksheets
are regenerated; every other part of the package is copied byte for byte, so
untouched sheets, images and styles survive exactly as they were. A cell keeps
its existing style unless the update supplies a `numberFormat`, formulas in a
patched cell are dropped rather than left stale, and missing cells or rows are
inserted in the right order.

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `options` | `object` | **yes** | – | See the table below |

**Options**

| Option | Type | Required | Default | Description |
|--------|------|----------|---------|-------------|
| `sourceFile` | `string \| Buffer` | **yes** | – | Workbook to patch: a path, or the `Buffer` from a previous write / update / render call |
| `updates` | `array` | **yes** | – | `{ sheet?, cell, value?, numberFormat? }` entries; `cell` is an A1 reference (`"B7"`), `sheet` defaults to the first sheet, `value` may be a string, number, boolean, `Date` or `null` to clear |
| `output` | `string` | no | – | Write the file natively instead of returning a Buffer |
| `compression` | `number` | no | `6` | 0 (store) .. 9 (maximum) |

**Returns** `Promise<Buffer>` — the patched workbook; or
`Promise<{ bytes, cells }>` when `output` is given.

**Example**

```javascript
const patched = await updateCells({
  sourceFile: 'book.xlsx',
  updates: [
    { cell: 'B7', value: 1234.5, numberFormat: '#,##0.00' },
    { cell: 'C7', value: 'text' },
    { sheet: 'Summary', cell: 'A1', value: new Date(2026, 0, 15) }
  ]
});
```

### renderTemplate(values, options?)

Renders a template workbook: fills markers in cell text and resolves to the
finished workbook. Repeated rows are copies of the template row's XML, so
styles, number formats, row heights, merged cells and conditional formats
survive untouched. Cells without markers, and every sheet without markers, are
copied byte for byte. A marker may live in the cell itself or in
`sharedStrings`, so templates authored in Excel work unchanged.

Two marker styles are understood, selected per sheet:

**1. ejsExcel syntax** — `<%...%>` markers evaluated as JavaScript. `_data_` is
the values argument; when it is an array, `_data_[i]` is sheet *i*'s data
(workbook order). Every sheet that contains markers is rendered.

| Marker | Meaning |
|--------|---------|
| `<%=expr%>` | Emit the expression's value as cell text |
| `<%~expr%>` | Emit a number / `Date` so the cell's number format applies (dates become Excel serials) |
| `<%#expr%>` | Dynamic formula: the expression evaluates to a formula string (`"=SUM(A1,A2)"`). Pair with `<%~result%>` to also store the pre-computed value — that is what keeps WPS from showing 0 until recalculation |
| `<%forRow item,i in expr%>` | The row containing the marker repeats once per item of `expr`; the loop variables (`item`, `i`) are in scope for the whole row |
| `<%forRBegin item,i in expr%>` … `<%forREnd%>` | The rows between the markers repeat per item |
| `<%forCell key in expr%>` | The cell containing the marker repeats horizontally, once per item |
| `<%ifCBegin cond%>` … `<%ifCEnd%>` | The rows in between are emitted only when `cond` is truthy |
| `_row` / `_col` / `_rc` | Emitted row number (`12`), column letters (`F`), cell reference (`F12`) |
| `_charPlus_(col, n)` / `_charToNum_(col)` | Column arithmetic: `"F"+3 → "I"`, `"F" → 6` |
| `_mergeCellFn_(range)` | Merge cells, e.g. `_mergeCellFn_("C"+_row+":E"+_row)` |
| `_outlineLevel_(n)` | Row grouping level on the emitted row |
| `_dataValidation_({sqref, formula1})` | Dropdown validation for a cell range |
| `_img_({imgPh, cellNumAdd, rowNumAdd})` | Insert an image anchored at the current cell. `imgPh` accepts an **http(s) URL, a `Buffer`, a base64 string, a data: URI or a file path**; the anchor spans `cellNumAdd` columns × `rowNumAdd` rows. Requires the template to already contain at least one picture (its drawing structure is extended) |
| `_qrcode_({text, size, cellNumAdd, rowNumAdd})` | Insert a QR code for `text` (`npm install qrcode`) |

**2. Native markers** — rendered on the worker thread without JS evaluation:

| Marker | Meaning |
|--------|---------|
| `${path}` | The value at `path` (`${user.name}`, `${items.0.amount}`). Resolved against the current `{{#each}}` item first; `../name` steps out of a loop, `${@index}` is the loop index. Unknown markers throw `TEMPLATE_ERROR` (`strict: false` writes an empty string) |
| `{{#each path}}` … `{{/each}}` | The rows between the markers repeat once per item of the array at `path` |

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `values` | `object \| array` | **yes** | – | Marker values; with `<%...%>` also `_data_` (array = per-sheet data) |
| `options` | `object` | **yes** | – | See the table below |

**Options**

| Option | Type | Required | Default | Description |
|--------|------|----------|---------|-------------|
| `template` | `string \| Buffer` | **yes** | – | Template workbook: a path, or bytes |
| `sheetName` | `string` | no | all sheets | Render only this sheet |
| `strict` | `boolean` | no | `true` | Native `${...}` markers only: throw on unknown markers instead of writing an empty string |
| `cache` | `boolean` | no | `false` | Keep the parsed template structure in a bounded in-process cache (repeat renders skip reading and scanning the template; a rewritten template is detected automatically) |
| `output` | `string` | no | – | Write the file instead of returning a Buffer |
| `compression` | `number` | no | `6` | 0 (store) .. 9 (maximum) |

**Returns** `Promise<Buffer>` — the rendered workbook; or
`Promise<{ bytes, sheets }>` when `output` is given (`sheets` lists the
rendered worksheet names).

**Example**

```javascript
// Template cells:  "报告 ${title}"                                (native marker)
//                  "<%forRow it,i in _data_.items%>" / "<%=it.name%>" / "<%~it.qty%>"
//                  "<%#\"=SUM(C2:C99)\"%><%~99%>"                  (formula + cached value)
//                  "<%_img_({imgPh:\"https://cdn.example.com/logo.png\", cellNumAdd:3})%>"
const buffer = await renderTemplate(
  { title: 'Q1', items: [{ name: 'a', qty: 1 }, { name: 'b', qty: 2 }] },
  { template: 'report-template.xlsx' }
);

// A report server rendering the same template over and over:
const filled = await renderTemplate(values, { template, cache: true });
```

An unclosed `forRBegin` / `ifCBegin`, or an error inside a marker expression,
throws `TEMPLATE_ERROR` naming the sheet and cell.

### Reading engine and typed values

By default (`engine: 'xlnt'`) the whole workbook is parsed into a model and the
requested sheet is read from it. `engine: 'xml'` skips the model: the sheet is
read straight out of the package — shared strings, the style table, the sheet
XML — with projection, caps and header resolution applied while scanning. Both
engines return exactly the same rows, and anything the direct reader does not
model makes it fall back to `'xlnt'` automatically.

With `values: 'typed'` (requires `engine: 'xml'`) the reader returns real
JavaScript values instead of formatted strings:

```javascript
const rows = await readTableAsJSON('big.xlsx', {
  engine: 'xml', values: 'typed', columns: ['amount', 'when']
});
// => [ { amount: 1200, when: Date 2026-01-15 }, ... ]
```

## Performance

The reader only does work you asked for:

- **Single-sheet reads.** `sheetName` is resolved natively, so no other
  worksheet is ever materialized.
- **No empty-cell materialization.** Cells are probed before being touched, so
  sparse sheets no longer allocate a cell per empty coordinate.
- **Images on demand.** Workbooks without `xl/media`, `xl/drawings` or
  `xl/cellimages` parts skip the image pipeline entirely; `includeImages: false`
  skips it unconditionally.
- **Column projection.** `columns` narrows the read before any cell value is
  produced.

Practical guidance for large files:

1. Pass `columns` when you only need a few of many columns.
2. Pass `includeImages: false` when you only need values.
3. Use `maxRows` / `maxCols` to bound a probe read.
4. Reads already run on the libuv thread pool; several reads then run in
   parallel (`UV_THREADPOOL_SIZE` controls it).
5. Use `onBatch` + `batchSize` for sheets that should never be materialized.

### Streaming with `onBatch`

```javascript
const { rowCount, warnings } = readTableAsJSON('huge.xlsx', {
  batchSize: 50000,                 // rows per callback, default 50000
  onBatch(rows, meta) {             // meta: { startIndex, count }
    // `rows` is a normal array of row objects
    appendToCsv(rows);
  }
});
```

Rows are produced and handed over while the sheet is parsed, so peak memory is
one batch rather than the whole sheet. The return value changes to
`{ rowCount, warnings }` — rows are never accumulated. All other options
(`columns`, `includeImages`, `skipRows`, `headerMap`, ...) behave exactly as
they do in the buffered case. Images are attached per row, so streaming still
returns them.

### Benchmarking

```bash
npm run bench                     # examples/sample.xlsx
npm run bench -- huge.xlsx        # your own file
npm run bench -- huge.xlsx --iterations 3 --batch 20000
```

It reports best/median wall time, rows per second and retained memory for the
buffered, image-less, column-projected and streamed paths. On a large file the
streamed row stays flat in memory while the buffered rows grow with the sheet,
which is the fastest way to see what the options above buy.

## Building from source

All platforms use [vcpkg](https://github.com/microsoft/vcpkg) with the `xlnt`
and `libzip` ports and require the `VCPKG_ROOT` environment variable (the build
fails with an explicit error when it is missing):

```bash
# 1. Install dependencies (triplet per platform)
vcpkg install xlnt:x64-windows libzip:x64-windows   # Windows x64
vcpkg install xlnt:x64-linux   libzip:x64-linux     # Linux x64
vcpkg install xlnt:arm64-osx   libzip:arm64-osx     # macOS (Apple Silicon)

# 2. Point the build at vcpkg
#    Windows:  set VCPKG_ROOT=C:\vcpkg
#    bash:     export VCPKG_ROOT=~/vcpkg

# 3. Build (Windows also copies the runtime DLLs)
npm run build:dev     # or: npm run build

# 4. Run the tests
npm test
```

## CI & releases

- Push to `master` / pull requests: lint + typecheck, native build, `npm test`.
- Push a tag `v*` (must equal `package.json` `version`): the release workflow
  1. builds the N-API prebuild archives for **win32-x64, linux-x64,
     linux-arm64, darwin-arm64, darwin-x64** (Windows archives bundle the
     runtime DLLs),
  2. runs the tests and verifies every archive contains the `.node` module,
  3. publishes the archives plus `checksums.txt` to the matching GitHub Release,
  4. publishes the package to npm with provenance (`npm publish --provenance`).

Only the N-API v8 binary is produced per platform: the ABI is stable, so
per-Node/per-Electron-version builds are unnecessary.

### Releasing a new version

```bash
# 1. bump the version (must equal the future tag without the leading "v")
npm version patch --no-git-tag-version     # or minor / major
# 2. update CHANGELOG.md, commit in English, push
git commit -am "chore: release v1.0.17"
git push origin master
# 3. tag -> builds, GitHub Release, npm publish
git tag -a v1.0.17 -m "v1.0.17" && git push origin v1.0.17
```

### npm publishing setup (one-time)

The workflow uses npm **Trusted Publishing** (OIDC), so no `NPM_TOKEN` secret
is stored in the repository:

1. Open <https://www.npmjs.com/package/baja-lite-xlsx> → *Settings* →
   *Trusted Publisher* → *GitHub Actions*.
2. Fill in: Organization/user `void-soul`, Repository `baja-lite-xlsx`,
   Workflow filename `prebuild.yml`, Environment (leave empty).
3. Save. The next `v*` tag can publish without any token.

Prefer a token? Add a repository secret `NPM_TOKEN` and replace the publish
step with:

```yaml
      - run: npm publish --access public
        env:
          NODE_AUTH_TOKEN: ${{ secrets.NPM_TOKEN }}
```

The published tarball only contains `index.js`, `index.d.ts`, `src/`,
`scripts/`, `binding.gyp` and the docs (see `package.json` `files`): binaries
are never bundled into the npm package — they come from the GitHub Release,
keeping the tarball small and always matching the release assets.

## Troubleshooting (Windows)

If the module fails to load:

1. Install the [VC++ Redistributable](https://aka.ms/vs/17/release/vc_redist.x64.exe).
2. Copy the DLLs from `%VCPKG_ROOT%\installed\x64-windows\bin` next to
   `baja_xlsx.node` — `node scripts/package-dlls.js` does this for you.
3. Set `BAJA_XLSX_TEST_LOAD=1` and re-run the install step to see the loader error.

## Security notes

- The library parses untrusted files: keep it updated and use `maxRows` /
  `maxCols` when consuming third-party workbooks.
- Prebuild archives are hashed with SHA-256 (`npm run checksums` writes
  `prebuilds/checksums.txt`); verify hashes when consuming release artifacts.

## License

MIT — see [LICENSE](LICENSE).
