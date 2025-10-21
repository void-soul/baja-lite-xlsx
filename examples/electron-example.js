/**
 * Electron 使用示例
 * 
 * 本示例演示如何在 Electron 应用中使用 baja-lite-xlsx
 */

// 在 Electron 主进程或渲染进程中使用
const { readTableAsJSON, getSheetNames } = require('baja-lite-xlsx');
const path = require('path');
const fs = require('fs');

// ========================================
// 示例 1: 在 Electron 主进程中读取 Excel 文件
// ========================================

// 假设这是在 Electron 主进程的某个 IPC 处理器中
function readExcelInMainProcess(filePath) {
  try {
    console.log('📖 开始读取 Excel 文件:', filePath);
    
    // 1. 获取所有工作表名称
    const sheetNames = getSheetNames(filePath);
    console.log('📋 工作表:', sheetNames);
    
    // 2. 读取第一个工作表的数据
    const data = readTableAsJSON(filePath, {
      sheetName: sheetNames[0],
      headerRow: 0,
      imageColumns: ['photo', 'avatar', 'picture']
    });
    
    console.log(`✅ 成功读取 ${data.length} 行数据`);
    
    return {
      success: true,
      sheetNames,
      data
    };
  } catch (error) {
    console.error('❌ 读取失败:', error.message);
    return {
      success: false,
      error: error.message
    };
  }
}

// ========================================
// 示例 2: Electron IPC 通信
// ========================================

// 主进程代码 (main.js)
/*
const { app, BrowserWindow, ipcMain } = require('electron');
const { readTableAsJSON, getSheetNames } = require('baja-lite-xlsx');

ipcMain.handle('read-excel', async (event, filePath) => {
  try {
    const sheetNames = getSheetNames(filePath);
    const data = readTableAsJSON(filePath, {
      headerRow: 0,
      imageColumns: ['photo']
    });
    
    return {
      success: true,
      sheetNames,
      data
    };
  } catch (error) {
    return {
      success: false,
      error: error.message
    };
  }
});
*/

// 渲染进程代码 (renderer.js)
/*
const { ipcRenderer } = require('electron');

// 用户选择文件后
async function handleFileSelect(filePath) {
  const result = await ipcRenderer.invoke('read-excel', filePath);
  
  if (result.success) {
    console.log('工作表:', result.sheetNames);
    console.log('数据:', result.data);
    
    // 显示在界面上
    displayData(result.data);
  } else {
    console.error('读取失败:', result.error);
  }
}
*/

// ========================================
// 示例 3: 使用 dialog 选择文件
// ========================================

/*
const { dialog } = require('electron');

// 打开文件选择对话框
async function selectAndReadExcel() {
  const result = await dialog.showOpenDialog({
    properties: ['openFile'],
    filters: [
      { name: 'Excel Files', extensions: ['xlsx', 'xls'] }
    ]
  });
  
  if (!result.canceled && result.filePaths.length > 0) {
    const filePath = result.filePaths[0];
    return readExcelInMainProcess(filePath);
  }
  
  return null;
}
*/

// ========================================
// 示例 4: 读取应用资源目录中的 Excel
// ========================================

/*
const { app } = require('electron');

function readExcelFromResources() {
  // 获取应用资源路径
  const resourcePath = process.resourcesPath;
  const excelPath = path.join(resourcePath, 'data', 'example.xlsx');
  
  if (fs.existsSync(excelPath)) {
    return readTableAsJSON(excelPath, {
      headerRow: 0
    });
  } else {
    console.error('Excel 文件不存在:', excelPath);
    return [];
  }
}
*/

// ========================================
// 示例 5: 使用 Buffer（通过网络下载的 Excel）
// ========================================

