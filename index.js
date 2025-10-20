const path = require('path');
const fs = require('fs');

// Try to load the native addon
let addon;
try {
  addon = require('./build/Release/baja_xlsx.node');
} catch (err) {
  try {
    addon = require('./build/Debug/baja_xlsx.node');
  } catch (err2) {
    throw new Error(
      'Native addon not found. Please run "npm install" or "npm run build" first.\n' +
      'Make sure you have installed xlnt library via vcpkg or system package manager.'
    );
  }
}

/**
 * Read Excel file and extract all data including sheets, images, and image positions
 * @param {string} filepath - Path to the Excel file
 * @returns {Object} Object containing sheets, images, and imagePositions
 */
function readExcel(filepath) {
  if (!filepath) {
    throw new Error('Filepath is required');
  }
  
  // Convert to absolute path
  const absolutePath = path.isAbsolute(filepath) ? filepath : path.resolve(filepath);
  
  // Check if file exists
  if (!fs.existsSync(absolutePath)) {
    throw new Error(`File not found: ${absolutePath}`);
  }
  
  return addon.readExcel(absolutePath);
}

/**
 * Extract only images from Excel file
 * @param {string} filepath - Path to the Excel file
 * @returns {Array} Array of image objects with name, data (Buffer), and type
 */
function extractImages(filepath) {
  if (!filepath) {
    throw new Error('Filepath is required');
  }
  
  // Convert to absolute path
  const absolutePath = path.isAbsolute(filepath) ? filepath : path.resolve(filepath);
  
  // Check if file exists
  if (!fs.existsSync(absolutePath)) {
    throw new Error(`File not found: ${absolutePath}`);
  }
  
  return addon.extractImages(absolutePath);
}

/**
 * 获取Excel文件中所有Sheet的名称
 * @param {string} filepath - Excel文件路径
 * @returns {string[]} Sheet名称数组
 */
function getSheetNames(filepath) {
  if (!filepath) {
    throw new Error('文件路径不能为空');
  }
  
  const absolutePath = path.isAbsolute(filepath) ? filepath : path.resolve(filepath);
  
  if (!fs.existsSync(absolutePath)) {
    throw new Error(`文件不存在: ${absolutePath}`);
  }
  
  const data = addon.readExcel(absolutePath);
  return data.sheets.map(sheet => sheet.name);
}

/**
 * 读取Excel表格并返回JSON数组
 * @param {string} filepath - Excel文件路径
 * @param {Object} options - 配置选项
 * @param {string} [options.sheetName] - 指定Sheet名称，不传则读取第一个Sheet
 * @param {number} [options.headerRow=0] - 表头所在行索引（从0开始）
 * @param {number[]} [options.skipRows=[]] - 需要跳过的行索引数组
 * @param {Object<string, string>} [options.headerMap={}] - 表头映射，将原表头映射为新的属性名
 * @returns {Array<Object>} JSON数组，每个元素代表一行数据
 * 
 * @example
 * const data = readTableAsJSON('./sample.xlsx', {
 *   sheetName: 'Sheet1',
 *   headerRow: 0,
 *   skipRows: [1, 2],
 *   headerMap: {
 *     '名称': 'name',
 *     '年龄': 'age'
 *   }
 * });
 */
