# 🚀 GitHub Actions 预编译包发布配置

## 📋 配置概览

GitHub Actions 已经完全配置好，包含完整的 DLL 自动打包流程。

### 工作流文件

- **文件位置**: `.github/workflows/prebuild.yml`
- **触发条件**: 
  - 推送 tag（格式 `v*`，如 `v1.0.13`）
  - 手动触发（workflow_dispatch）

---

## 🔄 完整工作流程

```yaml
┌─────────────────────────────────────────────────────────────┐
│              GitHub Actions 预编译流程                       │
└─────────────────────────────────────────────────────────────┘

1️⃣  安装 vcpkg
   └─ git clone vcpkg
   └─ bootstrap-vcpkg.bat

2️⃣  安装依赖
   └─ vcpkg install xlnt:x64-windows
   └─ vcpkg install libzip:x64-windows

3️⃣  验证依赖
   └─ 检查 xlnt.hpp
   └─ 检查 xlnt.lib

4️⃣  配置 binding.gyp
   └─ 替换 VCPKG_ROOT 路径

5️⃣  安装 npm 依赖
   └─ npm install --ignore-scripts

6️⃣  编译原生模块
   └─ npm run build (node-gyp rebuild)

7️⃣  ⭐ 复制 DLL 文件
   └─ node scripts/package-dlls.js
   └─ 从 C:\vcpkg\installed\x64-windows\bin\ 复制
   └─ 到 build/Release/

8️⃣  验证 DLL
   └─ 列出所有 *.dll 文件
   └─ 显示文件名和大小

9️⃣  创建预编译包
   ├─ npx prebuild --runtime napi --target 8
   └─ npx prebuild --runtime electron --target 34.0.0

🔟  ⭐ 打包 DLL 到 .tar.gz
   └─ node scripts/pack-dlls-into-prebuild.js
   └─ 解压 → 添加 DLL → 重新打包

1️⃣1️⃣  验证预编译包
   └─ 列出所有 .tar.gz 文件
   └─ 解压第一个包
   └─ 显示包内容
   └─ 检查是否包含 xlnt.dll 和 zlib*.dll

1️⃣2️⃣  上传 Artifacts
   └─ 上传 prebuilds/ 目录

1️⃣3️⃣  创建 GitHub Release
   └─ 附加所有 .tar.gz 文件
   └─ 生成 Release Notes
```

---

## ⭐ 关键步骤详解

### 步骤 7: 复制 DLL 文件

```yaml
- name: Package DLL dependencies
  run: node scripts/package-dlls.js
  env:
    VCPKG_ROOT: C:\vcpkg
```

**功能**: 
- 从 vcpkg 安装目录复制所有必需的 DLL
- 复制到 `build/Release/` 目录
- 验证文件完整性

**输出示例**:
```
📦 打包依赖的 DLL 文件...

✓ xlnt.dll - 已复制 (1731.5 KB)
✓ zlib1.dll - 已复制 (88.0 KB)
✓ bz2.dll - 已复制（可选）
✓ fmt.dll - 已复制（可选）
✓ zip.dll - 已复制（可选）

✅ 所有必需的 DLL 已成功打包！
```

---

### 步骤 8: 验证 DLL

```yaml
- name: Verify DLL files are packaged
  run: |
    Write-Host "Checking DLL files in build/Release..."
    Get-ChildItem build/Release -Filter *.dll | ForEach-Object {
      Write-Host "  ✓ $($_.Name) - $([math]::Round($_.Length/1KB, 1)) KB"
    }
    $dllCount = (Get-ChildItem build/Release -Filter *.dll).Count
    if ($dllCount -eq 0) {
      Write-Host "⚠️ Warning: No DLL files found!"
    } else {
      Write-Host "`n✅ Found $dllCount DLL file(s)"
    }
  shell: powershell
```

**功能**: 
- 列出 `build/Release/` 中的所有 DLL
- 显示文件大小
- 统计 DLL 数量
- 如果没有 DLL，发出警告

**输出示例**:
```
Checking DLL files in build/Release...
  ✓ xlnt.dll - 1731.5 KB
  ✓ zlib1.dll - 88.0 KB
  ✓ bz2.dll - 75.0 KB
  ✓ fmt.dll - 119.0 KB
  ✓ zip.dll - 111.5 KB

✅ Found 5 DLL file(s)
```

---

### 步骤 10: 打包 DLL 到 .tar.gz ⭐⭐⭐

```yaml
- name: Pack DLLs into prebuild packages
  run: node scripts/pack-dlls-into-prebuild.js
  env:
    VCPKG_ROOT: C:\vcpkg
