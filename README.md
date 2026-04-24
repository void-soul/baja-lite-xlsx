# Baja-Lite-XLSX

[![npm version](https://img.shields.io/npm/v/baja-lite-xlsx.svg)](https://www.npmjs.com/package/baja-lite-xlsx)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

高性能 Node.js Excel 文件读取和图片提取原生模块，基于 xlnt 库。

## 📋 目录

- [特性](#特性)
- [架构](#架构)
- [安装](#安装)
- [快速开始](#快速开始)
- [API 文档](#api-文档)
- [高级用法](#高级用法)

## ✨ 特性

- ⚡ **高性能**: 基于 C++ 原生模块，性能优异
- 📊 **完整支持**: 支持 .xlsx 格式的所有特性
- 🖼️ **图片提取**: 提取 Excel 中的图片资源
- 📝 **单元格读取**: 读取单元格值、格式、样式
- 📐 **工作表操作**: 读取多个工作表
- 🔢 **数据类型**: 支持字符串、数字、日期、布尔等类型
- 🎨 **样式信息**: 读取单元格样式和格式
- 🔗 **公式支持**: 读取单元格公式
- 💾 **内存优化**: 高效的内存使用
- 🌐 **跨平台**: 支持 Windows, Linux, macOS

## 🏗️ 架构

```
┌─────────────────────────────────────────────────────────┐
│                    Node.js Application                   │
│                   (JavaScript/TypeScript)                │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────┐
│                  JavaScript Binding Layer                │
│  ┌──────────────────────────────────────────────────┐   │
│  │  Node-API (N-API)                                │   │
│  │  - readWorkbook(path, options)                   │   │
│  │  - extractImages(path, outputDir)                │   │
│  │  - getWorksheetNames(path)                       │   │
│  │  - readWorksheet(path, sheetName)                │   │
│  └──────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────┐
│                    C++ Native Module                     │
│  ┌──────────────────────────────────────────────────┐   │
│  │  xlnt Library Wrapper                            │   │
│  │  - Workbook Loading                              │   │
│  │  - Worksheet Parsing                             │   │
│  │  - Cell Value Extraction                         │   │
│  │  - Image Resource Extraction                     │   │
│  │  - Memory Management                             │   │
│  └──────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────┐
│                      xlnt Library                        │
│  - XLSX File Parsing                                     │
│  - XML Processing                                        │
│  - ZIP Archive Handling                                  │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────┐
│                    Excel File (.xlsx)                    │
│  - Workbook Structure                                    │
│  - Worksheets                                            │
│  - Cells & Values                                        │
│  - Images & Media                                        │
└─────────────────────────────────────────────────────────┘
```

### 核心组件

1. **Node-API Binding**
   - JavaScript 与 C++ 的桥梁
   - 类型转换和错误处理
   - 异步操作支持

2. **xlnt Wrapper**
   - 封装 xlnt 库功能
   - 提供简化的 API
   - 内存管理和资源释放

3. **xlnt Library**
   - 底层 Excel 文件解析
   - XML 和 ZIP 处理
   - 高性能数据读取

## 📦 安装

### 自动安装（推荐）

```bash
npm install baja-lite-xlsx
```

模块会自动下载预编译的二进制文件。

### 从源码编译

如果自动安装失败，可以从源码编译：

#### Windows

```bash
# 安装 vcpkg
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg integrate install

# 安装依赖
.\vcpkg install xlnt:x64-windows

# 编译模块
npm install baja-lite-xlsx --build-from-source
```

#### Linux

```bash
# 安装依赖
sudo apt-get install build-essential cmake

# 安装 xlnt
git clone https://github.com/tfussell/xlnt.git
cd xlnt
mkdir build && cd build
cmake ..
make
sudo make install

# 编译模块
npm install baja-lite-xlsx --build-from-source
```

#### macOS

```bash
# 安装依赖
brew install cmake

# 安装 xlnt
brew install xlnt

# 编译模块
npm install baja-lite-xlsx --build-from-source
```

## 🚀 快速开始

### 1. 读取工作簿

```typescript
import { readWorkbook } from 'baja-lite-xlsx';

const workbook = readWorkbook('data.xlsx');

console.log('工作表数量:', workbook.sheets.length);
console.log('工作表名称:', workbook.sheetNames);

// 遍历所有工作表
workbook.sheets.forEach(sheet => {
  console.log(`工作表: ${sheet.name}`);
  console.log(`行数: ${sheet.rows.length}`);
  
  // 遍历所有行
  sheet.rows.forEach((row, rowIndex) => {
    console.log(`第 ${rowIndex + 1} 行:`, row);
  });
});
```

### 2. 读取特定工作表

```typescript
import { readWorksheet } from 'baja-lite-xlsx';

const sheet = readWorksheet('data.xlsx', 'Sheet1');

console.log('工作表名称:', sheet.name);
console.log('数据:', sheet.rows);
```

### 3. 提取图片

```typescript
import { extractImages } from 'baja-lite-xlsx';

const images = extractImages('data.xlsx', './output');

console.log('提取的图片数量:', images.length);

images.forEach(image => {
  console.log('图片路径:', image.path);
  console.log('图片类型:', image.type);
  console.log('图片大小:', image.size);
});
```

### 4. 获取工作表名称

```typescript
import { getWorksheetNames } from 'baja-lite-xlsx';

const names = getWorksheetNames('data.xlsx');

console.log('工作表名称:', names);
```

## 📚 API 文档

### readWorkbook(path, options?)

读取整个工作簿

```typescript
function readWorkbook(
  path: string,
  options?: ReadOptions
): Workbook;

interface ReadOptions {
  sheetNames?: string[];        // 只读取指定的工作表
  maxRows?: number;             // 每个工作表最大行数
  maxCols?: number;             // 每个工作表最大列数
  skipEmptyRows?: boolean;      // 跳过空行（默认 false）
  skipEmptyCells?: boolean;     // 跳过空单元格（默认 false）
  dateFormat?: string;          // 日期格式（默认 'YYYY-MM-DD'）
  includeFormulas?: boolean;    // 包含公式（默认 false）
  includeStyles?: boolean;      // 包含样式（默认 false）
}

interface Workbook {
  sheetNames: string[];         // 工作表名称列表
  sheets: Worksheet[];          // 工作表数组
  properties?: WorkbookProperties; // 工作簿属性
}

interface Worksheet {
  name: string;                 // 工作表名称
  rows: Row[];                  // 行数据
  rowCount: number;             // 行数
  columnCount: number;          // 列数
}

type Row = Cell[];

interface Cell {
  value: string | number | boolean | Date | null; // 单元格值
  type: CellType;               // 单元格类型
  formula?: string;             // 公式（如果有）
  style?: CellStyle;            // 样式（如果有）
}

enum CellType {
  Empty = 'empty',
  String = 'string',
  Number = 'number',
  Boolean = 'boolean',
  Date = 'date',
  Formula = 'formula'
}
```

**示例**:

```typescript
// 读取所有工作表
const workbook = readWorkbook('data.xlsx');

// 只读取指定工作表
const workbook = readWorkbook('data.xlsx', {
  sheetNames: ['Sheet1', 'Sheet2']
});

// 限制行列数
const workbook = readWorkbook('data.xlsx', {
  maxRows: 1000,
  maxCols: 50
});

// 跳过空行和空单元格
const workbook = readWorkbook('data.xlsx', {
  skipEmptyRows: true,
  skipEmptyCells: true
});

// 包含公式和样式
const workbook = readWorkbook('data.xlsx', {
  includeFormulas: true,
  includeStyles: true
});
```

### readWorksheet(path, sheetName, options?)

读取单个工作表

```typescript
function readWorksheet(
  path: string,
  sheetName: string,
  options?: ReadOptions
): Worksheet;
```

**示例**:

```typescript
const sheet = readWorksheet('data.xlsx', 'Sheet1');

console.log('工作表名称:', sheet.name);
console.log('行数:', sheet.rowCount);
console.log('列数:', sheet.columnCount);

// 访问特定单元格
const cell = sheet.rows[0][0];
console.log('A1 单元格值:', cell.value);
console.log('A1 单元格类型:', cell.type);
```

### extractImages(path, outputDir, options?)

提取 Excel 中的图片

```typescript
function extractImages(
  path: string,
  outputDir: string,
  options?: ExtractOptions
): ImageInfo[];

interface ExtractOptions {
  sheetNames?: string[];        // 只提取指定工作表的图片
  format?: 'original' | 'png' | 'jpg'; // 输出格式
  prefix?: string;              // 文件名前缀
}

interface ImageInfo {
  path: string;                 // 图片保存路径
  type: string;                 // 图片类型（png, jpg, etc.）
  size: number;                 // 文件大小（字节）
  width?: number;               // 图片宽度
  height?: number;              // 图片高度
  sheetName: string;            // 所在工作表
  position?: {                  // 图片位置
    row: number;
    col: number;
  };
}
```

**示例**:

```typescript
// 提取所有图片
const images = extractImages('data.xlsx', './images');

// 只提取指定工作表的图片
const images = extractImages('data.xlsx', './images', {
  sheetNames: ['Sheet1']
});

// 指定输出格式和前缀
const images = extractImages('data.xlsx', './images', {
  format: 'png',
  prefix: 'excel_'
});

// 处理提取的图片
images.forEach((image, index) => {
  console.log(`图片 ${index + 1}:`);
  console.log('  路径:', image.path);
  console.log('  类型:', image.type);
  console.log('  大小:', image.size, 'bytes');
  console.log('  位置:', image.sheetName, image.position);
});
```

### getWorksheetNames(path)

获取工作表名称列表

```typescript
function getWorksheetNames(path: string): string[];
```

**示例**:

```typescript
const names = getWorksheetNames('data.xlsx');

console.log('工作表列表:');
names.forEach((name, index) => {
  console.log(`  ${index + 1}. ${name}`);
});
```

### readWorkbookAsJSON(path, options?)

将工作簿读取为 JSON 格式

```typescript
function readWorkbookAsJSON(
  path: string,
  options?: JSONOptions
): Record<string, any[]>;

interface JSONOptions extends ReadOptions {
  header?: boolean;             // 第一行是否为表头（默认 true）
  headerRow?: number;           // 表头行号（默认 0）
  raw?: boolean;                // 是否保留原始值（默认 false）
}
```

**示例**:

```typescript
// 第一行作为表头
const data = readWorkbookAsJSON('data.xlsx');
// {
//   Sheet1: [
//     { name: 'John', age: 30, email: 'john@example.com' },
//     { name: 'Jane', age: 25, email: 'jane@example.com' }
//   ]
// }

// 不使用表头
const data = readWorkbookAsJSON('data.xlsx', {
  header: false
});
// {
//   Sheet1: [
//     ['name', 'age', 'email'],
//     ['John', 30, 'john@example.com'],
//     ['Jane', 25, 'jane@example.com']
//   ]
// }

// 指定表头行
const data = readWorkbookAsJSON('data.xlsx', {
  headerRow: 2  // 第 3 行作为表头
});
```

## 🔧 高级用法

### 1. 批量处理文件

```typescript
import { readWorkbook } from 'baja-lite-xlsx';
import { readdirSync } from 'fs';
import { join } from 'path';

function processExcelFiles(directory: string) {
  const files = readdirSync(directory)
    .filter(file => file.endsWith('.xlsx'));

  const results = [];

  for (const file of files) {
    const path = join(directory, file);
    console.log(`处理文件: ${file}`);

    try {
      const workbook = readWorkbook(path);
      results.push({
        file,
        sheetCount: workbook.sheets.length,
        totalRows: workbook.sheets.reduce(
          (sum, sheet) => sum + sheet.rowCount,
          0
        )
      });
    } catch (error) {
      console.error(`处理失败: ${file}`, error);
    }
  }

  return results;
}

const results = processExcelFiles('./data');
console.log('处理结果:', results);
```

### 2. 数据验证

```typescript
import { readWorksheet } from 'baja-lite-xlsx';

interface UserData {
  name: string;
  age: number;
  email: string;
}

function validateUserData(path: string): {
  valid: UserData[];
  invalid: any[];
} {
  const sheet = readWorksheet(path, 'Users');
  const valid: UserData[] = [];
  const invalid: any[] = [];

  // 跳过表头
  for (let i = 1; i < sheet.rows.length; i++) {
    const row = sheet.rows[i];
    const [name, age, email] = row.map(cell => cell.value);

    // 验证数据
    if (
      typeof name === 'string' &&
      typeof age === 'number' &&
      typeof email === 'string' &&
      email.includes('@')
    ) {
      valid.push({ name, age, email });
    } else {
      invalid.push({ row: i + 1, data: row });
    }
  }

  return { valid, invalid };
}

const result = validateUserData('users.xlsx');
console.log('有效数据:', result.valid.length);
console.log('无效数据:', result.invalid.length);
```

### 3. 数据转换

```typescript
import { readWorkbookAsJSON } from 'baja-lite-xlsx';

function convertToDatabase(path: string) {
  const data = readWorkbookAsJSON(path);

  const records = [];

  for (const [sheetName, rows] of Object.entries(data)) {
    for (const row of rows) {
      records.push({
        ...row,
        source: sheetName,
        importedAt: new Date()
      });
    }
  }

  return records;
}

const records = convertToDatabase('data.xlsx');
// 保存到数据库
// await db.insert('users', records);
```

### 4. 图片处理

```typescript
import { extractImages } from 'baja-lite-xlsx';
import sharp from 'sharp';
import { join } from 'path';

async function processImages(excelPath: string, outputDir: string) {
  const images = extractImages(excelPath, outputDir);

  for (const image of images) {
    // 生成缩略图
    const thumbnailPath = join(
      outputDir,
      'thumbnails',
      `thumb_${image.path.split('/').pop()}`
    );

    await sharp(image.path)
      .resize(200, 200, { fit: 'inside' })
      .toFile(thumbnailPath);

    console.log('生成缩略图:', thumbnailPath);
  }

  return images;
}

await processImages('data.xlsx', './images');
```

### 5. 流式读取大文件

```typescript
import { readWorksheet } from 'baja-lite-xlsx';

function* readLargeFile(path: string, sheetName: string, batchSize = 100) {
  let offset = 0;

  while (true) {
    const sheet = readWorksheet(path, sheetName, {
      maxRows: batchSize,
      skipEmptyRows: true
    });

    if (sheet.rows.length === 0) break;

    yield sheet.rows;
    offset += batchSize;
  }
}

// 使用
for (const batch of readLargeFile('large.xlsx', 'Sheet1')) {
  console.log('处理批次:', batch.length, '行');
  // 处理数据
}
```

### 6. 错误处理

```typescript
import { readWorkbook } from 'baja-lite-xlsx';

function safeReadWorkbook(path: string) {
  try {
    return readWorkbook(path);
  } catch (error) {
    if (error.code === 'ENOENT') {
      console.error('文件不存在:', path);
    } else if (error.message.includes('corrupted')) {
      console.error('文件损坏:', path);
    } else if (error.message.includes('password')) {
      console.error('文件受密码保护:', path);
    } else {
      console.error('读取失败:', error.message);
    }
    return null;
  }
}

const workbook = safeReadWorkbook('data.xlsx');
if (workbook) {
  // 处理数据
}
```

### 7. 性能优化

```typescript
import { readWorksheet } from 'baja-lite-xlsx';

// 只读取需要的列
function readSpecificColumns(
  path: string,
  sheetName: string,
  columns: number[]
) {
  const sheet = readWorksheet(path, sheetName, {
    skipEmptyRows: true
  });

  return sheet.rows.map(row =>
    columns.map(col => row[col]?.value)
  );
}

// 只读取前 N 行
function readFirstNRows(
  path: string,
  sheetName: string,
  n: number
) {
  return readWorksheet(path, sheetName, {
    maxRows: n
  });
}

// 使用
const data = readSpecificColumns('data.xlsx', 'Sheet1', [0, 2, 4]);
const preview = readFirstNRows('data.xlsx', 'Sheet1', 10);
```

## 📝 最佳实践

### 1. 内存管理

```typescript
// 处理大文件时分批读取
function processBatch(path: string, batchSize = 1000) {
  const names = getWorksheetNames(path);

  for (const name of names) {
    let processed = 0;

    while (true) {
      const sheet = readWorksheet(path, name, {
        maxRows: batchSize,
        skipEmptyRows: true
      });

      if (sheet.rows.length === 0) break;

      // 处理批次
      processBatchData(sheet.rows);

      processed += sheet.rows.length;
      console.log(`已处理 ${processed} 行`);
    }
  }
}
```

### 2. 类型安全

```typescript
interface ExcelRow {
  [key: string]: string | number | boolean | Date | null;
}

function readTypedData<T extends ExcelRow>(
  path: string,
  sheetName: string,
  mapper: (row: Cell[]) => T
): T[] {
  const sheet = readWorksheet(path, sheetName);

  return sheet.rows.slice(1).map(mapper);
}

// 使用
interface User {
  name: string;
  age: number;
  email: string;
}

const users = readTypedData<User>('users.xlsx', 'Sheet1', (row) => ({
  name: String(row[0].value),
  age: Number(row[1].value),
  email: String(row[2].value)
}));
```

### 3. 错误恢复

```typescript
function readWithRetry(
  path: string,
  maxRetries = 3
): Workbook | null {
  for (let i = 0; i < maxRetries; i++) {
    try {
      return readWorkbook(path);
    } catch (error) {
      console.warn(`尝试 ${i + 1} 失败:`, error.message);
      if (i === maxRetries - 1) {
        console.error('达到最大重试次数');
        return null;
      }
      // 等待后重试
      await new Promise(resolve => setTimeout(resolve, 1000));
    }
  }
  return null;
}
```

## 🔍 故障排除

### Windows

```bash
# 如果遇到编译错误
npm install --global windows-build-tools

# 重新编译
npm rebuild baja-lite-xlsx
```

### Linux

```bash
# 安装依赖
sudo apt-get install build-essential cmake libssl-dev

# 重新编译
npm rebuild baja-lite-xlsx
```

### macOS

```bash
# 安装 Xcode Command Line Tools
xcode-select --install

# 重新编译
npm rebuild baja-lite-xlsx
```

## 📄 License

MIT

## 🤝 贡献

欢迎提交 Issue 和 Pull Request！

## 📮 联系

- GitHub: [void-soul/baja-lite-xlsx](https://github.com/void-soul/baja-lite-xlsx)
- NPM: [baja-lite-xlsx](https://www.npmjs.com/package/baja-lite-xlsx)
