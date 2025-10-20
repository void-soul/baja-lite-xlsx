# 快速安装指南（解决 xlnt.hpp 找不到问题）

## 🚨 您遇到的错误

```
error C1083: 无法打开包括文件: "xlnt/xlnt.hpp": No such file or directory
```

**原因**: xlnt 库还未安装

## ✅ 解决方案（3 种方法）

### 方法一：使用自动安装脚本（最简单）⭐

1. **直接运行安装脚本**:
   ```powershell
   .\install-vcpkg-and-deps.bat
   ```

2. **等待安装完成**（大约 15-20 分钟）

3. **然后构建项目**:
   ```powershell
   yarn install
   # 或
   npm install
   ```

---

### 方法二：手动安装 vcpkg 和依赖库

#### 步骤 1: 安装 vcpkg

```powershell
# 进入 E 盘
cd E:\

# 克隆 vcpkg（需要 git）
git clone https://github.com/Microsoft/vcpkg.git

# 进入目录并引导安装
cd vcpkg
.\bootstrap-vcpkg.bat
```

#### 步骤 2: 安装 xlnt 和 libzip

```powershell
# 在 E:\vcpkg 目录下
.\vcpkg install xlnt:x64-windows
.\vcpkg install libzip:x64-windows
```

**注意**: 这个过程会下载并编译库，需要 10-20 分钟

#### 步骤 3: 验证安装

```powershell
# 检查是否安装成功
.\vcpkg list | findstr xlnt
.\vcpkg list | findstr libzip
```

应该看到：
```
xlnt:x64-windows
libzip:x64-windows
```

#### 步骤 4: 构建项目

```powershell
cd E:\pro\mysdk\baja-lite-xlsx
yarn install
# 或
npm install
```

---

### 方法三：使用不同的 vcpkg 安装位置

如果您想将 vcpkg 安装在 `C:\vcpkg`（而不是 `E:\vcpkg`）：

1. **修改 binding.gyp**，将路径改回：
   ```json
   "include_dirs": [
     "C:/vcpkg/installed/x64-windows/include"
   ],
   "libraries": [
     "C:/vcpkg/installed/x64-windows/lib/xlnt.lib",
     "C:/vcpkg/installed/x64-windows/lib/zip.lib"
   ]
   ```

2. **安装到 C 盘**:
   ```powershell
   cd C:\
   git clone https://github.com/Microsoft/vcpkg.git
   cd vcpkg
   .\bootstrap-vcpkg.bat
   .\vcpkg install xlnt:x64-windows libzip:x64-windows
   ```

---

## 📋 前置要求检查

在安装前，请确保已安装：

### ✅ 必需工具

1. **Git for Windows**
   - 下载: https://git-scm.com/download/win
   - 检查: `git --version`

2. **Visual Studio 2019 或 2022**（带 C++ 工具）
   - 下载: https://visualstudio.microsoft.com/
   - 工作负载: "使用 C++ 的桌面开发"

3. **Python 3.x**
   - 下载: https://www.python.org/downloads/
   - 检查: `python --version`
   - ⚠️ 安装时勾选 "Add Python to PATH"

4. **Node.js 16+**
   - 下载: https://nodejs.org/
   - 检查: `node --version`

---

## 🔍 故障排除

### 问题 1: git 命令找不到
```
'git' 不是内部或外部命令
```
**解决**: 安装 Git for Windows

### 问题 2: bootstrap-vcpkg.bat 失败
```
无法找到 Visual Studio
```
**解决**: 安装 Visual Studio C++ 构建工具

### 问题 3: vcpkg install 下载很慢
**解决**: 
- 设置代理（如果有）
- 或者使用国内镜像（需要额外配置）
- 或者耐心等待

### 问题 4: 编译时仍然找不到 xlnt.hpp
**检查**:
1. 确认 xlnt 已安装: `E:\vcpkg\vcpkg list | findstr xlnt`
2. 检查头文件是否存在: `dir E:\vcpkg\installed\x64-windows\include\xlnt`
3. 确认 binding.gyp 中的路径正确

### 问题 5: 链接错误（找不到 xlnt.lib）
**检查**:
1. 库文件是否存在: `dir E:\vcpkg\installed\x64-windows\lib\xlnt.lib`
2. binding.gyp 中的库路径是否正确

---

## 📝 完整流程总结

```powershell
# 1. 安装 vcpkg 和依赖（二选一）
.\install-vcpkg-and-deps.bat    # 自动安装
# 或手动按步骤安装

# 2. 验证安装
E:\vcpkg\vcpkg list | findstr xlnt

# 3. 清理之前的构建（如果有）
cd E:\pro\mysdk\baja-lite-xlsx
rm -rf build node_modules

# 4. 重新构建
yarn install
# 或
npm install

# 5. 测试（需要先创建 test/sample.xlsx）
npm test
```

---

## ⏱️ 预计时间

- vcpkg 安装: 2-3 分钟
- xlnt 编译安装: 10-15 分钟
- libzip 编译安装: 5-8 分钟
- 项目构建: 1-2 分钟

**总计**: 约 20-30 分钟

---

## 💡 建议

- ✅ 使用 **方法一** 的自动脚本（最省心）
- ✅ 确保网络畅通（需要下载大量文件）
- ✅ 在安装过程中不要中断
- ✅ 第一次安装时间较长是正常的

---

## 🎯 下一步

安装完成后：

1. **创建测试文件**: 在 `test/` 目录下创建包含图片的 `sample.xlsx`
2. **运行测试**: `npm test`
3. **查看示例**: `node examples/basic.js`

有问题请查看完整文档: `README.md` 或 `INSTALL.md`

