# ⚡ GitHub Actions 快速参考

## 🎯 一键发布

```bash
# 1. 本地验证
npm run test:prebuild

# 2. 更新版本
npm version patch

# 3. 推送触发 CI
git push origin main --tags
```

## 📊 关键步骤验证

访问：https://github.com/void-soul/baja-lite-xlsx/actions

### ✅ 必须成功的步骤

| 步骤 | 预期输出 | 重要性 |
|------|----------|--------|
| **Build native module** | 编译成功 | ⭐⭐⭐ |
| **Package DLL dependencies** | 复制 5 个 DLL | ⭐⭐⭐ |
| **Verify DLL files** | 显示 5 个 DLL | ⭐⭐⭐ |
| **Pack DLLs into prebuild** | 新包大小 ~1MB | ⭐⭐⭐ |
| **List generated files** | 包含 DLL ✅ | ⭐⭐⭐ |

### 🔍 关键输出检查

#### ✅ Package DLL dependencies
```
✓ xlnt.dll - 已复制 (1731.5 KB)
✓ zlib1.dll - 已复制 (88.0 KB)
```

#### ✅ Pack DLLs into prebuild packages
```
✓ 新包大小: 1052.3 KB
✅ 处理完成
```

#### ✅ List generated files
```
📄 baja-lite-xlsx-v1.0.13-napi-v8-win32-x64.tar.gz - 1052.3 KB
✅ Package contains required DLL files - Ready for distribution!
```

## 🎁 Release 验证

1. 访问：https://github.com/void-soul/baja-lite-xlsx/releases
2. 检查最新 Release
3. 验证 Assets:
   - ✅ 2 个 .tar.gz 文件
   - ✅ 每个约 1MB
   - ✅ 包含完整的 DLL

## ❌ 常见问题

### 包大小只有 111KB
**问题**: DLL 未打包  
**解决**: 检查 "Pack DLLs" 步骤是否失败

### 构建失败 - 找不到 xlnt.hpp
**问题**: vcpkg 安装失败  
**解决**: 检查 "Install vcpkg" 步骤日志

### DLL 验证失败
**问题**: DLL 未复制  
**解决**: 检查 VCPKG_ROOT 环境变量

## 📞 紧急处理

如果 CI 失败：

1. **查看日志** - 找到第一个失败的步骤
2. **检查对应脚本** - 查看 scripts/ 目录中的脚本
3. **本地复现** - 在本地运行相同的命令
4. **修复并重试** - 修复后重新推送 tag

## 🎯 成功标准

✅ CI 全绿  
✅ 包大小 ~1MB  
✅ 包含所有 DLL  
✅ Release 自动创建  
✅ 用户可以直接安装使用  

---

**详细文档**: [GITHUB-ACTIONS-SETUP.md](./GITHUB-ACTIONS-SETUP.md)

