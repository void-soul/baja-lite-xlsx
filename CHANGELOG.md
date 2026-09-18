# Changelog

## 1.7.0 (2026-09-18) — direct sheet reading

- New `engine: 'xml'` option for every read: the requested sheet is read straight
  out of the package (shared strings, style table, sheet XML) instead of building
  the whole workbook model first. Column projection, `maxRows` / `maxCols` and
  header resolution happen during the scan, and both buffered and streamed reads
  are supported.
- The direct reader produces exactly the same values as the xlnt path — same date
  and time detection from the number format code, same number formatting, same
  boolean and error text, same empty-cell and sparse-row handling. The test suite
  now asserts this by comparing both engines over every fixture and option set.
- Anything the direct reader does not model (ISO `t="d"` cells, out-of-order rows,
  unknown cell types, shared string indices out of range) hands the file back to
  the xlnt path, so a read can never get worse than before.
- `bench/` gained `engine: xml` scenarios.
- Internally: the number/date formatters and the A1 reference helpers moved into
  shared modules (`cell_format`, `a1_reference`), and the read-side types into
  `sheet_types.h`, so the direct reader carries no xlnt dependency.

## 1.6.0 (2026-09-18) — streaming writes

- `writeTableAsJSON` and `writeTableAsJSONAsync` now accept any iterable as
  `rows` (plus any async iterable for the async call). A table that comes from a
  cursor, a generator or a parser therefore never has to exist in JS as a whole:
  rows are folded into the native snapshot 20000 at a time and the package is
  built once, at the end. Peak memory is one batch plus the compact snapshot
  (16 bytes per cell, strings interned once) instead of the full table of JS
  objects. `options.columns` is required for an iterable, since it cannot be
  inferred from a first row.
- The async drain hands the event loop a turn between batches, so a long stream
  does not starve timers or sockets; the package is still assembled and written
  on the thread pool.

## 1.5.0 (2026-09-18) — template cache, Excel-authored templates

Fixed (both found by a new local harness that renders an Excel-style template,
see `docs/local/cachecheck.cpp`, and now covered in CI by
`test/fixtures/template-shared-strings.xlsx`):

- A template authored in Excel keeps its cell text in `sharedStrings`, so its
  sheet XML contains no marker at all. The "does this sheet need rendering?"
  check only looked at the sheet XML, so such a sheet was copied verbatim and
  every marker stayed in the output. The shared string table is now consulted
  too.
- Shared string references (`t="s"`) were decoded one byte too long, so the
  index never parsed and the cell text was never substituted.

New:

- `renderTemplate(values, { cache: true })`: the parsed template structure (row
  layout, marker positions, shared strings) stays in a bounded in-process LRU
  cache (8 entries / 64 MB), so a repeat render skips reading and scanning the
  template. Entries are keyed by the archive's central directory (entry names,
  sizes and CRCs) plus the sheet filter, so a rewritten template is always
  picked up; the cache is mutex-protected, so the async twin benefits as well.
- `bench/` now also measures the write side (`--rows` to size the workload):
  sync/async sheet writes, writing to a file, cell patching, and template
  rendering with and without the cache.

## 1.4.0 (2026-09-18) — asynchronous writes

- `writeTableAsJSONAsync`, `updateCellsAsync`, `renderTemplateAsync`: the JS
  input is copied into a compact native snapshot (identical strings interned
  once, 16 bytes per cell) and the whole build — worksheet XML, deflate, package
  assembly and the file write — then runs on the libuv thread pool, so the event
  loop keeps running. Same options, same results, same error codes.
- The sync and async entry points now share their C++ helpers (option parsing,
  update list, template spec, file write), so the two paths cannot drift apart.

## 1.3.0 (2026-09-18) — template rendering

New: `renderTemplate(values, options)` — fill a template workbook.

- `${path}` placeholders in cell text, resolved against the current
  `{{#each}}` item first then from the root; `../name` steps out of a loop and
  `${@index}` is the 0-based loop index.
- `{{#each path}}` / `{{/each}}` repeat the rows in between once per array item.
  A row holding only the marker delimits the block; a marker row that also has
  data cells is the first repeated row.
- Repeated rows are copies of the template row's XML, so styles, number
  formats, row heights, merged cells and conditional formats survive untouched.
  Only cells whose text contains a marker are rewritten, and sheets without
  markers are copied as compressed bytes.
- Values keep their type: numbers are written as numbers (so the template's
  number format applies) and `Date` values become real Excel dates. A marker
  with no value throws `TEMPLATE_ERROR` unless `strict: false` is passed.
