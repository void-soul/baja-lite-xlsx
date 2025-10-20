# JSON API 使用指南

## 概述

baja-lite-xlsx 提供了两套API：

1. **原始API** - 返回完整的Excel数据结构（sheets、images、positions）
2. **JSON API** - 直接返回JSON数组，自动处理图片（✨ 推荐使用）

## JSON API 详解

### 1. 获取所有Sheet名称

```javascript
const { getSheetNames } = require('baja-lite-xlsx');

// 或使用中文别名
const { 读取表格SheetName } = require('baja-lite-xlsx');

const names = getSheetNames('./sample.xlsx');
console.log(names); // ['Sheet1', 'Sheet2', 'Sheet3']
```

### 2. 读取表格为JSON数组

#### 基本用法

```javascript
const { readTableAsJSON } = require('baja-lite-xlsx');

// 最简单的用法 - 使用默认选项
const data = readTableAsJSON('./sample.xlsx');

// 结果示例:
// [
//   { '名称': '张三', '年龄': '25', 'Size': 'M' },
//   { '名称': '李四', '年龄': '30', 'Size': 'L' }
// ]
```

#### 完整配置

```javascript
const data = readTableAsJSON('./sample.xlsx', {
  sheetName: 'Sheet1',      // 可选，指定Sheet名称
  headerRow: 0,             // 可选，表头行索引（默认0）
  skipRows: [1, 2],         // 可选，跳过的行索引数组
  headerMap: {              // 可选，表头映射
    '名称': 'name',
    '年龄': 'age',
    'Size': 'size'
  },
  imageProperty: 'photo'    // 可选，图片属性名（默认'image'）
});

// 结果示例:
// [
//   { name: '张三', age: '25', size: 'M' },
//   { name: '李四', age: '30', size: 'L' }
// ]
```

#### 使用中文别名

```javascript
const { 读取表格 } = require('baja-lite-xlsx');

const data = 读取表格('./sample.xlsx', {
  sheetName: 'Sheet1',
  headerRow: 0,
  headerMap: {
    '名称': 'name',
    '年龄': 'age'
  }
});
```

## 配置选项详解

### sheetName (可选)

- **类型**: `string`
- **默认值**: `null` (读取第一个Sheet)
- **说明**: 指定要读取的Sheet名称

```javascript
// 读取第一个Sheet（默认）
const data1 = readTableAsJSON('./file.xlsx');

// 读取指定Sheet
const data2 = readTableAsJSON('./file.xlsx', {
  sheetName: 'Sheet2'
});
```

### headerRow (可选)

- **类型**: `number`
- **默认值**: `0`
- **说明**: 表头所在行的索引（从0开始）

```javascript
// 第一行是表头（默认）
const data1 = readTableAsJSON('./file.xlsx', {
  headerRow: 0
});

// 第三行是表头
const data2 = readTableAsJSON('./file.xlsx', {
  headerRow: 2
});
```

**Excel示例**:
```
第0行: 标题行（会被跳过）
第1行: 说明文字（会被跳过）
第2行: 名称 | 年龄 | 部门   ← headerRow: 2
第3行: 张三 | 25  | 技术部
第4行: 李四 | 30  | 市场部
```

### skipRows (可选)

- **类型**: `number[]`
- **默认值**: `[]`
- **说明**: 需要跳过的行索引数组（从0开始）

```javascript
// 跳过第2行和第4行（索引1和3）
const data = readTableAsJSON('./file.xlsx', {
  headerRow: 0,
  skipRows: [1, 3]
});
```

**注意**: 表头行会自动跳过，不需要在 `skipRows` 中指定

### headerMap (可选)

- **类型**: `Record<string, string>`
- **默认值**: `{}`
- **说明**: 将原始表头映射为新的属性名

```javascript
const data = readTableAsJSON('./file.xlsx', {
  headerMap: {
    '姓名': 'name',        // 原表头 → 新属性名
    '年龄': 'age',
    'Email': 'email',
    'Size': 'size'        // 没有映射的保持原样
  }
});

// 结果:
// [
//   { name: '张三', age: '25', email: 'zhang@example.com', Size: 'M' }
// ]
```

**使用场景**:
- 将中文表头转换为英文属性名
- 标准化属性名（驼峰命名等）
- 处理特殊字符或空格

## 图片如何添加到JSON

图片会**根据所在列的表头**自动添加为对应的属性名。

### 规则说明