/*
const https = require('https');

async function readExcelFromUrl(url) {
  return new Promise((resolve, reject) => {
    https.get(url, (response) => {
      const chunks = [];
      
      response.on('data', (chunk) => {
        chunks.push(chunk);
      });
      
      response.on('end', () => {
        try {
          const buffer = Buffer.concat(chunks);
          
          // 直接从 Buffer 读取
          const data = readTableAsJSON(buffer, {
            headerRow: 0
          });
          
          resolve(data);
        } catch (error) {
          reject(error);
        }
      });
    }).on('error', reject);
  });
}
*/

// ========================================
// 示例 6: 处理图片数据
// ========================================

function processImagesInElectron(data) {
  const imagesProcessed = data.map(row => {
    // 检查是否有图片
    if (row.photo && row.photo.data) {
      // 转换为 Data URL 供前端显示
      const base64 = row.photo.data.toString('base64');
      const dataUrl = `data:${row.photo.type};base64,${base64}`;
      
      return {
        ...row,
        photoUrl: dataUrl  // 可以直接在 <img> 标签中使用
      };
    }
    return row;
  });
  
  return imagesProcessed;
}

// ========================================
// 示例 7: 错误处理
// ========================================

function safeReadExcel(filePath, options = {}) {
  try {
    // 检查文件是否存在
    if (!fs.existsSync(filePath)) {
      throw new Error(`文件不存在: ${filePath}`);
    }
    
    // 检查文件扩展名
    const ext = path.extname(filePath).toLowerCase();
    if (ext !== '.xlsx' && ext !== '.xls') {
      throw new Error(`不支持的文件格式: ${ext}`);
    }
    
    // 读取数据
    const data = readTableAsJSON(filePath, options);
    
    // 验证数据
    if (!Array.isArray(data)) {
      throw new Error('读取结果不是数组');
    }
    
    return {
      success: true,
      data,
      rowCount: data.length
    };
  } catch (error) {
    console.error('❌ Excel 读取错误:', error);
    return {
      success: false,
      error: error.message,
      data: []
    };
  }
}

// ========================================
// 示例 8: 性能优化 - 大文件处理
// ========================================

function readLargeExcelFile(filePath, options = {}) {
  console.time('Excel读取耗时');
  
  try {
    const data = readTableAsJSON(filePath, {
      ...options,
      headerRow: 0
    });
    
    console.timeEnd('Excel读取耗时');
    console.log(`📊 共读取 ${data.length} 行数据`);
    
    return data;
  } catch (error) {
    console.timeEnd('Excel读取耗时');
    throw error;
  }
}

// ========================================
// 导出示例（如果作为模块使用）
// ========================================

module.exports = {
  readExcelInMainProcess,
  processImagesInElectron,
  safeReadExcel,
  readLargeExcelFile
};

// ========================================
// 使用说明
// ========================================

console.log(`
╔═══════════════════════════════════════════════════════════╗
║                                                           ║
║  🎯 Electron + baja-lite-xlsx 使用指南                     ║
║                                                           ║
╠═══════════════════════════════════════════════════════════╣
║                                                           ║
║  ✅ 支持的运行环境：                                       ║
║     • Electron 34+                                        ║
║     • Windows x64 (预编译包)                              ║
║     • macOS / Linux (源码编译)                            ║
║                                                           ║
║  📦 安装：                                                 ║
║     npm install baja-lite-xlsx                            ║
║                                                           ║
║  💡 推荐使用场景：                                         ║
║     • 主进程中读取 Excel 文件                              ║
║     • 通过 IPC 与渲染进程通信                              ║
║     • 处理用户上传的 Excel 文件                            ║
║     • 从网络下载并解析 Excel                               ║
║                                                           ║
║  ⚠️  注意事项：                                            ║
║     • Native 模块只能在主进程中直接 require                ║
║     • 如需在渲染进程使用，请通过 IPC 通信                   ║
║     • 确保 nodeIntegration 或 contextIsolation 配置正确    ║
║                                                           ║
║  📚 更多文档：                                             ║
║     https://github.com/void-soul/baja-lite-xlsx          ║
║                                                           ║
╚═══════════════════════════════════════════════════════════╝
`);

