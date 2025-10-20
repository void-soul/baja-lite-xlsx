# 预编译包配置指南

## 什么是预编译包？

预编译包允许用户无需安装编译环境即可使用你的原生模块。安装流程：
1. ✅ 先尝试下载预编译的 `.node` 文件
2. ❌ 如果下载失败，回退到本地编译

---

## 方案对比

### 1. prebuild + prebuild-install（推荐）✅

**优点：**
- 简单易用
- 自动上传到 GitHub Releases
- 支持多 Node.js 版本
- 社区广泛使用

**缺点：**
- 需要 GitHub 仓库
- 需要在多平台上构建

### 2. node-pre-gyp

**优点：**
- 功能强大
- 支持自定义存储（S3、CDN 等）

**缺点：**
- 配置复杂
- 需要管理存储服务

---

## 实现步骤：使用 prebuild

### 第1步：安装依赖

```bash
npm install --save prebuild-install
npm install --save-dev prebuild
```

### 第2步：修改 package.json

```json
{
  "name": "baja-lite-xlsx",
  "version": "1.0.0",
  "scripts": {
    "install": "prebuild-install || node-gyp rebuild",
    "build": "node-gyp rebuild",
    "prebuild": "prebuild --all --strip --verbose",
    "prebuild-upload": "prebuild --upload YOUR_GITHUB_TOKEN"
  },
  "binary": {
    "napi_versions": [3, 4, 5, 6, 7, 8]
  }
}
```

**说明：**
- `install`: 先尝试 `prebuild-install` 下载，失败则编译
- `prebuild`: 为所有平台构建
- `--strip`: 移除调试信息，减小体积
- `napi_versions`: 支持的 N-API 版本

### 第3步：在各平台构建

#### Windows 上构建
```powershell
# 确保 xlnt 已安装
npm run build

# 创建预编译包
npm run prebuild

# 会生成：prebuilds/baja-lite-xlsx-v1.0.0-napi-v3-win32-x64.tar.gz
```

#### Linux 上构建（使用 Docker）
```bash
# 创建构建脚本
cat > build-linux.sh << 'EOF'
#!/bin/bash
docker run --rm -v $(pwd):/app -w /app node:18 bash -c "
  apt-get update && apt-get install -y build-essential cmake git libz-dev
  cd /opt && git clone https://github.com/Microsoft/vcpkg.git
  cd vcpkg && ./bootstrap-vcpkg.sh && ./vcpkg install xlnt
  export VCPKG_ROOT=/opt/vcpkg
  cd /app
  npm install
  npm run prebuild
"
EOF

chmod +x build-linux.sh
./build-linux.sh
```

#### macOS 上构建
```bash
# 在 macOS 机器上
npm install
npm run prebuild
```

### 第4步：上传到 GitHub Releases

#### 方法A：手动上传

1. 在 GitHub 创建一个 Release（如 v1.0.0）
2. 将 `prebuilds/` 目录下的所有 `.tar.gz` 文件上传

#### 方法B：自动上传

```bash
# 1. 获取 GitHub Token
# 访问 https://github.com/settings/tokens
# 创建 token，需要 "repo" 权限

# 2. 设置环境变量
export GITHUB_TOKEN=your_github_token_here

# 3. 上传（会自动创建 Release）
npm run prebuild-upload
```

### 第5步：测试

```bash
# 在新环境测试（无编译工具）
npm install baja-lite-xlsx

# 应该直接下载预编译包，无需编译
```

---

## 使用 GitHub Actions 自动构建

创建 `.github/workflows/prebuild.yml`:

```yaml
name: Prebuild

on:
  push:
    tags:
      - 'v*'

jobs:
  build-windows:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v3
      
      - name: Setup Node.js
        uses: actions/setup-node@v3
        with:
          node-version: '18'
      
      - name: Install vcpkg
        run: |
          git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg
          cd C:\vcpkg
          .\bootstrap-vcpkg.bat
          .\vcpkg install xlnt:x64-windows
        shell: powershell
      
      - name: Set VCPKG_ROOT
        run: echo "VCPKG_ROOT=C:\vcpkg" >> $GITHUB_ENV
      
      - name: Install dependencies
        run: npm install
      
      - name: Prebuild
        run: npm run prebuild
      
      - name: Upload to Release
        run: npm run prebuild-upload
        env:
          GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}

  build-linux:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      
      - name: Setup Node.js
        uses: actions/setup-node@v3
        with:
          node-version: '18'
      
      - name: Install system dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y build-essential cmake git libz-dev
      
      - name: Install vcpkg and xlnt
        run: |
          cd /opt
          sudo git clone https://github.com/Microsoft/vcpkg.git
          cd vcpkg
          sudo ./bootstrap-vcpkg.sh
          sudo ./vcpkg install xlnt
          echo "VCPKG_ROOT=/opt/vcpkg" >> $GITHUB_ENV
      
      - name: Install dependencies
        run: npm install
      
      - name: Prebuild
        run: npm run prebuild
      
      - name: Upload to Release
        run: npm run prebuild-upload
        env:
          GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}

  build-macos:
    runs-on: macos-latest
    steps:
      - uses: actions/checkout@v3
      
      - name: Setup Node.js
        uses: actions/setup-node@v3
        with:
          node-version: '18'
      
      - name: Install dependencies
        run: brew install cmake
      
      - name: Install vcpkg and xlnt
        run: |
          cd ~
          git clone https://github.com/Microsoft/vcpkg.git
          cd vcpkg
          ./bootstrap-vcpkg.sh
          ./vcpkg install xlnt
          echo "VCPKG_ROOT=$HOME/vcpkg" >> $GITHUB_ENV
      
      - name: Install dependencies
        run: npm install
      
      - name: Prebuild
        run: npm run prebuild
      
      - name: Upload to Release
        run: npm run prebuild-upload
        env:
          GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}
```

