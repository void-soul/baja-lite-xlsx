# 审计进度 — baja-lite-xlsx

| 字段 | 值 |
|------|-----|
| 开始日期 | 2026-09-17 |
| 输出目录 | docs/audit/AUDIT-20260917-baja-lite-xlsx/ |
| 当前步骤 | CLOSED（审计 + 整改完成） |
| 下一步动作 | 剩余验证：在可用构建环境执行 node-gyp 重建 + npm test（本机 vcpkg 工具链与 VS18 不兼容，详见 AUDIT 文档"整改记录"） |

## 定位与范围（Phase 0 结论）

| 项 | 内容 |
|----|------|
| **项目定位** | 本地（全库）：npm 原生模块库（Node-API C++ addon，xlnt 封装），无对外网络服务端口，数据来自用户本地传入的 xlsx 文件 |
| **标注项过滤** | 查【本地】+【通用】；【在线】项跳过并在报告标 N/A |
| **特别提示** | 解析不可信输入（任意 .xlsx 可能来自网络），D1 文件路径安全 / C++ 恶意输入内存安全属【通用】项，重点审查 |
| **审计范围** | src/（C++ 核心）、index.js、index.d.ts、scripts/、package.json；examples/*.js|ts 仅核对 API 契约一致性 |
| **排除目录** | prebuilds/（预编译二进制产物）、examples/output/（运行输出）、examples/*.xlsx（fixture 数据） |
| **架构原则** | ① JS 层不绕过 C++ 校验直接操作文件；② C++ 不依赖 V8 之外的环境；③ N-API 边界类型转换集中收口；④ 公共 API 与 index.d.ts 声明一致（用户确认于 2026-09-17） |
| **用户确认状态** | 已确认（2026-09-17，定位/范围/原则三项均确认） |

## 阶段进度

| 项 | 状态 | 备注 |
|----|------|------|
| Phase 0 定位与范围确认 | DONE | 用户确认于 2026-09-17 |
| Phase 1 子项目定位 | DONE | 单库项目：C++ 核心(xlnt封装+ZIP解析) / JS 绑定层 / 构建脚本，依赖单向 |
| Phase 1 模块定位 | DONE | addon.cpp(N-API边界) / xlsx_reader(表数据) / image_extractor(ZIP+手写XML) / index.js(高层API) / scripts(支撑) / examples(边缘,仅契约核对)；已确认：examples 引用不存在导出、test/ 缺失、scripts/test-prebuild-package.js 缺失 |
| Phase 2 / D1 Security | DONE | 1 HIGH(zip炸弹/无资源上限) 1 MEDIUM(prebuild-install无校验和) 2 LOW；【在线】项18条N/A |
| Phase 2 / D2 Concurrency | DONE | 1 HIGH(主线程同步阻塞,Electron UI冻结) 1 MEDIUM(图片Buffer重复拷贝) 1 LOW；在线项4条N/A |
| Phase 2 / D3 Observability | DONE | 3 HIGH(静默吞异常×2、extractImages空占位静默返回) 2 MEDIUM 1 LOW；在线项6条N/A |
| Phase 2 / D4 Data & Storage | DONE | 1 HIGH(全量无上限加载) 3 MEDIUM(双次解析/O(n·m·k)扫描/孤儿媒体全量解压)；在线项7条N/A |
| Phase 2 / D5 API Contracts | DONE | 3 HIGH(README虚构API/示例全崩/浮动图sheetName硬编码"Sheet1") 3 MEDIUM 1 LOW；在线项9条N/A |
| Phase 2 / D6 Engineering | DONE | 2 HIGH(npm test指向不存在test/、引用不存在脚本×2) 4 MEDIUM 1 LOW |
| Phase 2 / D7 Defensive | DONE | 1 CRITICAL(中文路径Windows必现失败) 2 HIGH(魔数偏移解析/模糊匹配挂错图) 2 MEDIUM 1 LOW |
| Phase 2 / D8 Architecture | DONE | 2 HIGH(死代码链/image_extractor.cpp 506行上帝文件) 4 MEDIUM 1 LOW |
| Phase 3 / OVERVIEW.md | DONE | |
| Phase 3 / STRUCTURE.md | DONE | |
| Phase 3 / SUBPROJECT-baja-lite-xlsx.md | DONE | 单库项目，单文档 |
| Phase 4 审计总文档 | DONE | AUDIT-20260917-baja-lite-xlsx.md，42 项（1C/15H/18M/8L）按重要性排序 |

## 发现问题计数（随做随记，最终汇总进审计总文档）

| Pass | CRITICAL | HIGH | MEDIUM | LOW |
|------|----------|------|--------|-----|
| D1 Security | 0 | 1 | 1 | 2 |
| D2 Concurrency | 0 | 1 | 1 | 1 |
| D3 Observability | 0 | 3 | 2 | 1 |
| D4 Data & Storage | 0 | 1 | 3 | 0 |
| D5 API Contracts | 0 | 3 | 3 | 1 |
| D6 Engineering | 0 | 2 | 4 | 1 |
| D7 Defensive | 1 | 2 | 2 | 1 |
| D8 Architecture | 0 | 2 | 4 | 1 |
| **合计**（2 组同源合并后 42 项） | **1** | **15** | **18** | **8** |

## 整改（2026-09-17 同日完成）

- 42 项发现全部整改，状态已回写至 AUDIT 文档（3 项带限制条件：016/020/011 全量验证）
- 代码变更：src/ 重构为 5 个翻译单元（5/5 MSVC 编译零错误）、index.js/index.d.ts 重写、scripts 工具链统一、test/test.js 新增、README/CHANGELOG/CONTRIBUTING/CI、examples 全部修复、package.json → 1.0.16
- 未做 git 提交（等待用户确认）

## 已生成文档

- [x] OVERVIEW.md
- [x] STRUCTURE.md
- [x] SUBPROJECT-baja-lite-xlsx.md
- [x] AUDIT-20260917-baja-lite-xlsx.md
