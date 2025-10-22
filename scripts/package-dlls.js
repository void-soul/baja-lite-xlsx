/**
 * 在预编译时将依赖的 DLL 文件打包到 build/Release 目录
 * 这样预编译包就可以开箱即用，无需用户手动复制 DLL
 */

const fs = require('fs');
const path = require('path');
const os = require('os');

console.log('📦 打包依赖的 DLL 文件...\n');

// 目标目录（.node 文件所在目录）
const releaseDir = path.join(__dirname, '..', 'build', 'Release');

if (!fs.existsSync(releaseDir)) {
  console.error('❌ build/Release 目录不存在，请先编译原生模块');
  process.exit(1);
}

// 根据平台确定需要的 DLL
const platform = os.platform();

if (platform === 'win32') {
  console.log('🪟 Windows 平台 - 打包 DLL 文件\n');
  
  // Windows DLL 源目录列表（优先级从高到低）
  const vcpkgSources = [
    process.env.VCPKG_ROOT ? path.join(process.env.VCPKG_ROOT, 'installed', 'x64-windows', 'bin') : null,
    'C:\\vcpkg\\installed\\x64-windows\\bin',
    'E:\\vcpkg\\installed\\x64-windows\\bin',
    path.join(process.env.LOCALAPPDATA || '', 'vcpkg', 'installed', 'x64-windows', 'bin'),
  ].filter(Boolean);
  
  // 需要打包的 DLL 列表
  const requiredDlls = [
    'xlnt.dll',      // xlnt 库
    'zlib1.dll',     // zlib 压缩库
  ];
  
  // 可选的 DLL（可能存在，xlnt 的依赖）
  const optionalDlls = [
    'bz2.dll',       // bzip2 压缩库
    'fmt.dll',       // fmt 格式化库
    'zip.dll',       // libzip 库
    'zlib.dll',      // 有些系统用 zlib.dll 而不是 zlib1.dll
  ];
  
  let copiedCount = 0;
  let failedDlls = [];
  
  // 复制必需的 DLL
  for (const dllName of requiredDlls) {
    const targetPath = path.join(releaseDir, dllName);
    
    // 检查是否已存在
    if (fs.existsSync(targetPath)) {
      console.log(`✓ ${dllName} - 已存在`);
      copiedCount++;
      continue;
    }
    
    // 查找源文件
    let found = false;
    for (const sourceDir of vcpkgSources) {
      if (!fs.existsSync(sourceDir)) continue;
      
      const sourcePath = path.join(sourceDir, dllName);
      if (fs.existsSync(sourcePath)) {
        try {
          fs.copyFileSync(sourcePath, targetPath);
          const stats = fs.statSync(targetPath);
          console.log(`✓ ${dllName} - 已复制 (${(stats.size / 1024).toFixed(1)} KB)`);
          console.log(`  来源: ${sourcePath}`);
          copiedCount++;
          found = true;
          break;
        } catch (err) {
          console.error(`✗ ${dllName} - 复制失败: ${err.message}`);
        }
      }
    }
    
    if (!found) {
      failedDlls.push(dllName);
    }
  }
  
  // 尝试复制可选的 DLL
  for (const dllName of optionalDlls) {
    const targetPath = path.join(releaseDir, dllName);
    if (fs.existsSync(targetPath)) {
      continue; // 已存在，跳过
    }
    
    for (const sourceDir of vcpkgSources) {
      if (!fs.existsSync(sourceDir)) continue;
      
      const sourcePath = path.join(sourceDir, dllName);
      if (fs.existsSync(sourcePath)) {
        try {
          fs.copyFileSync(sourcePath, targetPath);
          console.log(`✓ ${dllName} - 已复制（可选）`);
          copiedCount++;
          break;
        } catch (err) {
          // 忽略可选 DLL 的复制错误
        }
      }
    }
  }
  
  console.log('\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━');
  console.log(`总计: 成功打包 ${copiedCount} 个 DLL 文件\n`);
  
  if (failedDlls.length > 0) {
    console.error(`⚠️ 警告: 以下 DLL 未找到，可能导致模块无法加载:`);
    failedDlls.forEach(dll => console.error(`  - ${dll}`));
    console.error('\n请检查 vcpkg 是否正确安装了 xlnt 和其依赖项：');
    console.error('  vcpkg install xlnt:x64-windows\n');
    
    // 在 CI 环境中，缺少必需的 DLL 应该失败
    if (process.env.CI) {
      console.error('❌ CI 环境中必须包含所有必需的 DLL');
      process.exit(1);
    }
  } else {
    console.log('✅ 所有必需的 DLL 已成功打包！');
    console.log('📦 预编译包现在可以开箱即用\n');
  }
  
} else if (platform === 'linux') {
  console.log('🐧 Linux 平台 - 检查共享库链接\n');
  
  // Linux 上通常使用系统包管理器安装的 .so 文件
  // 或静态链接，所以不需要额外打包
  console.log('ℹ️  Linux 版本使用系统共享库或静态链接');
  console.log('   用户需要通过包管理器安装 xlnt 或相关依赖\n');
  
} else if (platform === 'darwin') {
  console.log('🍎 macOS 平台 - 检查动态库链接\n');
  
  // macOS 类似 Linux
  console.log('ℹ️  macOS 版本使用 Homebrew 安装的库或静态链接');
  console.log('   用户需要: brew install xlnt\n');
  
} else {
  console.log(`⚠️  未知平台: ${platform}，跳过 DLL 打包\n`);
}

// 列出 build/Release 目录下的所有文件
console.log('📂 build/Release 目录内容:');
const files = fs.readdirSync(releaseDir);
files.forEach(file => {
  const filePath = path.join(releaseDir, file);
  const stats = fs.statSync(filePath);
  const size = stats.isFile() ? `(${(stats.size / 1024).toFixed(1)} KB)` : '(目录)';
  console.log(`  - ${file} ${size}`);
});

console.log('\n✅ DLL 打包完成！\n');

