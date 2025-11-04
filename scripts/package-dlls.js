/**
 * 复制 vcpkg DLL 文件到 build/Release 目录
 */

const fs = require('fs');
const path = require('path');
const os = require('os');

if (os.platform() !== 'win32') {
  console.log('ℹ️  非 Windows 系统，跳过 DLL 复制');
  process.exit(0);
}

const buildDir = path.join(__dirname, '..', 'build', 'Release');

// 检查 build/Release 目录是否存在
if (!fs.existsSync(buildDir)) {
  console.log('ℹ️  build/Release 目录不存在，跳过 DLL 复制');
  process.exit(0);
}

// vcpkg 可能的路径
const vcpkgPaths = [
  process.env.VCPKG_ROOT ? path.join(process.env.VCPKG_ROOT, 'installed', 'x64-windows', 'bin') : null,
  'E:\\vcpkg\\installed\\x64-windows\\bin',
  'C:\\vcpkg\\installed\\x64-windows\\bin',
].filter(Boolean);

// 需要复制的 DLL 文件
const requiredDlls = ['xlnt.dll', 'zlib1.dll', 'bz2.dll', 'fmt.dll', 'zip.dll'];

let copiedCount = 0;
let sourceDir = null;

// 查找 vcpkg bin 目录
for (const testPath of vcpkgPaths) {
  if (fs.existsSync(testPath)) {
    sourceDir = testPath;
    break;
  }
}

if (!sourceDir) {
  console.log('⚠️  未找到 vcpkg bin 目录，跳过 DLL 复制');
  console.log('   如果编译后无法运行，请检查 VCPKG_ROOT 环境变量');
  process.exit(0);
}

console.log(`📦 从 ${sourceDir} 复制 DLL...\n`);

requiredDlls.forEach(dllName => {
  const sourcePath = path.join(sourceDir, dllName);
  const targetPath = path.join(buildDir, dllName);
  
  if (fs.existsSync(sourcePath)) {
    try {
      fs.copyFileSync(sourcePath, targetPath);
      console.log(`  ✓ ${dllName}`);
      copiedCount++;
    } catch (err) {
      console.error(`  ✗ ${dllName} - 复制失败: ${err.message}`);
    }
  } else {
    console.log(`  ⚠ ${dllName} - 源文件不存在`);
  }
});

console.log(`\n✅ 已复制 ${copiedCount}/${requiredDlls.length} 个 DLL 文件`);

