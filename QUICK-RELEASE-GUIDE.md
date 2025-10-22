# 📦 预编译包发布快速指南

## 🎯 目标

发布一个**真正开箱即用**的预编译包，包含所有必需的 DLL 文件。

## ✅ 发布前检查清单

### 1️⃣ 本地构建测试

```bash
# 清理旧的构建
npm run clean

# 重新构建
npm run build

# 复制 DLL 文件
npm run copy-dlls

# 验证 build/Release 目录
ls build/Release/
# 应该看到:
# - baja_xlsx.node
# - xlnt.dll
# - zlib1.dll
# - libexpat.dll

# 运行测试
npm test
```

### 2️⃣ 创建预编译包

```bash
# 创建预编译包（会自动复制 DLL）
npm run prebuild

# 测试预编译包完整性
npm run test:prebuild
```

**期望输出**:
```
✅ 所有预编译包验证通过！
📦 可以安全发布到 GitHub Release
```

### 3️⃣ 更新版本号

```bash
# 更新版本（会自动更新 package.json 和创建 git tag）
npm version patch   # 1.0.12 -> 1.0.13
# 或
npm version minor   # 1.0.12 -> 1.1.0
# 或
npm version major   # 1.0.12 -> 2.0.0
```

### 4️⃣ 推送到 GitHub

```bash
# 推送代码和 tag
git push origin main
git push origin --tags

# 或一次性推送
git push origin main --tags
```

### 5️⃣ 监控 GitHub Actions

访问: https://github.com/void-soul/baja-lite-xlsx/actions

检查以下步骤是否成功：

- ✅ Install vcpkg and dependencies
- ✅ Build native module
- ✅ **Package DLL dependencies** ⬅️ 新增
- ✅ **Verify DLL files are packaged** ⬅️ 新增
- ✅ Create prebuild package for Node.js
- ✅ Create prebuild package for Electron 34
- ✅ Create Release

### 6️⃣ 验证 GitHub Release

1. 访问: https://github.com/void-soul/baja-lite-xlsx/releases
2. 检查最新的 Release
3. 下载预编译包（如 `baja-lite-xlsx-v1.0.13-napi-v8-win32-x64.tar.gz`）
4. 解压并验证内容：

```bash
tar -tzf baja-lite-xlsx-v1.0.13-napi-v8-win32-x64.tar.gz

# 应该看到:
# build/Release/baja_xlsx.node
# build/Release/xlnt.dll
# build/Release/zlib1.dll
# build/Release/libexpat.dll
```

### 7️⃣ 测试安装

在一个**干净的测试环境**中测试：

```bash
# 创建测试目录
mkdir test-install
cd test-install

# 初始化 npm 项目
npm init -y

# 安装包
npm install baja-lite-xlsx

# 验证 DLL 是否存在
ls node_modules/baja-lite-xlsx/build/Release/
# 应该看到 .node 和所有 DLL

# 测试使用
node -e "const xlsx = require('baja-lite-xlsx'); console.log('✅ 加载成功!');"
```

### 8️⃣ 发布到 npm（可选）

```bash
# 如果还要发布到 npm
npm publish
```

**注意**: npm 包会包含源码，用户安装时会先尝试从 GitHub Release 下载预编译包。

## 🔧 常用命令速查

| 命令 | 说明 |
|------|------|
| `npm run build` | 编译原生模块 |
| `npm run copy-dlls` | 手动复制 DLL 文件 |
| `npm run prebuild` | 创建预编译包（自动复制 DLL） |
| `npm run test:prebuild` | 测试预编译包完整性 |
| `npm run clean` | 清理构建文件 |
| `npm test` | 运行测试 |

## 📂 关键文件说明

| 文件 | 作用 |
|------|------|
| `scripts/package-dlls.js` | 在预编译前复制 DLL 文件 |
| `scripts/postinstall.js` | 用户安装后验证 DLL |
| `scripts/test-prebuild-package.js` | 测试预编译包完整性 |
| `.github/workflows/prebuild.yml` | CI/CD 配置 |
| `PREBUILD-DLL-PACKAGING.md` | 完整技术文档 |

## 🐛 故障排除

### 问题: 本地构建后缺少 DLL

**解决**:
```bash
# 手动复制 DLL
npm run copy-dlls

# 或指定 vcpkg 路径
set VCPKG_ROOT=E:\vcpkg
npm run copy-dlls
```

### 问题: CI 构建失败 - 找不到 DLL

**原因**: vcpkg 安装失败或路径不对

**解决**: 检查 `.github/workflows/prebuild.yml` 中的：
- `Install vcpkg and dependencies` 步骤
- `VCPKG_ROOT` 环境变量

### 问题: 预编译包验证失败

**解决**:
```bash
# 1. 清理并重新构建
npm run clean
npm run build
npm run copy-dlls

# 2. 重新创建预编译包
npm run prebuild

# 3. 再次测试
npm run test:prebuild
```

### 问题: 用户安装后提示找不到模块

**可能原因**:
1. 预编译包缺少 DLL（发布前应该用 `npm run test:prebuild` 验证）
2. 用户的 Node.js 版本不匹配（预编译包是 Node 20+）
3. 用户的架构不匹配（预编译包是 x64）

**解决**:
- 确保 CI 构建成功
- 查看 CI 日志中的 "Verify DLL files are packaged" 步骤
- 在多个环境测试安装

## 📊 预编译包覆盖范围

当前配置的预编译包：

| 平台 | 架构 | Node.js | Electron | 文件名 |
|------|------|---------|----------|--------|
| Windows | x64 | 20+ (N-API v8) | - | `*-napi-v8-win32-x64.tar.gz` |
| Windows | x64 | - | 34+ | `*-electron-v34.0-win32-x64.tar.gz` |

**未覆盖的环境**（将回退到源码编译）:
- Windows x86 (32位)
- Linux（所有架构）
- macOS（所有架构）
- Node.js 16, 18

## 🎯 最佳实践

1. ✅ **每次发布前都运行 `npm run test:prebuild`**
2. ✅ **监控 CI 构建日志，确认 DLL 被打包**
3. ✅ **在干净环境测试安装**
4. ✅ **更新 CHANGELOG.md**
5. ✅ **为每个 Release 添加详细说明**
6. ✅ **保持版本号规范（语义化版本）**

## 🚀 完整发布流程

```bash
# 1. 开发和测试
npm run build
npm run copy-dlls
npm test

# 2. 创建并测试预编译包
npm run prebuild
npm run test:prebuild

# 3. 更新版本
npm version patch  # 或 minor/major

# 4. 推送
git push origin main --tags

# 5. 等待 CI 完成并验证 Release

# 6. 在测试环境验证安装
mkdir ../test-install && cd ../test-install
npm init -y
npm install baja-lite-xlsx
node -e "require('baja-lite-xlsx')"

# 7. 完成！🎉
```

## 📚 相关文档

- [完整技术文档](./PREBUILD-DLL-PACKAGING.md)
- [DLL 依赖问题解决](./DLL-DEPENDENCY-FIX.md)
- [项目 README](./README.md)
- [中文 README](./README.zh-CN.md)

---

**记住**: 真正的预编译包应该是**开箱即用**的！如果用户还需要手动配置，那就不叫预编译。✨

