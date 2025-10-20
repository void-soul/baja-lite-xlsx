# 推送代码到 GitHub

## 🚀 完整流程

### 1. 初始化 Git（如果还没有）

```bash
# 检查是否已经是 git 仓库
git status

# 如果不是，初始化
git init
```

### 2. 添加 .gitignore

确保有 `.gitignore` 文件，排除不需要的文件：

```gitignore
# Node
node_modules/
npm-debug.log*
yarn-debug.log*
yarn-error.log*

# Build
build/
*.node
prebuilds/

# IDE
.vscode/
.idea/
*.swp
*.swo
*~

# OS
.DS_Store
Thumbs.db

# Temp
*.log
temp-*/
debug-*.js
debug-*.txt

# vcpkg 本地路径相关
E:/vcpkg/
```

### 3. 提交所有代码

```bash
# 添加所有文件
git add .

# 查看将要提交的文件
git status

# 提交
git commit -m "feat: 初始化项目，添加 Windows 预编译支持"
```

### 4. 关联远程仓库

```bash
# 添加远程仓库（替换为你的仓库地址）
git remote add origin https://github.com/yourusername/baja-lite-xlsx.git

# 查看远程仓库
git remote -v
```

### 5. 推送代码

```bash
# 推送主分支
git branch -M main
git push -u origin main
```

### 6. 验证

访问你的 GitHub 仓库：
```
https://github.com/yourusername/baja-lite-xlsx
```

应该看到：
- ✅ 所有源代码文件
- ✅ `.github/workflows/prebuild.yml`
- ✅ `package.json`
- ✅ README.md

---

## 🎯 触发第一次构建

### 方法1: 推送 tag（推荐）

```bash
# 创建版本 tag
git tag v1.0.0-beta

# 推送 tag
git push origin v1.0.0-beta

# 这会自动触发 GitHub Actions！
```

### 方法2: 手动触发

1. 访问你的仓库
2. 点击 "Actions" 标签页
3. 选择 "Prebuild Native Module (Windows Only)"
4. 点击 "Run workflow" → "Run workflow"

---

## 📊 查看构建状态

### 在 GitHub 网页上查看

1. **访问 Actions 页面**
   ```
   https://github.com/yourusername/baja-lite-xlsx/actions
   ```

2. **点击最新的 workflow run**

3. **查看构建详情**
   - 点击 "build-windows"
   - 展开每个步骤查看日志
   - 等待完成（约 10 分钟）

### 构建成功的标志

✅ 所有步骤都是绿色对勾  
✅ "Create Release" 步骤完成  
✅ 在 Releases 页面看到新版本

### 查看 Release

```
https://github.com/yourusername/baja-lite-xlsx/releases
```

应该看到：
```
v1.0.0-beta
Assets:
  - baja-lite-xlsx-v1.0.0-beta-napi-v8-win32-x64.tar.gz (2-5 MB)
```

---

## 🔍 VS Code 中查看（可选）

安装并配置 GitHub Actions 扩展后：

1. **安装扩展**
   - 在 VS Code 中搜索 "GitHub Actions"
   - 安装官方扩展

2. **登录 GitHub**
   - 点击左侧 GitHub 图标
   - 选择 "Sign in to GitHub"
   - 授权访问

3. **查看 Actions**
   - 推送代码后
   - 在 VS Code 左侧可以看到 workflow 运行状态
   - 点击查看详细日志

---

## 🚨 常见问题

### Q: 推送失败："remote: Permission denied"

**原因：** 没有权限访问仓库

**解决：**
```bash
# 使用 SSH（推荐）
git remote set-url origin git@github.com:yourusername/baja-lite-xlsx.git

# 或配置 SSH key
ssh-keygen -t ed25519 -C "your_email@example.com"
# 然后将公钥添加到 GitHub
```

### Q: Actions 没有运行

**检查：**
1. 是否推送了 tag？（`git push origin v1.0.0-beta`）
2. 仓库是否 Public？（Private 仓库有时间限制）
3. Actions 是否启用？（Settings → Actions → Allow all actions）

### Q: 构建失败

**查看日志：**
1. 访问 Actions 页面
2. 点击失败的 workflow
3. 展开红色 ✗ 的步骤
4. 查看错误信息

**常见错误：**
- vcpkg 安装失败 → 重新运行 workflow
- xlnt 编译错误 → 检查 vcpkg 版本
- npm install 失败 → 检查 package.json

---

## ✅ 完整检查清单

推送前确认：

- [ ] `.gitignore` 已配置
- [ ] 所有代码已提交
- [ ] GitHub 仓库已创建
- [ ] 远程仓库已关联
- [ ] 代码已推送到 main 分支
- [ ] `.github/workflows/prebuild.yml` 已上传
- [ ] 准备好创建 tag

触发构建：

- [ ] 创建并推送 tag
- [ ] 访问 Actions 页面查看状态
- [ ] 等待构建完成（~10 分钟）
- [ ] 检查 Releases 页面

---

## 📝 快速命令总结

```bash
# 完整流程
git init
git add .
git commit -m "feat: 初始化项目"
git remote add origin https://github.com/yourusername/baja-lite-xlsx.git
git branch -M main
git push -u origin main

# 触发构建
git tag v1.0.0-beta
git push origin v1.0.0-beta

# 查看状态
# 访问：https://github.com/yourusername/baja-lite-xlsx/actions
```

---

## 🎯 下一步

1. ✅ 推送代码到 GitHub
2. ✅ 创建并推送 tag
3. ✅ 等待 Actions 构建完成
4. ✅ 验证 Release 中的预编译包
5. ✅ 发布到 npm: `npm publish`

祝你构建成功！🚀

