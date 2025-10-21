# 预编译包文件名格式问题修复

## 🐛 问题描述

用户在安装 v1.0.9 时遇到以下错误：

```
prebuild-install warn install No prebuilt binaries found (target=8 runtime=napi arch=x64 libc= platform=win32)
```

## 🔍 根本原因

**package.json 配置的文件名格式与 prebuild 实际生成的文件名不匹配！**

### 错误配置 (v1.0.9)

```json
{
  "binary": {
    "package_name": "{runtime}-v{abi}-{platform}-{arch}.tar.gz"
  }
}
```

**prebuild-install 查找的 URL：**
```
https://github.com/void-soul/baja-lite-xlsx/releases/download/v1.0.9/napi-v8-win32-x64.tar.gz
```

**实际存在的文件：**
```
https://github.com/void-soul/baja-lite-xlsx/releases/download/v1.0.9/baja-lite-xlsx-v1.0.9-napi-v8-win32-x64.tar.gz
```

**结果：** 404 Not Found ❌

---

## ✅ 解决方案

### 修复后的配置 (v1.0.10+)

```json
{
  "binary": {
    "package_name": "{name}-v{version}-{runtime}-v{abi}-{platform}-{arch}.tar.gz"
  }
}
```

**prebuild-install 查找的 URL：**
```
https://github.com/void-soul/baja-lite-xlsx/releases/download/v1.0.10/baja-lite-xlsx-v1.0.10-napi-v8-win32-x64.tar.gz
```

**prebuild 生成的文件：**
```
baja-lite-xlsx-v1.0.10-napi-v8-win32-x64.tar.gz
```

**结果：** 完全匹配 ✅

---

## 📊 变量说明

| 变量 | 值 (示例) | 说明 |
|------|-----------|------|
| `{name}` | `baja-lite-xlsx` | package.json 的 name 字段 |
| `{version}` | `1.0.10` | package.json 的 version 字段 |
| `{runtime}` | `napi` 或 `electron` | 运行时类型 |
| `{abi}` | `8` 或 `34.0` | ABI 版本号 |
| `{platform}` | `win32`, `linux`, `darwin` | 操作系统 |
| `{arch}` | `x64`, `arm64` | CPU 架构 |

---

## 🎯 为什么 prebuild 生成带 name 和 version 的文件名？

这是 `prebuild` 工具的**默认行为**，目的是：

1. **避免文件名冲突** - 不同版本的预编译包有唯一的文件名
2. **便于管理** - 从文件名就能看出包名和版本
3. **符合规范** - 遵循 Node.js 社区的最佳实践

---

## 🔄 完整的文件名格式

### Node.js (N-API)

```
{name}-v{version}-{runtime}-v{abi}-{platform}-{arch}.tar.gz

示例:
baja-lite-xlsx-v1.0.10-napi-v8-win32-x64.tar.gz
```

### Electron

```
{name}-v{version}-{runtime}-v{abi}-{platform}-{arch}.tar.gz

示例:
baja-lite-xlsx-v1.0.10-electron-v34.0-win32-x64.tar.gz
```

---

## 📝 验证步骤

我们创建了 `verify-url.js` 脚本来验证配置：

```bash
node verify-url.js
```

**输出示例：**

```
📦 URL 验证

配置信息:
  name: baja-lite-xlsx
  version: 1.0.10
  package_name: {name}-v{version}-{runtime}-v{abi}-{platform}-{arch}.tar.gz

生成的 URL:

NAPI:
  https://github.com/void-soul/baja-lite-xlsx/releases/download/v1.0.10/baja-lite-xlsx-v1.0.10-napi-v8-win32-x64.tar.gz

ELECTRON:
  https://github.com/void-soul/baja-lite-xlsx/releases/download/v1.0.10/baja-lite-xlsx-v1.0.10-electron-v34.0-win32-x64.tar.gz

格式匹配检查:
  ✅ 格式完全匹配！
```

---

## 🚀 版本状态

| 版本 | package_name 格式 | 预编译包状态 | 安装状态 |
|------|------------------|-------------|---------|
| v1.0.8 及以前 | ❌ 缺少 binary.host | ❌ 无法下载 | ❌ 失败 |
| v1.0.9 | ❌ 格式错误 | ✅ 文件存在但 URL 不匹配 | ❌ 失败 |
| **v1.0.10+** | **✅ 格式正确** | **✅ 完全匹配** | **✅ 成功** |

---

## 📋 用户操作指南

### 对于 v1.0.10+ 用户

```bash
# 安装最新版本
npm install baja-lite-xlsx@latest

# 预期输出 (Windows x64 + Node 20):
# prebuild-install info trying https://github.com/.../baja-lite-xlsx-v1.0.10-napi-v8-win32-x64.tar.gz
# prebuild-install info success
```

### 对于 v1.0.9 用户

**建议升级到 v1.0.10：**

```bash
npm uninstall baja-lite-xlsx
npm install baja-lite-xlsx@latest
```

---

## 🎓 经验教训

### 1. 配置要与工具默认行为一致

`prebuild` 的默认文件名格式包含 `{name}` 和 `{version}`，所以 `package.json` 的 `binary.package_name` 也必须包含这些变量。

### 2. 发布前务必验证

使用 `verify-url.js` 或 `check-prebuild-status.js` 在发布前验证配置是否正确。

### 3. 完整的 binary 配置

```json
{
  "binary": {
    "napi_versions": [3, 4, 5, 6, 7, 8],
    "host": "https://github.com/your-org/your-repo/releases/download/",
    "remote_path": "{version}",
    "package_name": "{name}-v{version}-{runtime}-v{abi}-{platform}-{arch}.tar.gz"
  }
}
```

### 4. 测试流程

```bash
# 1. 本地构建测试
npm run build
npx prebuild --runtime napi --target 8

# 2. 检查生成的文件名
ls prebuilds/

# 3. 确认文件名与 package.json 配置匹配
node verify-url.js

# 4. 提交、标签、推送
npm version patch
git push origin master --tags

# 5. 等待 GitHub Actions 完成

# 6. 验证 Release
node check-prebuild-status.js

# 7. 发布到 npm
npm publish
```

---

## 🔗 相关文件

- `package.json` - 包含 binary 配置
- `verify-url.js` - URL 格式验证脚本
- `check-prebuild-status.js` - 预编译包状态检查脚本
- `wait-for-prebuild.js` - 等待预编译包就绪脚本
- `.github/workflows/prebuild.yml` - GitHub Actions 工作流

---

## 📚 参考文档

- [prebuild 文档](https://github.com/prebuild/prebuild)
- [prebuild-install 文档](https://github.com/prebuild/prebuild-install)
- [Node.js N-API](https://nodejs.org/api/n-api.html)

---

**修复版本：** v1.0.10  
**修复日期：** 2025-10-21  
**状态：** ✅ 已解决

