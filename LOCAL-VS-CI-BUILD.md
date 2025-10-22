# 🔧 本地构建 vs CI 构建

## 📋 区别说明

### 本地构建
- 需要手动设置 `VCPKG_ROOT` 环境变量
- 使用专用脚本 `scripts/create-prebuilds.bat`

### CI 构建（GitHub Actions）
- 自动设置 `VCPKG_ROOT=C:\vcpkg`
- 直接运行 `npm run prebuild`

---

## 🖥️ 本地构建方法

### ⭐ 推荐方法：使用专用脚本

```bash
# 一键创建预编译包（自动设置 VCPKG_ROOT）
.\scripts\create-prebuilds.bat
```

该脚本会：
1. ✅ 自动检测并设置 VCPKG_ROOT
2. ✅ 编译原生模块
3. ✅ 复制 DLL 文件
4. ✅ 创建预编译包
5. ✅ 将 DLL 打包到 .tar.gz

---

### 方法 2：手动设置环境变量

#### PowerShell
```powershell
# 设置环境变量（临时）
$env:VCPKG_ROOT="E:\vcpkg"

# 运行 prebuild
npm run prebuild

# 验证
npm run test:prebuild
```

#### CMD
```cmd
# 设置环境变量（临时）
set VCPKG_ROOT=E:\vcpkg

# 运行 prebuild
npm run prebuild

# 验证
npm run test:prebuild
```

---

## 🚀 GitHub Actions 构建

### 工作流程

```yaml
1. Install vcpkg
   └─ 安装到 C:\vcpkg
   
2. Install dependencies
   └─ vcpkg install xlnt:x64-windows
   
3. Install npm dependencies
   └─ npm install --ignore-scripts
   
4. Create prebuild packages
   ├─ npx prebuild --runtime napi (编译 + 初步打包)
   └─ npx prebuild --runtime electron (编译 + 初步打包)
   └─ env: VCPKG_ROOT=C:\vcpkg  ⬅️ 自动设置
   
5. Pack DLLs into packages
   └─ node scripts/pack-dlls-into-prebuild.js
   └─ 自动复制 DLL 并重新打包
```

### 触发方式

```bash
# 推送 tag
git tag v1.0.13
git push origin v1.0.13

# 或手动触发
# 访问 GitHub Actions 页面，点击 "Run workflow"
```

---

## 📊 流程对比

| 步骤 | 本地（脚本） | 本地（手动） | CI（GitHub Actions） |
|------|------------|------------|---------------------|
| 设置 VCPKG_ROOT | ✅ 自动 | ❌ 手动 | ✅ 自动 |
| 编译原生模块 | ✅ | ✅ | ✅ |
| 复制 DLL | ✅ | ✅ | ✅ (在 pack-dlls 中) |
| 创建预编译包 | ✅ | ✅ | ✅ |
| 打包 DLL 到 tar.gz | ✅ | ✅ | ✅ |
| 验证包内容 | ✅ | ❌ 手动 | ✅ |

---

## ✅ 验证方法

### 本地验证

```bash
# 1. 创建预编译包
.\scripts\create-prebuilds.bat

# 2. 测试包完整性
npm run test:prebuild

# 预期输出：
# ✅ 所有预编译包验证通过！
# 📦 可以安全发布到 GitHub Release
```

### CI 验证

1. 访问 GitHub Actions: https://github.com/void-soul/baja-lite-xlsx/actions
2. 查看最新的工作流运行
3. 检查关键步骤：
   - ✅ Create prebuild package for Node.js
   - ✅ Create prebuild package for Electron 34
   - ✅ Pack DLLs into prebuild packages
   - ✅ List generated files（应显示包含 DLL）

---

## ❌ 常见错误

### 错误 1: 找不到 xlnt/xlnt.hpp

```
error C1083: 无法打开包括文件: "xlnt/xlnt.hpp"
```

**原因**: `VCPKG_ROOT` 未设置

**解决**:
```bash
# 本地：使用专用脚本
.\scripts\create-prebuilds.bat

# 或手动设置
set VCPKG_ROOT=E:\vcpkg
npm run prebuild
```

---

### 错误 2: build/Release 目录不存在

```
❌ build/Release 目录不存在，请先编译原生模块
```

**原因**: 试图在未编译的情况下复制 DLL

**解决**: 这个错误已修复，`pack-dlls-into-prebuild.js` 现在会自动处理

---

### 错误 3: 预编译包不包含 DLL

**症状**: 包大小只有 111KB

**原因**: `pack-dlls-into-prebuild.js` 未运行或失败

**解决**:
```bash
# 确保完整运行 prebuild
npm run prebuild

# 或手动运行
node scripts/pack-dlls-into-prebuild.js

# 然后验证
npm run test:prebuild
```

---

## 🎯 最佳实践

### 本地开发

1. ✅ **始终使用** `scripts/create-prebuilds.bat`
2. ✅ 发布前运行 `npm run test:prebuild`
3. ✅ 验证包大小（应约 1MB）

### CI/CD

1. ✅ 每次推送 tag 自动触发
2. ✅ 监控构建日志
3. ✅ 验证 Release 中的预编译包
4. ✅ 在干净环境测试安装

---

## 📝 快速命令参考

```bash
# === 本地开发 ===

# 编译（不创建预编译包）
npm run build

# 创建预编译包（推荐）
.\scripts\create-prebuilds.bat

# 测试预编译包
npm run test:prebuild

# 运行功能测试
npm test

# === CI/CD ===

# 触发构建
git tag v1.0.13
git push origin v1.0.13

# 查看构建状态
# https://github.com/void-soul/baja-lite-xlsx/actions

# 验证 Release
# https://github.com/void-soul/baja-lite-xlsx/releases
```

---

## 💡 提示

1. **本地构建失败？** → 使用 `scripts/create-prebuilds.bat`
2. **CI 构建失败？** → 检查 Actions 日志，查看具体错误
3. **包不含 DLL？** → 运行 `npm run test:prebuild` 验证
4. **环境变量问题？** → 确保 `VCPKG_ROOT` 指向正确的 vcpkg 目录

---

**记住**: 本地和 CI 的主要区别是环境变量的设置方式。使用专用脚本可以避免大部分问题！✨

