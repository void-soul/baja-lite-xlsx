# 预编译包查找机制 - 快速版

## 🎯 核心问题

**用户问：npm 发布的版本是 1.0.7，它怎么知道去哪里找预编译包？**

---

## ✅ 简单答案

通过 `package.json` 的 **`binary`** 配置！

```json
{
  "version": "1.0.7",
  "binary": {
    "host": "https://github.com/void-soul/baja-lite-xlsx/releases/download/",
    "remote_path": "{version}",
    "package_name": "{runtime}-v{abi}-{platform}-{arch}.tar.gz"
  }
}
```

---

## 🔄 5 步流程

```
1️⃣ 用户安装
   npm install baja-lite-xlsx@1.0.7

2️⃣ npm 下载包（包含 package.json）
   从 npmjs.com 下载源码包

3️⃣ 执行 install 脚本
   prebuild-install --runtime napi

4️⃣ prebuild-install 构建 URL
   host + remote_path + package_name
   = https://github.com/.../releases/download/v1.0.7/napi-v8-win32-x64.tar.gz

5️⃣ 下载预编译包
   成功 ✅ → 使用预编译包
   失败 ❌ → 源码编译
```

---

## 🔗 URL 构建示例

### Windows x64 + Node 20

```javascript
{
  version: "1.0.7",          // → v1.0.7
  runtime: "napi",           // → napi
  abi: "8",                  // → v8
  platform: "win32",         // → win32
  arch: "x64"                // → x64
}

↓ 替换模板

https://github.com/void-soul/baja-lite-xlsx/releases/download/v1.0.7/napi-v8-win32-x64.tar.gz
```

### Windows x64 + Electron 34

```javascript
{
  version: "1.0.7",          // → v1.0.7
  runtime: "electron",       // → electron
  abi: "34.0",               // → v34.0
  platform: "win32",         // → win32
  arch: "x64"                // → x64
}

↓ 替换模板

https://github.com/void-soul/baja-lite-xlsx/releases/download/v1.0.7/electron-v34.0-win32-x64.tar.gz
```

---

## 🎨 可视化流程

```
┌─────────────────────┐
│  npm registry       │
│  baja-lite-xlsx     │
│  v1.0.7             │
└──────────┬──────────┘
           │ npm install
           ↓
┌─────────────────────┐
│  package.json       │
│  "version": "1.0.7" │
│  "binary": {        │
│    "host": "..."    │
│  }                  │
└──────────┬──────────┘
           │ prebuild-install 读取
           ↓
┌─────────────────────┐
│  构建下载 URL       │
│  github.com/...     │
│  /releases/         │
│  download/v1.0.7/   │
│  napi-v8-...        │
└──────────┬──────────┘
           │ HTTP GET
           ↓
┌─────────────────────┐
│  GitHub Releases    │
│  v1.0.7             │
│  ├─ napi-v8-...     │
│  └─ electron-v...   │
└──────────┬──────────┘
           │ 下载
           ↓
┌─────────────────────┐
│  解压到             │
│  build/Release/     │
│  baja_xlsx.node     │
└─────────────────────┘
```

---

## 🔑 关键点

1. **版本匹配**
   - npm 版本 1.0.7
   - Git tag v1.0.7
   - GitHub Release v1.0.7
   - **必须完全一致！**

2. **自动变量替换**
   - `{version}` → 从 package.json
   - `{platform}` → 自动检测（win32/linux/darwin）
   - `{arch}` → 自动检测（x64/arm64）
   - `{runtime}` → 命令行参数
   - `{abi}` → 根据运行时版本

3. **回退机制**
   - 找不到预编译包？自动源码编译
   - 无需用户干预

---

## 🐛 刚才发现的问题

**之前缺少：**
```json
{
  "binary": {
    "napi_versions": [3, 4, 5, 6, 7, 8]
    // ❌ 缺少 host、remote_path、package_name
  }
}
```

**已修复：**
```json
{
  "binary": {
    "napi_versions": [3, 4, 5, 6, 7, 8],
    "host": "https://github.com/void-soul/baja-lite-xlsx/releases/download/",
    "remote_path": "{version}",
    "package_name": "{runtime}-v{abi}-{platform}-{arch}.tar.gz"
    // ✅ 完整配置
  }
}
```

---

## 📚 更多详情

查看完整文档：[PREBUILD_MECHANISM.md](./PREBUILD_MECHANISM.md)