- Package plumbing (part lookup, compressed-entry copy, assembly) is now shared
  by all three write modes instead of living inside the patching code.

## 1.2.0 (2026-09-18) — writing into existing workbooks

- `writeTableAsJSON(rows, { template })`: writes into an existing workbook,
  replacing the target sheet's data. Only that sheet (and `styles.xml` when new
  number formats are needed) is regenerated; every other entry is copied as
  compressed bytes, so other sheets, images, themes and document properties
  survive untouched.
- `updateCells({ template, updates, output })`: rewrites individual cells
  (`{ sheet?, cell, value?, numberFormat? }`). The sheet is patched in place —
  existing cell styles are kept, formulas in the patched cell are dropped
  rather than left stale, and missing cells/rows are inserted in the correct
  order. Untouched cells, other sheets and every other part stay byte-identical.
- Sheet part lookup resolves `workbook.xml` relationships to the real part and
  tolerates packages whose targets are relative in unusual ways.

## 1.1.0 (2026-09-18) — writing

New: `writeTableAsJSON(rows, options)` — a JSON array becomes a worksheet.

- Numbers are written as numbers, booleans as booleans and `Date` values as
  Excel dates (serial + automatic date number format), so the result stays
  computable in Excel instead of turning into text.
- Columns are configured with `{ prop: { header, numberFormat, align, width } }`
  (or an array of `{ key | index, ... }` for array rows); widths, number
  formats and an optional frozen header are written directly into the sheet.
- Output is a `Buffer`, or a `{ bytes, rowCount, sheetName }` summary when
  `options.output` is given (written natively, no Buffer copy).
- The package is assembled by a purpose-built ZIP writer: parts are deflated as
  they are produced, so a large worksheet never exists in memory as
  uncompressed XML, and the whole package is written exactly once.
- Values are pulled straight out of the JS array while the XML is generated, so
  the data is not copied into an intermediate representation.

Fixed (this also fixes reading):

- **Non-ASCII paths on Windows.** UTF-8 paths were handed to narrow file APIs,
  which interpret them as ANSI, so any Chinese file or directory name failed to
  open. All filesystem access now goes through `src/path_util.*`, which uses the
  wide API on Windows (`std::filesystem::u8path` + `_wfopen` /
  `zip_source_win32w_create`) and the UTF-8 path elsewhere.

## 1.0.21 (2026-09-18) — performance

- `npm run bench [file] [--iterations N] [--batch N]`: reports best/median wall
  time, rows per second and retained memory for the buffered, image-less,
  column-projected and streamed paths, so the options above can be measured
  against a real workbook instead of trusted.
- The design notes and the remaining SAX-reader plan live in
  `docs/plans/2026-09-17-performance-design.md`.

## 1.0.20 (2026-09-18) — performance

New option:

- `onBatch(rows, meta)` + `batchSize`: rows are pushed to the callback while
  the sheet is parsed instead of being accumulated, so peak memory is one
  batch regardless of sheet size. The call then resolves to
  `{ rowCount, warnings }`. Works with both `readTableAsJSON` (direct
  callback) and `readTableAsJSONAsync` (ThreadSafeFunction, batches delivered
  on the main thread). All other options keep their behaviour, and images are
  still attached — now per row, which is what makes streaming possible.

## 1.0.19 (2026-09-18) — performance

- Cell values travel through a shared string pool: every distinct value
  becomes one V8 string and cells carry its index, so low-cardinality columns
  (status flags, categories, repeated codes) no longer allocate a string per
  row. Images are referenced by index too, which removes the last per-cell
  object allocation.

## 1.0.18 (2026-09-18) — performance

- Buffer / base64 input is parsed straight from memory
  (`workbook::load(const std::vector<uint8_t>&)` + a libzip memory source).
  No temporary file is written, so those callers no longer pay for a full
  write + read round-trip of the workbook, and nothing is left behind on
  crash. The only remaining disk use is the rare WPS sanitizing fallback.

## 1.0.17 (2026-09-17) — performance

Zero API change:

- Only the requested worksheet is materialized (`sheetName` is resolved in the
  native layer); other sheets are no longer read at all.
- Cells are probed with `worksheet::has_cell()` before being touched, so empty
  coordinates no longer allocate (and retain) an empty cell.
- Workbooks without `xl/media`, `xl/drawings` or `xl/cellimages` parts skip the
  whole image pipeline — no second pass over the archive.
