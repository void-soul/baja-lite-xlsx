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
  }

  /** A single output row: plain string values or image objects. */
  export type ReadTableRow = Record<string, string | ImageDataObject | ImageDataObject[]>;

  /** Result shape when `includeWarnings` is true. */
  export interface ReadTableResult {
    rows: ReadTableRow[];
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
  export function readTableAsJSON(input: string | Buffer, options?: ReadTableOptions): ReadTableRow[];
  export function readTableAsJSON(
    input: string | Buffer,
    options: ReadTableOptions & { includeWarnings: true }
  ): ReadTableResult;

  /**
   * Async variant of `readTableAsJSON`: parsing runs on the libuv thread pool
   * so the event loop is not blocked. Same options and error codes.
   */
  export function readTableAsJSONAsync(input: string | Buffer, options?: ReadTableOptions): Promise<ReadTableRow[]>;
  export function readTableAsJSONAsync(
    input: string | Buffer,
    options: ReadTableOptions & { includeWarnings: true }
  ): Promise<ReadTableResult>;
}
