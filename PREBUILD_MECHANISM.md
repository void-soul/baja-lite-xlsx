# 预编译包查找机制详解

## 📦 完整流程图

```
┌─────────────────────────────────────────────────────────────┐
│  用户执行: npm install baja-lite-xlsx                        │
└────────────────────────┬────────────────────────────────────┘
                         ↓
┌─────────────────────────────────────────────────────────────┐
│  npm 从 registry 下载 baja-lite-xlsx@1.0.7                   │
│  内容: package.json + index.js + src/ + ...                  │
└────────────────────────┬────────────────────────────────────┘
                         ↓
┌─────────────────────────────────────────────────────────────┐
│  npm 执行 "install" 脚本:                                     │
│  prebuild-install --runtime napi || node-gyp rebuild        │
└────────────────────────┬────────────────────────────────────┘
                         ↓
┌─────────────────────────────────────────────────────────────┐
│  prebuild-install 读取 package.json 的 binary 配置           │
│  {                                                           │
│    "binary": {                                               │
│      "host": "https://github.com/.../releases/download/",   │
│      "remote_path": "{version}",                             │
│      "package_name": "{runtime}-v{abi}-{platform}-{arch}..." │
│    }                                                         │
│  }                                                           │
└────────────────────────┬────────────────────────────────────┘
                         ↓
┌─────────────────────────────────────────────────────────────┐
│  构建下载 URL:                                                │
│  https://github.com/void-soul/baja-lite-xlsx/               │
│    releases/download/v1.0.7/napi-v8-win32-x64.tar.gz        │
│                                                              │
│  参数替换:                                                    │
│  - {version}   → v1.0.7   (从 package.json version 字段)     │
│  - {runtime}   → napi     (运行时类型)                        │
│  - {abi}       → 8        (N-API ABI 版本)                   │
│  - {platform}  → win32    (操作系统: win32/linux/darwin)     │
│  - {arch}      → x64      (架构: x64/arm64)                  │
└────────────────────────┬────────────────────────────────────┘
                         ↓
                    尝试下载
                         ↓
        ┌────────────────┴────────────────┐
        ↓                                 ↓
  ✅ 下载成功                        ❌ 下载失败
        ↓                                 ↓
┌───────────────────┐          ┌────────────────────┐
│ 解压到 build/     │          │ 执行 node-gyp      │
│ Release/          │          │ rebuild            │
│                   │          │ （源码编译）        │
│ baja_xlsx.node    │          └────────────────────┘
└───────────────────┘
        ↓
    安装完成 🎉
```

---

## 🔧 核心配置详解

### 1. package.json 的 binary 配置

```json
{
  "binary": {
    "napi_versions": [3, 4, 5, 6, 7, 8],
    "host": "https://github.com/void-soul/baja-lite-xlsx/releases/download/",
    "remote_path": "{version}",
    "package_name": "{runtime}-v{abi}-{platform}-{arch}.tar.gz"
  }
}
```

**字段说明：**

| 字段 | 作用 | 示例值 |
|------|------|--------|
| `napi_versions` | 支持的 N-API 版本列表 | `[3, 4, 5, 6, 7, 8]` |
| `host` | 预编译包托管的基础 URL | `https://github.com/.../releases/download/` |
| `remote_path` | 相对路径模板 | `{version}` → `v1.0.7` |
| `package_name` | 文件名模板 | `napi-v8-win32-x64.tar.gz` |

---

### 2. URL 构建规则

**完整 URL 格式：**
```
{host}{remote_path}/{package_name}
```

**实际示例（Node.js）：**
```
https://github.com/void-soul/baja-lite-xlsx/releases/download/v1.0.7/napi-v8-win32-x64.tar.gz
│                                                              │       │                  │
└─ host                                                        └─ remote_path            └─ package_name
```

**实际示例（Electron）：**
```
https://github.com/void-soul/baja-lite-xlsx/releases/download/v1.0.7/electron-v34.0-win32-x64.tar.gz
```

---

### 3. 变量替换规则

| 变量 | 获取方式 | Windows x64 + Node 20 | Windows x64 + Electron 34 |
|------|---------|----------------------|---------------------------|
| `{version}` | `package.json` 的 `version` 字段 + `v` 前缀 | `v1.0.7` | `v1.0.7` |
| `{runtime}` | 命令行参数或自动检测 | `napi` | `electron` |
| `{abi}` | 根据运行时版本映射 | `8` (N-API v8) | `34.0` (Electron ABI) |
| `{platform}` | `process.platform` | `win32` | `win32` |
| `{arch}` | `process.arch` | `x64` | `x64` |

