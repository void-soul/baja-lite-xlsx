# Windows 原生模块 DLL 依赖问题解决方案

## 问题描述

虽然 `.node` 文件存在，但 Node.js 提示：
```
The specified module could not be found.
\\?\E:\pro\gld\gld-web\attack-service\node_modules\baja-lite-xlsx\build\Release\baja_xlsx.node
```

**根本原因**：这个错误消息具有误导性。真正的问题不是 `.node` 文件本身找不到，而是 **该文件依赖的 DLL 文件缺失**。

## 原生模块依赖关系

`baja_xlsx.node` 是用 C++ 编译的，它依赖：
1. **xlnt.dll** - Excel 文件处理库
2. **zlib.dll** / **zlib1.dll** - 压缩库
3. **expat.dll** / **libexpat.dll** - XML 解析库
4. **Visual C++ Runtime** - Microsoft Visual C++ 运行时库

## 快速解决方案

### 方案 1: 使用自动修复脚本（推荐）

```bash
node fix-dll-dependencies.js "E:\pro\gld\gld-web\attack-service\node_modules\baja-lite-xlsx"
```

该脚本会自动：
- 查找所需的 DLL 文件
- 复制到正确的位置
- 测试模块是否可以加载

### 方案 2: 手动复制 DLL 文件

1. **找到 DLL 源目录**（如果使用 vcpkg 安装的 xlnt）：
   ```
   E:\vcpkg\installed\x64-windows\bin\
   或
   C:\vcpkg\installed\x64-windows\bin\
   ```

2. **复制以下 DLL 到目标目录**：
   ```
   目标目录: E:\pro\gld\gld-web\attack-service\node_modules\baja-lite-xlsx\build\Release\
   
   需要复制的文件:
   - xlnt.dll
   - zlib1.dll (或 zlib.dll)
   - libexpat.dll (或 expat.dll)
   ```

### 方案 3: 安装 Visual C++ Redistributable

如果上述方案都不行，可能缺少 VC++ 运行时：

1. 下载并安装：
   - **64位 Node.js**: https://aka.ms/vs/17/release/vc_redist.x64.exe
   - **32位 Node.js**: https://aka.ms/vs/17/release/vc_redist.x86.exe

2. 安装后重启应用程序

### 方案 4: 重新编译模块

如果 Node.js 版本不匹配，需要重新编译：

```bash
cd E:\pro\gld\gld-web\attack-service\node_modules\baja-lite-xlsx
npm rebuild
```

**注意**：重新编译需要：
- Visual Studio Build Tools
- Python 3.x
- xlnt 库（通过 vcpkg 安装）

## 诊断工具

使用诊断脚本查看详细依赖信息：

```bash
node diagnose-dll.js "E:\pro\gld\gld-web\attack-service\node_modules\baja-lite-xlsx\build\Release\baja_xlsx.node"
```

该脚本会显示：
- 文件是否存在
- 依赖的 DLL 列表
- DLL 是否在正确位置
- Node.js 版本和架构信息

## 预防措施

### 对于包的使用者

在项目中添加 postinstall 脚本，自动复制 DLL：

```json
{
  "scripts": {
    "postinstall": "node scripts/copy-dlls.js"
  }
}
```

### 对于包的维护者（baja-lite-xlsx）

1. **使用 prebuild**：发布预编译的二进制文件（已配置）
2. **包含 DLL**：将必要的 DLL 打包到 npm 包中
3. **添加安装脚本**：在 install 脚本中自动复制 DLL

例如，在 `package.json` 中：
```json
{
  "scripts": {
    "install": "node scripts/post-install.js"
  }
}
```

`scripts/post-install.js`:
```javascript
const fs = require('fs');
const path = require('path');

// 如果存在预打包的 DLL，复制它们
const dllDir = path.join(__dirname, '..', 'prebuilt-dlls', process.arch);
const targetDir = path.join(__dirname, '..', 'build', 'Release');

if (fs.existsSync(dllDir) && fs.existsSync(targetDir)) {
  const dlls = fs.readdirSync(dllDir).filter(f => f.endsWith('.dll'));
  dlls.forEach(dll => {
    fs.copyFileSync(
      path.join(dllDir, dll),
      path.join(targetDir, dll)
    );
  });
  console.log(`Copied ${dlls.length} DLL files`);
}
```

## 常见问题

### Q: 为什么错误消息说"找不到模块"而不是"找不到 DLL"？

A: 这是 Windows 的行为。当加载 DLL 时，如果它的依赖项缺失，系统会报告 DLL 本身找不到，而不是具体说明哪个依赖项缺失。

### Q: 如何确定需要哪些 DLL？

A: 使用 `dumpbin` 工具（Visual Studio 自带）：
```bash
dumpbin /dependents baja_xlsx.node
```

或使用 [Dependencies Walker](https://www.dependencywalker.com/)

### Q: 可以把 DLL 放在系统 PATH 中吗？

A: 可以，但不推荐。最好将 DLL 放在 `.node` 文件同目录中，这样更易于部署和隔离。

### Q: 为什么预编译的二进制文件也有这个问题？

A: 预编译的二进制文件只是预编译了 `.node` 文件，但第三方 DLL（如 xlnt.dll）通常不包含在内，因为它们可能有不同的许可证。

## 推荐的长期解决方案

为 `baja-lite-xlsx` 包维护者：

1. **静态链接**：将 xlnt 和其他依赖静态链接到 `.node` 文件中
   - 优点：无需外部 DLL
   - 缺点：文件变大

2. **捆绑 DLL**：将必要的 DLL 包含在 npm 包中
   - 在 `prebuilt-dlls/` 目录中按架构存放
   - 安装时自动复制

3. **文档改进**：在 README 中明确说明 DLL 依赖和安装步骤

## 检查清单

- [ ] 检查 `.node` 文件是否存在
- [ ] 检查 `xlnt.dll` 是否在同目录
- [ ] 检查 `zlib.dll` / `zlib1.dll` 是否在同目录  
- [ ] 检查 `expat.dll` / `libexpat.dll` 是否在同目录
- [ ] 安装 Visual C++ Redistributable
- [ ] 验证 Node.js 版本和架构匹配
- [ ] 尝试重新编译模块

## 相关资源

- [Node.js Native Addons](https://nodejs.org/api/addons.html)
- [node-gyp on Windows](https://github.com/nodejs/node-gyp#on-windows)
- [Visual Studio Build Tools](https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022)
- [vcpkg Package Manager](https://github.com/microsoft/vcpkg)

