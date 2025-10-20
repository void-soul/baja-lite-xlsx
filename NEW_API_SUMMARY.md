# 图片处理新方案 - 实现总结

## ✅ 已完成修改

按照您的要求，图片现在**根据所在列的表头自动命名**，不再需要 `imageProperty` 参数。

## 📸 图片处理逻辑

### 核心规则

1. **图片属性名 = 图片所在列的表头**
2. **浮动图片**：使用左上角坐标（`from.col`，`from.row`）
3. **内嵌图片**：使用所在单元格坐标（`from.col`，`from.row`）

### 示例说明

#### Excel 表格：

```
  A列    B列    C列      D列
1 姓名   年龄   photo1   photo2
2 张三   25    [图片1]   
3 李四   30             [图片2]
```

#### 代码：

```javascript
const { readTableAsJSON } = require('baja-lite-xlsx');

const data = readTableAsJSON('./file.xlsx', {
  headerMap: {
    '姓名': 'name',
    '年龄': 'age'
  }
});
```

#### 结果：

```javascript
[
  {
    name: '张三',
    age: '25',
    photo1: {              // ← 列名photo1自动作为属性名
      data: Buffer,
      name: '1.png',
      type: 'image/png'
    }
  },
  {
    name: '李四',
    age: '30',
    photo2: {              // ← 列名photo2自动作为属性名
      data: Buffer,
      name: '2.jpg',
      type: 'image/jpeg'
    }
  }
]
```

## 🔄 API 变化

### 移除的参数

- ❌ `imageProperty` - 已移除，不再需要

### 配置选项

现在只有4个选项：

```javascript
{
  sheetName: string,           // 可选，Sheet名称
  headerRow: number,           // 可选，表头行索引，默认0
  skipRows: number[],          // 可选，跳过的行索引数组
  headerMap: Record<string, string>  // 可选，表头映射
}
```

## 💡 使用示例

### 示例1：基本用法

```javascript
const data = readTableAsJSON('./file.xlsx');

// 访问图片
data.forEach(row => {
  if (row.photo1) {
    console.log(row.photo1.name);  // 图片文件名
    fs.writeFileSync(row.photo1.name, row.photo1.data);
  }
});
```

### 示例2：表头映射

```javascript
const data = readTableAsJSON('./file.xlsx', {
  headerMap: {
    '照片1': 'avatar',  // photo列映射为avatar
    '姓名': 'name'
  }
});

// 结果：
// {
//   name: '张三',
//   avatar: { data: Buffer, name: 'img.png', type: 'image/png' }
// }
```

### 示例3：多列图片

```javascript
// Excel:
// | 姓名  | photo1 | photo2 | photo3 |
// | 张三  | [图1]  | [图2]  | [图3]  |

const data = readTableAsJSON('./file.xlsx');

// 结果：
// {
//   '姓名': '张三',
//   photo1: { data: Buffer, ... },
//   photo2: { data: Buffer, ... },
//   photo3: { data: Buffer, ... }
// }
```

### 示例4：自动遍历所有图片

```javascript
const data = readTableAsJSON('./file.xlsx');

data.forEach(row => {
  Object.keys(row).forEach(key => {
    const value = row[key];
    
    // 检查是否是图片
    if (value && value.data && Buffer.isBuffer(value.data)) {
      console.log(`${key}: ${value.name}`);
      fs.writeFileSync(`${key}_${value.name}`, value.data);
    }
  });
});
```

## 📁 修改的文件

### 核心实现
- ✅ `index.js`
  - 移除 `imageProperty` 参数
  - 修改图片处理逻辑，根据列索引获取表头
  - 更新 `findImagesForRow` 函数

### 类型定义
- ✅ `index.d.ts`
  - 移除 `imageProperty` 选项
  - 添加 `ImageDataObject` 接口
  - 更新文档注释和示例

### 文档
- ✅ `API_USAGE_CN.md`
  - 移除 `imageProperty` 部分
  - 添加"图片如何添加到JSON"章节
  - 更新所有示例代码

### 示例
- ✅ `examples/json-api.js`
  - 更新为新的图片访问方式
  - 展示如何遍历图片列

## 🎯 关键改进

### 之前（使用 imageProperty）

```javascript
const data = readTableAsJSON('./file.xlsx', {
  imageProperty: 'photo'  // 所有图片都叫photo
});

// 结果：
// {
//   name: '张三',
//   photo: Buffer,          // 无法区分是哪一列的图片
//   photoName: 'img.png',
//   photoType: 'image/png'
// }
```

### 现在（根据列名自动命名）

```javascript
const data = readTableAsJSON('./file.xlsx');

// 结果：
// {
//   name: '张三',
//   photo1: {                // 清晰知道是photo1列的图片
//     data: Buffer,
//     name: 'img.png',
//     type: 'image/png'
//   },
//   photo2: {                // 清晰知道是photo2列的图片
//     data: Buffer,
//     name: 'img2.jpg',
//     type: 'image/jpeg'
//   }
// }
```

## ✨ 优势

1. **更直观** - 属性名与Excel列名一致
2. **支持多列图片** - 每列图片有独立属性
3. **更简洁** - 不需要额外的 `imageProperty` 参数
4. **更灵活** - 可以通过 `headerMap` 映射图片列名

## 🧪 测试

```bash
# 测试API是否正常
node test-json-api.js

# 运行示例（需要先创建test/sample.xlsx）
npm run example:json
```

## 📖 文档

详细使用方法请查看：
- [API_USAGE_CN.md](API_USAGE_CN.md) - 完整API文档
- [README.zh-CN.md](README.zh-CN.md) - 主文档
- [examples/json-api.js](examples/json-api.js) - 示例代码

## ✅ 完成状态

所有修改已完成并测试通过！

- ✅ 移除 `imageProperty` 参数
- ✅ 图片根据列名自动命名
- ✅ 支持浮动图片和内嵌图片
- ✅ 更新所有文档和示例
- ✅ TypeScript 类型定义更新
- ✅ 功能测试通过

🎉 **API 已按您的要求完成修改！**