```

**功能**: 
- 解压已生成的 `.tar.gz` 文件
- 将 DLL 文件添加到包中
- 重新打包成 `.tar.gz`

**这是最关键的步骤！** 因为 `prebuild` 默认只打包 `.node` 文件。

**输出示例**:
```
📦 将 DLL 文件打包到预编译包中...

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

📄 处理: baja-lite-xlsx-v1.0.13-napi-v8-win32-x64.tar.gz

🔓 解压现有包...

📚 添加 DLL 文件:
  ✓ xlnt.dll (1731.5 KB)
  ✓ zlib1.dll (88.0 KB)
  ✓ bz2.dll (75.0 KB)
  ✓ fmt.dll (119.0 KB)
  ✓ zip.dll (111.5 KB)

添加了 5 个 DLL 文件

📦 重新打包...
✓ 新包大小: 1052.3 KB

✅ 处理完成
```

---

### 步骤 11: 验证预编译包

```yaml
- name: List generated files
  run: |
    # ... 列出文件 ...
    
    # 解压第一个包验证内容
    $firstTarGz = Get-ChildItem prebuilds -Filter *.tar.gz | Select-Object -First 1
    if ($firstTarGz) {
      $tempDir = New-Item -ItemType Directory -Path "temp_extract" -Force
      tar -xzf $firstTarGz.FullName -C $tempDir.FullName
      
      # 显示内容
      Get-ChildItem $tempDir.FullName -Recurse
      
      # 检查必需的 DLL
      $hasXlnt = (Get-ChildItem $tempDir.FullName -Recurse -Filter "xlnt.dll").Count -gt 0
      $hasZlib = (Get-ChildItem $tempDir.FullName -Recurse -Filter "zlib*.dll").Count -gt 0
      
      if ($hasXlnt -and $hasZlib) {
        Write-Host "✅ Package contains required DLL files - Ready for distribution!"
      } else {
        Write-Host "⚠️  Warning: Package may be missing required DLL files"
      }
    }
  shell: powershell
```

**输出示例**:
```
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
📦 Generated prebuild packages:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  📄 baja-lite-xlsx-v1.0.13-napi-v8-win32-x64.tar.gz - 1052.3 KB
  📄 baja-lite-xlsx-v1.0.13-electron-v132-win32-x64.tar.gz - 1052.3 KB

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
🔍 Verifying package contents:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Contents of baja-lite-xlsx-v1.0.13-napi-v8-win32-x64.tar.gz:
  📁 build/
  📁 Release/
  🔷 baja_xlsx.node - 229.0 KB
  📚 xlnt.dll - 1731.5 KB
  📚 zlib1.dll - 88.0 KB
  📚 bz2.dll - 75.0 KB
  📚 fmt.dll - 119.0 KB
  📚 zip.dll - 111.5 KB

✅ Package contains required DLL files - Ready for distribution!
```

---

## 📝 如何触发构建

### 方法 1: 推送 Tag（推荐）

```bash
# 本地测试通过后
npm run test:prebuild  # 验证本地预编译包

# 更新版本号
npm version patch  # 1.0.12 -> 1.0.13

# 推送到 GitHub
git push origin main --tags
```

GitHub Actions 会自动：
1. 检测到新的 tag
2. 触发 `prebuild.yml` 工作流
3. 编译、打包、验证
4. 创建 GitHub Release
5. 上传预编译包

### 方法 2: 手动触发

1. 访问 GitHub 仓库
2. 进入 **Actions** 标签页
3. 选择 **Prebuild Native Module** 工作流
4. 点击 **Run workflow** 按钮
5. 选择分支（通常是 `main`）
6. 点击 **Run workflow**

---

## ✅ 验证 CI 构建

### 1. 查看 Actions 日志

访问：`https://github.com/void-soul/baja-lite-xlsx/actions`

**关键步骤检查清单**:

- ✅ **Install vcpkg and dependencies** - vcpkg 安装成功
- ✅ **Verify vcpkg installation** - xlnt 和 libzip 已安装
- ✅ **Build native module** - 编译成功
- ✅ **Package DLL dependencies** - DLL 复制成功
- ✅ **Verify DLL files are packaged** - 显示 5 个 DLL
- ✅ **Create prebuild package for Node.js** - 创建成功
- ✅ **Create prebuild package for Electron 34** - 创建成功
- ✅ **Pack DLLs into prebuild packages** - DLL 打包成功
- ✅ **List generated files** - 显示包大小约 1MB，包含 DLL
- ✅ **Create Release** - Release 创建成功

