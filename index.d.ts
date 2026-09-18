/**
 * TypeScript type definitions for baja-lite-xlsx (v2: all APIs are async).
 */

declare module 'baja-lite-xlsx' {
  /** Rows may be objects (keyed by column) or arrays (positional). */
  export type TableRows = Array<Record<string, unknown>> | unknown[][];
  /** Any iterable or async iterable of rows (generators, cursors, ...). */
  export type AsyncTableRows =
    | TableRows
    | Iterable<Record<string, unknown> | unknown[]>
    | AsyncIterable<Record<string, unknown> | unknown[]>;

  /**
   * Image data object returned in cells that hold a picture.
   */
  export interface ImageDataObject {
    /** Image data as a Buffer. */
    data: Buffer;
    /** Image filename (e.g. "image1.png"). */
    name: string;
    /** MIME type (e.g. "image/png", "image/jpeg"). */
    type: string;
  }

  /** Options for reading a table. */
  export interface ReadTableOptions {
    /** Sheet to read; defaults to the first sheet. */
    sheetName?: string;

    /** Header row index (0-based, non-negative). Default: 0 */
    headerRow?: number;

    /** Row indices to skip (0-based, non-negative). */
    skipRows?: number[];

    /**
     * Header mapping from the original header text to the output property
     * name, e.g. `{ 'name': 'fullName' }`.
     */
    headerMap?: Record<string, string>;

    /**
     * Force how a string input is interpreted. Currently only 'base64'.
     * When omitted, strings are treated as file paths unless the base64
     * heuristic matches (long string, base64 charset, decodes to ZIP "PK"
     * magic -- otherwise an INVALID_INPUT error is thrown).
     */
    inputEncoding?: 'base64';

    /**
     * Cap on rows read per sheet (0 = no cap beyond the format maximum).
     * Excess rows are truncated and reported through `warnings`.
     */
    maxRows?: number;

    /** Cap on columns read per sheet (0 = no cap beyond the format maximum). */
    maxCols?: number;

    /**
     * When true the call resolves to `{ rows, warnings }` instead of just the
     * rows array. Warnings cover unattached images, truncated sheets, vendor
     * extension workarounds, etc.
     */
    includeWarnings?: boolean;

    /**
     * When false the image pipeline is skipped entirely: no second pass over
     * the archive and no media decompression. Default: true.
     */
    includeImages?: boolean;

    /**
     * Read only these columns, given as header texts ("Amount") or Excel
     * references ("B", "C:E"). Resolved inside the native layer, so
     * unrequested columns are never read. An empty array means all columns.
     */
    columns?: string[];

    /**
     * How the worksheet is read. `"xlnt"` (default) builds the workbook model
     * and reads one sheet from it. `"xml"` reads the requested sheet straight
     * out of the package — shared strings, the style table and the sheet XML,
     * nothing else — which is ~4x faster for projected and streamed reads on
     * large files.
     *
     * Both engines return exactly the same rows, and a file that uses something
     * the direct reader does not model falls back to `"xlnt"` automatically. One
     * exception: a streamed read (`onBatch`) with images enabled stays with
     * `"xlnt"`, because streamed rows are handed over as they are produced.
     */
    engine?: 'xlnt' | 'xml';

    /**
     * `"typed"` turns numbers, booleans and dates into real JS values instead
     * of formatted strings (numbers keep full precision, dates come back as
     * `Date`). Requires `engine: 'xml'`. Default: `"string"`.
     */
    values?: 'string' | 'typed';

    /**
     * Stream rows in batches instead of returning them all: memory stays flat
     * no matter how large the sheet is. When set, the call resolves to
     * `{ rowCount, warnings }`.
     */
    onBatch?: OnBatch;

    /** Rows per batch when `onBatch` is used. Default: 50000. */
    batchSize?: number;
  }

  /** A single output row: plain string values or image objects. */
  export type ReadTableRow = Record<string, string | ImageDataObject | ImageDataObject[]>;
  /** Row shape produced with `values: 'typed'`. */
  export type TypedReadTableRow = Record<
    string,
    string | number | boolean | Date | ImageDataObject | ImageDataObject[]
  >;

  /** Result shape when `includeWarnings` is true. */
  export interface ReadTableResult {
    rows: ReadTableRow[];
    warnings: string[];
  }

  /** Meta passed to every `onBatch` call. */
  export interface BatchMeta {
    /** 0-based index of the first source row in this batch. */
    startIndex: number;
    /** Number of rows in this batch. */
    count: number;
  }

  /** Called with each batch of rows while the sheet is being read. */
  export type OnBatch = (rows: ReadTableRow[], meta: BatchMeta) => void;

  /** Result shape when `onBatch` is used: rows are never accumulated. */
  export interface ReadTableStreamResult {
    /** Number of rows handed to `onBatch`. */
    rowCount: number;
    warnings: string[];
  }