- Sheet data is converted to JS row by row and released as it goes, so the C++
  copy no longer coexists with the full JS result.
- Integral numbers skip the `snprintf("%g")` round-trip probing.

New options (additive, existing behaviour unchanged):

- `columns`: read only the listed columns, given as header texts or Excel
  references (`"B"`, `"C:E"`). Resolved natively, so unrequested columns are
  never read.
- `includeImages: false`: skip the image pipeline unconditionally.

Docs: examples now show the exact return shape (rows, image cells, and the
`{ rows, warnings }` variant).

## 1.0.16 (2026-09-17) — audit remediation

Security & robustness:

- Reject ZIP entries with no declared size or above the size caps
  (256 MB generic / 128 MB media) — zip-bomb protection.
- Add `maxRows` / `maxCols` read caps with warning-reported truncation.
- Remove the floating-image substring "fuzzy match" that could attach the
  wrong image to a cell.
- Base64 heuristic now requires ZIP "PK" magic after decoding; explicit
  `inputEncoding: 'base64'` option added.
- SHA-256 checksum generation for prebuild archives (`npm run checksums`).

Fixes:

- WPS workbooks with proprietary relationship types (e.g.
  `http://www.wps.cn/officeDocument/2020/cellImage`) and/or backslash ZIP
  entry separators no longer fail with
  `xlnt::exception: key not found in container`: loading is retried through
  a sanitized copy that normalizes entry names to `/` and strips
  non-standard relationship types / content-type overrides (reported via
  `warnings`); images are still extracted from the original file, and image
  extraction itself now tolerates backslash entry names.
- Number cells no longer forced to 6 fixed decimals (shortest round-trip
  formatting); date cells render as deterministic `YYYY-MM-DD[ HH:MM:SS]`.
- Drawing anchors are mapped to their sheets via `workbook.xml` +
  relationships (was hardcoded "Sheet1": images misattached/missing on
  multi-sheet workbooks).
- Image extraction failure no longer discards sheet data (fail-open with
  warnings); all "silently swallowed" error paths now record warnings.
- XML scanning rewritten without hardcoded offsets; tolerant of attribute
  order, quote style and whitespace; malformed anchors are skipped with a
  warning instead of silently attaching to A1.
- Copy of each image shared across cells (one Buffer per image, not per cell).
- `fs.existsSync` race window narrowed; coded errors everywhere
  (`err.code`); friendly `headerRow` validation errors.
- Temp files for Buffer/base64 input are cleaned up from crashed runs.

API:

- New `readTableAsJSONAsync`: non-blocking parse on the libuv thread pool
  (Promise). `readTableAsJSON` remains synchronous.
- Removed the dead native `extractImages` export and empty placeholder
  methods (they always returned `[]` silently).

Engineering:

- Prebuild matrix now covers win32-x64, linux-x64, linux-arm64, darwin-arm64
  and darwin-x64 (the two newer runner labels are allowed to fail without
  blocking the release); Linux/macOS link flags are generated from the vcpkg
  lib directory so the static xlnt/libzip dependency chain resolves without a
  hardcoded list.
- Cross-platform archive verification (`npm run verify:prebuild`) asserts every
  published archive contains the `.node` module (and DLLs on Windows).
- Documentation layout: `README.md` is English, `README-CN.md` is Chinese
  (cross-linked, both shipped in the npm tarball); every other artifact
  (code, comments, scripts, CHANGELOG, commit messages, release output) is
  English-only.
- npm publishing automated in the release workflow via npm Trusted
  Publishing (OIDC, no stored token); runs only after the GitHub Release
  assets exist and skips versions already on the registry.
- `package.json` `files` whitelist added so the npm tarball ships only the
  JS binding, C++ sources, build scripts and docs (no stale prebuild
  archives or example workbooks).
- Examples rewritten against the real API (previously crashed on import).
- README rewritten to match the actual API.
- `npm test` now runs a real regression suite (`test/test.js`).
- vcpkg lookup unified in `scripts/lib/vcpkg.js`; `VCPKG_ROOT` is validated
  with a clear error during builds.
- CI workflow (lint/typecheck, Windows native build + test) with a
  tag-triggered `release` job: builds Windows prebuilds (napi + electron,
  DLLs bundled), verifies checksums and publishes them to the matching
  GitHub Release so `prebuild-install` resolves on install.
- `binding.gyp` Linux/macOS sections now consume `VCPKG_ROOT` instead of
  hardcoded `/usr/local` paths.

## 1.0.15

Previous release (public history not reconstructed in this file).