---

## 发布流程（使用预编译包）

### 1. 打 Tag 触发构建
```bash
# 更新版本
npm version 1.0.0

# 推送 tag
git push origin v1.0.0
```

### 2. GitHub Actions 自动执行
- ✅ 在 Windows、Linux、macOS 上构建
- ✅ 自动上传到 GitHub Releases
- ✅ 生成各平台的预编译包

### 3. 发布到 npm
```bash
npm publish
```

### 4. 用户安装
```bash
npm install baja-lite-xlsx
# ✅ 自动下载预编译包
# ✅ 无需编译环境
# ✅ 安装快速（几秒钟）
```

---

## 目录结构

```
baja-lite-xlsx/
├── prebuilds/              # 预编译包（不提交到 git）
│   ├── baja-lite-xlsx-v1.0.0-napi-v3-win32-x64.tar.gz
│   ├── baja-lite-xlsx-v1.0.0-napi-v3-linux-x64.tar.gz
│   └── baja-lite-xlsx-v1.0.0-napi-v3-darwin-x64.tar.gz
├── src/                    # C++ 源码
├── binding.gyp
├── package.json
└── .github/
    └── workflows/
        └── prebuild.yml
```

---

## 调试和测试

### 查看下载地址
```bash
# prebuild-install 会从这里下载：
# https://github.com/yourusername/baja-lite-xlsx/releases/download/v1.0.0/baja-lite-xlsx-v1.0.0-napi-v3-win32-x64.tar.gz
```

### 强制本地编译测试
```bash
npm install --build-from-source
```

### 查看安装日志
```bash
npm install baja-lite-xlsx --verbose
```

---

## 常见问题

### Q: 预编译包体积多大？
A: 通常每个平台 2-5 MB（压缩后）

### Q: 需要为所有 Node.js 版本构建吗？
A: 不需要！使用 N-API 可以跨 Node.js 版本兼容

### Q: 如何减小体积？
```bash
# 使用 --strip 移除调试符号
npm run prebuild -- --strip

# 压缩 DLL（Windows）
upx --best build/Release/*.dll
```

### Q: 下载失败怎么办？
A: 会自动回退到本地编译：
```json
"install": "prebuild-install || node-gyp rebuild"
```

### Q: 支持私有仓库吗？
A: 支持，但需要配置访问令牌：
```bash
npm install baja-lite-xlsx --prebuild-install-token YOUR_TOKEN
```

---

## 成本考虑

### GitHub Actions
- ✅ 公开仓库：免费
- ⚠️ 私有仓库：有限制（每月 2000 分钟）

### 存储
- ✅ GitHub Releases：免费且无限制
- ⚠️ 每个 Release 最大 2 GB

### 带宽
- ✅ GitHub CDN：全球加速
- ✅ 无流量限制

---

## 替代方案：node-pre-gyp

如果需要更多控制（如使用 S3、CDN），可以使用 `node-pre-gyp`:

```json
{
  "binary": {
    "module_name": "baja_xlsx",
    "module_path": "./lib/binding/",
    "host": "https://your-cdn.com/releases/",
    "remote_path": "{version}",
    "package_name": "{module_name}-v{version}-{node_abi}-{platform}-{arch}.tar.gz"
  }
}
```

但配置更复杂，需要自己管理存储。

---

## 推荐配置

对于 baja-xlsx，推荐：

1. ✅ 使用 `prebuild` + GitHub Releases
2. ✅ 使用 GitHub Actions 自动构建
3. ✅ 支持 Windows、Linux、macOS
4. ✅ 保留源码编译作为后备

**好处：**
- 95% 用户可以直接安装（无需编译环境）
- 5% 特殊平台用户可以源码编译
- 完全免费
- 维护简单