  /**
   * Reads an Excel table and resolves to one object per data row.
   *
   * All cell values are strings; a cell holding pictures contains an
   * ImageDataObject (or an array of them when several images attach to the
   * same cell). Pass `values: 'typed'` (with `engine: 'xml'`) to get real
   * numbers, booleans and `Date` objects instead.
   *
   * Invalid arguments throw synchronously; parse failures reject. Parsing runs
   * on the libuv thread pool, so the event loop is not blocked.
   *
   * @param input File path, Buffer, or base64 string.
   * @param options See ReadTableOptions.
   * @throws Error with a `code` property:
   *   ADDON_LOAD_FAILED / ADDON_NOT_FOUND / FILE_NOT_FOUND / FILE_OPEN_FAILED /
   *   INVALID_INPUT / INVALID_OPTIONS / NO_SHEETS / SHEET_NOT_FOUND /
   *   HEADER_ROW_OUT_OF_RANGE / PARSE_ERROR / READ_FAILED
   */
  export function readTableAsJSON(
    input: string | Buffer,
    options: ReadTableOptions & { onBatch: OnBatch }
  ): Promise<ReadTableStreamResult>;
  export function readTableAsJSON(
    input: string | Buffer,
    options: ReadTableOptions & { includeWarnings: true }
  ): Promise<ReadTableResult>;
  export function readTableAsJSON(
    input: string | Buffer,
    options?: ReadTableOptions
  ): Promise<ReadTableRow[]>;

  /** Per-column configuration for `writeTableAsJSON`. */
  export interface WriteColumnOptions {
    /** Header text. Defaults to the source property name. */
    header?: string;
    /** Source property of each row object. */
    key?: string | number;
    /** Source index, for rows given as arrays. */
    index?: number;
    /** Number format, e.g. "#,##0.00", "0.00%", "yyyy-mm-dd". */
    numberFormat?: string;
    /** Horizontal alignment. */
    align?: 'left' | 'center' | 'right';
    /** Column width in characters. */
    width?: number;
  }

  export interface WriteTableOptions {
    /**
     * Worksheet name. Default: "Sheet1" for a new workbook. With
     * `sourceFile`: the sheet the rows are appended to; a sheet that does not
     * exist yet is created (this is how multi-sheet workbooks are built by
     * chaining calls).
     */
    sheetName?: string;
    /**
     * Write a header row from the column headers. Default: true for a fresh
     * workbook; with `sourceFile` the default is "auto" — the header is
     * written only when the sheet was created or had no rows, so chained
     * appends do not duplicate it.
     */
    includeHeader?: boolean;
    /** Freeze the header row (fresh sheets only). Default: false. */
    freezeHeader?: boolean;
    /**
     * Workbook to write into: a file path, or a Buffer returned by any write
     * call. Default `append: true` adds the rows after the sheet's last row;
     * `append: false` replaces the sheet's data instead.
     */
    sourceFile?: string | Buffer;
    /**
     * Append after the sheet's existing rows (default) instead of replacing
     * them. Only meaningful together with `sourceFile`.
     */
    append?: boolean;
    /** Write the file natively instead of returning a Buffer. */
    output?: string;
    /** Compression level: 0 (store) .. 9 (maximum). Default: 6. */
    compression?: number;
    /**
     * Column definition: either an object keyed by property name
     * (`{ amount: { header: 'Amount', numberFormat: '#,##0.00' } }`) or an
     * array of specs for array rows (`[{ index: 0, header: 'A' }]`).
     * When omitted, columns come from the keys of the first row object.
     */
    columns?: Record<string, WriteColumnOptions> | WriteColumnOptions[];
  }

  /** Summary returned when `output` was written by the native layer. */
  export interface WriteResult {
    /** Size of the written package in bytes. */
    bytes: number;
    /** Data rows written (header excluded). */
    rowCount: number;
    /** Worksheet that received the data. */
    sheetName: string;
  }

  /**
   * Writes a JSON array into a worksheet and resolves to the .xlsx bytes (or
   * a summary when `output` is given).
   *
   * Numbers are written as numbers, booleans as booleans and `Date` objects as
   * Excel dates, so the values stay computable in Excel instead of becoming
   * text. Rows may also be any iterable or async iterable (a generator, a
   * database cursor, ...): they are folded into the write batch by batch, so
   * the table never has to exist in JS at once (`columns` is required then,
   * since it cannot be inferred from a first row).
   *
   * Invalid arguments throw synchronously; write failures reject.
   *
   * @throws Error with a `code` property: INVALID_OPTIONS / FILE_OPEN_FAILED /
   *   SHEET_NOT_FOUND / FILE_WRITE_FAILED / WRITE_FAILED / ADDON_LOAD_FAILED.
   */
  export function writeTableAsJSON(
    rows: AsyncTableRows,
    options: WriteTableOptions & { output: string }
  ): Promise<WriteResult>;
  export function writeTableAsJSON(
    rows: AsyncTableRows,
    options?: WriteTableOptions
  ): Promise<Buffer>;

  /** One cell rewrite for `updateCells`. */
  export interface CellUpdate {
    /** Worksheet to patch; defaults to the first sheet. */
    sheet?: string;
    /** A1-style reference, e.g. "B7". */
    cell: string;
    /** New value: string, number, boolean, Date, or null/undefined to clear. */
    value?: unknown;
    /** Number format to apply, e.g. "#,##0.00" or "yyyy-mm-dd". */
    numberFormat?: string;
  }

