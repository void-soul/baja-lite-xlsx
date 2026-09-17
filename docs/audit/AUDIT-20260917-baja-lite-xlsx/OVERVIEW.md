# 项目综述 — baja-lite-xlsx

## 定位与范围（Phase 0 结论）

| 项 | 内容 |
|----|------|
| **项目定位** | 本地（全库）：npm 原生模块库（Node-API C++ addon，xlnt 封装），无对外网络服务端口，数据来自用户本地传入的 xlsx 文件 |
| **标注项过滤** | 查【本地】+【通用】；【在线】项跳过并在报告标 N/A |
| **特别提示** | 解析不可信输入（任意 .xlsx 可能来自网络），文件路径安全 / 恶意输入内存安全按【通用】重点审查 |
| **审计范围** | src/（C++ 核心）、index.js、index.d.ts、scripts/、package.json；examples/*.js|ts 仅核对 API 契约一致性 |
| **排除目录** | prebuilds/（预编译二进制产物）、examples/output/（运行输出）、examples/*.xlsx（fixture 数据） |
| **架构原则** | ① JS 层不绕过 C++ 校验直接操作文件；② C++ 不依赖 V8 之外的环境；③ N-API 边界类型转换集中收口；④ 公共 API 与 index.d.ts 声明一致（用户确认于 2026-09-17） |
| **用户确认状态** | 已确认（2026-09-17） |

## 技术栈

| 层 | 技术 | 版本/说明 |
|----|------|-----------|
| 绑定层 | Node-API (node-addon-api) | ^8.0.0，napi 8；binding.gyp 定义 `NAPI_DISABLE_CPP_EXCEPTIONS` |
| 核心解析 | C++17 + xlnt | vcpkg 提供（xlnt 已基本停止维护） |
| ZIP/媒体 | libzip | 直接解析 xlsx 包内 `xl/media`、`xl/drawings`、`xl/cellimages.xml` |
| 高层 API | Node.js (CommonJS) | index.js 仅导出 `readTableAsJSON` |
| 构建/分发 | node-gyp + prebuild/prebuild-install | 发布 napi + electron 34 预编译产物（Windows 包内附 vcpkg DLL） |

## 子项目清单

单库项目，按层划分为模块（详见 STRUCTURE.md）：

| 模块 | 职责 | 架构角色 | 定位 | 依赖 | 风险初判 |
|------|------|----------|------|------|----------|
| `src/addon.cpp` | N-API 边界：参数校验、结果对象构造、图片归属挂载 | 边界层（Presentation↔Domain 之间） | 本地 | xlsx_reader | 业务逻辑混入边界层（D8-6）；同步阻塞（D2-1） |
| `src/xlsx_reader.*` | xlnt 封装：工作簿加载、全表读取、单元格转字符串 | 核心解析（Domain） | 本地 | xlnt, image_extractor | 无上限全量加载（D4-1）；静默吞异常（D3-1）；中文路径失效（D7-1） |
| `src/image_extractor.*` | libzip 直接解析媒体/锚点/WPS cellimages | 核心解析（Domain） | 本地 | libzip | 手写字符串式 XML 解析脆弱（D7-2）；506 行上帝文件（D8-2）；sheetName 硬编码（D5-3） |
| `index.js` | 高层 API：路径/Buffer/base64 归一化、临时文件、表→JSON | 应用层（Application） | 本地 | build/Release/baja_xlsx.node | base64 启发式误判面（D5-6） |
| `index.d.ts` | TS 类型契约 | 契约 | 本地 | — | 与实际导出一致（原则④满足） |
| `scripts/` | 安装检查、DLL 打包、预编译重打包 | 支撑（Infrastructure） | 本地 | — | 引用不存在脚本（D6-2）；vcpkg 逻辑 4 处重复（D8-4） |
| `examples/` | 使用示例 | 边缘 | 本地 | index | 大量引用不存在导出，开箱即崩（D5-2） |

## 架构图（文本）

```
consumer (npm 包使用者)
   │  require('baja-lite-xlsx')
   ▼
index.js  ──(路径/Buffer/base64 → 临时文件归一化)──┐
   │                                              │
   ▼                                              │
build/Release/baja_xlsx.node (N-API)              │
   ▼                                              │
src/addon.cpp ── ReadExcel / ExtractImages(死)    │
   ▼                                              │
src/xlsx_reader.cpp ── xlnt::workbook.load ───────┼──► 同一 .xlsx 文件被打开解析两次 (D4-2)
   ▼                                              │
src/image_extractor.cpp ── libzip (ZIP_RDONLY) ───┘
   ├── xl/media/*            → 图片字节
   ├── xl/drawings/*.xml     → 锚点(sheetName 硬编码"Sheet1")
   └── xl/cellimages.xml     → WPS DISPIMG 映射
```

关键数据流：
1. `readTableAsJSON(path|Buffer|base64, options)` → 归一化为文件路径 → 原生 `readExcel` → sheets（全字符串二维数组，图片单元格为 `{data,name,type}` 对象）→ JS 层按表头行映射为对象数组。
2. 图片归属三通道：WPS DISPIMG（`__IMAGE_CELL__:ID_xxx`）→ cellImageMappings；标准嵌入（oneCellAnchor）；浮动图（twoCellAnchor，挂到左上角单元格）。
3. 安装链：npm install → prebuild-install 下载（无校验和）→ postinstall 检查 Windows DLL。

## 关键结论

- **依赖方向单向、无环、无孤岛子项目**（examples 属演示性孤岛，归 D5-2 处理）。
- **原则④（d.ts 与实际导出一致）满足，但与 README/examples 严重脱节** —— 对外契约的"文档面"整体失真（D5-1/D5-2），是本次审计最高权重的工程问题之一。
- **原则③（N-API 边界收口）轻度违反**：图片归属业务逻辑位于 addon.cpp 边界层（D8-6）。
- 原生层存在**死代码链**（ExtractImages 导出 → 空占位实现），既是死代码也是静默失败源（D3-3/D8-1）。
