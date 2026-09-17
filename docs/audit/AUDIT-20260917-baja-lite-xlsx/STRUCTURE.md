# 项目结构 — baja-lite-xlsx

## 目录树（标注审计范围）

```
baja-lite-xlsx/
├── src/                        【审计】C++ 核心
│   ├── addon.cpp               (302 行) N-API 边界层
│   ├── xlsx_reader.h/.cpp      (83/233 行) xlnt 封装、全表读取
│   └── image_extractor.h/.cpp  (77/506 行) libzip 媒体/锚点/WPS 解析
├── index.js                    【审计】(212 行) 高层 API
├── index.d.ts                  【审计】(130 行) TS 契约
├── package.json                【审计】构建/安装脚本定义
├── package-prebuild.json       【审计】发布变体模板
├── scripts/                    【审计】构建与安装支撑
│   ├── binding.gyp             node-gyp 配置
│   ├── postinstall.js          安装后 DLL 检查
│   ├── package-dlls.js         vcpkg DLL 复制
│   ├── pack-dlls-into-prebuild.js  DLL 打入预编译包
│   └── *.bat / *.sh            平台构建脚本（抽查）
├── examples/                   【审计·仅契约核对】
│   ├── basic.js / json-api.js / electron-example.js  ← 引用不存在导出（D5-2）
│   ├── embedded-images-example.js / test.js / typescript-example.ts
│   ├── output/                 【排除】运行输出产物
│   └── *.xlsx                  【排除】fixture 数据
├── prebuilds/                  【排除】预编译二进制产物
├── README.md                   【审计】文档契约（与实现严重脱节，D5-1）
└── yarn.lock / tsconfig.json / LICENSE
```

注：`package.json` 引用的 `test/test.js` 与 `scripts/test-prebuild-package.js` **不存在**（D6-1/D6-2）。

## 模块清单（单子项目内部划分）

| 模块 | 职责 | 对外暴露 | 依赖 | 级别 |
|------|------|----------|------|------|
| `addon.cpp` | N-API 边界：入参校验、C++→JS 类型转换、图片归属挂载 | 原生导出 `readExcel`、`extractImages`（后者为死链） | xlsx_reader | 核心 |
| `xlsx_reader` | 工作簿加载、全表扫描、单元格→字符串（含 DISPIMG 标记） | `XlsxReader` 类 | xlnt, image_extractor | 核心 |
| `image_extractor` | ZIP 直读媒体、drawing/cellimages XML 解析 | `ImageExtractor` 类 | libzip | 核心 |
| `index.js` | 输入归一化（路径/Buffer/base64）、临时文件生命周期、表头映射 | `readTableAsJSON` | 原生 .node | 核心 |
| `index.d.ts` | 类型契约 | `readTableAsJSON`、`ReadTableOptions`、`ImageDataObject` | — | 核心（契约） |
| `scripts/*` | 安装期 DLL 检查、预编译打包 | npm scripts | vcpkg 本机状态 | 支撑 |
| `examples/*` | 用法演示 | — | index（部分引用断裂） | 边缘 |

核心模块（Phase 2 深入对象）：addon.cpp、xlsx_reader、image_extractor、index.js —— 四者均已逐文件审计完毕。

## 文件规模与复杂度热点

| 文件 | 行数 | 判定 |
|------|------|------|
| image_extractor.cpp | 506 | 500-999 区间（D8-2，建议三拆） |
| addon.cpp | 302 | 正常，但 `sheetsToArray` 复杂度偏高且含重复分支（D8-3） |
| xlsx_reader.cpp | 233 | 正常 |
| index.js | 212 | 正常 |
