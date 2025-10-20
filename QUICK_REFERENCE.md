# baja-lite-xlsx 快速参考

## 安装

```bash
npm install
```

## API 速查

### 获取Sheet名称

```javascript
const { getSheetNames } = require('baja-lite-xlsx');
const sheets = getSheetNames('./file.xlsx');
// ['Sheet1', 'Sheet2']
```

### 读取为JSON

```javascript
const { readTableAsJSON } = require('baja-lite-xlsx');

const data = readTableAsJSON('./file.xlsx', {
  sheetName: 'Sheet1',     // 可选
  headerRow: 0,            // 可选，默认0
  skipRows: [1, 2],        // 可选，默认[]
  headerMap: {             // 可选，默认{}
    '姓名': 'name',
    '年龄': 'age'
  }
});
```

## 图片处理规则

### ✅ 图片自动命名

- **图片属性名 = 图片所在列的表头**
- 浮动图片：使用左上角坐标的列
- 内嵌图片：使用所在单元格的列

### 示例

Excel表格：
```
| 姓名  | photo1 | photo2 |
| 张三  | [图片] |        |
| 李四  |        | [图片] |
```

结果：
```javascript
[
  {
    '姓名': '张三',
    photo1: {
      data: Buffer,
      name: 'img1.png',
      type: 'image/png'
    }
  },
  {
    '姓名': '李四',
    photo2: {
      data: Buffer,
      name: 'img2.jpg',
      type: 'image/jpeg'
    }
  }
]
```

## 使用图片

### 访问特定列

```javascript
data.forEach(row => {
  if (row.photo1) {
    fs.writeFileSync(row.photo1.name, row.photo1.data);
  }
});
```

### 遍历所有图片

```javascript
data.forEach(row => {
  Object.keys(row).forEach(key => {
    const val = row[key];
    if (val && val.data && Buffer.isBuffer(val.data)) {
      // 这是一张图片
      console.log(`${key}: ${val.name}`);
    }
  });
});
```

## 常用场景

### 场景1：简单读取

```javascript
const data = readTableAsJSON('./file.xlsx');
```

### 场景2：英文属性名

```javascript
const data = readTableAsJSON('./file.xlsx', {
  headerMap: {
    '姓名': 'name',
    '年龄': 'age',
    '照片': 'photo'
  }
});
```

### 场景3：跳过标题行

```javascript
const data = readTableAsJSON('./file.xlsx', {
  headerRow: 2,        // 第3行是表头
  skipRows: [0, 1]     // 跳过前两行
});
```

### 场景4：中文API

```javascript
const { 读取表格 } = require('baja-lite-xlsx');
const data = 读取表格('./file.xlsx');
```

## 数据结构

### 普通数据

```javascript
{
  name: '张三',
  age: '25'
}
```

### 包含图片

```javascript
{
  name: '张三',
  age: '25',
  photo: {
    data: Buffer,      // 图片Buffer
    name: 'img.png',   // 文件名
    type: 'image/png'  // MIME类型
  }
}
```

### 多张图片（同一列）

```javascript
{
  name: '张三',
  photos: [
    { data: Buffer, name: 'img1.png', type: 'image/png' },
    { data: Buffer, name: 'img2.jpg', type: 'image/jpeg' }
  ]
}
```

## 中文API

```javascript
const { 
  读取表格,
  读取表格SheetName 
} = require('baja-lite-xlsx');

const sheets = 读取表格SheetName('./file.xlsx');
const data = 读取表格('./file.xlsx', { /* 选项 */ });
```

## 原始API

需要完整控制时使用：

```javascript
const { readExcel, extractImages } = require('baja-lite-xlsx');

// 完整数据
const excel = readExcel('./file.xlsx');
// { sheets: [...], images: [...], imagePositions: [...] }

// 仅图片
const images = extractImages('./file.xlsx');
// [{ name, data, type }, ...]
```

## 配置选项

| 选项 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `sheetName` | string | null | Sheet名称 |
| `headerRow` | number | 0 | 表头行索引 |
| `skipRows` | number[] | [] | 跳过的行索引 |
| `headerMap` | object | {} | 表头映射 |

## 文档链接

- 📖 [API_USAGE_CN.md](API_USAGE_CN.md) - 完整文档
- 📖 [README.zh-CN.md](README.zh-CN.md) - 主文档
- 💡 [examples/json-api.js](examples/json-api.js) - 示例代码
- 🚀 [NEW_API_SUMMARY.md](NEW_API_SUMMARY.md) - 新API说明

## 常见问题

**Q: 如何判断某个属性是图片？**

```javascript
if (row.photo && row.photo.data && Buffer.isBuffer(row.photo.data)) {
  // 这是图片
}
```

**Q: 如何保存所有图片？**

```javascript
Object.keys(row).forEach(key => {
  const val = row[key];
  if (val?.data && Buffer.isBuffer(val.data)) {
    fs.writeFileSync(val.name, val.data);
  }
});
```

**Q: 表头映射后，图片属性名会变吗？**

会！图片属性名使用**映射后的表头**。

```javascript
// headerMap: { '照片': 'avatar' }
// 图片会在 row.avatar 中
```

---

**快速开始**: `npm run example:json`

