/**
 * 将 DLL 文件添加到已有的预编译包中
 * 
 * prebuild 默认只打包 .node 文件，
 * 这个脚本将 DLL 文件添加到生成的 .tar.gz 包中
 */

const fs = require('fs');
const path = require('path');
const { execSync } = require('child_process');
const os = require('os');

console.log('📦 将 DLL 文件打包到预编译包中...\n');

const prebuildsDir = path.join(__dirname, '..', 'prebuilds');
const buildReleaseDir = path.join(__dirname, '..', 'build', 'Release');

// 首先确保 DLL 文件在 build/Release 目录中
console.log('🔍 检查并复制 DLL 文件...\n');

if (os.platform() === 'win32') {
  const vcpkgSources = [
    process.env.VCPKG_ROOT ? path.join(process.env.VCPKG_ROOT, 'installed', 'x64-windows', 'bin') : null,
    'C:\\vcpkg\\installed\\x64-windows\\bin',
    'E:\\vcpkg\\installed\\x64-windows\\bin',
  ].filter(Boolean);
  
  const requiredDlls = ['xlnt.dll', 'zlib1.dll', 'bz2.dll', 'fmt.dll', 'zip.dll'];
  
  let copiedCount = 0;
  
  requiredDlls.forEach(dllName => {
    const targetPath = path.join(buildReleaseDir, dllName);
    
    // 如果已存在，跳过
    if (fs.existsSync(targetPath)) {
      console.log(`  ✓ ${dllName} - 已存在`);
      copiedCount++;
      return;
    }
    
    // 查找并复制
    for (const sourceDir of vcpkgSources) {
      if (!fs.existsSync(sourceDir)) continue;
      
      const sourcePath = path.join(sourceDir, dllName);
      if (fs.existsSync(sourcePath)) {
        try {
          fs.copyFileSync(sourcePath, targetPath);
          console.log(`  ✓ ${dllName} - 已复制`);
          copiedCount++;
          break;
        } catch (err) {
          console.error(`  ✗ ${dllName} - 复制失败: ${err.message}`);
        }
      }
    }
  });
  
  console.log(`\n已准备 ${copiedCount} 个 DLL 文件\n`);
}

console.log('━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n');

if (!fs.existsSync(prebuildsDir)) {
  console.error('❌ prebuilds 目录不存在');
  process.exit(1);
}

// 获取所有 .tar.gz 文件
const tarFiles = fs.readdirSync(prebuildsDir).filter(f => f.endsWith('.tar.gz') && f.includes('win32'));

if (tarFiles.length === 0) {
  console.log('ℹ️  没有找到 Windows 预编译包');
  process.exit(0);
}

// 需要添加的 DLL 文件
const dllFiles = ['xlnt.dll', 'zlib1.dll', 'bz2.dll', 'fmt.dll', 'zip.dll'];

tarFiles.forEach(tarFile => {
  console.log(`━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━`);
  console.log(`\n📄 处理: ${tarFile}\n`);
  
  const tarPath = path.join(prebuildsDir, tarFile);
  const tempDir = path.join(os.tmpdir(), `prebuild-repack-${Date.now()}`);
  
  try {
    // 1. 创建临时目录
    fs.mkdirSync(tempDir, { recursive: true });
    
    // 2. 解压现有的 tar.gz
    console.log('🔓 解压现有包...');
    execSync(`tar -xzf "${tarPath}" -C "${tempDir}"`, { stdio: 'pipe' });
    
    // 3. 复制 DLL 到解压目录
    console.log('\n📚 添加 DLL 文件:');
    const targetDir = path.join(tempDir, 'build', 'Release');
    
    let addedCount = 0;
    dllFiles.forEach(dllName => {
      const sourcePath = path.join(buildReleaseDir, dllName);
      if (fs.existsSync(sourcePath)) {
        const targetPath = path.join(targetDir, dllName);
        fs.copyFileSync(sourcePath, targetPath);
        const stats = fs.statSync(targetPath);
        console.log(`  ✓ ${dllName} (${(stats.size / 1024).toFixed(1)} KB)`);
        addedCount++;
      } else {
        console.log(`  ⚠ ${dllName} - 未找到（跳过）`);
      }
    });
    
    console.log(`\n添加了 ${addedCount} 个 DLL 文件`);
    
    // 4. 重新打包
    console.log('\n📦 重新打包...');
    
    // 删除旧的 tar.gz
    fs.unlinkSync(tarPath);
    
    // 创建新的 tar.gz
    const cwd = process.cwd();
    process.chdir(tempDir);
    execSync(`tar -czf "${tarPath}" build`, { stdio: 'pipe' });
    process.chdir(cwd);
    
    const newStats = fs.statSync(tarPath);
    console.log(`✓ 新包大小: ${(newStats.size / 1024).toFixed(1)} KB`);
    
    console.log(`\n✅ ${tarFile} - 处理完成\n`);
    
  } catch (err) {
    console.error(`\n❌ 处理失败: ${err.message}\n`);
  } finally {
    // 清理临时目录
    if (fs.existsSync(tempDir)) {
      fs.rmSync(tempDir, { recursive: true, force: true });
    }
  }
});

console.log('━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n');
console.log('✅ 所有预编译包已更新！\n');
console.log('接下来可以运行: npm run test:prebuild\n');

