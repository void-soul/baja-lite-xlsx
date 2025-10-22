# 预编译 DLL 自动打包方案

## 📋 问题背景

之前的预编译包虽然包含了 `.node` 文件，但缺少依赖的 DLL 文件（xlnt.dll, zlib1.dll, libexpat.dll），导致用户下载预编译包后仍然无法使用，提示：

```
The specified module could not be found.
```

这根本不能算"预编译"，因为用户还需要手动处理 DLL 依赖。

## ✅ 解决方案

### 核心思路

**在预编译时自动将所有依赖的 DLL 文件打包到预编译包中**，让用户下载后真正做到开箱即用。

### 实现机制

```
┌─────────────────────────────────────────────────────────────┐
│                   GitHub Actions 构建流程                    │
└─────────────────────────────────────────────────────────────┘

1. 安装 vcpkg 并编译 xlnt
   ↓
2. 编译原生模块 (.node 文件)
   ↓
3. 🆕 运行 scripts/package-dlls.js
   └─ 自动从 vcpkg/installed/x64-windows/bin/ 复制所需 DLL
   └─ 复制到 build/Release/ 目录（与 .node 文件同目录）
   ↓
4. 验证 DLL 文件是否成功复制
   ↓
5. 运行 prebuild 打包
   └─ prebuild 会将 build/Release/ 下的所有文件打包
   └─ 现在预编译包包含: .node + 所有 DLL
   ↓
6. 上传到 GitHub Release
   ↓
7. 用户安装时
   └─ prebuild-install 下载预编译包
   └─ 解压后包含完整的 .node + DLL
   └─ 🆕 运行 scripts/postinstall.js 验证
   └─ ✅ 直接可用！
```

## 📦 涉及的文件

### 1. `scripts/package-dlls.js`

**作用**: 在预编译前自动复制依赖的 DLL 文件

**何时执行**: 
- GitHub Actions 构建时（在 `prebuild` 之前）
- 本地手动运行: `npm run copy-dlls`

**功能**:
- ✅ 从 vcpkg 安装目录查找所需的 DLL
- ✅ 复制到 `build/Release/` 目录
- ✅ 验证文件大小和完整性
- ✅ 在 CI 环境中，缺少 DLL 会导致构建失败

### 2. `scripts/postinstall.js`

**作用**: 用户安装后验证 DLL 是否就绪

**何时执行**: 
- 用户运行 `npm install baja-lite-xlsx` 后自动执行

**功能**:
- ✅ 检查 Windows 平台是否有必需的 DLL
- ✅ 提供友好的错误提示和解决方案
- ✅ 可选：测试模块加载（设置环境变量 `BAJA_XLSX_TEST_LOAD=1`）

### 3. `package.json` - 更新的脚本

```json
{
  "scripts": {
    "install": "prebuild-install --runtime napi || node-gyp rebuild",
    "postinstall": "node scripts/postinstall.js",
    "prebuild": "npm run prebuild:napi && npm run prebuild:electron",
    "prebuild:napi": "node-gyp rebuild && node scripts/package-dlls.js && prebuild --runtime napi --target 8 --strip",
    "prebuild:electron": "node-gyp rebuild && node scripts/package-dlls.js && prebuild --runtime electron --target 34.0.0 --strip",
    "copy-dlls": "node scripts/package-dlls.js"
  }
}
```

**关键变化**:
- `prebuild:napi` 和 `prebuild:electron` 在打包前执行 `package-dlls.js`
- 添加 `postinstall` 钩子进行安装后验证

### 4. `.github/workflows/prebuild.yml` - CI 配置

新增步骤：

```yaml
- name: Package DLL dependencies
  run: node scripts/package-dlls.js
  env:
    VCPKG_ROOT: C:\vcpkg

- name: Verify DLL files are packaged
  run: |
    # 列出并验证 DLL 文件
    Get-ChildItem build/Release -Filter *.dll

- name: List generated files
  run: |
    # 解压第一个 .tar.gz 预编译包
    # 验证包内确实包含 DLL 文件
```

## 🔄 完整工作流

### 开发阶段（本地）

```bash
# 1. 安装 vcpkg 依赖
vcpkg install xlnt:x64-windows

# 2. 编译模块
npm run build

# 3. 手动复制 DLL（用于本地测试）
npm run copy-dlls

# 4. 测试
npm test
```

### 预编译阶段（GitHub Actions）

```bash
# 自动执行（当推送 tag 时）
git tag v1.0.13
git push origin v1.0.13

# GitHub Actions 自动完成：
# 1. 安装 vcpkg + xlnt
# 2. 编译 .node
# 3. 📦 复制 DLL （scripts/package-dlls.js）
# 4. 验证 DLL
# 5. 创建预编译包（包含 DLL）
# 6. 上传到 GitHub Release
```

### 用户安装阶段

```bash
# 用户执行
npm install baja-lite-xlsx

# 自动发生：
# 1. prebuild-install 尝试下载预编译包
# 2. 如果找到匹配的预编译包：
#    - 下载 .tar.gz
#    - 解压到 node_modules/baja-lite-xlsx/build/Release/
#    - 包含 baja_xlsx.node + xlnt.dll + zlib1.dll + libexpat.dll
#    - 运行 postinstall.js 验证
#    - ✅ 完成！可以直接使用
# 3. 如果没有找到预编译包：
#    - 回退到 node-gyp rebuild（源码编译）
```

## 📊 预编译包内容对比

### ❌ 之前（不完整）

```
baja-lite-xlsx-v1.0.12-napi-v8-win32-x64.tar.gz
└── build/Release/
    └── baja_xlsx.node (约 100KB)
```

