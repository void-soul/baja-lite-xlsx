# 安装指南

## 系统要求

- Node.js >= 16.0.0
- Python 3.x
- C++ 编译环境

---

## Windows 安装

### 1. 安装编译工具

#### 方法1: 使用 windows-build-tools（推荐）
```powershell
# 以管理员身份运行 PowerShell
npm install --global windows-build-tools
```

#### 方法2: 手动安装
1. 安装 [Visual Studio Build Tools](https://visualstudio.microsoft.com/downloads/)
   - 选择 "Desktop development with C++"
   - 包含 MSVC v142 和 Windows 10 SDK

2. 安装 [Python](https://www.python.org/downloads/)
   - 确保勾选 "Add Python to PATH"

### 2. 安装 vcpkg 和依赖库

```powershell
# 安装 vcpkg
cd C:\
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat

# 安装 xlnt 库
.\vcpkg install xlnt:x64-windows
.\vcpkg integrate install

# 设置环境变量（重要！）
$env:VCPKG_ROOT = "C:\vcpkg"
[Environment]::SetEnvironmentVariable("VCPKG_ROOT", "C:\vcpkg", "User")
```

### 3. 安装 baja-lite-xlsx

```bash
npm install baja-lite-xlsx
```

如果遇到编译错误，尝试：
```bash
npm install baja-lite-xlsx --verbose
```

---

## Linux 安装

### Ubuntu/Debian
```bash
# 安装编译工具
sudo apt-get update
sudo apt-get install -y build-essential cmake python3 git

# 安装依赖库
sudo apt-get install -y libz-dev

# 安装 vcpkg
cd ~
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh

# 安装 xlnt
./vcpkg install xlnt

# 设置环境变量
echo 'export VCPKG_ROOT="$HOME/vcpkg"' >> ~/.bashrc
source ~/.bashrc

# 安装 baja-lite-xlsx
npm install baja-lite-xlsx
```

### CentOS/RHEL
```bash
# 安装编译工具
sudo yum groupinstall "Development Tools"
sudo yum install cmake python3 git zlib-devel

# 其他步骤同 Ubuntu
```

---

## macOS 安装

```bash
# 安装 Xcode Command Line Tools
xcode-select --install

# 安装 Homebrew（如果还没有）
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# 安装 cmake
brew install cmake

# 安装 vcpkg
cd ~
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh

# 安装 xlnt
./vcpkg install xlnt

# 设置环境变量
echo 'export VCPKG_ROOT="$HOME/vcpkg"' >> ~/.zshrc
source ~/.zshrc

# 安装 baja-lite-xlsx
npm install baja-lite-xlsx
```

---

## 验证安装

创建测试文件 `test.js`:

```javascript
const { readExcel } = require('baja-lite-xlsx');

try {
  console.log('✓ baja-lite-xlsx 安装成功！');
  console.log('可用的函数:', Object.keys(require('baja-lite-xlsx')));
} catch (error) {
  console.error('✗ 安装失败:', error.message);
}
```

运行测试：
```bash
node test.js
```

---

## 常见问题

### Windows: "找不到 xlnt.lib"

**解决方法:**
```powershell
# 确认 vcpkg 安装路径
echo $env:VCPKG_ROOT

# 检查 xlnt 是否正确安装
C:\vcpkg\vcpkg list | Select-String xlnt

# 重新安装
C:\vcpkg\vcpkg install xlnt:x64-windows --reconfigure
```

### Linux: "找不到 xlnt 头文件"

**解决方法:**
```bash
# 设置环境变量
export VCPKG_ROOT="$HOME/vcpkg"

# 或者在 binding.gyp 中添加路径（开发者选项）
```

### macOS: "库版本不兼容"

**解决方法:**
```bash
# 确保使用正确的架构
arch -x86_64 npm install baja-lite-xlsx  # Intel Mac
npm install baja-lite-xlsx  # Apple Silicon (M1/M2)
```

### 通用: "node-gyp rebuild 失败"

**检查清单:**
1. ✅ Python 已安装且在 PATH 中
2. ✅ C++ 编译器已安装
3. ✅ node-gyp 已安装: `npm install -g node-gyp`
4. ✅ VCPKG_ROOT 环境变量已设置
5. ✅ xlnt 已通过 vcpkg 安装

**调试方法:**
```bash
# 启用详细日志
npm install baja-lite-xlsx --verbose --foreground-scripts

# 手动编译测试
cd node_modules/baja-lite-xlsx
node-gyp rebuild --verbose
```

---

## Docker 安装

创建 `Dockerfile`:

```dockerfile
FROM node:18

# 安装编译工具
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libz-dev \
    && rm -rf /var/lib/apt/lists/*

# 安装 vcpkg
RUN cd /opt && \
    git clone https://github.com/Microsoft/vcpkg.git && \
    cd vcpkg && \
    ./bootstrap-vcpkg.sh && \
    ./vcpkg install xlnt

ENV VCPKG_ROOT=/opt/vcpkg

WORKDIR /app
COPY package*.json ./
RUN npm install

COPY . .
CMD ["node", "index.js"]
```

构建和运行：
```bash
docker build -t my-xlsx-app .
docker run my-xlsx-app
```

---

## 离线安装

1. 在有网络的机器上准备：
```bash
# 下载依赖
npm pack baja-lite-xlsx
# 生成 baja-lite-xlsx-1.0.0.tgz

# 同时准备 vcpkg 和 xlnt
```

2. 在离线机器上安装：
```bash
# 先安装 vcpkg 和 xlnt（从离线包）
# 然后安装 npm 包
npm install baja-lite-xlsx-1.0.0.tgz
```

---

## 获取帮助

- 📖 查看 [README.md](README.md) 了解使用方法
- 📖 查看 [API_USAGE_CN.md](API_USAGE_CN.md) 了解 API 详情
- 🐛 遇到问题？提交 [Issue](https://github.com/yourusername/baja-lite-xlsx/issues)
- 💬 讨论和建议？参与 [Discussions](https://github.com/yourusername/baja-lite-xlsx/discussions)

