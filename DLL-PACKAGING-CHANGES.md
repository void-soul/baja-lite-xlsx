# DLL 自动打包功能 - 变更总结

## 📝 变更概述

为了让预编译包真正做到**开箱即用**，我们实现了自动 DLL 打包机制。

### 核心改进

**之前**: 预编译包只包含 `.node` 文件，用户需要手动安装 VC++ Redistributable 或复制 DLL  
**现在**: 预编译包包含完整的 `.node` + 所有依赖 DLL，用户安装后直接可用

## 📂 新增文件

### 脚本文件

| 文件 | 说明 |
|------|------|
| `scripts/package-dlls.js` | 在预编译前自动复制依赖的 DLL 文件 |
| `scripts/postinstall.js` | 用户安装后验证 DLL 是否就绪 |
| `scripts/test-prebuild-package.js` | 测试预编译包完整性的工具 |

### 文档文件

| 文件 | 说明 |
|------|------|
| `PREBUILD-DLL-PACKAGING.md` | 完整的技术文档和原理说明 |
| `QUICK-RELEASE-GUIDE.md` | 快速发布指南 |
| `DLL-PACKAGING-CHANGES.md` | 本文件，变更总结 |

### 诊断工具

| 文件 | 说明 |
|------|------|
| `diagnose-dll.js` | 诊断 DLL 依赖问题 |
| `fix-dll-dependencies.js` | 自动修复用户端的 DLL 问题 |
| `DLL-DEPENDENCY-FIX.md` | DLL 问题解决方案文档 |

## 🔄 修改的文件

### 1. `package.json`

**修改的脚本**:

```json
{
  "scripts": {
    // 新增: 安装后验证
    "postinstall": "node scripts/postinstall.js",
    
    // 修改: prebuild 拆分为两个步骤
    "prebuild": "npm run prebuild:napi && npm run prebuild:electron",
    "prebuild:napi": "node-gyp rebuild && node scripts/package-dlls.js && prebuild --runtime napi --target 8 --strip",
    "prebuild:electron": "node-gyp rebuild && node scripts/package-dlls.js && prebuild --runtime electron --target 34.0.0 --strip",
    
    // 新增: 独立的 DLL 复制命令
    "copy-dlls": "node scripts/package-dlls.js",
    
    // 新增: 测试预编译包
    "test:prebuild": "node scripts/test-prebuild-package.js"
  }
}
```

**关键变化**:
- ✅ `prebuild:napi` 和 `prebuild:electron` 在打包前自动运行 `package-dlls.js`
- ✅ 新增 `postinstall` 钩子进行安装后验证
- ✅ 新增 `test:prebuild` 用于测试预编译包完整性

### 2. `.github/workflows/prebuild.yml`

**新增步骤**:

```yaml
# 在 Build native module 之后，Create prebuild package 之前
- name: Package DLL dependencies
  run: node scripts/package-dlls.js
  env:
    VCPKG_ROOT: C:\vcpkg

# 验证 DLL 已复制
- name: Verify DLL files are packaged
  run: |
    Get-ChildItem build/Release -Filter *.dll
  shell: powershell
```

**增强的步骤**:

```yaml
- name: List generated files
  # 现在会解压第一个 .tar.gz 并显示内容
  # 验证 DLL 是否被成功打包
```

**影响**:
- ✅ CI 构建的预编译包现在包含所有 DLL
- ✅ 自动验证 DLL 是否成功打包
- ✅ 构建日志更详细，便于排查问题

## 🔍 工作原理

### 本地开发流程

```
npm run build
    ↓
编译 .node 文件
    ↓
npm run copy-dlls (可选，手动)
    ↓
scripts/package-dlls.js
    ├─ 查找 vcpkg/installed/x64-windows/bin/
    ├─ 复制 xlnt.dll, zlib1.dll, libexpat.dll
    └─ 复制到 build/Release/
    ↓
npm run prebuild
    ├─ 重新编译 .node
    ├─ 自动运行 package-dlls.js
    └─ prebuild 打包 build/Release/ 下所有文件
    ↓
生成 .tar.gz (包含 .node + DLL)
```

### CI/CD 流程

```
推送 tag (v1.0.13)
    ↓
GitHub Actions 触发
    ↓
安装 vcpkg + xlnt
    ↓
npm run build
    ↓
node scripts/package-dlls.js ⬅️ 新增
    ├─ 从 C:\vcpkg\installed\x64-windows\bin\ 复制 DLL
    └─ 到 build/Release/
    ↓
验证 DLL 文件 ⬅️ 新增
    ├─ 列出 DLL
    └─ 检查是否完整
    ↓
npx prebuild
    └─ 打包 build/Release/ (包含 DLL)
    ↓
解压验证包内容 ⬅️ 新增
    ├─ 提取第一个 .tar.gz
    ├─ 显示内容
    └─ 验证包含必需的 DLL
    ↓
上传到 GitHub Release
```

### 用户安装流程

```
npm install baja-lite-xlsx
    ↓
prebuild-install 尝试下载预编译包
    ↓
从 GitHub Release 下载 .tar.gz
    ↓
解压到 node_modules/baja-lite-xlsx/build/Release/
    ├─ baja_xlsx.node
    ├─ xlnt.dll ⬅️ 新增
    ├─ zlib1.dll ⬅️ 新增
    └─ libexpat.dll ⬅️ 新增
    ↓
运行 postinstall.js ⬅️ 新增
    ├─ 检查 DLL 是否存在
    ├─ 提供友好的错误提示
    └─ (可选) 测试模块加载
    ↓
✅ 安装完成，可以直接使用
```

## 📊 影响分析

### 预编译包大小变化