---

## 🔄 版本匹配机制

### npm 版本 ↔ GitHub Release Tag

**关键点：版本号必须完全匹配！**

| npm 包版本 | package.json | Git Tag | GitHub Release | 预编译包 URL |
|-----------|--------------|---------|----------------|-------------|
| 1.0.7 | `"version": "1.0.7"` | `v1.0.7` | Release v1.0.7 | `.../v1.0.7/napi-v8-...` |
| 1.0.8 | `"version": "1.0.8"` | `v1.0.8` | Release v1.0.8 | `.../v1.0.8/napi-v8-...` |

**流程：**

```bash
# 1. 更新 package.json 版本
npm version patch  # 1.0.7 → 1.0.8

# 2. Git 自动创建 tag
git tag v1.0.8     # 由 npm version 自动完成

# 3. 推送到 GitHub
git push origin master --tags

# 4. GitHub Actions 触发（监听 v* tag）
# 构建预编译包并上传到 Release v1.0.8

# 5. 发布到 npm
npm publish        # 发布 1.0.8 版本

# 6. 用户安装
npm install baja-lite-xlsx@1.0.8
# → prebuild-install 查找：
#   https://github.com/.../releases/download/v1.0.8/...
```

---

## 🎯 ABI 版本映射

### Node.js (N-API)

`prebuild-install` 使用 `--runtime napi`，ABI 版本固定：

| Node.js 版本 | N-API ABI | 文件名 |
|-------------|-----------|--------|
| 16.x | v8 | `napi-v8-win32-x64.tar.gz` |
| 18.x | v8 | `napi-v8-win32-x64.tar.gz` |
| 20.x | v8 | `napi-v8-win32-x64.tar.gz` |

**优势：** 一个预编译包支持多个 Node.js 版本！

### Electron

每个 Electron 版本有独立的 ABI：

| Electron 版本 | ABI | 文件名 |
|--------------|-----|--------|
| 34.0.0 | v34.0 | `electron-v34.0-win32-x64.tar.gz` |
| 35.0.0 | v35.0 | `electron-v35.0-win32-x64.tar.gz` |

---

## 📂 GitHub Release 结构

### Release v1.0.7 示例

```
Release: v1.0.7
├─ Tag: v1.0.7
├─ Assets:
│  ├─ napi-v8-win32-x64.tar.gz           (约 800KB)
│  │  └─ build/Release/baja_xlsx.node
│  │
│  └─ electron-v34.0-win32-x64.tar.gz    (约 800KB)
│     └─ build/Release/baja_xlsx.node
│
└─ Release Notes:
   - 支持的平台和版本
   - 安装说明
   - ...
```

---

## 🧪 调试预编译包下载

### 查看 prebuild-install 的详细日志

```bash
# 方法 1: 设置环境变量
set PREBUILD_INSTALL_DEBUG=1
npm install baja-lite-xlsx

# 方法 2: 直接运行 prebuild-install
npx prebuild-install --runtime napi --verbose
```

**输出示例：**

```
prebuild-install info begin Prebuild-install version 7.1.3
prebuild-install info looking for local prebuild @ prebuilds/napi-v8-win32-x64.tar.gz
prebuild-install info looking for cached prebuild @ C:\Users\...\prebuild\napi-v8-win32-x64.tar.gz
prebuild-install http request https://github.com/void-soul/baja-lite-xlsx/releases/download/v1.0.7/napi-v8-win32-x64.tar.gz
prebuild-install http 200 https://github.com/.../napi-v8-win32-x64.tar.gz
prebuild-install info downloading to @ C:\Users\...\prebuild\napi-v8-win32-x64.tar.gz.tmp
prebuild-install info downloading to @ C:\Users\...\prebuild\napi-v8-win32-x64.tar.gz
prebuild-install info verified @ C:\Users\...\prebuild\napi-v8-win32-x64.tar.gz
prebuild-install info unpacking @ C:\Users\...\prebuild\napi-v8-win32-x64.tar.gz
prebuild-install info success
```

---

## ❓ 常见问题