  export interface UpdateCellsOptions {
    /**
     * Workbook to patch: a file path, or a Buffer returned by any write call.
     */
    sourceFile: string | Buffer;
    /** Cells to rewrite. Everything else is copied verbatim. */
    updates: CellUpdate[];
    /** Write the file natively instead of returning a Buffer. */
    output?: string;
    /** Compression level: 0 (store) .. 9 (maximum). Default: 6. */
    compression?: number;
  }

  /** Summary returned when `output` was written by the native layer. */
  export interface UpdateCellsResult {
    /** Size of the written package in bytes. */
    bytes: number;
    /** Number of cells that were rewritten. */
    cells: number;
  }

  /**
   * Rewrites individual cells of an existing workbook.
   *
   * Only the affected worksheets are regenerated and every other part of the
   * package is copied byte for byte, so untouched sheets, images and styles
   * survive exactly as they were. A cell keeps its existing style unless the
   * update supplies a `numberFormat`.
   *
   * @throws Error with a `code` property: INVALID_OPTIONS / FILE_OPEN_FAILED /
   *   SHEET_NOT_FOUND / FILE_WRITE_FAILED / WRITE_FAILED.
   */
  export function updateCells(
    options: UpdateCellsOptions & { output: string }
  ): Promise<UpdateCellsResult>;
  export function updateCells(options: UpdateCellsOptions): Promise<Buffer>;

  export interface RenderTemplateOptions {
    /** Template workbook (path or bytes). */
    template: string | Buffer;
    /**
     * Render only this sheet. By default every sheet containing a marker is
     * rendered; sheets without markers are copied untouched.
     */
    sheetName?: string;
    /**
     * Native `${...}` markers only: throw `TEMPLATE_ERROR` when a marker has
     * no value (default), or substitute an empty string when false.
     */
    strict?: boolean;
    /**
     * Keep the parsed template structure in a bounded in-process cache —
     * useful when the same template is rendered repeatedly, as in a report
     * server. Entries are keyed by the template's identity, so a rewritten
     * template is always picked up. Default: false.
     */
    cache?: boolean;
    /** Write the file instead of returning a Buffer. */
    output?: string;
    /** Compression level: 0 (store) .. 9 (maximum). Default: 6. */
    compression?: number;
  }

  export interface RenderTemplateResult {
    /** Size of the written package in bytes. */
    bytes: number;
    /** Worksheets that were rewritten. */
    sheets: string[];
  }

  /**
   * Renders a template workbook.
   *
   * Two marker styles are understood inside cell text, selected per sheet:
   *
   * 1. ejsExcel syntax — `<%...%>` markers evaluated as JavaScript:
   *      `<%=expr%>`   emit the value as text
   *      `<%~expr%>`   emit a number/Date so the cell's number format applies
   *      `<%#expr%>`   dynamic formula ("=SUM(A1,A2)"); pair with `<%~result%>`
   *                    to also cache the computed value
   *      `<%forRow item,i in _data_.list%>`    repeat the marker row per item
   *      `<%forRBegin ...%>` ... `<%forREnd%>` repeat the rows in between
   *      `<%forCell key in [...]%>`            repeat the cell horizontally
   *      `<%ifCBegin cond%>` ... `<%ifCEnd%>`  conditional region
   *      `_row` / `_col` / `_rc`               emitted row, column, cell ref
   *      `_charPlus_(col,n)` / `_charToNum_(col)`   column arithmetic
   *      `_mergeCellFn_(range)`                merge cells
   *      `_outlineLevel_(n)`                   row grouping
   *      `_dataValidation_({sqref, formula1})` dropdown validation
   *      `_img_({imgPh, cellNumAdd, rowNumAdd})`  image from URL / Buffer /
   *                                              base64 / file path
   *      `_qrcode_({text, size, ...})`         QR code (`npm install qrcode`)
   *    `_data_` is the values argument; when it is an array, `_data_[i]` is
   *    sheet i's data (workbook order). Every sheet that contains markers is
   *    rendered. Images require the template to already contain a picture.
   *
   * 2. Native markers, rendered on the worker thread without JS evaluation:
   *      `${path}`  the value at `path`; `../name` steps out of a loop and
   *                 `${@index}` is the loop index
   *      `{{#each path}}` ... `{{/each}}`  repeat the rows in between
   *
   * Repeated rows are copies of the template row XML, so styles, number
   * formats, row heights, merged cells and conditional formats are preserved.
   * Cells without markers, and every other part of the package, stay
   * byte-identical.
   *
   * @throws Error with a `code` property: INVALID_OPTIONS / FILE_OPEN_FAILED /
   *   SHEET_NOT_FOUND / TEMPLATE_ERROR / FILE_WRITE_FAILED / WRITE_FAILED.
   */
  export function renderTemplate(
    values: Record<string, unknown> | unknown[],
    options: RenderTemplateOptions & { output: string }
  ): Promise<RenderTemplateResult>;
  export function renderTemplate(
    values: Record<string, unknown> | unknown[],
    options: RenderTemplateOptions
  ): Promise<Buffer>;
}
