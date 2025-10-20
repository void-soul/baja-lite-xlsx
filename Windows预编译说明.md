# Windows 预编译包配置说明

## 📦 当前配置

**仅为以下平台提供预编译包：**
- ✅ Windows x64
- ✅ Node.js v20+

**其他平台会怎样？**
- Linux、macOS 用户：自动回退到源码编译
- Node.js v16、v18 用户：自动回退到源码编译

---

## 🚀 为什么只编译 Windows + Node 20？

### 优点

1. **简化构建**
   - 只需 1 个构建任务（vs 原来 9 个）
   - 构建时间：~10 分钟（vs 原来 ~30 分钟）
   - 维护成本低

2. **覆盖主要用户**
   - Windows 是最常见的开发平台
   - Node 20 是 LTS 版本，覆盖大多数用户
   - 使用 N-API，Node 20+ 都兼容

3. **降低安装门槛**
   - Windows 编译环境最复杂（VS Build Tools）
   - Windows 用户最需要预编译包
   - Linux/macOS 编译环境相对简单

### 适用场景

- ✅ Windows 开发者为主的项目
- ✅ 追求简单配置
- ✅ 预算有限（GitHub Actions 时间）
- ✅ 早期阶段项目

---

## 📊 覆盖率估算

根据 npm 下载统计：

| 平台/版本 | 占比 | 预编译 |
|----------|------|--------|
| Windows x64 + Node 20+ | ~50-60% | ✅ 有 |
| Linux + Node 20+ | ~20-25% | ❌ 源码编译 |
| macOS + Node 20+ | ~10-15% | ❌ 源码编译 |
| 其他 Node 版本 | ~5-10% | ❌ 源码编译 |

**预计效果：**
- 50-60% 用户享受快速安装
- 40-50% 用户需要编译环境

---

## 🔧 本地测试

### Windows 上测试预编译

```bash
# 1. 构建预编译包
npm run build
npm run prebuild

# 2. 查看生成的文件
ls prebuilds/
# 输出: baja-lite-xlsx-v1.0.0-napi-v8-win32-x64.tar.gz

# 3. 测试安装
npm pack
npm install baja-lite-xlsx-1.0.0.tgz --verbose
```

### 模拟其他平台（源码编译）

```bash
# 强制源码编译
npm install --build-from-source
```

---

## 📦 发布流程

### 1. 本地开发和测试

```bash
# 开发时使用
npm run build:dev  # 包含 DLL 复制
npm test
npm run example:json
```

### 2. 推送 tag 触发构建

```bash
# 提交代码
git add .
git commit -m "chore: 准备发布 v1.0.0"
git push

# 创建并推送 tag
npm version 1.0.0
git push origin v1.0.0
```

### 3. GitHub Actions 自动执行

- ⏰ 约 10 分钟完成
- ✅ 在 Windows 上构建
- ✅ 上传到 GitHub Releases

访问查看：`https://github.com/yourusername/baja-lite-xlsx/actions`

### 4. 验证 Release

访问：`https://github.com/yourusername/baja-lite-xlsx/releases`

应该看到：
```
baja-lite-xlsx-v1.0.0-napi-v8-win32-x64.tar.gz
```

### 5. 发布到 npm

```bash
npm publish
```

---

## 🧪 验证安装

### Windows + Node 20（应该下载预编译包）

```bash
# 新建测试目录
mkdir test-windows
cd test-windows
npm init -y

# 安装
npm install baja-lite-xlsx --verbose

# 应该看到：
# > prebuild-install info begin Prebuild-install version X.X.X
# > prebuild-install info install downloading from url
# ✓ 安装成功（3-10 秒）
```

### Linux 或其他 Node 版本（会源码编译）

```bash
npm install baja-lite-xlsx

# 会看到：
# > node-gyp rebuild
# ⏰ 需要 3-10 分钟编译
```

---

## 🔄 扩展到其他平台

如果将来需要支持更多平台，很容易扩展：

### 添加 Linux 支持

修改 `.github/workflows/prebuild.yml`，添加：

```yaml
jobs:
  build-linux:
    name: Build Linux x64 - Node 20
    runs-on: ubuntu-latest
    
    steps:
      - uses: actions/checkout@v3
      - uses: actions/setup-node@v3
        with:
          node-version: '20'
      # ... 其他步骤
```

### 添加 macOS 支持

```yaml
jobs:
  build-macos:
    name: Build macOS - Node 20
    runs-on: macos-latest
    
    steps:
      - uses: actions/checkout@v3
      - uses: actions/setup-node@v3
        with:
          node-version: '20'
      # ... 其他步骤
```

### 添加更多 Node.js 版本

修改 `prebuild` 命令或添加 matrix：

```yaml
strategy:
  matrix:
    node: [18, 20, 22]
```

---

## 💡 最佳实践

### 1. 在 README 中说明

```markdown
## 安装

### Windows 用户（Node.js >= 20）
✅ 自动下载预编译包，安装快速（几秒钟），无需编译环境。

### 其他平台或 Node.js 版本
⚠️ 需要编译环境：
- Linux: build-essential, cmake, vcpkg + xlnt
- macOS: Xcode CLI, cmake, vcpkg + xlnt
- Windows (Node < 20): VS Build Tools, Python, vcpkg + xlnt

详见 [INSTALLATION.md](INSTALLATION.md)
```

### 2. package.json 保持回退

```json
{
  "scripts": {
    "install": "prebuild-install --runtime napi || node-gyp rebuild"
  }
}
```

确保其他平台能正常编译。

### 3. 监控安装成功率

通过 npm 统计或用户反馈，了解：
- 有多少用户使用预编译包
- 有多少用户需要源码编译
- 是否需要扩展到其他平台

---

## 📊 成本分析

### GitHub Actions 使用时间

| 配置 | 每次发布时间 | 月度限制 |
|------|-------------|---------|
| 仅 Windows + Node 20 | ~10 分钟 | 2000 分钟 |
| 可发布次数 | ~200 次/月 | 足够使用 |

### 存储空间

| 项目 | 大小 |
|------|------|
| 单个预编译包 | ~2-5 MB |
| 10 个版本 | ~20-50 MB |
| GitHub Releases | 免费无限 |

---

## ❓ 常见问题

### Q: Linux 用户能安装吗？

A: 能！会自动回退到源码编译。他们需要安装编译环境。

### Q: Node 18 用户能用吗？

A: 能！也会回退到源码编译。或者他们可以升级到 Node 20。

### Q: 为什么不支持所有平台？

A: 简化配置，降低维护成本。根据实际需求，可以随时扩展。

### Q: 如何知道是否使用了预编译包？

```bash
npm install baja-lite-xlsx --verbose

# 如果看到 "prebuild-install info install" 就是预编译包
# 如果看到 "node-gyp rebuild" 就是源码编译
```

---

## ✅ 总结

**当前配置：**
- ✅ Windows x64 + Node 20 → 预编译包（快速安装）
- ⚠️ 其他平台/版本 → 源码编译（需要环境）

**优点：**
- 简单、快速、易维护
- 覆盖最主要的用户群体
- 降低 Windows 用户的安装门槛

**何时扩展：**
- 用户反馈需要其他平台支持
- 下载量增长，值得投入更多资源
- 有足够时间维护多平台构建

现在的配置已经是一个很好的平衡点！🎯