1. **图片所在列的表头** = **JSON属性名**
2. **浮动图片**：使用左上角坐标所在的列
3. **内嵌图片**：使用图片所在单元格的列

### 示例

假设Excel表格如下：

```
  A列      B列    C列      D列
1 名称    年龄   photo1   photo2
2 张三    25    [图片1]
3 李四    30             [图片2]
```

读取代码：

```javascript
const data = readTableAsJSON('./file.xlsx', {
  headerMap: {
    '名称': 'name',
    '年龄': 'age'
  }
});
```

结果：

```javascript
[
  {
    name: '张三',
    age: '25',
    photo1: {               // 列名photo1
      data: Buffer,
      name: 'img1.png',
      type: 'image/png'
    }
  },
  {
    name: '李四',
    age: '30',
    photo2: {               // 列名photo2
      data: Buffer,
      name: 'img2.jpg',
      type: 'image/jpeg'
    }
  }
]
```

### 多张图片在同一列

如果同一列同一行有多张图片（虽然不常见），会转换为数组：

```javascript
{
  name: '王五',
  photo1: [
    { data: Buffer, name: 'img1.png', type: 'image/png' },
    { data: Buffer, name: 'img2.jpg', type: 'image/jpeg' }
  ]
}
```

## 图片识别详解

### 两种图片类型

#### 1. 浮动图片
以图片的**左上角坐标**所在单元格来识别属于哪一行和哪一列

```
  A列     B列     C列
1 名称    年龄    照片
2 张三    25     [图片左上角在这里 C2]
3 李四    30
```

- 图片会附加到**第2行**（张三）
- 属性名为 **"照片"**（C列的表头）

#### 2. 内嵌图片
以图片所在的单元格来识别

```
  A列     B列     C列
1 名称    年龄    照片  
2 张三    25     [图片完全在C2单元格内]
3 李四    30
```

- 图片会附加到**第2行**（张三）
- 属性名为 **"照片"**（C列的表头）

### 处理图片数据

```javascript
const data = readTableAsJSON('./file.xlsx', {
  headerMap: { '姓名': 'name' }
});

data.forEach(row => {
  console.log(`姓名: ${row.name}`);
  
  // 遍历所有属性，找到图片
  Object.keys(row).forEach(key => {
    const value = row[key];
    
    // 单张图片：{ data: Buffer, name: string, type: string }
    if (value && typeof value === 'object' && value.data && Buffer.isBuffer(value.data)) {
      console.log(`${key}: ${value.name} (${value.data.length} bytes, ${value.type})`);
      
      // 保存图片
      fs.writeFileSync(`${row.name}_${key}_${value.name}`, value.data);
    }
    
    // 多张图片：[{ data, name, type }, ...]
    if (Array.isArray(value) && value[0] && value[0].data) {
      console.log(`${key}: ${value.length} 张图片`);
      value.forEach((img, idx) => {
        console.log(`  - ${img.name} (${img.data.length} bytes)`);
        fs.writeFileSync(`${row.name}_${key}_${idx}_${img.name}`, img.data);
      });
    }
  });
});
```

### 访问特定列的图片

```javascript
const data = readTableAsJSON('./file.xlsx');

data.forEach(row => {
  // 直接访问photo1列的图片
  if (row.photo1) {
    console.log(`照片1: ${row.photo1.name}`);
    fs.writeFileSync(row.photo1.name, row.photo1.data);
  }
  
  // 访问photo2列的图片
  if (row.photo2) {
    console.log(`照片2: ${row.photo2.name}`);
    fs.writeFileSync(row.photo2.name, row.photo2.data);
  }
});
```

## 完整示例

假设有如下Excel文件（`employees.xlsx`）:

```
  A列      B列    C列     D列     E列
1 员工信息
2 姓名    年龄   部门    邮箱    头像
3 张三    25    技术部   zhang@example.com   [图片]
4 -------- 这行是分隔线，需要跳过 --------
5 李四    30    市场部   li@example.com
6 王五    28    技术部   wang@example.com    [图片]
```

读取代码:

