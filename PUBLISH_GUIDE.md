# 发布指南

## 发布前检查清单

### 1. 代码检查
- [ ] 所有功能测试通过
- [ ] 示例代码运行正常
- [ ] 没有硬编码路径
- [ ] 文档更新完整

### 2. 版本更新
```bash
# 补丁版本 (1.0.0 -> 1.0.1)
npm version patch

# 小版本 (1.0.0 -> 1.1.0)
npm version minor

# 大版本 (1.0.0 -> 2.0.0)
npm version major
```

### 3. 测试发布包内容
```bash
# 查看将要发布的文件
npm pack --dry-run

# 或实际打包查看
npm pack
# 会生成 baja-lite-xlsx-1.0.0.tgz，解压查看内容
tar -xzf baja-lite-xlsx-1.0.0.tgz
```

### 4. 发布到 npm

#### 首次发布
```bash
# 登录 npm（如果还没登录）
npm login

# 发布
npm publish
```

#### 发布测试版本
```bash
# 发布 beta 版本
npm version prerelease --preid=beta
npm publish --tag beta

# 用户可以通过以下方式安装
# npm install baja-lite-xlsx@beta
```

#### 发布到私有仓库
```bash
# 如果是私有包，需要修改 package.json
# "name": "@your-scope/baja-lite-xlsx"

npm publish --access public  # 或 --access restricted
```

---

## 发布方式选择

### 方案1: 源码发布（当前配置）✅

**优点:**
- 包体积小
- 支持多平台（用户本地编译）
- 代码开源透明

**缺点:**
- 用户需要安装编译环境
- 首次安装时间长（需要编译）
- 可能遇到编译错误

**用户需要的环境:**
- Windows: Visual Studio Build Tools, Python, vcpkg + xlnt
- Linux: build-essential, cmake, libz-dev
- macOS: Xcode Command Line Tools, cmake

**当前配置:**
```json
"scripts": {
  "install": "node-gyp rebuild"  // 用户安装时自动编译
}
```

### 方案2: 预编译二进制发布

**优点:**
- 用户安装快速
- 无需编译环境
- 安装成功率高

**缺点:**
- 需要为每个平台编译
- 包体积大
- 需要使用 prebuild 工具

**实现方式:**

1. 安装 prebuild 工具
```bash
npm install --save-dev prebuild prebuild-install
```

2. 修改 package.json
```json
{
  "scripts": {
    "install": "prebuild-install || node-gyp rebuild",
    "prebuild": "prebuild --all --strip",
    "upload": "prebuild --upload YOUR_GITHUB_TOKEN"
  },
  "binary": {
    "napi_versions": [3, 4, 5, 6, 7, 8]
  }
}
```

3. 编译并上传预编译包
```bash
npm run prebuild
npm run upload
```

---

## 当前推荐流程

由于这是一个需要 C++ 编译的原生模块，推荐采用**源码发布 + 详细安装文档**的方式。

### 发布步骤

1. **清理开发文件**
```bash
npm run clean
rm -rf build/
rm -rf node_modules/
```

2. **重新安装依赖**
```bash
npm install
```

3. **测试编译**
```bash
npm run build
```

4. **运行所有测试**
```bash
npm test
npm run example
npm run example:json
npm run example:advanced
```

5. **检查发布内容**
```bash
npm pack --dry-run
```

确保包含：
- ✅ src/ (源代码)
- ✅ binding.gyp
- ✅ index.js
- ✅ index.d.ts
- ✅ package.json
- ✅ README.md
- ✅ LICENSE

确保排除：
- ❌ build/
- ❌ node_modules/
- ❌ test/
- ❌ *.node
- ❌ 临时文件

6. **更新版本号**
```bash
npm version patch  # 或 minor/major
```

7. **发布**
```bash
npm publish
```

---

## 发布后验证

### 1. 在新环境中测试安装
```bash
# 创建测试目录
mkdir test-install
cd test-install
npm init -y

# 安装你发布的包
npm install baja-lite-xlsx

# 测试使用
node -e "const xlsx = require('baja-lite-xlsx'); console.log('OK');"
```

### 2. 检查 npm 页面
访问: https://www.npmjs.com/package/baja-lite-xlsx

确认：
- 版本号正确
- README 显示正确
- 文件列表正确

---

## 常见问题

### Q: 如何撤销已发布的版本？
```bash
# 只能撤销 72 小时内发布的版本
npm unpublish baja-lite-xlsx@1.0.0

# 撤销整个包（谨慎！）
npm unpublish baja-lite-xlsx --force
```

### Q: 如何更新包的 README？
修改 README.md 后，不需要发新版本：
```bash
npm publish --dry-run  # 预览
```

### Q: 如何废弃某个版本？
```bash
npm deprecate baja-lite-xlsx@1.0.0 "此版本有严重bug，请升级到1.0.1"
```

---

## 持续集成建议

创建 `.github/workflows/publish.yml`:
```yaml
name: Publish to npm

on:
  release:
    types: [created]

jobs:
  publish:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - uses: actions/setup-node@v3
        with:
          node-version: '18'
          registry-url: 'https://registry.npmjs.org'
      - run: npm ci
      - run: npm test
      - run: npm publish
        env:
          NODE_AUTH_TOKEN: ${{ secrets.NPM_TOKEN }}
```