### 2. 检查输出日志

**"Pack DLLs into prebuild packages" 步骤应该显示**:

```
📦 将 DLL 文件打包到预编译包中...
✓ xlnt.dll (1731.5 KB)
✓ zlib1.dll (88.0 KB)
...
✅ 所有预编译包已更新！
```

**"List generated files" 步骤应该显示**:

```
📄 baja-lite-xlsx-v1.0.13-napi-v8-win32-x64.tar.gz - 1052.3 KB
...
✅ Package contains required DLL files - Ready for distribution!
```

### 3. 验证 GitHub Release

1. 访问：`https://github.com/void-soul/baja-lite-xlsx/releases`
2. 找到最新的 Release（如 `v1.0.13`）
3. 检查 **Assets** 部分应该包含：
   - `baja-lite-xlsx-v1.0.13-napi-v8-win32-x64.tar.gz` (~1MB)
   - `baja-lite-xlsx-v1.0.13-electron-v132-win32-x64.tar.gz` (~1MB)

4. 下载一个包，解压验证：
   ```bash
   tar -tzf baja-lite-xlsx-v1.0.13-napi-v8-win32-x64.tar.gz
   ```
   
   应该看到：
   ```
   build/Release/baja_xlsx.node
   build/Release/xlnt.dll
   build/Release/zlib1.dll
   build/Release/bz2.dll
   build/Release/fmt.dll
   build/Release/zip.dll
   ```

---

## 🐛 故障排除

### 问题 1: 构建失败 - "找不到 xlnt/xlnt.hpp"

**原因**: vcpkg 安装失败或路径不对

**解决**:
1. 检查 **Install vcpkg and dependencies** 步骤日志
2. 确认 `VCPKG_ROOT` 环境变量设置为 `C:\vcpkg`
3. 验证 xlnt 安装成功

### 问题 2: 预编译包不包含 DLL

**原因**: `pack-dlls-into-prebuild.js` 步骤失败

**解决**:
1. 检查 **Pack DLLs into prebuild packages** 步骤日志
2. 确认 `build/Release/` 目录包含 DLL
3. 检查 `scripts/pack-dlls-into-prebuild.js` 文件是否存在

### 问题 3: DLL 验证失败

**原因**: DLL 文件未正确复制

**解决**:
1. 检查 **Package DLL dependencies** 步骤
2. 检查 **Verify DLL files are packaged** 步骤
3. 确认显示了 5 个 DLL 文件

---

## 📊 预期结果

### 构建成功后

✅ **GitHub Actions**:
- 所有步骤显示绿色 ✓
- 总耗时约 15-20 分钟

✅ **GitHub Release**:
- 自动创建新 Release
- 包含 2 个预编译包
- 每个包约 1MB

✅ **用户安装**:
```bash
npm install baja-lite-xlsx@1.0.13
```
- 自动下载预编译包
- 解压包含完整的 .node + DLL
- 无需任何额外配置
- 直接可用！

---

## 🎯 最佳实践

1. **每次发布前本地验证**
   ```bash
   npm run test:prebuild
   ```

2. **版本号遵循语义化**
   - Patch: 1.0.12 → 1.0.13（bug 修复）
   - Minor: 1.0.13 → 1.1.0（新功能）
   - Major: 1.0.13 → 2.0.0（破坏性变更）

3. **监控 CI 日志**
   - 关注关键步骤
   - 验证 DLL 是否正确打包
   - 检查包大小是否正常（约 1MB）

4. **测试 Release**
   - 在干净环境测试安装
   - 验证模块可以正常加载
   - 确认无 DLL 缺失错误

---

## 📚 相关文档

- [PREBUILD-DLL-PACKAGING.md](./PREBUILD-DLL-PACKAGING.md) - 完整技术文档
- [QUICK-RELEASE-GUIDE.md](./QUICK-RELEASE-GUIDE.md) - 快速发布指南
- [FINAL-DLL-PACKAGING-SUMMARY.md](./FINAL-DLL-PACKAGING-SUMMARY.md) - 总结文档

---

## 🎉 总结

GitHub Actions 配置已经完全就绪：

✅ 自动安装依赖（vcpkg + xlnt）  
✅ 自动编译原生模块  
✅ 自动复制 DLL 文件  
✅ 自动创建预编译包  
✅ **自动将 DLL 打包到 .tar.gz**  
✅ 自动验证包内容  
✅ 自动创建 GitHub Release  
✅ 自动上传预编译包  

**您只需要**：
```bash
npm version patch
git push origin main --tags
```

其余一切都是自动的！🚀

