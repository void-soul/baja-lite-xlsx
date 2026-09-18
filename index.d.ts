/**
 * TypeScript type definitions for baja-lite-xlsx
 */

declare module 'baja-lite-xlsx' {
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
   * Reads an Excel table synchronously and returns one object per row.
   *
   * All cell values are strings; a cell holding pictures contains an
   * ImageDataObject (or an array of them when several images attach to the
   * same cell). NOTE: the whole parse runs on the calling thread -- for large
   * files or Electron UIs prefer `readTableAsJSONAsync`.
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
  ): ReadTableStreamResult;
  export function readTableAsJSON(
    input: string | Buffer,
    options: ReadTableOptions & { includeWarnings: true }
  ): ReadTableResult;
  export function readTableAsJSON(input: string | Buffer, options?: ReadTableOptions): ReadTableRow[];

  /**
   * Async variant of `readTableAsJSON`: parsing runs on the libuv thread pool
   * so the event loop is not blocked. Same options and error codes.
   */
  export function readTableAsJSONAsync(
    input: string | Buffer,
    options: ReadTableOptions & { onBatch: OnBatch }
  ): Promise<ReadTableStreamResult>;
  export function readTableAsJSONAsync(
    input: string | Buffer,
    options: ReadTableOptions & { includeWarnings: true }
  ): Promise<ReadTableResult>;
  export function readTableAsJSONAsync(input: string | Buffer, options?: ReadTableOptions): Promise<ReadTableRow[]>;

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
    /** Worksheet name. Default: "Sheet1". */
    sheetName?: string;
    /** Write a header row from the column headers. Default: true. */
    includeHeader?: boolean;
    /** Freeze the header row. Default: false. */
    freezeHeader?: boolean;
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

    /**
     * Existing workbook to write into (path or bytes). Only the target sheet's
     * data is replaced and every other part is copied byte for byte, so other
     * sheets, images and styles survive unchanged. Without it a new workbook is
     * created.
     */
    template?: string | Buffer;
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
   * Writes a JSON array into a worksheet and returns the .xlsx bytes.
   *
   * Numbers are written as numbers, booleans as booleans and `Date` objects as
   * Excel dates with an automatic date format, so the values stay computable in
   * Excel instead of becoming text.
   *
   * @throws Error with a `code` property: INVALID_OPTIONS / FILE_WRITE_FAILED /
   *   WRITE_FAILED / ADDON_LOAD_FAILED.
   */
  export function writeTableAsJSON(
    rows: Array<Record<string, unknown>> | unknown[][],
    options: WriteTableOptions & { output: string }
  ): WriteResult;
  export function writeTableAsJSON(
    rows: Array<Record<string, unknown>> | unknown[][],
    options?: WriteTableOptions
  ): Buffer;

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
    /** Workbook to patch (path or bytes). */
    template: string | Buffer;
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
  export function updateCells(options: UpdateCellsOptions & { output: string }): UpdateCellsResult;
  export function updateCells(options: UpdateCellsOptions): Buffer;
}