| 版本 | 内容 | 压缩前 | 压缩后 |
|------|------|--------|--------|
| 旧版本 | 仅 .node 文件 | ~100 KB | ~50 KB |
| 新版本 | .node + DLL | ~1000 KB | ~500-700 KB |

**增加**: 约 500-650 KB (压缩后)

### 用户体验改进

| 场景 | 旧版本 | 新版本 |
|------|--------|--------|
| Windows + Node 20 用户 | ❌ 需要手动安装 VC++ 或复制 DLL | ✅ 直接可用 |
| 无编译环境的用户 | ❌ 无法使用 | ✅ 直接可用 |
| CI/CD 环境 | ❌ 需要额外配置 | ✅ 直接可用 |
| Docker 环境 | ❌ 需要安装依赖 | ✅ 直接可用 |

### 开发流程改进

| 阶段 | 旧版本 | 新版本 |
|------|--------|--------|
| 本地测试 | 手动复制 DLL | `npm run copy-dlls` |
| 预编译 | 需要记得复制 DLL | 自动打包 DLL |
| 验证 | 手动解压检查 | `npm run test:prebuild` |
| CI 构建 | 无验证 | 自动验证包含 DLL |

## 🧪 测试方法

### 本地测试

```bash
# 1. 清理
npm run clean

# 2. 构建
npm run build

# 3. 复制 DLL
npm run copy-dlls

# 4. 验证 DLL
ls build/Release/
# 应该看到: baja_xlsx.node, xlnt.dll, zlib1.dll, libexpat.dll

# 5. 创建预编译包
npm run prebuild

# 6. 测试预编译包
npm run test:prebuild
# 应该显示: ✅ 所有预编译包验证通过！

# 7. 运行功能测试
npm test
```

### CI 测试

1. 推送 tag: `git tag v1.0.13-test && git push origin v1.0.13-test`
2. 查看 Actions: https://github.com/void-soul/baja-lite-xlsx/actions
3. 检查关键步骤:
   - ✅ Package DLL dependencies
   - ✅ Verify DLL files are packaged
   - ✅ List generated files (查看包内容)
4. 下载 Artifacts 并手动验证

### 端到端测试

```bash
# 在干净的目录
mkdir test-e2e
cd test-e2e

# 安装发布的包
npm init -y
npm install baja-lite-xlsx@1.0.13

# 验证
ls node_modules/baja-lite-xlsx/build/Release/
# 应该看到 .node 和所有 DLL

# 测试加载
node -e "const xlsx = require('baja-lite-xlsx'); console.log('OK');"
# 应该输出: OK

# 测试功能
node -e "
const xlsx = require('baja-lite-xlsx');
const data = xlsx.getSheetNames('../sample.xlsx');
console.log('Sheets:', data);
"
```

## ✅ 验收标准

发布前必须满足：

- [ ] 本地构建成功: `npm run build`
- [ ] DLL 复制成功: `npm run copy-dlls` 且 `build/Release/` 包含所有 DLL
- [ ] 功能测试通过: `npm test`
- [ ] 预编译包创建成功: `npm run prebuild`
- [ ] 预编译包验证通过: `npm run test:prebuild` 显示 ✅
- [ ] CI 构建成功: GitHub Actions 全绿
- [ ] CI 验证步骤成功: "Verify DLL files are packaged" 显示 ✅
- [ ] 端到端测试成功: 在干净环境安装并使用

## 🐛 已知问题和限制

### 平台限制

- ✅ **Windows x64**: 完全支持，自动打包 DLL
- ⚠️ **Windows x86**: 需要源码编译
- ⚠️ **Linux**: 需要源码编译（计划未来添加预编译）
- ⚠️ **macOS**: 需要源码编译（计划未来添加预编译）

### Node.js 版本限制

- ✅ **Node.js 20+**: 完全支持预编译包
- ⚠️ **Node.js 16, 18**: 需要源码编译

### DLL 版本

- 当前打包的 DLL 版本取决于 CI 环境的 vcpkg
- 如果 xlnt 更新，需要重新触发构建

## 🚀 未来改进

### 短期（下一个版本）

- [ ] 添加 Linux x64 预编译支持
- [ ] 添加 macOS arm64/x64 预编译支持
- [ ] 添加 Node.js 18 预编译支持
- [ ] 优化预编译包大小（静态链接？）

### 中期

- [ ] 添加自动版本检测和 DLL 更新
- [ ] 提供 Docker 测试镜像
- [ ] 添加更多诊断工具
- [ ] 改进错误提示

### 长期

- [ ] 考虑静态链接（消除 DLL 依赖）
- [ ] 支持更多 Node.js 版本
- [ ] 支持 ARM 架构

## 📚 相关资源

- [完整技术文档](./PREBUILD-DLL-PACKAGING.md) - 深入了解原理和机制
- [快速发布指南](./QUICK-RELEASE-GUIDE.md) - 发布流程和命令速查
- [DLL 问题解决方案](./DLL-DEPENDENCY-FIX.md) - 用户端问题排查

## 📞 支持

如有问题，请：

1. 查看文档: [PREBUILD-DLL-PACKAGING.md](./PREBUILD-DLL-PACKAGING.md)
2. 运行诊断: `node diagnose-dll.js <path-to-node-file>`
3. 查看 Issues: https://github.com/void-soul/baja-lite-xlsx/issues
4. 提交新 Issue: https://github.com/void-soul/baja-lite-xlsx/issues/new

---

**总结**: 这次改进让 `baja-lite-xlsx` 真正成为一个"预编译"包，用户在 Windows + Node 20 环境下可以直接安装使用，无需任何额外配置。🎉