```javascript
const { readTableAsJSON } = require('baja-lite-xlsx');
const fs = require('fs');

const employees = readTableAsJSON('./employees.xlsx', {
  sheetName: 'Sheet1',
  headerRow: 2,              // 第3行是表头（索引2）
  skipRows: [0, 1, 4],      // 跳过标题行、说明行、分隔线
  headerMap: {
    '姓名': 'name',
    '年龄': 'age',
    '部门': 'department',
    '邮箱': 'email',
    '头像': 'avatar'        // 将"头像"映射为"avatar"
  }
});

// 结果:
// [
//   {
//     name: '张三',
//     age: '25',
//     department: '技术部',
//     email: 'zhang@example.com',
//     avatar: {                    // E列表头"头像"映射后为"avatar"
//       data: Buffer,
//       name: 'photo1.png',
//       type: 'image/png'
//     }
//   },
//   {
//     name: '李四',
//     age: '30',
//     department: '市场部',
//     email: 'li@example.com'
//     // 没有图片
//   },
//   {
//     name: '王五',
//     age: '28',
//     department: '技术部',
//     email: 'wang@example.com',
//     avatar: {
//       data: Buffer,
//       name: 'photo2.jpg',
//       type: 'image/jpeg'
//     }
//   }
// ]

// 使用数据
employees.forEach(emp => {
  console.log(`${emp.name} - ${emp.department}`);
  
  if (emp.avatar) {
    // 保存员工头像
    const ext = emp.avatar.type.split('/')[1];
    const filename = `${emp.name}_avatar.${ext}`;
    fs.writeFileSync(`./photos/${filename}`, emp.avatar.data);
  }
});
```

## 原始API

如果需要更底层的控制，可以使用原始API：

```javascript
const { readExcel, extractImages } = require('baja-lite-xlsx');

// 读取完整数据
const data = readExcel('./file.xlsx');
console.log(data.sheets);         // 所有Sheet数据
console.log(data.images);         // 所有图片
console.log(data.imagePositions); // 所有图片位置

// 仅提取图片
const images = extractImages('./file.xlsx');
images.forEach(img => {
  fs.writeFileSync(img.name, img.data);
});
```

## 错误处理

```javascript
const { readTableAsJSON } = require('baja-lite-xlsx');

try {
  const data = readTableAsJSON('./file.xlsx', {
    sheetName: 'NotExists'
  });
} catch (error) {
  if (error.message.includes('未找到')) {
    console.error('Sheet不存在');
  } else if (error.message.includes('文件不存在')) {
    console.error('文件不存在');
  } else {
    console.error('读取错误:', error.message);
  }
}
```

## TypeScript 支持

```typescript
import { readTableAsJSON, ReadTableOptions } from 'baja-lite-xlsx';

interface Employee {
  name: string;
  age: string;
  department: string;
  email?: string;
  photo?: Buffer;
  photoName?: string;
  photoType?: string;
}

const options: ReadTableOptions = {
  sheetName: 'Sheet1',
  headerRow: 0,
  headerMap: {
    '姓名': 'name',
    '年龄': 'age',
    '部门': 'department'
  }
};

const employees = readTableAsJSON('./employees.xlsx', options) as Employee[];

employees.forEach((emp: Employee) => {
  console.log(`${emp.name} - ${emp.department}`);
});
```

## 更多示例

查看 `examples/json-api.js` 获取更多使用示例。

## 性能提示

1. **大文件**: 对于大型Excel文件，使用 `skipRows` 跳过不需要的行可以提高性能
2. **图片**: 如果不需要图片，原始图片数据仍会被读取，但不会影响JSON结果
3. **内存**: 图片会作为Buffer保存在内存中，处理大量图片时注意内存使用

## 常见问题

### Q: 如何处理空单元格？
A: 空单元格的值为空字符串 `""`

### Q: 数字会自动转换吗？
A: 不会，所有值都是字符串。如需数字，请自行转换：`Number(row.age)`

### Q: 支持公式吗？
A: 返回公式的计算结果（字符串形式）

### Q: 如何获取原始行号？
A: 可以通过索引计算：`实际行号 = 数组索引 + headerRow + 1 + 跳过的行数`

### Q: 图片为什么没有附加到数据中？
A: 检查：
1. 图片位置是否在有效行范围内
2. Sheet名称是否正确
3. 图片是否确实在Excel中

## 总结

JSON API 提供了简单直观的方式来读取Excel数据：

- ✅ 自动转换为JSON数组
- ✅ 灵活的表头映射
- ✅ 自动处理图片
- ✅ 支持跳过行
- ✅ 中文API别名
- ✅ TypeScript 支持

开始使用:

```javascript
const { readTableAsJSON } = require('baja-lite-xlsx');

const data = readTableAsJSON('./file.xlsx', {
  headerMap: { '姓名': 'name', '年龄': 'age' }
});

console.log(data);
```

