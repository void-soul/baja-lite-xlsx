/**
 * TypeScript type definitions for baja-lite-xlsx
 */

declare module 'baja-lite-xlsx' {
  /**
   * Sheet data structure
   */
  export interface SheetData {
    /** Sheet name */
    name: string;
    /** 2D array of cell values (rows x columns) */
    data: string[][];
  }

  /**
   * Image data structure
   */
  export interface ImageData {
    /** Image filename */
    name: string;
    /** Image data as Buffer */
    data: Buffer;
    /** MIME type (e.g., 'image/png', 'image/jpeg') */
    type: string;
  }

  /**
   * Image position information
   */
  export interface ImagePosition {
    /** Image filename reference */
    image: string;
    /** Sheet name where the image is located */
    sheet: string;
    /** Top-left anchor position */
    from: {
      /** Column index (0-based) */
      col: number;
      /** Row index (0-based) */
      row: number;
    };
    /** Bottom-right anchor position */
    to: {
      /** Column index (0-based) */
      col: number;
      /** Row index (0-based) */
      row: number;
    };
  }

  /**
   * Complete Excel data structure
   */
  export interface ExcelData {
    /** All sheets in the workbook */
    sheets: SheetData[];
    /** All images embedded in the workbook */
    images: ImageData[];
    /** Position information for all images */
    imagePositions: ImagePosition[];
  }

  /**
   * Options for reading table as JSON
   */
  export interface ReadTableOptions {
    /** 
     * Sheet name to read. If not provided, reads the first sheet 
     * 指定要读取的Sheet名称，不传则读取第一个Sheet
     */
    sheetName?: string;
    
    /** 
     * Header row index (0-based). Default: 0
     * 表头所在行索引（从0开始），默认为0
     */
    headerRow?: number;
    
    /** 
     * Row indices to skip (0-based)
     * 需要跳过的行索引数组（从0开始）
     */
    skipRows?: number[];
    
    /** 
     * Header mapping from original header to new property name
     * 表头映射，将原表头名称映射为新的属性名
     * @example { '名称': 'name', '年龄': 'age' }
     */
    headerMap?: Record<string, string>;
  }

  /**
   * Image data object
   * 图片数据对象
   */
  export interface ImageDataObject {
    /** Image data as Buffer */
    data: Buffer;
    /** Image filename */
    name: string;
    /** MIME type */
    type: string;
  }

  /**
   * Read complete Excel file including sheets, images, and image positions
   * 
   * @param filepath - Path to the .xlsx file (absolute or relative)
   * @returns Object containing sheets, images, and imagePositions
   * @throws {Error} If file not found or cannot be read
   * 
   * @example
   * ```javascript
   * const { readExcel } = require('baja-lite-xlsx');
   * const data = readExcel('./sample.xlsx');
   * console.log(data.sheets[0].name);
   * console.log(data.images.length);
   * ```
   */
  export function readExcel(filepath: string): ExcelData;

  /**
   * Extract only images from Excel file
   * 
   * @param filepath - Path to the .xlsx file (absolute or relative)
   * @returns Array of image objects
   * @throws {Error} If file not found or cannot be read
   * 
   * @example
   * ```javascript
   * const { extractImages } = require('baja-lite-xlsx');
   * const images = extractImages('./sample.xlsx');
   * images.forEach(img => {
   *   console.log(`${img.name}: ${img.data.length} bytes`);
   * });
   * ```
   */
  export function extractImages(filepath: string): ImageData[];

  /**
   * Get all sheet names from Excel file
   * 获取Excel文件中所有Sheet的名称
   * 
   * @param input - Excel file path (string), Buffer, or base64 string
   * @returns Array of sheet names
   * @throws {Error} If file not found or cannot be read
   * 
   * @example
   * ```javascript
   * const { getSheetNames } = require('baja-lite-xlsx');
   * 
   * // Using file path
   * const names1 = getSheetNames('./sample.xlsx');
   * console.log(names1); // ['Sheet1', 'Sheet2']
   * 
   * // Using Buffer
   * const buffer = fs.readFileSync('./sample.xlsx');
   * const names2 = getSheetNames(buffer);
   * 
   * // Using base64
   * const base64 = buffer.toString('base64');
   * const names3 = getSheetNames(base64);
   * ```
   */
  export function getSheetNames(input: string | Buffer): string[];

  /**
   * Read Excel table and return as JSON array
   * 读取Excel表格并返回JSON数组
   * 
   * Images are automatically attached to rows based on their column position:
   * - Floating images: Use top-left corner coordinates (from.col, from.row)
   * - Embedded images: Use the cell they are in (from.col, from.row)
   * - Image property name is determined by the column header
   * 
   * 图片会根据所在列自动附加到行数据：
   * - 浮动图片：使用左上角坐标所在的列和行（from.col, from.row）
   * - 内嵌图片：使用图所在单元格的列和行（from.col, from.row）
   * - 图片属性名由该列的表头决定
   * 
   * @param input - Excel file path (string), Buffer, or base64 string
   * @param options - Configuration options
   * @returns Array of objects, each representing a row
   * @throws {Error} If file not found or sheet not found
   * 
   * @example
   * ```javascript
   * const { readTableAsJSON } = require('baja-lite-xlsx');
   * const fs = require('fs');
   * 
   * // Excel表格:
   * // | 名称  | 年龄 | photo1 | photo2 |
   * // | 张三  | 25   | [图片] |        |
   * // | 李四  | 30   |        | [图片] |
   * 
   * // Using file path
   * const data1 = readTableAsJSON('./sample.xlsx', {
   *   sheetName: 'Sheet1',
   *   headerRow: 0,
   *   headerMap: {
   *     '名称': 'name',
   *     '年龄': 'age'
   *   }
   * });
   * 
   * // Using Buffer
   * const buffer = fs.readFileSync('./sample.xlsx');
   * const data2 = readTableAsJSON(buffer, { headerRow: 0 });
   * 
   * // Using base64
   * const base64 = buffer.toString('base64');
   * const data3 = readTableAsJSON(base64, { headerRow: 0 });
   * 
   * // 结果:
   * // [
   * //   { 
   * //     name: '张三', 
   * //     age: '25', 
   * //     photo1: { data: Buffer, name: 'img1.png', type: 'image/png' }
   * //   },
   * //   { 
   * //     name: '李四', 
   * //     age: '30',
   * //     photo2: { data: Buffer, name: 'img2.jpg', type: 'image/jpeg' }
   * //   }
   * // ]
   * ```
   */
  export function readTableAsJSON(
    input: string | Buffer,
    options?: ReadTableOptions
  ): Array<Record<string, any>>;

}
