# Changelog

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
