/**
 * Postinstall 脚本
 * 确保预编译包中的 DLL 文件被正确放置
 * 
 * 执行时机：
 * 1. 用户执行 npm install 时
 * 2. prebuild-install 成功下载预编译包后
 */

const fs = require('fs');
const path = require('path');
const os = require('os');

const platform = os.platform();

// 只在 Windows 上执行
if (platform !== 'win32') {
  console.log('ℹ️  非 Windows 平台，跳过 DLL 检查');
  process.exit(0);
}

console.log('🔍 检查 Windows DLL 依赖...\n');

const releaseDir = path.join(__dirname, '..', 'build', 'Release');
const nodeFile = path.join(releaseDir, 'baja_xlsx.node');

// 检查 .node 文件是否存在
if (!fs.existsSync(nodeFile)) {
  console.log('ℹ️  原生模块尚未构建，跳过 DLL 检查');
  console.log('   (这是正常的，如果正在下载预编译包或准备编译)\n');
  process.exit(0);
}

// 需要的 DLL 文件
const requiredDlls = ['xlnt.dll', 'zlib1.dll'];
const optionalDlls = ['bz2.dll', 'fmt.dll', 'zip.dll', 'zlib.dll'];

let missingDlls = [];
let foundDlls = [];

// 检查必需的 DLL
requiredDlls.forEach(dllName => {
  const dllPath = path.join(releaseDir, dllName);
  if (fs.existsSync(dllPath)) {
    foundDlls.push(dllName);
    console.log(`✓ ${dllName} - 已找到`);
  } else {
    missingDlls.push(dllName);
  }
});

// 检查可选的 DLL
optionalDlls.forEach(dllName => {
  const dllPath = path.join(releaseDir, dllName);
  if (fs.existsSync(dllPath)) {
    foundDlls.push(dllName);
    console.log(`✓ ${dllName} - 已找到（可选）`);
  }
});

console.log('');

if (missingDlls.length === 0) {
  console.log('✅ 所有必需的 DLL 文件都已就绪！\n');
  console.log('📦 模块已准备好使用\n');
} else {
  console.warn('⚠️  警告: 以下 DLL 文件缺失:\n');
  missingDlls.forEach(dll => console.warn(`  - ${dll}`));
  console.warn('\n这可能导致模块加载失败。\n');
  
  console.log('🔧 解决方法：\n');
  console.log('1. 安装 Visual C++ Redistributable:');
  console.log('   https://aka.ms/vs/17/release/vc_redist.x64.exe\n');
  
  console.log('2. 或者从 vcpkg 手动复制 DLL:');
  console.log(`   来源: C:\\vcpkg\\installed\\x64-windows\\bin\\`);
  console.log(`   目标: ${releaseDir}\n`);
  
  console.log('3. 或者运行自动修复脚本:');
  console.log(`   node ${path.join(__dirname, '..', 'fix-dll-dependencies.js')} "${path.dirname(releaseDir)}"\n`);
  
  // 在开发环境下提示，但不要让 install 失败
  // 因为用户可能会在后续安装 VC++ Redistributable
}

// 尝试加载模块进行测试（可选）
if (process.env.BAJA_XLSX_TEST_LOAD) {
  console.log('🧪 测试模块加载...\n');
  try {
    const addon = require(nodeFile);
    console.log('✅ 模块加载成功！\n');
  } catch (err) {
    console.error('❌ 模块加载失败:');
    console.error(`   ${err.message}\n`);
    console.error('请参考上述解决方法修复 DLL 依赖问题\n');
  }
}

