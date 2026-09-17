# 子项目分析 — baja-lite-xlsx（单库）

## 定位

| 项 | 说明 |
|----|------|
| **职责** | 高性能读取 .xlsx 工作表数据并提取嵌入/浮动图片（含 WPS DISPIMG 特有格式），以 `readTableAsJSON` 单一高层 API 输出 |
| **架构角色** | 基础库（被其他应用作为 npm 依赖消费） |
| **依赖关系** | 运行时依赖 node-addon-api / prebuild-install；原生依赖 xlnt / libzip（vcpkg）；无反向依赖方（本仓库即最终交付物） |
| **对外接口** | npm 包：`readTableAsJSON(input: string|Buffer, options)` → `Array<Record<string, string \| ImageDataObject>>`；预编译二进制（napi 8 / electron 34 × win32/linux/darwin） |
| **项目定位** | 本地（无网络服务；消费方可能在服务器端使用，此时 D1 资源上限问题影响面放大） |

## 模块分析

### addon.cpp（N-API 边界层）
- 职责：两个原生导出（`ReadExcel`/`ExtractImages`）、C++ 结果→JS 对象转换、图片归属挂载。
- 与架构原则关系：**原则③轻度违反** —— 归属判定（WPS ID 匹配/位置匹配/模糊匹配）是业务逻辑，混入转换层（D8-6）；转换层内两处近乎相同的挂载分支重复（D8-3）。
- 风险：同步阻塞（D2-1）、NAPI_DISABLE_CPP_EXCEPTIONS 下静默失败（D3-4）、子串模糊匹配挂错图（D7-3）。

### xlsx_reader（核心解析层）
- 职责：xlnt 加载工作簿、全表扫描、单元格→字符串（数值/布尔/公式/DISPIMG 特判）。
- 与架构原则关系：无越界；但存在死占位方法（D8-1）与未使用的 `getImageType`（D8-5）。
- 风险：无上限全量加载（D4-1）、静默吞异常（D3-1）、`std::to_string(double)` 精度失真（D5-4）、中文路径必现失败（D7-1）。

### image_extractor（媒体解析层）
- 职责：libzip 直读 `xl/media/*`、字符串式解析 relationships/drawing/cellimages XML。
- 与架构原则关系：无越界；为 506 行上帝文件（D8-2），twoCell/oneCell 解析重复（D8-3）。
- 风险：偏移魔数脆弱解析（D7-2）、sheetName 硬编码 "Sheet1"（D5-3）、无资源上限（D1-1）、四处吞异常（D3-2）。

### index.js（应用层）
- 职责：输入归一化（路径 / Buffer / base64 启发式）、临时文件生命周期、表头映射与跳行。
- 与架构原则关系：不绕过 C++（临时文件仍交原生层读取），原则①满足。
- 风险：base64 启发式误判面（D5-6）、`headerRow` 无校验（D5-5）。

## 发现的问题（Phase 1 级汇总）

| # | 问题 | 对应发现 |
|---|------|----------|
| 1 | 对外契约文档面失真：README 全套虚构 API、示例开箱即崩 | D5-1 / D5-2 |
| 2 | 原生死代码链：`ExtractImages` 导出 → 空占位实现 | D8-1 / D3-3 |
| 3 | 工具链断裂：npm test / test:prebuild 指向不存在文件 | D6-1 / D6-2 |
| 4 | 解析健壮性：手写偏移式 XML 解析 + 静默吞错 | D7-2 / D3-1 / D3-2 |
| 5 | 资源无上限：zip 炸弹 / 超大 dimension 全量加载 | D1-1 / D4-1 |

文件级发现（D1–D8 全量 31 条）见 `AUDIT-20260917-baja-lite-xlsx.md`。
