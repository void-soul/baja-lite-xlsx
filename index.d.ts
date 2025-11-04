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
   * Read Excel table and return as JSON array
   * 读取Excel表格并返回JSON数组
   * 
   * Images are automatically processed and attached to the corresponding cells.
   * Supports both standard Excel and WPS Excel formats:
   * - Floating images: Images that span multiple cells (twoCellAnchor)
   * - Embedded images (Standard Excel): Images anchored to a single cell (oneCellAnchor)
   * - Embedded images (WPS Excel): Images using DISPIMG formula with cellimages.xml
   * 
   * All image types are automatically converted to the same object format:
   * { data: Buffer, name: string, type: string }
   * 
   * 图片会自动处理并附加到对应的单元格。支持标准 Excel 和 WPS Excel 格式：
   * - 浮动图片：跨越多个单元格的图片（twoCellAnchor）
   * - 嵌入式图片（标准 Excel）：锚定到单个单元格的图片（oneCellAnchor）
   * - 嵌入式图片（WPS Excel）：使用 DISPIMG 公式和 cellimages.xml
   * 
   * 所有图片类型都会自动转换为统一的对象格式：
   * { data: Buffer, name: string, type: string }
   * 
   * @param input - Excel file path (string), Buffer, or base64 string
   * @param options - Configuration options
   * @returns Array of objects, each representing a row. Image cells contain ImageDataObject.
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
   * 
   * // Access image data
   * if (data1[0].photo1 && data1[0].photo1.data) {
   *   fs.writeFileSync('output.png', data1[0].photo1.data);
   * }
   * ```
   */
  export function readTableAsJSON(
    input: string | Buffer,
    options?: ReadTableOptions
  ): Array<Record<string, string | ImageDataObject>>;

}
