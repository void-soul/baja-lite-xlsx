# Write Support — Design

Three exported functions (chosen over a single options-only entry point):

```js
writeTableAsJSON(rows, options)          // mode 1: full sheet write
updateCells({ template, updates, output })  // mode 2: patch cells
renderTemplate(values, options)          // mode 3: template rendering
```

All three return a `Buffer` when no `output` path is given, and a small
`{ bytes, ... }` summary when the output was written by the native layer.

## Status

| Mode | Shipped in |
|------|-----------|
| `writeTableAsJSON` (new workbook) | 1.1.0 |
| 1b `writeTableAsJSON({ template })` | 1.2.0 |
| 2 `updateCells` | 1.2.0 |
| 3 `renderTemplate` | 1.3.0 |
| Async twins for the write side | 1.4.0 |
| Template structure cache (`cache: true`) | 1.5.0 |
| Write benchmarks (`bench/`) | 1.5.0 |
| Streaming writes (iterable / async iterable rows) | 1.6.0 |
| Direct sheet reading, `engine: 'xml'` (P3) | 1.7.0 |

## The two requirements: fast and accurate

**Fast.** The reference implementation (`ejsExcel-http`) is slow for structural
reasons, and each one is avoidable:

| Its bottleneck | Evidence | Our approach |
|----------------|----------|--------------|
| Whole ZIP rebuilt per modified part | `lib/hzip/hzip.js:373` + `BufferExt.js:5` | One assembly pass; untouched entries are copied as *compressed bytes* |
| string → DOM → string → JSON → string → JS → string per sheet | `ejsExcel.js:897→983→1029→1265→1287→1293` | Structured generation straight into the ZIP entry stream |
| Compiled templates cached on disk, `eval` per render | `ejsExcel.js:889/1290/1293` | In-process cache keyed by template identity |
| Per-value reverse scan of the output buffer with regexes | `ejsExcel.js:163-171,236-302` | Values are written where they are produced |
| `sharedStrings`/`styles`/`workbook` re-parsed every render | `ejsExcel.js:369/390/873` | Parsed once, reused |
| Many copies of the workbook resident at once | whole flow | One output buffer (compressed size, not XML size) |

**Accurate.**

- Numbers are written as numbers (`<v>123.45</v>`, no `t` attribute) with a
  shortest-round-trip representation, so Excel sees real numbers and no
  precision is lost. Number formats come from `numberFormat`.
- Strings are escaped strictly (`& < > " '`), XML-illegal control characters
  are dropped, and `xml:space="preserve"` is emitted when a value has leading
  or trailing whitespace.
- `Date` values become Excel serials plus a date number format, instead of
  being stringified into something Excel cannot compute with.
- Booleans use `t="b"`; `null`/`undefined` become empty cells.

## Components

- `src/zip_writer.{h,cpp}` — a purpose-built ZIP writer:
  - `addEntry(name, bytes)` — deflate (or store) an in-memory payload.
  - `addStreamedEntry(name, producer)` — the producer is called repeatedly with
    a sink; chunks are deflated as they arrive, so the *uncompressed* XML never
    has to exist in full. Compressed bytes are collected (an order of magnitude
    smaller) and the local header is then written with known sizes, avoiding
    ZIP data descriptors entirely.
  - `copyEntryFrom(archive, index, name)` — copies an entry's raw compressed
    payload and reuses its CRC-32, sizes and compression method. This is what
    makes "modify one cell" O(target sheet) instead of O(package).
- `src/xlsx_writer.{h,cpp}` — package assembly (minimal `[Content_Types].xml`,
  `_rels/.rels`, `xl/workbook.xml`, `xl/_rels/workbook.xml.rels`,
  `xl/styles.xml`, `xl/worksheets/sheetN.xml`), sheet XML generation
  (streaming), and the styles table for `numberFormat`.
- `src/addon.cpp` — `writeExcel` entry point; pulls values from the JS array on
  demand (no bulk copy) for the synchronous path.

## Modes

1. **`writeTableAsJSON(rows, options)`** — with no `template`, builds a new
   workbook: header row (optional), one `<row>` per data row, column widths,
   number formats, optional frozen header. With `template`, only the target
   sheet's `sheetData` is regenerated and every other entry is copied
   compressed.
2. **`updateCells({ template, updates, output })`** — each update targets
   `{ sheet, cell, value }`; the cell's `<c>` element is rewritten in place
   (dropping any `<f>` formula), everything else in the sheet and the package is
   copied verbatim. Values are written as inline strings or numbers so
   `sharedStrings` never has to be touched.
3. **`renderTemplate(values, options)`** — `${path}` substitution inside cell
   text, and row repetition driven by a marker cell (`{{#each items}}` /
   `{{/each}}`). Repeated rows are produced by copying the template row's XML
   and renumbering it, which is why formatting, merged cells, row heights and
   conditional formats survive untouched.

## Error handling

Same `CODE|message` contract as the reader: `INVALID_OPTIONS` for bad specs,
`FILE_OPEN_FAILED` when a template cannot be parsed, `TEMPLATE_ERROR` for
malformed template markers, `PARSE_ERROR`/`WRITE_FAILED` for anything else.
No silent failures: an unresolved `${placeholder}` or an unknown sheet is
reported (warnings array where non-fatal, thrown error where the output would
be wrong).
