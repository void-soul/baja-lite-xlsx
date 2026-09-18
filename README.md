# Baja-Lite-XLSX

[![npm version](https://img.shields.io/npm/v/baja-lite-xlsx.svg)](https://www.npmjs.com/package/baja-lite-xlsx)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

**English** | [简体中文](./README-CN.md)

High-performance Node.js native module (N-API) for reading Excel `.xlsx` tables and
extracting embedded images, built on [xlnt](https://github.com/tfussell/xlnt) and libzip.

## Features

- Read a sheet as JSON rows with a single call: `readTableAsJSON` / `readTableAsJSONAsync`
- Input as file path, `Buffer`, or base64 string
- Image extraction: floating images (`twoCellAnchor`), embedded images
  (`oneCellAnchor`) and WPS `DISPIMG` / `cellimages.xml` images
- Robust against real-world files: WPS workbooks with proprietary relationship
  types or backslash ZIP entry names are loaded through a sanitized copy
- Non-blocking async variant running on the libuv thread pool
- Read caps (`maxRows` / `maxCols`) and per-entry size limits against hostile files
- Coded errors (`err.code`) plus non-fatal diagnostics (`warnings`)
- Prebuilt binaries for Windows / Linux / macOS as **N-API v8**: one binary per
  platform+architecture covers every Node.js >= 16 and every Electron version

> Every cell value is returned as a **string** (numbers and dates included;
> dates are formatted as `YYYY-MM-DD[ HH:MM:SS]`). Cells containing pictures
> return image objects instead.

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
const { readTableAsJSON, readTableAsJSONAsync } = require('baja-lite-xlsx');
const fs = require('fs');

// From a file path
// => [ { fullName: 'Ada',  age: '36' },
//      { fullName: 'Alan', age: '41' } ]
const rows = readTableAsJSON('data.xlsx', {
  sheetName: 'Sheet1',                                  // default: first sheet
  headerRow: 0,                                         // header row (0-based)
  skipRows: [1, 2],                                     // rows to skip
  headerMap: { 'name': 'fullName', 'age': 'age' }       // rename headers
});

// From a Buffer -- identical result shape
const rows2 = readTableAsJSON(fs.readFileSync('data.xlsx'));

// From base64 (pass the option explicitly; the heuristic requires ZIP magic)
const rows3 = readTableAsJSON(base64String, { inputEncoding: 'base64' });

// Non-fatal diagnostics
// => { rows: [ { ... } ], warnings: [ "Sheet 'Sheet1' truncated to 100000 of 500000 rows (maxRows)" ] }
const { rows: rows4, warnings } = readTableAsJSON('data.xlsx', { includeWarnings: true });

// Only the columns you need: header text or Excel reference ("B", "C:E").
// Unrequested columns are never read, so this is also the fast path.
// => [ { Amount: '1200' }, { Amount: '980' } ]
const amounts = readTableAsJSON('big.xlsx', { columns: ['Amount'] });

// Async: parsing runs off the event loop, same return shape
const rows5 = await readTableAsJSONAsync('big.xlsx', { maxRows: 100000 });

// Skip the image pipeline entirely when you only need values:
// no second pass over the archive, no media decompression.
const rows6 = await readTableAsJSONAsync('big.xlsx', { includeImages: false });

// Million-row sheet: stream it in batches, memory stays flat.
// => { rowCount: 1250000, warnings: [] }
const { rowCount } = await readTableAsJSONAsync('huge.xlsx', {
  batchSize: 50000,
  onBatch(rows, meta) {
    // rows: up to 50000 row objects; meta: { startIndex, count }
    writeToDatabase(rows);
  }
});
```

## API

### readTableAsJSON(input, options?)

Synchronous. Blocks the calling thread while parsing — prefer
`readTableAsJSONAsync` for large files, servers, or Electron UIs.

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `sheetName` | `string` | first sheet | Sheet to read |
| `headerRow` | `number` | `0` | Header row index (0-based, non-negative) |
| `skipRows` | `number[]` | `[]` | Row indices to skip (0-based) |
| `headerMap` | `Record<string,string>` | `{}` | Rename headers in the output objects |
| `inputEncoding` | `'base64'` | – | Force base64 interpretation of string input |
| `maxRows` | `number` | `0` | Cap on rows read per sheet (0 = no cap); excess rows are truncated and reported via `warnings` |
| `maxCols` | `number` | `0` | Cap on columns read per sheet (0 = no cap) |
| `columns` | `string[]` | `[]` | Read only these columns: header texts (`"Amount"`) or Excel references (`"B"`, `"C:E"`); empty = all columns |
| `includeImages` | `boolean` | `true` | `false` skips the whole image pipeline (no second archive pass, no media decompression) |
| `includeWarnings` | `boolean` | `false` | Return `{ rows, warnings }` instead of the rows array |

**Returns** `Array<Object>` — one plain object per data row, keyed by the
header texts after `headerMap` is applied. Every value is a string; a cell
holding a picture contains an `ImageDataObject` instead:

```javascript
// data.xlsx: | name | age | photo |
[
  { name: 'Ada', age: '36', photo: { data: <Buffer ...>, name: 'image1.png', type: 'image/png' } },
  { name: 'Alan', age: '41', photo: { data: <Buffer ...>, name: 'image2.png', type: 'image/png' } }
]
```

With `includeWarnings: true` the return value changes shape:

```javascript
{
  rows: [ { name: 'Ada', age: '36' } ],
  warnings: [ "Sheet 'Sheet1' truncated to 2 of 900 rows (maxRows)" ]
}
```

When several images attach to one cell the value is an array of
`ImageDataObject`. Cells outside the requested `columns` are not returned at
all.

### readTableAsJSONAsync(input, options?)

Same options and result, returns a `Promise`. Parsing runs on the libuv thread
pool, so the event loop (and any Electron UI) stays responsive.

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

### Create a workbook from JSON

```javascript
const { writeTableAsJSON, updateCells } = require('baja-lite-xlsx');

// => Buffer
const bytes = writeTableAsJSON(rows, { sheetName: 'Data' });
fs.writeFileSync('out.xlsx', bytes);

// Or let the native layer write the file: => { bytes, rowCount, sheetName }
writeTableAsJSON(rows, {
  output: 'out.xlsx',
  columns: { amount: { header: 'Amount', numberFormat: '#,##0.00', width: 14 } }
});
```

Numbers stay numbers, booleans stay booleans and `Date` values become real Excel
dates with an automatic date format, so everything stays computable in Excel
instead of turning into text.

### Write into an existing workbook

```javascript
// Replace the target sheet's data and keep the rest of the workbook untouched:
// other sheets, images, themes and styles are copied byte for byte.
const buffer = writeTableAsJSON(rows, { template: 'template.xlsx' });

// Rewrite individual cells: => Buffer, or { bytes, cells } when output is given
const patched = updateCells({
  template: 'book.xlsx',
  updates: [
    { cell: 'B7', value: 1234.5, numberFormat: '#,##0.00' },
    { cell: 'C7', value: 'text' },
    { sheet: 'Summary', cell: 'A1', value: new Date(2026, 0, 15) }
  ]
});
```

`updateCells` patches the worksheet in place: a cell keeps its existing style
unless the update supplies a `numberFormat`, formulas in a patched cell are
dropped rather than left stale, and missing cells or rows are inserted in the
right order. Everything not listed stays byte-identical.

### Templates

```javascript
const { renderTemplate } = require('baja-lite-xlsx');

// Template cells: "Report ${title}", "{{#each items}}" / "{{/each}}", "${name}"
const buffer = renderTemplate(
  { title: 'Q1', items: [{ name: 'a', amount: 1 }, { name: 'b', amount: 2 }] },
  { template: 'report-template.xlsx' }
);
```

Two markers are understood inside cell text:

| Marker | Meaning |
|--------|---------|
| `${path}` | The value at `path` (`${user.name}`, `${items.0.amount}`). Resolved against the current `{{#each}}` item first, then from the root; `../name` steps out of a loop and `${@index}` is the 0-based loop index. |
| `{{#each path}}` … `{{/each}}` | The rows between the markers repeat once per item of the array at `path`. |

Repeated rows are copies of the template row's XML, so styles, number formats,
row heights, merged cells and conditional formats survive untouched. Only cells
whose text contains a marker are rewritten; a cell keeps its style, and every
sheet without markers is copied byte for byte.

A marker may live in the cell itself or in `sharedStrings` — the way Excel
stores cell text — so templates authored in Excel work unchanged.

A row holding nothing but the marker delimits the block; a marker row that also
carries data cells is the first repeated row. A marker with no matching value
throws `TEMPLATE_ERROR` (pass `strict: false` to write an empty string instead),
and an unclosed `{{#each}}` always throws.

#### Reusing a template: `cache: true`

```javascript
// A report server rendering the same template over and over
const filled = await renderTemplateAsync(values, { template, cache: true });
```

The parsed template structure — row layout, marker positions and the shared
string table — is kept in a bounded in-process cache (8 templates / 64 MB, LRU),
so a repeat render skips reading and scanning the template altogether. Entries
are keyed by the template's identity (its zip central directory: entry names,
sizes and CRCs) plus the sheet filter, so a rewritten template is always picked
up. Default: `false`.

### Asynchronous writes

Every write has an async twin that resolves to the same value:

```javascript
const {
  writeTableAsJSONAsync, updateCellsAsync, renderTemplateAsync
} = require('baja-lite-xlsx');

await writeTableAsJSONAsync(rows, { output: 'report.xlsx' }); // { bytes, rowCount, sheetName }
const patched = await updateCellsAsync({ template, updates });
const filled = await renderTemplateAsync(values, { template });
```

The rows / updates / values are copied into a compact native snapshot on the
calling thread — the data lives in JS, so that step cannot move off it — and
everything expensive (worksheet XML, deflate, package assembly, the file write)
then runs on the libuv thread pool. A server or an Electron main process keeps
serving requests while a large workbook is produced, and failures reject with
the same `code` the synchronous call throws.

### Streaming writes

`rows` does not have to be an array. Any iterable — and any async iterable for
the async call — is written batch by batch, which is what a database cursor, a
generator or a file parser needs:

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

Rows are folded into the same compact native snapshot the async path uses
(16 bytes per cell, identical strings interned once), 20000 rows at a time, so
peak memory is one batch plus that snapshot rather than the whole table as JS
objects. The synchronous `writeTableAsJSON` accepts iterables too, but drains
them on the calling thread. `options.columns` is required either way, because it
cannot be inferred from a first row.

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
4. Use `readTableAsJSONAsync` in servers and Electron; several reads then run
   in parallel on the libuv thread pool (`UV_THREADPOOL_SIZE` controls it).
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
