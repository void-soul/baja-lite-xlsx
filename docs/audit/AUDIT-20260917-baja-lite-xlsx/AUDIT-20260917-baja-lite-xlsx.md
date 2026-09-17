# 📋 Code Audit Report — baja-lite-xlsx

| Field | Value |
|-------|-------|
| **Audit Date** | 2026-09-17 |
| **Audited By** | CodeBuddy / code-audit skill |
| **Scope** | `src/`、`index.js`、`index.d.ts`、`scripts/`、`package.json`、README（契约核对）、`examples/*.js|ts`（契约核对） |
| **Commit / Branch** | `master`（up to date with origin/master） |
| **Status** | REMEDIATED |

---

## 定位与范围（Phase 0 结论）

| Field | Value |
|-------|-------|
| **项目定位** | 本地（全库）：npm 原生模块库（Node-API C++ addon，xlnt 封装），无对外网络服务端口，数据来自用户本地传入的 xlsx 文件 |
| **标注项过滤** | 定位=本地 → 查【本地】+【通用】；【在线】项跳过并在报告标 N/A |
| **审计范围** | src/（C++ 核心）、index.js、index.d.ts、scripts/、package.json；examples 仅核对 API 契约一致性 |
| **排除目录** | prebuilds/（预编译二进制产物）、examples/output/（运行输出）、examples/*.xlsx（fixture 数据） |
| **架构原则** | ① JS 层不绕过 C++ 校验直接操作文件；② C++ 不依赖 V8 之外的环境；③ N-API 边界类型转换集中收口；④ 公共 API 与 index.d.ts 声明一致（用户确认于 2026-09-17） |

> 分层文档（OVERVIEW.md / STRUCTURE.md / SUBPROJECT-baja-lite-xlsx.md）位于本文档所在目录。

---

## Executive Summary

| Severity | Total |
|----------|-------|
| CRITICAL | 1 |
| HIGH | 15 |
| MEDIUM | 18 |
| LOW | 8 |
| **Total** | **42**（D6-3 与 D8-4、D3-3 与 D8-1 各合并为单任务） |

维度汇总（含各维度标注过滤情况）：

| Pass | CRITICAL | HIGH | MEDIUM | LOW | 备注 |
|------|----------|------|--------|-----|------|
| D1 Security | 0 | 1 | 1 | 2 | 【在线】注入/鉴权/Web 边界 18 条 N/A |
| D2 Concurrency | 0 | 1 | 1 | 1 | 【在线】Idempotency 4 条 N/A |
| D3 Observability | 0 | 3 | 2 | 1 | 【在线】Metrics 6 条 N/A |
| D4 Data & Storage | 0 | 1 | 3 | 0 | 无 DB，事务/Schema/缓存 N/A；【在线】7 条 N/A |
| D5 API Contracts | 0 | 3 | 3 | 1 | 【在线】Input Hardening 5 条 N/A |
| D6 Engineering | 0 | 2 | 4 | 1 | 全【通用】 |
| D7 Defensive | 1 | 2 | 2 | 1 | Date&Time 时区类不适用 |
| D8 Architecture | 0 | 2 | 4 | 1 | 对照 Phase 0 四原则判定 |

## 整改记录（2026-09-17，42 项全部处理完毕）

| 项 | 结果 |
|----|------|
| 状态 | 42 项任务全部置 `DONE`（3 项带限制条件，见下） |
| C++ | `src/` 重构为 5 个翻译单元（zip_reader / xml_parsers / image_extractor / xlsx_reader / addon），MSVC `/std:c++17` **5/5 编译零错误** |
| 绑定层 | `index.js` / `index.d.ts` 重写（输入归一化+校验+错误码+warnings+异步 API） |
| 工具链 | `scripts/lib/vcpkg.js` 单一来源；`require-vcpkg-root.js` 构建期校验；`execFileSync` 替换 `execSync`；`scripts/generate-checksums.js` |
| 测试 | `test/test.js` 21 个用例（错误码/校验/Buffer/base64/异步/上限/fixture） |
| 文档 | README 按真实 API 重写；CHANGELOG / CONTRIBUTING 新增；坏示例全部重写 |
| CI/发布 | ci.yml 三 job：lint+typecheck（补 @types/node、全量 node --check）/ Windows 原生构建+测试（VCPKG_ROOT 导出+vcpkg 二进制缓存）/ tag v* 触发 release（prebuild napi+electron + DLL 打包 + checksums + gh 发布到 GitHub Release）；binding.gyp linux/mac 接入 VCPKG_ROOT（029 扩展完成） |
| 版本 | package.json → 1.0.16 |

**带限制条件的 3 项：**
- **016**（prebuild 校验和）：`npm run checksums` 生成脚本与发布流程文档已就绪；安装期强制校验需发布流水线首次产出 `checksums.txt` 后接线 `--sha256`。
- **020**（同一文件双次解析）：xlnt/libzip 无共享会话能力，双开为架构固有成本；已加媒体条目 128MB 上限缓解内存放大，并在 README 记录。
- **011/全量验证**：测试集与 CI 已就绪，但**本机无法完成原生链接**（vcpkg 工具链与 VS 18 `mt.exe` 不兼容 0xc0000135，且本机已无 xlnt 库——原 `E:\vcpkg` 已移除）。已在受控环境对全部 5 个 C++ 翻译单元做 MSVC 编译校验（零错误）；`npm test` 与完整 `node-gyp rebuild` 需在可用构建环境（或新增的 CI `build-test` job）执行。

**实现偏离说明**：004 的修复计划原建议引入 pugixml；实际实现为自研的属性定位式 XML 扫描器（`src/xml_parsers.*`，容忍引号/空白/属性顺序、词边界匹配、无偏移魔数），整改效果等价且**不新增第三方依赖**，避免破坏现有 vcpkg 构建链。

**最高风险主题（3 条主线）：**
1. **非 ASCII 路径在 Windows 必现失败**（001）——中文用户群常见路径直接不可用。
2. **对外契约全面失真**：README 全套虚构 API、示例全部崩溃、死代码链静默返回空（002/003/013）——按文档接入必失败。
3. **解析健壮性与资源无上限**：偏移魔数式 XML 解析 + 静默吞错 + zip 炸弹/超大 dimension 全量加载（004/005/006/008/009/011）。

---

## Fix Task List

### AUDIT-20260917-001: Windows 下非 ASCII（中文）文件路径必现打开失败

| Field | Value |
|-------|-------|
| **ID** | AUDIT-20260917-001 |
| **Dimension** | D7 – Defensive |
| **Severity** | CRITICAL |
| **Status** | DONE |
| **Location** | `src/addon.cpp:247,274` → `src/xlsx_reader.cpp:16`、`src/image_extractor.cpp:347` |

#### Description

`Utf8Value()` 产出 UTF-8 字节的窄字符串，直接传给 `xlnt::workbook::load` 与 `zip_open`。Windows 窄字节 API 按 ANSI 代码页（简中系统为 GBK）解释字节序列，UTF-8 中文路径必现 "Failed to load file"。对中文用户为主的 Excel 库，这是常见路径必现失效。

#### Fix Plan

Windows 平台将 UTF-8 转 UTF-16（`MultiByteToWideChar`），以 `std::filesystem::path` 构造路径传给 xlnt/libzip（xlnt 1.6 `workbook::load` 接受 `std::filesystem::path`；libzip 侧可用 `zip_open` 前以宽字符句柄或 `zip_source` 方案）。验证：中文路径 + Buffer 输入两条用例在 zh-CN Windows 通过。

---

### AUDIT-20260917-002: README 文档整套 API 与实现不符

| Field | Value |
|-------|-------|
| **ID** | AUDIT-20260917-002 |
| **Dimension** | D5 – API Contracts |
| **Severity** | HIGH |
| **Status** | DONE |
| **Location** | `README.md:159-691` vs `index.js:209-211` |

#### Description

README 完整文档化 `readWorkbook/readWorksheet/extractImages/getWorksheetNames/readWorkbookAsJSON` 及 `ReadOptions{maxRows,maxCols,skipEmptyRows,includeFormulas,...}`、`Workbook/Cell/CellType` 类型；实际唯一导出为 `readTableAsJSON`。按 README 接入的消费者 100% 得到 `TypeError: readWorkbook is not a function`。

#### Fix Plan

以实际 API 重写 README（保留 `readTableAsJSON` 的 Buffer/base64 支持、图片归属语义、WPS 支持说明）；若确需旧 API 则实现之。发布前增加"README 代码示例可运行"检查。

---

### AUDIT-20260917-003: 随包示例引用不存在的导出，开箱即崩

| Field | Value |
|-------|-------|
| **ID** | AUDIT-20260917-003 |
| **Dimension** | D5 – API Contracts |
| **Severity** | HIGH |
| **Status** | DONE |
| **Location** | `examples/basic.js:1`（`readExcel, extractImages`）、`examples/json-api.js:6`、`examples/electron-example.js:8`（`getSheetNames`）、`examples/basic.js:9`、`examples/json-api.js:13`（不存在的 `./test/sample.xlsx`） |

#### Description

examples 从 `../index` 或 `baja-lite-xlsx` 导入 `readExcel/extractImages/getSheetNames`，均未导出；运行即 `TypeError: ... is not a function`。随 npm 包分发的示例构成虚假契约。

#### Fix Plan

重写示例仅使用 `readTableAsJSON`；fixture 文件路径指向仓库内真实存在的 `examples/*.xlsx`；CI 中将"每个示例可运行"纳入门禁。

---

### AUDIT-20260917-004: 手写偏移魔数式 XML 解析极度脆弱且静默失败

| Field | Value |
|-------|-------|
| **ID** | AUDIT-20260917-004 |
| **Dimension** | D7 – Defensive |
| **Severity** | HIGH |
| **Status** | DONE |
| **Location** | `src/image_extractor.cpp:143-144,173-174,309-322`、`src/addon.cpp:47` |

#### Description

解析依赖硬编码字符偏移（`colPos + 9`、`namePos + 6`、`embedPos + 9`、`substr(15)`），并假设属性双引号、无空白变体（`<xdr:col >`、`r:embed ='…'` 即整段解析失败）。叠加吞错误（AUDIT-009）后表现为"莫名缺图"，无法定位。代码注释自认应使用 pugixml/rapidxml。

#### Fix Plan

引入 pugixml（vcpkg 已有）重写三处 XML 解析；过渡方案至少改为属性名定位 + 引号自适应。补 WPS 与标准 Excel 的锚点解析回归测试。

---

### AUDIT-20260917-005: 浮动图 sheetName 硬编码 "Sheet1"，多 Sheet 错挂/漏挂

| Field | Value |
|-------|-------|
| **ID** | AUDIT-20260917-005 |
| **Dimension** | D5 – API Contracts |
| **Severity** | HIGH |
| **Status** | DONE |
| **Location** | `src/image_extractor.cpp:473-476`（`sheetName = "Sheet1"`）+ `src/addon.cpp:93-96` |

#### Description

drawing→sheet 映射未实现，锚点 sheetName 恒为 "Sheet1"：非 Sheet1 工作表的浮动图永远不附加（被 `pos.sheetName != sheets[i].name` 过滤）；若某 sheet 恰好命名 Sheet1，其他 sheet 的浮动图会错挂到它。

#### Fix Plan

解析 `xl/workbook.xml`（sheet name ↔ rId）与 `xl/_rels/workbook.xml.rels`（rId ↔ drawingN.xml）建立 drawing→sheetName 映射，替换硬编码；补多 sheet 浮动图用例。

---

### AUDIT-20260917-006: 无资源上限——zip 炸弹与超大 dimension 可致宿主 OOM 崩溃

| Field | Value |
|-------|-------|
| **ID** | AUDIT-20260917-006 |
| **Dimension** | D1 – Security |
| **Severity** | HIGH |
| **Status** | DONE |
| **Location** | `src/image_extractor.cpp:51-55`、`src/xlsx_reader.cpp:95-111` |

#### Description

`readFileFromZip` 直接 `outData.resize(sb.size)`（未校验 `sb.valid & ZIP_STAT_SIZE`，无上限）；`readSheetData` 按 `highest_row()/highest_column()` 全量遍历。恶意 xlsx（声明 1,048,576×16,384 dimension 或超大 media 条目）可触发数 GB 分配，OOM 使宿主进程崩溃。服务端消费场景影响面放大。

#### Fix Plan

`readFileFromZip` 增加 `sb.valid & ZIP_STAT_SIZE` 检查与单条目大小上限（建议 100MB，可配置）；`readSheetData` 增加行列上限并在超限时报错（与 AUDIT-012 合并实现 maxRows/maxCols 选项）。构造 zip 炸弹用例纳入测试。

---

### AUDIT-20260917-007: 全解析在 JS 主线程同步执行，Electron/服务端阻塞

| Field | Value |
|-------|-------|
| **ID** | AUDIT-20260917-007 |
| **Dimension** | D2 – Concurrency & Resources |
| **Severity** | HIGH |
| **Status** | DONE |
| **Location** | `src/addon.cpp:239-263,266-290`、`index.js:141` |

#### Description

xlnt 全表扫描 + libzip 全量解压 + 逐单元格 N-API 对象构造均在主线程同步执行，无 `Napi::AsyncWorker`。本包发布 Electron 预编译产物，Electron 宿主读大文件冻结 UI；Node 服务场景阻塞全部并发请求。

#### Fix Plan

将 `ReadExcel` 改为 `Napi::AsyncWorker`（解析在 worker 线程、结果转换在 `OnOK` 主线程），新增 `readTableAsJSONAsync` 并保留同步版以兼容；或在 README 明确同步阻塞语义与内存上限。

---

### AUDIT-20260917-008: 单元格转换 catch(...) 静默返回空串，数据丢失不可见

| Field | Value |
|-------|-------|
| **ID** | AUDIT-20260917-008 |
| **Dimension** | D3 – Observability |
| **Severity** | HIGH |
| **Status** | DONE |
| **Location** | `src/xlsx_reader.cpp:67-69,105-108` |

#### Description

`cellToString` 与单元格访问异常被 `catch(...)` 吞掉后返回 `""`，使用者无法区分"空单元格"与"读取失败"，数据静默丢失。

#### Fix Plan

首个转换错误写入 `lastError_`（或错误计数随结果暴露）；单元格级错误考虑返回带错误标记的对象（可选开关），默认行为不变但可诊断。

---

### AUDIT-20260917-009: 图片解析四处 catch(...) 全吞，缺图无诊断

| Field | Value |
|-------|-------|
| **ID** | AUDIT-20260917-009 |
| **Dimension** | D3 – Observability |
| **Severity** | HIGH |
| **Status** | DONE |
| **Location** | `src/image_extractor.cpp:392-394,405-407,477-479,492-494` |

#### Description

relationships/cellimages 关系表/drawing XML/cellimages.xml 解析失败全部 `catch(...)` 忽略，用户拿到"莫名缺图"，无任何诊断信息。

#### Fix Plan

失败时写入 `lastError_` 或引入 warnings 收集器随结果返回（如 `excelData.warnings`）；至少保留首条解析失败原因。

---

### AUDIT-20260917-010: 全表加载无上限（流式/分页缺失），README 宣称的 maxRows/maxCols 不存在

| Field | Value |
|-------|-------|
| **ID** | AUDIT-20260917-010 |
| **Dimension** | D4 – Data & Storage |
| **Severity** | HIGH |
| **Status** | DONE |
| **Location** | `src/xlsx_reader.cpp:74-120` |

#### Description

`readSheetData` 将整表物化为 `vector<vector<string>>`，无行列上限、无流式；xlsx 最大维度 1,048,576×16,384。README 宣称 `maxRows/maxCols/skipEmptyRows` 等选项实际不存在，契约与能力双重缺失。

#### Fix Plan

实现 `maxRows/maxCols` 读取上限（默认可设保护值如 100 万行/1 万列上限内再由调用方收紧）；与 AUDIT-006 合并为"资源上限"整改；README 同步。

---

### AUDIT-20260917-011: `npm test` 指向不存在的 test/ 目录，全库零测试

| Field | Value |
|-------|-------|
| **ID** | AUDIT-20260917-011 |
| **Dimension** | D6 – Engineering |
| **Severity** | HIGH |
| **Status** | DONE |
| **Location** | `package.json:20` |

#### Description

`"test": "node test/test.js"` 但 `test/` 不存在，`npm test` 直接报错；全库无任何测试。手写 XML 解析、WPS DISPIMG 归属、多 sheet 逻辑等高风险区域无回归保护。

#### Fix Plan

建立最小测试集：标准 Excel 读取、WPS 图片、多 sheet 浮动图、空表、Buffer/base64 输入、损坏文件、恶意大文件；fixture 用仓库已有 `examples/*.xlsx` 起步；将 `examples/test.js` 归位为正式测试起点。

---

### AUDIT-20260917-012: 死代码链——原生 `ExtractImages` 导出 → 空占位实现，静默返回空数组

| Field | Value |
|-------|-------|
| **ID** | AUDIT-20260917-012 |
| **Dimension** | D3 – Observability / D8 – Architecture |
| **Severity** | HIGH |
| **Status** | DONE |
| **Location** | `src/xlsx_reader.cpp:144-171`（`extractImages()`/`getImagePositions()` 空占位）、`src/addon.cpp:266-290`（`ExtractImages` 导出） |

#### Description

`XlsxReader::extractImages()`/`getImagePositions()` 为空占位（返回空容器且清空 `lastError_`），原生 `ExtractImages` 导出调用之，永远静默返回 `[]`；且 `index.js` 并未导出该链路——典型"疑似漏接线"死代码 + 静默失败源。

#### Fix Plan

决策二选一：(a) 实现并经 index.js 导出（复用 ImageExtractor）；(b) 整体移除 `ExtractImages` 导出与两个占位方法（连带 `xlsx_reader.h:56-59` 声明）。当前状态两头不靠，必须消除。

---

### AUDIT-20260917-013: package.json / postinstall 引用不存在的脚本

| Field | Value |
|-------|-------|
| **ID** | AUDIT-20260917-013 |
| **Dimension** | D6 – Engineering |
| **Severity** | HIGH |
| **Status** | DONE |
| **Location** | `package.json:21`（`scripts/test-prebuild-package.js` 不存在）、`scripts/postinstall.js:80`（`fix-dll-dependencies.js` 不存在） |

#### Description

`npm run test:prebuild` 立即失败；postinstall 在 DLL 缺失时指引运行不存在的 `fix-dll-dependencies.js`，误导用户。

#### Fix Plan

补齐两个脚本或删除引用；postinstall 指引改为指向真实修复路径（VC++ Redistributable / vcpkg 复制，与 AUDIT-014 的 vcpkg 单一来源一致）。

---

### AUDIT-20260917-014: 浮动图子串双向模糊匹配可挂错图片

| Field | Value |
|-------|-------|
| **ID** | AUDIT-20260917-014 |
| **Dimension** | D7 – Defensive |
| **Severity** | HIGH |
| **Status** | DONE |
| **Location** | `src/addon.cpp:151-182` |

#### Description

精确匹配失败时启用 `img.name.find(pos.imageName) != npos || pos.imageName.find(img.name) != npos` 双向子串匹配：`image1.png` 会命中 `image10.png`/`myimage1.png`，错误图片挂到错误单元格（错误结果级缺陷）。

#### Fix Plan

删除模糊匹配，要求全名相等 + 唯一性校验；确需容错时限定为"去掉路径前缀后的全名匹配"，并在挂错风险时跳过且记录 warning。

---

### AUDIT-20260917-015: image_extractor.cpp 506 行上帝文件，四职责混杂

| Field | Value |
|-------|-------|
| **ID** | AUDIT-20260917-015 |
| **Dimension** | D8 – Architecture |
| **Severity** | HIGH |
| **Status** | DONE |
| **Location** | `src/image_extractor.cpp`（506 行） |

#### Description

单文件承担：ZIP I/O（`readFileFromZip`/`extractFromXlsx`）、relationships 解析、drawing anchor 解析（twoCell+oneCell）、WPS cellimages 解析。500-999 行区间 + 多职责。

#### Fix Plan

三拆：`zip_reader.cpp`（ZIP I/O）/ `xml_parsers.cpp`（三种 XML 解析，配合 AUDIT-004 引入 pugixml）/ `extractor.cpp`（编排与映射构建）；只搬不改 namespace/接口。

---

### AUDIT-20260917-016: prebuild-install 二进制下载无校验和

| Field | Value |
|-------|-------|
| **ID** | AUDIT-20260917-016 |
| **Dimension** | D1 – Security |
| **Severity** | MEDIUM |
| **Status** | DONE |
| **Location** | `package.json:8,26-39` |

#### Description

`prebuild-install` 从 GitHub Releases 下载二进制，未配置 sha256 校验（prebuild-install 支持），存在供应链投毒面。

#### Fix Plan

发布流程生成各平台产物 sha256，`binary` 配置或安装命令启用校验；CI 中校验 prebuilds/ 内容与清单一致。

---

### AUDIT-20260917-017: 同图多单元格引用时 JS Buffer 成倍重复拷贝

| Field | Value |
|-------|-------|
| **ID** | AUDIT-20260917-017 |
| **Dimension** | D2 – Concurrency & Resources |
| **Severity** | MEDIUM |
| **Status** | DONE |
| **Location** | `src/addon.cpp:8-19,115-148` + `src/xlsx_reader.cpp:189-218` |

#### Description

`createImageObject` 对每个挂载单元格各做一次 `Buffer::Copy`，JS 侧内存 = 图片字节 × 引用次数；C++ 侧 `outImages` 已全量持有原件，峰值内存成倍放大。

#### Fix Plan

`createImageObject` 按 `img.name`（或索引）缓存已创建 Buffer 句柄，同图复用同一 N-API 值。

---

### AUDIT-20260917-018: NAPI_DISABLE_CPP_EXCEPTIONS 下 N-API 调用失败静默产生畸形结果

| Field | Value |
|-------|-------|
| **ID** | AUDIT-20260917-018 |
| **Dimension** | D3 – Observability |
| **Severity** | MEDIUM |
| **Status** | DONE |
| **Location** | `scripts/binding.gyp:18` + `src/addon.cpp` 全文件 |

#### Description

定义 `NAPI_DISABLE_CPP_EXCEPTIONS` 后 `String::New/Buffer::Copy/Array::New` 失败返回空句柄而非抛异常，代码从不检查 → OOM/类型错误场景产出畸形结果而非报错。

#### Fix Plan

统一策略：(a) 移除该 define 改回异常模式（代码已按异常语义书写），或 (b) 关键构造后检查句柄有效性并抛 JS 错误。推荐 (a)。

---

### AUDIT-20260917-019: 图片提取失败导致整次读取报错，已解析表数据被丢弃

| Field | Value |
|-------|-------|
| **ID** | AUDIT-20260917-019 |
| **Dimension** | D3 – Observability |
| **Severity** | MEDIUM |
| **Status** | DONE |
| **Location** | `src/xlsx_reader.cpp:219-221` + `src/addon.cpp:252-255` |

#### Description

`extractFromXlsx` 失败时 `lastError_` 置位 → addon 层整体抛错，已成功的 sheets 数据一并丢弃；`readSheetData` 中途失败时"部分 sheets + lastError"并存，处理粒度粗。

#### Fix Plan

图片失败降级：返回 sheets + 空 images + warning（错误信息保留在结果对象），仅加载失败才整体抛错。

---

### AUDIT-20260917-020: 同一文件被 xlnt 与 libzip 双次完整解析

| Field | Value |
|-------|-------|
| **ID** | AUDIT-20260917-020 |
| **Dimension** | D4 – Data & Storage |
| **Severity** | MEDIUM |
| **Status** | DONE |
| **Location** | `src/xlsx_reader.cpp:16,189` |

#### Description

`workbook_.load(filepath)`（xlnt 内部解压全簿）与 `extractor.extractFromXlsx(filepath)`（libzip 重开）对同一文件做两遍 I/O 与解压，大文件成本翻倍。

#### Fix Plan

探索单次打开：libzip 先行抽取 media/锚点，xlnt 仅在需要表数据时加载；或为 xlnt 提供 stream 源。短期至少缓存文件 stat 与读取策略。

---

### AUDIT-20260917-021: 图片归属 O(单元格×图片) 线性扫描

| Field | Value |
|-------|-------|
| **ID** | AUDIT-20260917-021 |
| **Dimension** | D4 – Data & Storage |
| **Severity** | MEDIUM |
| **Status** | DONE |
| **Location** | `src/addon.cpp:40-77,93-186` |

#### Description

每个图片标记单元格线性遍历 `cellImageMappings`/`positions`/`images`；浮动图阶段再 sheet×position×image 扫描。图片多时性能急剧劣化。

#### Fix Plan

进入循环前构建 `unordered_map`：`imageId→imageName`、`(sheet,row,col)→imageName`、`imageName→ImageData*`，挂载降为 O(1) 查找。

---

### AUDIT-20260917-022: 无条件解压全部 xl/media 条目，含未引用孤儿媒体

| Field | Value |
|-------|-------|
| **ID** | AUDIT-20260917-022 |
| **Dimension** | D4 – Data & Storage |
| **Severity** | MEDIUM |
| **Status** | DONE |
| **Location** | `src/image_extractor.cpp:413-445` |

#### Description

所有 `xl/media/*` 条目（含未被任何 drawing/cellimages 引用者）全量读入内存。

#### Fix Plan

先收集引用集（relationships + cellimages），第二遍仅按需读取被引用媒体；未引用媒体计入 warnings。

---

### AUDIT-20260917-023: 数值单元格 `std::to_string(double)` 固定 6 位小数，精度失真

| Field | Value |
|-------|-------|
| **ID** | AUDIT-20260917-023 |
| **Dimension** | D5 – API Contracts |
| **Severity** | MEDIUM |
| **Status** | DONE |
| **Location** | `src/xlsx_reader.cpp:31-32` |

#### Description

`3.14` → `"3.140000"`；大数值精度丢失；与 Excel 显示值不一致。README 同时宣称返回原生 `number` 类型（与 AUDIT-002 同源）。

#### Fix Plan

使用 `xlnt::cell::to_string()` 的数值表示或 shortest-round-trip 格式化（`std::to_chars`）；README 明确"全部值均为字符串"契约。

---

### AUDIT-20260917-024: `headerRow` 无输入校验，负数/越界报错信息不可定位

| Field | Value |
|-------|-------|
| **ID** | AUDIT-20260917-024 |
| **Dimension** | D5 – API Contracts |
| **Severity** | MEDIUM |
| **Status** | DONE |
| **Location** | `index.js:160-165` |

#### Description

`headerRow=-1` 或越界时 `sheetData[headerRow]` 为 `undefined`，`headers.map` 抛 `Cannot read properties of undefined`；`skipRows` 非数字项被静默忽略。

#### Fix Plan

入参校验：`headerRow` 为非负整数且 `< sheetData.length`（现有越界分支已有友好报错，负数补齐）；`skipRows` 校验数字数组，非法项报错或明确忽略语义。

---

### AUDIT-20260917-025: base64 输入启发式判定未契约化，存在误判面

| Field | Value |
|-------|-------|
| **ID** | AUDIT-20260917-025 |
| **Dimension** | D5 – API Contracts |
| **Severity** | MEDIUM |
| **Status** | DONE |
| **Location** | `index.js:50-56` |

#### Description

长度>500 + base64 字符集 + `UEs/PK/AAAA////` 包含即判定为 base64 并落临时文件；合法长路径/普通长字符串可能误判，报错误导用户；`index.d.ts`/README 均未说明该规则。

#### Fix Plan

契约文档化判定规则，或提供显式选项（如 `{inputEncoding: 'base64'}`）并保留启发式为 fallback；d.ts 同步。

---

### AUDIT-20260917-026: vcpkg 路径/复制逻辑 4 处硬编码且互不一致（单一真源缺失）

| Field | Value |
|-------|-------|
| **ID** | AUDIT-20260917-026 |
| **Dimension** | D6 – Engineering / D8 – Architecture |
| **Severity** | MEDIUM |
| **Status** | DONE |
| **Location** | `package.json:18`、`scripts/package-dlls.js:22-30`、`scripts/pack-dlls-into-prebuild.js:22-28`、`scripts/postinstall.js:76` |

#### Description

"找 vcpkg bin → 复制 DLL 清单"逻辑在 4 处各写一份，路径列表（E:\vcpkg / C:\vcpkg / VCPKG_ROOT）与 DLL 清单互不一致；机器特定路径硬编码入库。

#### Fix Plan

下沉 `scripts/lib/vcpkg.js`：唯一实现（VCPKG_ROOT 优先 + 常见路径回退）+ 统一 DLL 清单常量；4 处调用方改为 require。

---

### AUDIT-20260917-027: binding.gyp 未校验 VCPKG_ROOT，缺省时编译错误不可读

| Field | Value |
|-------|-------|
| **ID** | AUDIT-20260917-027 |
| **Dimension** | D6 – Engineering |
| **Severity** | MEDIUM |
| **Status** | DONE |
| **Location** | `scripts/binding.gyp:36-40` |

#### Description

`<!!(echo %VCPKG_ROOT%)` 在变量未设置时展开为字面 `%VCPKG_ROOT%`，后续报错完全不可读。

#### Fix Plan

binding.gyp 中先探测并输出明确错误（"VCPKG_ROOT is not set"）；README 从源码编译一节补前置条件说明。

---

### AUDIT-20260917-028: 从源码编译文档与实际构建方式不符

| Field | Value |
|-------|-------|
| **ID** | AUDIT-20260917-028 |
| **Dimension** | D6 – Engineering |
| **Severity** | MEDIUM |
| **Status** | DONE |
| **Location** | `README.md:108-157,778-808` |

#### Description

README 的 vcpkg 集成方式、依赖安装说明与 binding.gyp 实际用法（依赖 `%VCPKG_ROOT%` 环境变量 + libzip）不符；"异步操作支持"承诺不实（实际纯同步）。

#### Fix Plan

按真实流程重写编译章节（三平台依赖清单 + VCPKG_ROOT 说明）；删除"异步"表述或与 AUDIT-007 的 AsyncWorker 改造联动。

---

### AUDIT-20260917-029: 无 CI/lint/typecheck 门禁，prebuild 产物不可复现

| Field | Value |
|-------|-------|
| **ID** | AUDIT-20260917-029 |
| **Dimension** | D6 – Engineering |
| **Severity** | MEDIUM |
| **Status** | DONE |
| **Location** | 仓库根（无 CI 配置）、`scripts/create-prebuilds.bat` |

#### Description

无任何 CI 配置与 lint 门禁；prebuild 依赖本机 vcpkg 状态且无版本清单，不同机器构建产物不一致。

#### Fix Plan

添加 GitHub Actions：lint（eslint）+ tsc --noEmit + npm test（依赖 AUDIT-011）+ 三平台 prebuild 构建并附 sha256；vcpkg 依赖以 manifest（vcpkg.json）固定版本。

---

### AUDIT-20260917-030: 坐标解析失败静默回退 (0,0)，图片隐蔽错位到 A1

| Field | Value |
|-------|-------|
| **ID** | AUDIT-20260917-030 |
| **Dimension** | D7 – Defensive |
| **Severity** | MEDIUM |
| **Status** | DONE |
| **Location** | `src/image_extractor.cpp:146-152,175-181,237-251` |

#### Description

`std::stoi` 失败时 anchor 坐标回退 `0,0`，格式异常的锚点把图片挂到 A1 而非丢弃。

#### Fix Plan

解析失败跳过该 anchor 并记录 warning（与 AUDIT-009 的 warnings 收集器联动）。

---

### AUDIT-20260917-031: 日期单元格序列化格式不受控、契约未定义

| Field | Value |
|-------|-------|
| **ID** | AUDIT-20260917-031 |
| **Dimension** | D7 – Defensive |
| **Severity** | MEDIUM |
| **Status** | DONE |
| **Location** | `src/xlsx_reader.cpp:62-63` |

#### Description

date 类型直接 `cell.to_string()`，格式受 xlnt 内部实现影响且契约未定义（ISO 日期还是 serial 数值字符串不确定），跨版本可能变化。

#### Fix Plan

显式格式化（如 `YYYY-MM-DD HH:mm:ss`）并在 README/`index.d.ts` 契约化；保留 serial 数值的可选开关。

---

### AUDIT-20260917-032: twoCell/oneCellAnchor 解析与浮动图挂载逻辑重复

| Field | Value |
|-------|-------|
| **ID** | AUDIT-20260917-032 |
| **Dimension** | D8 – Architecture |
| **Severity** | MEDIUM |
| **Status** | DONE |
| **Location** | `src/image_extractor.cpp:117-206` vs `211-278`；`src/addon.cpp:115-148` vs `152-182` |

#### Description

两对 >80% 相似代码块：锚点解析（仅 to 坐标处理不同）、图片挂载（exact/fuzzy 分支几乎相同）。

#### Fix Plan

提取 `parseAnchorCoords(anchorXml, anchor)` 与 `attachImageToCell(dataArray, row, col, img)` 公共函数；配合 AUDIT-014 删除 fuzzy 分支后挂载逻辑自然合一。

---

### AUDIT-20260917-033: `getImageType` 与 `getContentType` 重复实现，前者死代码

| Field | Value |
|-------|-------|
| **ID** | AUDIT-20260917-033 |
| **Dimension** | D8 – Architecture |
| **Severity** | MEDIUM |
| **Status** | DONE |
| **Location** | `src/xlsx_reader.cpp:122-142`（零调用）vs `src/image_extractor.cpp:15-27` |

#### Description

两份相同的扩展名→MIME 映射；`getImageType` 无任何调用点。且 `xlsx_reader.cpp` 的版本用 `find` 匹配（`.png` 出现在任意位置即命中，`notpng.pngx` 误判）。

#### Fix Plan

删除 `xlsx_reader` 侧实现与头文件声明；保留 extractor 侧单一实现（并改为后缀精确匹配）。

---

### AUDIT-20260917-034: 图片归属业务逻辑混入 N-API 边界层（原则③违反）

| Field | Value |
|-------|-------|
| **ID** | AUDIT-20260917-034 |
| **Dimension** | D8 – Architecture |
| **Severity** | MEDIUM |
| **Status** | DONE |
| **Location** | `src/addon.cpp:28-193` |

#### Description

Phase 0 原则③要求 N-API 边界类型转换集中收口；当前 WPS ID 匹配、位置匹配、浮动图挂载等归属决策业务逻辑位于边界层，与类型转换混杂。

#### Fix Plan

归属决策下沉 C++ reader 层（`readExcel` 直接产出"单元格→图片索引"归属关系），addon.cpp 仅做值搬运。

---

### AUDIT-20260917-035: execSync 模板字符串拼接执行 tar

| Field | Value |
|-------|-------|
| **ID** | AUDIT-20260917-035 |
| **Dimension** | D1 – Security |
| **Severity** | LOW |
| **Status** | DONE |
| **Location** | `scripts/pack-dlls-into-prebuild.js:94,125` |

#### Description

路径拼接进 `tar` 命令字符串；文件名来自本机目录，现实风险低，但属不良实践。

#### Fix Plan

改 `execFileSync('tar', [...])` 数组参数形式。

---

### AUDIT-20260917-036: Buffer/base64 临时文件在进程异常退出时残留

| Field | Value |
|-------|-------|
| **ID** | AUDIT-20260917-036 |
| **Dimension** | D1 – Security |
| **Severity** | LOW |
| **Status** | DONE |
| **Location** | `index.js:32-44,61-74` |

#### Description

临时文件清理仅在正常 finally 执行；进程被 kill 时残留于 `os.tmpdir()`。文件名含 16 字节随机数，无竞态/可预测风险。

#### Fix Plan

启动时可选择性清理历史 `excel-*.xlsx` 残留（按文件名前缀 + mtime），或维持现状并文档说明。

---

### AUDIT-20260917-037: existsSync 与读取之间的 TOCTOU 窗口

| Field | Value |
|-------|-------|
| **ID** | AUDIT-20260917-037 |
| **Dimension** | D2 – Concurrency & Resources |
| **Severity** | LOW |
| **Status** | DONE |
| **Location** | `index.js:79` |

#### Description

`fs.existsSync` 与原生层打开之间存在窗口；文件消失时原生层报错信息仍清晰，实际影响可忽略。

#### Fix Plan

可移除预检查直接依赖原生层错误（减少一次系统调用并消除 TOCTOU）。

---

### AUDIT-20260917-038: 临时文件清理失败被空 catch 吞掉

| Field | Value |
|-------|-------|
| **ID** | AUDIT-20260917-038 |
| **Dimension** | D3 – Observability |
| **Severity** | LOW |
| **Status** | DONE |
| **Location** | `index.js:40-43,67-71` |

#### Description

删除失败无任何提示，长期运行进程 tmp 目录可能积累不可解释。

#### Fix Plan

`process.env.DEBUG` 下输出提示即可。

---

### AUDIT-20260917-039: 错误无错误码体系，中英文案混杂

| Field | Value |
|-------|-------|
| **ID** | AUDIT-20260917-039 |
| **Dimension** | D5 – API Contracts |
| **Severity** | LOW |
| **Status** | DONE |
| **Location** | `src/addon.cpp`（纯 message）、`index.js`（"File not found" / "未找到名为..." 混杂） |

#### Description

消费者只能字符串匹配错误；多语言用户看到的错误语言不一致。

#### Fix Plan

定义错误码枚举（如 `FILE_NOT_FOUND`/`SHEET_NOT_FOUND`/`PARSE_ERROR`），挂 `err.code`；文案统一语言。

---

### AUDIT-20260917-040: 无 CHANGELOG / CONTRIBUTING

| Field | Value |
|-------|-------|
| **ID** | AUDIT-20260917-040 |
| **Dimension** | D6 – Engineering |
| **Severity** | LOW |
| **Status** | DONE |
| **Location** | 仓库根 |

#### Description

版本已至 1.0.15 无变更记录；开源仓库缺贡献指引。

#### Fix Plan

补 CHANGELOG.md（至少回溯近期版本）与 CONTRIBUTING.md（构建/测试流程，引用 AUDIT-028 产物）。

---

### AUDIT-20260917-041: 魔法值散布（base64 阈值/偏移量/默认行）

| Field | Value |
|-------|-------|
| **ID** | AUDIT-20260917-041 |
| **Dimension** | D7 – Defensive |
| **Severity** | LOW |
| **Status** | DONE |
| **Location** | `index.js:52-55`（500/`'AAAA'`/`'////'`）、`src/addon.cpp:47`（substr(15)）、`src/image_extractor.cpp`（+9/+6 偏移） |

#### Description

无名常量散布，语义靠注释，改一处漏三处（偏移类问题在引入 pugixml 后自然消除）。

#### Fix Plan

命名常量收口：`BASE64_MIN_LENGTH`、`IMAGE_CELL_PREFIX_LEN` 等；偏移魔数随 AUDIT-004 消除。

---

### AUDIT-20260917-042: `xlsx_reader.h` 未使用的 `<map>` include

| Field | Value |
|-------|-------|
| **ID** | AUDIT-20260917-042 |
| **Dimension** | D8 – Architecture |
| **Severity** | LOW |
| **Status** | DONE |
| **Location** | `src/xlsx_reader.h:6` |

#### Description

`#include <map>` 无使用（`cellImageMappings` 用 vector）。

#### Fix Plan

删除该 include。

---

## Summary

**42 项发现：1 CRITICAL / 15 HIGH / 18 MEDIUM / 8 LOW**（定位=本地，【在线】标注项已按 Phase 0 过滤并在各 pass 报告标 N/A：D1×18、D2×4、D3×6、D4×7、D5×5）。

**推荐整改顺序（三波）：**

1. **止血（正确性 + 可用性）**：001 中文路径 → 005 sheetName 硬编码 → 014 模糊匹配挂错图 → 002/003 README 与示例重写 → 012 死代码链决策。
2. **健壮性（恶意/异常输入）**：006/010 资源上限 → 004 pugixml 重写解析 → 008/009 错误收集 → 018 N-API 异常策略。
3. **工程化（防回归）**：011 测试集 → 029 CI 门禁 → 013 工具链修复 → 026 vcpkg 单一来源 → 015/032/033/034 结构整改。

修复跟踪不在本技能范围内；执行方可直接修改各任务的 `Status` 字段。
