/**
 * TypeScript type definitions for baja-lite-xlsx
 */

declare module 'baja-lite-xlsx' {
  /**
   * Image data object (returned in cell values)
   * 图片数据对象（在单元格值中返回）
   */
  export interface ImageDataObject {
    /** Image data as Buffer */
    data: Buffer;
    /** Image filename */
    name: string;
    /** MIME type (e.g., 'image/png', 'image/jpeg') */
    type: string;
  }

  /**
   * Options for reading a table.
   */
  export interface ReadTableOptions {
    /**
     * Sheet name to read. If not provided, reads the first sheet.
     * 指定要读取的Sheet名称，不传则读取第一个Sheet
     */
    sheetName?: string;

    /**
     * Header row index (0-based, non-negative). Default: 0
     * 表头所在行索引（从0开始），默认为0
     */
    headerRow?: number;

    /**
     * Row indices to skip (0-based, non-negative).
     * 需要跳过的行索引数组（从0开始）
     */
    skipRows?: number[];

    /**
     * Header mapping from original header to new property name.
     * 表头映射，将原表头名称映射为新的属性名
     * @example { '名称': 'name', '年龄': 'age' }
     */
    headerMap?: Record<string, string>;

    /**
     * Force how a string input is interpreted. Currently only 'base64'.
     * When omitted, strings are treated as file paths unless the base64
     * heuristic matches (long string, base64 charset, decodes to ZIP "PK"
     * magic -- otherwise an INVALID_INPUT error is thrown).
     * 强制字符串输入的解析方式；不传时按路径处理，仅当启发式判定为
     * base64 且解码后为 ZIP "PK" 头才按 base64 处理。
     */
    inputEncoding?: 'base64';

    /**
     * Cap on rows read per sheet (0 = no cap beyond the format maximum).
     * Excess rows are truncated and reported via warnings.
     * 每个 Sheet 读取的行数上限（0 = 不限制），超出部分截断并通过 warnings 报告。
     */
    maxRows?: number;

    /**
     * Cap on columns read per sheet (0 = no cap beyond the format maximum).
     * 每个 Sheet 读取的列数上限（0 = 不限制）。
     */
    maxCols?: number;

    /**
     * When true, returns { rows, warnings } instead of just rows.
     * Warnings cover skipped/unmapped images, truncated sheets, etc.
     * 为 true 时返回 { rows, warnings }；warnings 记录图片未挂载、
     * Sheet 截断等非致命诊断信息。
     */
    includeWarnings?: boolean;
  }

  /**
   * Read Excel table and return as JSON array (synchronous).
   * All cell values are strings; image cells contain ImageDataObject
   * (or an array of them when multiple images attach to one cell).
   *
   * NOTE: this runs the whole parse on the calling thread. For large
   * files or Electron UIs prefer readTableAsJSONAsync.
   *
   * 同步读取 Excel 表格并返回 JSON 数组。所有单元格值为字符串；
   * 图片单元格为 ImageDataObject（多图挂同一单元格时为数组）。
   * 大文件/Electron 场景请使用 readTableAsJSONAsync。
   *
   * @param input - Excel file path (string), Buffer, or base64 string
   * @param options - Configuration options
   * @returns Array of objects, each representing a row (or { rows, warnings }
   *          when options.includeWarnings is true).
   * @throws Error with a `code` property:
   *   - ADDON_NOT_FOUND / FILE_NOT_FOUND / FILE_OPEN_FAILED
   *   - INVALID_INPUT / INVALID_OPTIONS
   *   - NO_SHEETS / SHEET_NOT_FOUND / HEADER_ROW_OUT_OF_RANGE
   *   - PARSE_ERROR / READ_FAILED
   */
  export function readTableAsJSON(
    input: string | Buffer,
    options?: ReadTableOptions
  ): Array<Record<string, string | ImageDataObject | ImageDataObject[]>> |
     { rows: Array<Record<string, string | ImageDataObject | ImageDataObject[]>>, warnings: string[] };

  /**
   * Async variant of readTableAsJSON. Parsing runs on the libuv thread
   * pool; the event loop is not blocked. Same options, same error codes.
   *
   * 异步版本：解析在线程池执行，不阻塞事件循环；参数与错误码相同。
   */
  export function readTableAsJSONAsync(
    input: string | Buffer,
    options?: ReadTableOptions
  ): Promise<Array<Record<string, string | ImageDataObject | ImageDataObject[]>> |
           { rows: Array<Record<string, string | ImageDataObject | ImageDataObject[]>>, warnings: string[] }>;
}
