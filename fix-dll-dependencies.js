/**
 * 自动修复 baja-lite-xlsx 的 DLL 依赖问题
 * 
 * 使用方法：
 * node fix-dll-dependencies.js "E:\pro\gld\gld-web\attack-service\node_modules\baja-lite-xlsx"
 */

const fs = require('fs');
const path = require('path');

const targetModulePath = process.argv[2];

if (!targetModulePath) {
  console.error('请提供模块路径作为参数');
  console.error('用法: node fix-dll-dependencies.js <module-path>');
  console.error('示例: node fix-dll-dependencies.js "E:\\pro\\gld\\gld-web\\attack-service\\node_modules\\baja-lite-xlsx"');
  process.exit(1);
}

console.log('🔧 自动修复 DLL 依赖问题\n');

// 检查目标路径
const nodeFilePath = path.join(targetModulePath, 'build', 'Release', 'baja_xlsx.node');
if (!fs.existsSync(nodeFilePath)) {
  console.error(`✗ 找不到 .node 文件: ${nodeFilePath}`);
  process.exit(1);
}

console.log(`目标模块: ${targetModulePath}`);
console.log(`.node 文件: ${nodeFilePath}\n`);

// 可能的 DLL 源目录列表
const possibleDllSources = [
  'E:\\vcpkg\\installed\\x64-windows\\bin',
  'E:\\vcpkg\\installed\\x86-windows\\bin',
  'C:\\vcpkg\\installed\\x64-windows\\bin',
  'C:\\vcpkg\\installed\\x86-windows\\bin',
  path.join(__dirname, 'build', 'Release'),  // 当前项目的 build 目录
];

// 需要的 DLL 文件列表（根据 xlnt 依赖）
const requiredDlls = [
  'xlnt.dll',
  'zlib1.dll',
  'bz2.dll',
  'fmt.dll',
  'zip.dll',
];

// 备选 DLL 名称
const alternativeDlls = [
  'zlib.dll',  // 有些系统用 zlib.dll 而不是 zlib1.dll
];

const targetDir = path.dirname(nodeFilePath);
console.log(`目标目录: ${targetDir}\n`);

let copiedCount = 0;
let alreadyExistCount = 0;

// 查找并复制 DLL
for (const dllName of requiredDlls) {
  // 检查是否已经存在
  const targetDllPath = path.join(targetDir, dllName);
  
  if (fs.existsSync(targetDllPath)) {
    console.log(`✓ ${dllName} - 已存在`);
    alreadyExistCount++;
    continue;
  }
  
  // 查找源文件
  let found = false;
  for (const sourceDir of possibleDllSources) {
    if (!fs.existsSync(sourceDir)) continue;
    
    const sourceDllPath = path.join(sourceDir, dllName);
    if (fs.existsSync(sourceDllPath)) {
      try {
        fs.copyFileSync(sourceDllPath, targetDllPath);
        console.log(`✓ ${dllName} - 已复制`);
        console.log(`  来源: ${sourceDllPath}`);
        copiedCount++;
        found = true;
        break;
      } catch (err) {
        console.error(`✗ ${dllName} - 复制失败: ${err.message}`);
      }
    }
  }
  
  if (!found) {
    console.log(`⚠ ${dllName} - 未找到（可能不需要）`);
  }
}

console.log('\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━');
console.log(`\n总结:`);
console.log(`  已存在的 DLL: ${alreadyExistCount}`);
console.log(`  新复制的 DLL: ${copiedCount}`);

if (copiedCount > 0) {
  console.log(`\n✓ 已复制 ${copiedCount} 个 DLL 文件到目标目录`);
}

// 测试加载
console.log('\n🧪 测试模块加载...\n');
try {
  require(nodeFilePath);
  console.log('✅ 成功！模块现在可以正常加载了！');
} catch (err) {
  console.log('❌ 模块仍然无法加载');
  console.log(`错误: ${err.message}\n`);
  
  console.log('━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━');
  console.log('🔍 进一步检查：\n');
  
  console.log('1. 检查是否安装了 Visual C++ Redistributable');
  console.log('   下载: https://aka.ms/vs/17/release/vc_redist.x64.exe\n');
  
  console.log('2. 检查 Node.js 版本是否匹配');
  console.log(`   当前版本: ${process.version}`);
  console.log(`   当前架构: ${process.arch}\n`);
  
  console.log('3. 尝试重新编译模块');
  console.log(`   cd "${targetModulePath}"`);
  console.log('   npm rebuild\n');
  
  console.log('4. 查看详细依赖信息');
  console.log(`   node diagnose-dll.js "${nodeFilePath}"\n`);
  
  console.log('━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━');
}