### Q1: 如果 npm 版本是 1.0.7，但 GitHub Release 是 v1.0.6 会怎样？

**A:** 下载失败，回退到源码编译。

```
prebuild-install http 404 https://github.com/.../v1.0.7/napi-v8-win32-x64.tar.gz
prebuild-install WARN install No prebuilt binaries found
→ 执行 node-gyp rebuild
```

**解决方案：** 确保 npm 版本和 GitHub Release tag 一致。

---

### Q2: 可以为旧版本补充预编译包吗？

**A:** 可以！手动创建对应版本的 Release 并上传预编译包。

```bash
# 1. 切换到旧版本
git checkout v1.0.5

# 2. 构建预编译包
npm run build
npx prebuild --runtime napi --target 8

# 3. 手动上传到 GitHub Release v1.0.5
# 通过 GitHub Web UI 或 GitHub CLI
```

---

### Q3: 为什么用 GitHub Releases 而不是 npm？

**优势：**

| GitHub Releases | npm |
|----------------|-----|
| ✅ 免费托管大文件 | ❌ 包大小限制 |
| ✅ 多个预编译包共存 | ❌ 一个版本一个包 |
| ✅ 按需下载 | ❌ 全部下载 |
| ✅ CDN 加速 | ✅ CDN 加速 |

**npm 包内容：**
- 源代码（必需）
- TypeScript 类型定义
- 示例代码
- 文档

**GitHub Releases：**
- 预编译包（可选，按平台/运行时）

---

### Q4: 如何支持更多平台？

**修改 `.github/workflows/prebuild.yml`：**

```yaml
strategy:
  matrix:
    include:
      # Windows
      - os: windows-latest
        runtime: napi
        target: 8
      
      # Linux
      - os: ubuntu-latest
        runtime: napi
        target: 8
      
      # macOS
      - os: macos-latest
        runtime: napi
        target: 8
```

每个平台会生成独立的预编译包：
- `napi-v8-win32-x64.tar.gz`
- `napi-v8-linux-x64.tar.gz`
- `napi-v8-darwin-x64.tar.gz`
- `napi-v8-darwin-arm64.tar.gz` (Apple Silicon)

---

## 🎯 最佳实践

### ✅ 发布流程检查清单

```bash
# 1. 确保代码已提交
git status

# 2. 更新版本（会自动创建 git tag）
npm version patch

# 3. 推送代码和 tag
git push origin master --tags

# 4. 等待 GitHub Actions 完成（约 10-12 分钟）
# 查看 https://github.com/void-soul/baja-lite-xlsx/actions

# 5. 验证 Release 创建成功
# 查看 https://github.com/void-soul/baja-lite-xlsx/releases/latest

# 6. 发布到 npm
npm publish

# 7. 测试安装（在干净环境）
npm install baja-lite-xlsx@latest
```

---

## 📊 用户安装体验对比

### Windows x64 + Node 20 用户

```bash
npm install baja-lite-xlsx

# 输出:
> baja-lite-xlsx@1.0.7 install
> prebuild-install --runtime napi || node-gyp rebuild

prebuild-install info begin
prebuild-install http request GET https://github.com/.../v1.0.7/napi-v8-win32-x64.tar.gz
prebuild-install http 200 https://github.com/.../napi-v8-win32-x64.tar.gz
prebuild-install info success

✅ 耗时: ~10 秒
✅ 无需编译环境
```

### Linux 用户（无预编译包）

```bash
npm install baja-lite-xlsx

# 输出:
> baja-lite-xlsx@1.0.7 install
> prebuild-install --runtime napi || node-gyp rebuild

prebuild-install WARN install No prebuilt binaries found (tried 1 download)
gyp info spawn make
gyp info spawn args [ 'BUILDTYPE=Release', '-C', 'build' ]
...

⚙️ 耗时: ~3-5 分钟
⚙️ 需要编译环境（gcc, make, cmake）
```

---

## 🔗 相关资源

- [prebuild-install 文档](https://github.com/prebuild/prebuild-install)
- [prebuild 文档](https://github.com/prebuild/prebuild)
- [GitHub Releases API](https://docs.github.com/en/rest/releases)
- [N-API 版本矩阵](https://nodejs.org/api/n-api.html#node-api-version-matrix)

---

**文档版本：** v1.0.8  
**最后更新：** 2025-10-21

