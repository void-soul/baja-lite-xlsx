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

## Status

| Item | What it does | Shipped in |
|------|--------------|-----------|
| P0-1 | Only the requested worksheet is materialized (`sheetName` resolved natively) | 1.0.17 |
| P0-2 | `worksheet::has_cell()` probe before `worksheet::cell()`, so empty coordinates no longer allocate cells | 1.0.17 |
| P0-3 | Image pipeline skipped when the package has no media parts, or with `includeImages: false` | 1.0.17 |
| P0-4 | Buffer / base64 parsed from memory (`workbook::load(vector<uint8_t>)` + libzip memory source); no temp file | 1.0.18 |
| P0-5 | Sheet data is released row by row while it is converted to JS | 1.0.17 |
| P1-1 | One V8 string per distinct value (string pool + index-encoded cells) | 1.0.19 |
| P1-2 | `columns` projection, resolved natively (header text or `A` / `C:E`) | 1.0.17 |
| P1-3 | Integral numbers skip the `%g` round-trip probing | 1.0.17 |
| P2-1 | `onBatch` + `batchSize` streaming, sync callback and async ThreadSafeFunction | 1.0.20 |
| P3 | SAX reader over the sheet XML (planned) | — |

## Where time goes (original bottlenecks)

| Id | Bottleneck | Location |
|----|------------|----------|
| B1 | Every worksheet was materialized; JS picked one afterwards | `readSheetData` looped over `workbook_` |
| B2 | Dense `rowCap x colCap` scan calling `worksheet::cell()`, which **creates and keeps** an empty cell for every empty coordinate | per-cell loop |
| B3 | Image pipeline ran unconditionally: second full ZIP open + all `xl/media/*` decompressed | `extractFromXlsx` call |
| B4 | Buffer / base64 input was written to `os.tmpdir()` and read back | `index.js` temp-file path |
| B5 | C++ sheet data and the JS result were both fully resident | `sheetsToArray` |
| B6 | Every numeric cell paid 1-3 `snprintf` + `strtod` round-trip checks | `formatDouble` |

## Design notes

- **Streaming pipeline.** Images are extracted *before* rows are read, which is
  what allows image attachment to happen per row (`attachRowImages`) instead of
  in a post-pass over materialized data. Buffered and streamed reads share one
  pipeline (`XlsxReader::runPipeline`); only the row sink differs, so there is
  no second copy of the sheet-reading logic.
- **Async streaming and lifetime.** `AsyncWorker` is destroyed immediately
  after `OnOK`, so a batch callback must never be able to fire later. Each
  batch is therefore a rendezvous: the worker thread calls `BlockingCall` and
  waits on a condition variable until the main thread has run the callback.
  That also bounds the queue (backpressure) instead of buffering rows ahead.
- **Streaming and the string pool.** Batches carry real values (strings / image
  objects) rather than pool indices: a pool would have to be retained for the
  whole read, which is exactly what streaming avoids. Buffered reads keep the
  pool.
- **Error handling.** Every new option is validated in JS (`INVALID_OPTIONS`)
  before the native call; native failures keep the `CODE|message` shape.
  Unresolvable column names produce warnings; if none resolve at all the read
  fails with `INVALID_OPTIONS`.

## P3 — SAX reader (planned, the remaining lever for 1M+ rows)

Even after P0-P2, xlnt still parses the whole package before any row is
produced: every sheet, the full stylesheet, and all shared strings with an
index. For a single huge sheet that dominates the runtime and sets a floor on
memory.

Plan:

1. **New component** `src/sax_reader.{h,cpp}` that reads only what the options
   ask for:
   - `xl/workbook.xml` + `xl/_rels/workbook.xml.rels` → sheet name → part path
     (already implemented for drawings in `image_extractor.cpp`, reuse it).
   - `xl/sharedStrings.xml` → string table (`<si>`/`<t>`, concatenating rich
     text runs).
   - `xl/worksheets/sheetN.xml` streamed with a small pull parser; only the
     requested columns are decoded, and rows are pushed to the existing
     `RowBatchSink` as they are parsed.
   - `xl/styles.xml` is *not* parsed for styling; only the `numFmtId` of the
     `cellXfs` entry a cell references is needed, and only for date detection.
2. **Parsing primitives** are already in `xml_parsers.cpp` (attribute lookup
   that tolerates quoting/whitespace); the sheet parser needs streaming variants
   that do not copy the whole part into a string, plus a helper for
   `<dimension>`/`<row>`/`<c r="B7" t="s">`.
3. **Opt-in first.** The SAX path is used only when it is known to be safe:
   `onBatch` / `maxRows` / `columns` reads with `includeImages: false` and a
   single sheet. Otherwise the xlnt path stays the default. `validateOptions`
   decides, so the public API does not change; a mismatch simply falls back.
4. **Correctness must be pinned first.** Dates (numFmtId → date detection),
   formula cells (`<f>` without cached `<v>`), inline strings, `xml:space`
   handling and merged/blank cells are the risk areas. The exit criterion is
   that the existing suite passes with the SAX path forced on, plus a
   differential test that compares SAX and xlnt output row by row on the
   fixtures.
5. **Expected effect**: no whole-package parse, no stylesheet parse, memory
   proportional to one batch. This is the only change that improves the
   *floor* rather than the constants.

## Benchmarking

`npm run bench [file] [--iterations N] [--batch N]` reports best/median wall
time, rows per second and retained memory for the buffered, image-less,
column-projected and streamed paths. Run it against a real 100k-1M row
workbook: the streamed row should show flat memory while buffered reads grow
with the sheet.
