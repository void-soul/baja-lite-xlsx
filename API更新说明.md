# API 更新说明

## 更新内容

### 1. ✅ 删除中文别名

**已删除的 API：**
- ~~`读取表格(filepath, options)`~~ → 使用 `readTableAsJSON(input, options)`
- ~~`读取表格SheetName(filepath)`~~ → 使用 `getSheetNames(input)`

**原因：** 统一使用英文 API，提高代码可维护性和国际化支持。

---

### 2. ✅ 支持多种输入方式

**支持的输入格式：**

#### `readTableAsJSON(input, options)`
#### `getSheetNames(input)`

**`input` 参数现在支持三种类型：**

1. **文件路径（字符串）**
   ```javascript
   const data = readTableAsJSON('./sample.xlsx', options);
   ```

2. **Buffer 对象**
   ```javascript
   const buffer = fs.readFileSync('./sample.xlsx');
   const data = readTableAsJSON(buffer, options);
   ```

3. **Base64 字符串**
   ```javascript
   const base64 = fs.readFileSync('./sample.xlsx').toString('base64');
   const data = readTableAsJSON(base64, options);
   ```

---

## 技术实现

### 输入类型识别

库会自动识别输入类型：

- **Buffer：** 直接写入临时文件
- **Base64：** 检测特征（长度 > 500，包含特定文件头），解码后写入临时文件
- **文件路径：** 验证文件存在性后直接使用

### 临时文件管理

- Buffer 和 base64 输入会创建临时文件
- 使用完成后自动清理临时文件
- 临时文件位置：系统 `temp` 目录
- 文件名格式：`excel-{随机16位hex}.xlsx`

---

## 示例代码

### 1. 使用文件路径（兼容旧版）

```javascript
const { readTableAsJSON, getSheetNames } = require('baja-lite-xlsx');

// 获取 Sheet 名称
const names = getSheetNames('./sample.xlsx');
console.log(names); // ['Sheet1', 'Sheet2', 'Sheet3']

// 读取数据
const data = readTableAsJSON('./sample.xlsx', {
  sheetName: 'Sheet1',
  headerRow: 0,
  headerMap: {
    '名称': 'name',
    '年龄': 'age'
  }
});
```

### 2. 使用 Buffer

```javascript
const fs = require('fs');
const { readTableAsJSON, getSheetNames } = require('baja-lite-xlsx');

// 读取文件为 Buffer
const buffer = fs.readFileSync('./sample.xlsx');

// 使用 Buffer
const names = getSheetNames(buffer);
const data = readTableAsJSON(buffer, { headerRow: 0 });
```

### 3. 使用 Base64

```javascript
const fs = require('fs');
const { readTableAsJSON, getSheetNames } = require('baja-lite-xlsx');

// 转换为 base64
const buffer = fs.readFileSync('./sample.xlsx');
const base64 = buffer.toString('base64');

// 使用 base64
const names = getSheetNames(base64);
const data = readTableAsJSON(base64, { headerRow: 0 });
```

### 4. 从网络请求读取

```javascript
const axios = require('axios');
const { readTableAsJSON } = require('baja-lite-xlsx');

// 下载 Excel 文件
const response = await axios.get('https://example.com/file.xlsx', {
  responseType: 'arraybuffer'
});

// 直接使用 Buffer
const buffer = Buffer.from(response.data);
const data = readTableAsJSON(buffer, { headerRow: 0 });
```

---

## 迁移指南

### 从中文 API 迁移

**旧代码：**
```javascript
const { 读取表格, 读取表格SheetName } = require('baja-lite-xlsx');

const names = 读取表格SheetName('./sample.xlsx');
const data = 读取表格('./sample.xlsx', options);
```

**新代码：**
```javascript
const { readTableAsJSON, getSheetNames } = require('baja-lite-xlsx');

const names = getSheetNames('./sample.xlsx');
const data = readTableAsJSON('./sample.xlsx', options);
```

**注意：** 只需要修改函数名，参数和返回值完全一致。

---

## 性能说明

### 临时文件开销

- **文件路径：** 无额外开销
- **Buffer：** 需要写入临时文件（几毫秒）
- **Base64：** 需要解码 + 写入临时文件（取决于文件大小）

### 推荐使用场景

1. **文件路径：** 本地文件处理（最快）
2. **Buffer：** 网络请求、内存中的数据
3. **Base64：** API 传输、数据库存储

---

## 兼容性

- ✅ 完全向后兼容（文件路径方式）
- ✅ Node.js >= 12.0.0
- ✅ Windows / Linux / macOS

---

## 测试验证

所有功能已通过完整测试：

```bash
npm run example:json  # 运行示例
```

**测试覆盖：**
- ✅ 文件路径读取
- ✅ Buffer 读取
- ✅ Base64 读取
- ✅ 数据一致性验证
- ✅ 图片提取功能
- ✅ 临时文件清理

---

## 更新日志

**版本：** 1.0.3（待发布）

**日期：** 2025-10-20

**更改：**
1. 移除中文别名 API
2. `readTableAsJSON` 支持 Buffer 和 base64 输入
3. `getSheetNames` 支持 Buffer 和 base64 输入
4. 添加自动临时文件管理
5. 更新 TypeScript 类型定义
6. 更新示例代码

**破坏性更改：**
- 移除 `读取表格` 函数（使用 `readTableAsJSON` 替代）
- 移除 `读取表格SheetName` 函数（使用 `getSheetNames` 替代）

---

## 联系支持

如有问题，请提交 Issue 或查看文档：
- GitHub: [项目地址]
- 文档: `README.md`
- 示例: `examples/json-api.js`