function readTableAsJSON(filepath, options = {}) {
  if (!filepath) {
    throw new Error('文件路径不能为空');
  }
  
  const absolutePath = path.isAbsolute(filepath) ? filepath : path.resolve(filepath);
  
  if (!fs.existsSync(absolutePath)) {
    throw new Error(`文件不存在: ${absolutePath}`);
  }
  
  // 默认选项
  const {
    sheetName = null,
    headerRow = 0,
    skipRows = [],
    headerMap = {}
  } = options;
  
  // 读取Excel数据
  const excelData = addon.readExcel(absolutePath);
  
  // 选择目标Sheet
  let targetSheet;
  if (sheetName) {
    targetSheet = excelData.sheets.find(sheet => sheet.name === sheetName);
    if (!targetSheet) {
      throw new Error(`未找到名为 "${sheetName}" 的Sheet`);
    }
  } else {
    if (excelData.sheets.length === 0) {
      throw new Error('Excel文件中没有Sheet');
    }
    targetSheet = excelData.sheets[0];
  }
  
  const sheetData = targetSheet.data;
  
  // 检查数据是否足够
  if (sheetData.length <= headerRow) {
    throw new Error(`表头行索引 ${headerRow} 超出数据范围（共 ${sheetData.length} 行）`);
  }
  
  // 获取表头
  const headers = sheetData[headerRow];
  
  // 应用表头映射
  const mappedHeaders = headers.map(header => {
    return headerMap[header] || header;
  });
  
  // 创建跳过行的Set（包含表头行）
  const skipRowsSet = new Set([headerRow, ...skipRows]);
  
  // 构建JSON数组
  const result = [];
  
  for (let rowIndex = 0; rowIndex < sheetData.length; rowIndex++) {
    // 跳过指定的行
    if (skipRowsSet.has(rowIndex)) {
      continue;
    }
    
    const row = sheetData[rowIndex];
    const rowObj = {};
    
    // 填充数据
    for (let colIndex = 0; colIndex < mappedHeaders.length; colIndex++) {
      const header = mappedHeaders[colIndex];
      const value = row[colIndex] || '';
      
      // 只有在表头不为空时才添加属性
      if (header) {
        rowObj[header] = value;
      }
    }
    
    // 处理图片
    // 查找该行对应的图片（行号从1开始，因为Excel是1-based）
    const excelRowNumber = rowIndex + 1;
    const rowImages = findImagesForRow(
      excelData.images,
      excelData.imagePositions,
      targetSheet.name,
      excelRowNumber
    );
    
    // 根据图片所在列的表头添加图片属性
    if (rowImages.length > 0) {
      rowImages.forEach(img => {
        // img.col 是0-based的列索引
        const colIndex = img.col;
        
        // 获取该列的表头（映射后的）
        if (colIndex < mappedHeaders.length) {
          const columnHeader = mappedHeaders[colIndex];
          
          if (columnHeader) {
            // 检查该列是否已有图片
            if (rowObj[columnHeader] && typeof rowObj[columnHeader] === 'object' && rowObj[columnHeader].data) {
              // 该列已有图片，转换为数组
              if (!Array.isArray(rowObj[columnHeader])) {
                const existing = rowObj[columnHeader];
                rowObj[columnHeader] = [existing];
              }
              // 添加新图片
              rowObj[columnHeader].push({
                data: img.data,
                name: img.name,
                type: img.type
              });
            } else {
              // 该列还没有图片，直接添加
              rowObj[columnHeader] = {
                data: img.data,
                name: img.name,
                type: img.type
              };
            }
          }
        }
      });
    }
    
    result.push(rowObj);
  }
  
  return result;
}

/**
 * 查找指定行的图片
 * @private
 * 图片识别规则：
 * 1. 浮动图片：使用左上角坐标（from）所在的行和列
 * 2. 内嵌图片：使用图片所在单元格的行和列
 */
function findImagesForRow(images, imagePositions, sheetName, rowNumber) {
  const rowImages = [];
  
  // 遍历所有图片位置
  for (const pos of imagePositions) {
    // 检查是否在目标Sheet
    if (pos.sheet !== sheetName) {
      continue;
    }
    
    // 转换为1-based行号
    const imageStartRow = pos.from.row + 1;
    const imageEndRow = pos.to.row + 1;
    const imageStartCol = pos.from.col;
    const imageEndCol = pos.to.col;
    
    // 判断图片是否在该行
    let isInRow = false;
    let imageCol = imageStartCol; // 默认使用左上角的列
    
    // 情况1: 内嵌图片 - 起始行和结束行相同
    if (imageStartRow === imageEndRow && imageStartRow === rowNumber) {
      isInRow = true;
      // 内嵌图片：使用图片所在单元格的列（左上角）
      imageCol = imageStartCol;
    }
    // 情况2: 浮动图片 - 跨越多行
    else if (rowNumber >= imageStartRow && rowNumber <= imageEndRow) {
      // 浮动图片：只在左上角所在行识别
      if (rowNumber === imageStartRow) {
        isInRow = true;
        // 浮动图片：使用左上角坐标的列
        imageCol = imageStartCol;
      }
    }
    
    if (isInRow) {
      // 找到对应的图片数据
      // 优先精确匹配，然后尝试包含匹配
      let image = images.find(img => img.name === pos.image);
      
      if (!image) {
        // 如果精确匹配失败，尝试包含匹配（但要求双方都非空）
        image = images.find(img => 
          img.name && pos.image && 
          (img.name.includes(pos.image) || pos.image.includes(img.name))
        );
      }
      
      if (image) {
        rowImages.push({
          data: image.data,
          name: image.name,
          type: image.type,
          col: imageCol, // 图片所在列（0-based）
          position: pos
        });
      }
    }
  }
  
  // 按列排序
  rowImages.sort((a, b) => a.col - b.col);
  
  return rowImages;
}

/**
 * 读取表格为JSON（别名，更简洁）
 */
function 读取表格(filepath, options) {
  return readTableAsJSON(filepath, options);
}

/**
 * 读取表格Sheet名称（别名）
 */
function 读取表格SheetName(filepath) {
  return getSheetNames(filepath);
}

module.exports = {
  // 原始API
  readExcel,
  extractImages,
  
  // 新增高级API（英文）
  readTableAsJSON,
  getSheetNames,
  
  // 新增高级API（中文别名）
  读取表格,
  读取表格SheetName
};