**问题**: 缺少 DLL，用户无法使用

### ✅ 现在（完整）

```
baja-lite-xlsx-v1.0.13-napi-v8-win32-x64.tar.gz
└── build/Release/
    ├── baja_xlsx.node      (~100 KB)
    ├── xlnt.dll            (~600 KB)
    ├── zlib1.dll           (~100 KB)
    └── libexpat.dll        (~200 KB)
```

**总大小**: 约 1MB（压缩后约 500-700KB）

**优势**: 开箱即用，无需任何额外配置

## 🎯 关键技术点

### 1. prebuild 的工作原理

`prebuild` 会将 `module_path` 指定目录下的**所有文件**打包：

```json
"binary": {
  "module_path": "./build/Release/"
}
```

只要我们在运行 `prebuild` 之前把 DLL 放到这个目录，它们就会被自动包含。

### 2. DLL 查找策略

`scripts/package-dlls.js` 的查找顺序：

```javascript
const vcpkgSources = [
  process.env.VCPKG_ROOT + '/installed/x64-windows/bin',  // CI 环境
  'C:\\vcpkg\\installed\\x64-windows\\bin',               // 标准位置
  'E:\\vcpkg\\installed\\x64-windows\\bin',               // 自定义位置
  // ...
];
```

### 3. Windows DLL 加载机制

Windows 加载 DLL 的搜索路径：

1. **应用程序所在目录**（优先级最高）
2. System32 目录
3. PATH 环境变量中的目录

我们将 DLL 放在 `.node` 文件同目录，确保优先加载。

### 4. CI 环境变量

```yaml
env:
  VCPKG_ROOT: C:\vcpkg
```

确保 `package-dlls.js` 能找到正确的 vcpkg 安装路径。

## 🔍 验证方法

### 本地验证

```bash
# 构建并打包
npm run build
npm run copy-dlls
npm run prebuild

# 检查预编译包内容
tar -tzf prebuilds/baja-lite-xlsx-v1.0.13-napi-v8-win32-x64.tar.gz

# 应该看到：
# build/Release/baja_xlsx.node
# build/Release/xlnt.dll
# build/Release/zlib1.dll
# build/Release/libexpat.dll
```

### CI 验证

GitHub Actions 会自动：

1. 列出打包前 `build/Release/` 目录内容
2. 列出预编译包文件
3. 解压第一个 `.tar.gz` 并显示内容
4. 验证是否包含必需的 DLL

查看 Actions 日志中的 "Verify DLL files are packaged" 步骤。

### 用户端验证

```bash
# 用户安装后
npm install baja-lite-xlsx

# 检查安装目录
cd node_modules/baja-lite-xlsx/build/Release
ls -la

# 应该看到 .node 和所有 DLL 文件
```

## 📝 维护指南

### 添加新的 DLL 依赖

如果将来 xlnt 更新或添加了新的依赖：

1. 更新 `scripts/package-dlls.js` 中的 `requiredDlls` 数组
2. 更新 `scripts/postinstall.js` 中的检查逻辑
3. 更新此文档

### 支持其他平台

当前方案主要针对 Windows。Linux 和 macOS 的处理方式：

**Linux**:
- 通常使用系统包管理器安装共享库（.so）
- 或者静态链接（推荐）

**macOS**:
- 使用 Homebrew 安装动态库（.dylib）
- 或者静态链接

如需添加 Linux/macOS 的 DLL 打包，可以扩展 `package-dlls.js`。

## 🚀 发布清单

发布新版本前的检查清单：

- [ ] 本地构建并测试 `npm run build && npm run copy-dlls`
- [ ] 验证 DLL 文件存在于 `build/Release/`
- [ ] 运行测试 `npm test`
- [ ] 创建预编译包 `npm run prebuild`
- [ ] 检查 `.tar.gz` 包内容
- [ ] 推送 tag 触发 GitHub Actions
- [ ] 验证 CI 构建日志
- [ ] 验证 GitHub Release 中的预编译包
- [ ] 在干净环境测试安装 `npm install baja-lite-xlsx`

## 💡 最佳实践

1. **始终在 CI 环境验证**: 确保 DLL 成功打包
2. **保持 DLL 最小化**: 只包含必需的 DLL
3. **文档清晰**: 说明预编译包的内容和限制
4. **提供回退方案**: 如果预编译失败，仍可源码编译
5. **版本匹配**: 确保 DLL 版本与编译时使用的一致

## 🐛 故障排除

### 问题: CI 构建时找不到 DLL

**解决**:
- 检查 `VCPKG_ROOT` 环境变量
- 验证 vcpkg install 步骤成功
- 检查 DLL 文件名（可能是 `zlib.dll` 而不是 `zlib1.dll`）

### 问题: 用户安装后仍提示找不到模块

**解决**:
- 验证预编译包确实包含 DLL
- 检查用户的 Node.js 版本和架构是否匹配
- 建议用户安装 VC++ Redistributable

### 问题: 预编译包太大

**解决**:
- 考虑静态链接（但会增加编译复杂度）
- 使用更激进的 strip 选项
- 压缩 DLL（但可能影响加载性能）

## 📚 相关资源

- [node-pre-gyp 文档](https://github.com/mapbox/node-pre-gyp)
- [prebuild 文档](https://github.com/prebuild/prebuild)
- [Windows DLL 搜索路径](https://docs.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-search-order)
- [vcpkg 使用指南](https://vcpkg.io/)

---

**总结**: 通过这套自动化方案，我们的预编译包真正做到了"预编译"——用户下载即用，无需任何额外配置，大幅提升了用户体验。

