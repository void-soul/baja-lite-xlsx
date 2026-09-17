# Performance Roadmap — baja-lite-xlsx

Goal: make the reader viable for **1M+ row / hundreds of MB** workbooks while
keeping the public surface exactly as it is today:

```js
readTableAsJSON(input, options)          // sync
readTableAsJSONAsync(input, options)     // async
```

Hard rule: **no new exported functions.** Every capability is either an
internal optimization or a new `options` field. Error codes, `includeWarnings`
semantics and the sync/async pair stay untouched.

## Where time goes today (measured by code path, not by profiler)

| Id | Bottleneck | Location |
|----|------------|----------|
| B1 | Every worksheet is materialized; JS picks one afterwards | `readSheetData` looped `for (auto ws : workbook_)` |
| B2 | Dense `rowCap x colCap` scan calling `worksheet::cell()`, which **creates and keeps** an empty cell for every empty coordinate | per-cell loop |
| B3 | Image pipeline runs unconditionally: second full ZIP open + all `xl/media/*` decompressed | `extractFromXlsx` call |
| B4 | Buffer / base64 input is written to `os.tmpdir()` and read back | `index.js` temp-file path |
| B5 | C++ sheet data and JS result are both fully resident at once | `sheetsToArray` |
| B6 | Every numeric cell pays 1-3 `snprintf` + `strtod` round-trip checks | `formatDouble` |

## Design

### P0 — zero API change (implemented first)

- **P0-1 single-sheet read.** `sheetName` is pushed into the native layer;
  `sheet_by_title()` / `sheet_by_index(0)` is used and only that sheet is ever
  materialized. Not-found is reported as `SHEET_NOT_FOUND|...` from C++.
- **P0-2 no empty-cell materialization.** Every coordinate is probed with
  `worksheet::has_cell()` before `worksheet::cell()`. `has_cell` is `const` and
  allocates nothing, so sparse sheets no longer grow the cell map.
- **P0-3 images on demand.** `zipio::packageHasMedia()` scans the ZIP central
  directory (no decompression) for `xl/media/`, `xl/drawings/`,
  `xl/cellimages/`. Without such entries the whole image pipeline is skipped.
  `options.includeImages: false` skips it unconditionally.
- **P0-4 in-memory input.** Buffer / base64 is handed to the addon as bytes and
  loaded through `workbook::load(const std::vector<uint8_t>&)` plus a libzip
  `zip_source_buffer_create()` source — no temp file at all.
- **P0-5 single materialization.** `sheetsToArray` moves each row out of the
  C++ vector while converting it, so C++ memory is released row by row.

### P1 — additive option fields

- **P1-1 string interning** (deferred): return a unique-string pool plus integer
  cell indices instead of per-cell JS strings, so low-cardinality columns
  allocate one V8 string each. Requires a native/JS contract change, so it is
  delivered only after P0 is green.
- **P1-2 `options.columns`**: projection resolved inside C++ (header text or
  `A` / `C:E` references), so unrequested columns are never read. Resolved
  headers travel back on `sheet.headers`.
- **P1-3 numeric fast path**: integral doubles below 1e15 skip `snprintf("%g")`
  and format as integers.

### P2 — very large files

- **P2-1 `options.onBatch` + `batchSize`**: constant memory. Sync path calls the
  callback directly; async path pushes batches through a
  `Napi::ThreadSafeFunction` from the `AsyncWorker` thread.

### P3 — beyond xlnt (planned, not implemented)

xlnt always parses every sheet, the full stylesheet and every shared string. For
100M+ row files the endgame is a SAX reader over `xl/worksheets/sheetN.xml` +
`sharedStrings.xml` only. `xml_parsers.cpp` already provides the attribute-level
parsing primitives to build on.

## Error handling

Every new option is validated in JS first (`INVALID_OPTIONS`); native failures
keep the existing `CODE|message` shape so `err.code` consumers are unaffected.
Unresolvable column names produce warnings; if none resolve at all the read
fails with `INVALID_OPTIONS`.

## Testing

`test/test.js` gains cases for: column projection (value equality with the
unprojected read), `includeImages: false` (same rows, no image objects),
single-sheet pushdown, and Buffer input equivalence. CI runs the suite on all
five prebuild platforms, so every optimization is verified where it ships.
