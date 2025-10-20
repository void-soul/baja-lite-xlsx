# Baja-XLSX 项目总结

## 项目概述

这是一个Node.js原生模块，用于读取Excel (.xlsx) 文件和提取其中的图片。该模块使用C++编写，通过node-addon-api绑定到Node.js。

## 技术栈

- **C++ 库**:
  - `xlnt` - 用于读取.xlsx文件的单元格数据
  - `libzip` - 用于直接访问.xlsx ZIP结构提取图片
  - `node-addon-api` - Node.js C++ 绑定

- **构建工具**:
  - `node-gyp` - 原生模块构建系统
  - `vcpkg` (Windows) - C++包管理器

## 项目结构

```
baja-lite-xlsx/
├── src/                      # C++ 源代码
│   ├── addon.cpp            # Node.js 绑定入口
│   ├── xlsx_reader.h/cpp    # Excel 读取核心逻辑
│   └── image_extractor.h/cpp # 图片提取实现
│
├── examples/                 # 使用示例
│   ├── basic.js             # 基础用法
│   ├── advanced.js          # 高级用法
│   └── typescript-example.ts # TypeScript 示例
│
├── test/                     # 测试文件
│   ├── test.js              # 测试脚本
│   └── README.md            # 测试说明
│
├── scripts/                  # 辅助脚本
│   ├── install-deps-windows.bat
│   ├── install-deps-linux.sh
│   └── install-deps-macos.sh
│
├── .github/workflows/        # CI/CD 配置
│   └── build.yml            # GitHub Actions
│
├── index.js                  # JavaScript API 入口
├── index.d.ts               # TypeScript 类型定义
├── binding.gyp              # node-gyp 构建配置
├── package.json             # npm 项目配置
│
├── README.md                # 完整文档
├── INSTALL.md               # 详细安装指南
├── QUICKSTART.md            # 快速入门
├── CHANGELOG.md             # 版本更新日志
└── LICENSE                  # MIT 许可证
```

## 核心功能

### 1. 读取Excel数据 (`readExcel`)

```javascript
const data = readExcel('./file.xlsx');
// 返回: { sheets, images, imagePositions }
```

- 读取所有工作表的单元格数据
- 提取所有嵌入的图片
- 获取图片在工作表中的位置信息

### 2. 提取图片 (`extractImages`)

```javascript
const images = extractImages('./file.xlsx');
// 返回: [{ name, data: Buffer, type }]
```

- 仅提取图片数据
- 返回Buffer格式，方便处理

## 实现细节

### Excel 数据读取
使用 `xlnt` 库：
- 打开.xlsx文件
- 遍历所有工作表
- 读取单元格数据并转换为字符串数组

### 图片提取
直接解析.xlsx ZIP结构：
- 使用 `libzip` 打开.xlsx文件（本质是ZIP）
- 从 `xl/media/` 目录提取图片文件
- 从 `xl/drawings/*.xml` 解析图片位置信息

### Node.js 绑定
使用 `node-addon-api`：
- 将C++函数暴露给JavaScript
- 处理数据类型转换（C++ ↔ JavaScript）
- 错误处理和异常传递

## API 设计

### readExcel(filepath)

**输入**: 字符串 - Excel 文件路径

**输出**: 对象
```javascript
{
  sheets: [
    { name: string, data: string[][] }
  ],
  images: [
    { name: string, data: Buffer, type: string }
  ],
  imagePositions: [
    {
      image: string,
      sheet: string,
      from: { col: number, row: number },
      to: { col: number, row: number }
    }
  ]
}
```

### extractImages(filepath)

**输入**: 字符串 - Excel 文件路径

**输出**: 数组
```javascript
[
  { name: string, data: Buffer, type: string }
]
```

## 构建流程

1. **准备阶段**:
   - 检查依赖库（xlnt, libzip）
   - 配置编译器路径

2. **编译阶段**:
   - node-gyp 配置项目
   - C++ 编译器编译源文件
   - 链接 xlnt 和 libzip 库

3. **生成产物**:
   - `build/Release/baja_xlsx.node` - 原生模块

## 平台支持

### Windows
- Visual Studio 2019+ 构建工具
- vcpkg 管理依赖库
- 支持 x64 架构

### Linux
- GCC/G++ 编译器
- 从源码或包管理器安装依赖
- 支持主流发行版

### macOS
- Xcode Command Line Tools
- Homebrew 管理依赖
- 支持 10.15+

## 依赖关系

```
baja-lite-xlsx
├── Node.js (>= 16.0.0)
├── Python 3.x (node-gyp 需要)
├── C++ 编译器
│   ├── Windows: MSVC (VS 2019+)
│   ├── Linux: GCC/G++
│   └── macOS: Clang (Xcode)
├── xlnt (C++ 库)
│   └── 依赖: C++17, zip 支持
└── libzip (C++ 库)
    └── 依赖: zlib
```

## 性能考虑

- **原生性能**: C++ 实现，比纯 JavaScript 快得多
- **内存管理**: 使用 RAII 和智能指针
- **大文件支持**: 流式读取，避免全部加载到内存
- **并发**: 可以在多个 Worker 线程中使用

## 未来改进方向

1. **图片位置映射**: 改进图片名称与位置的关联
2. **更多格式**: 支持 .xls (老格式)
3. **写入功能**: 支持创建和修改 Excel 文件
4. **样式信息**: 读取单元格样式和格式
5. **公式计算**: 支持读取和计算公式

## 测试策略

- **单元测试**: 测试单个功能模块
- **集成测试**: 测试完整读取流程
- **平台测试**: 在 Windows/Linux/macOS 上测试
- **CI/CD**: GitHub Actions 自动构建

## 已知限制

1. xlnt 库对图片的 API 支持有限，因此使用 libzip 直接解析
2. 图片位置解析依赖 XML 结构，可能不是所有 Excel 版本都完全兼容
3. 目前仅支持 .xlsx 格式，不支持 .xls
4. 单元格值全部转换为字符串

## 文档

- **README.md**: 完整的使用文档和 API 参考
- **INSTALL.md**: 详细的安装步骤（各平台）
- **QUICKSTART.md**: 快速入门指南
- **examples/**: 丰富的使用示例
- **index.d.ts**: TypeScript 类型定义

## 贡献指南

欢迎贡献！请确保：
1. 代码遵循 C++17 标准
2. 添加适当的错误处理
3. 更新文档和测试
4. 在所有平台上测试

## 许可证

MIT License - 可自由使用、修改和分发


