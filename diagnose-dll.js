/**
 * 诊断和修复 Windows 原生模块依赖问题
 * 
 * 使用方法：
 * node diagnose-dll.js "E:\pro\gld\gld-web\attack-service\node_modules\baja-lite-xlsx\build\Release\baja_xlsx.node"
 */

const { execSync } = require('child_process');
const fs = require('fs');
const path = require('path');

const nodeFilePath = process.argv[2];

if (!nodeFilePath) {
  console.error('请提供 .node 文件路径作为参数');
  console.error('用法: node diagnose-dll.js <path-to-node-file>');
  process.exit(1);
}

if (!fs.existsSync(nodeFilePath)) {
  console.error(`文件不存在: ${nodeFilePath}`);
  process.exit(1);
}

console.log('🔍 诊断 .node 文件依赖...\n');
console.log(`文件: ${nodeFilePath}\n`);

// 1. 检查文件是否存在
console.log('✓ 文件存在');

// 2. 获取文件信息
const stats = fs.statSync(nodeFilePath);
console.log(`✓ 文件大小: ${(stats.size / 1024).toFixed(2)} KB`);

// 3. 检查依赖的DLL（使用 dumpbin 或 Dependencies Walker）
console.log('\n📦 检查DLL依赖...\n');

try {
  // 尝试使用 dumpbin（如果已安装 Visual Studio）
  try {
    const output = execSync(`dumpbin /dependents "${nodeFilePath}"`, { 
      encoding: 'utf8',
      stdio: ['pipe', 'pipe', 'pipe']
    });
    
    // 提取依赖的DLL
    const lines = output.split('\n');
    const dllStart = lines.findIndex(line => line.includes('Image has the following dependencies'));
    
    if (dllStart !== -1) {
      console.log('依赖的DLL文件：');
      for (let i = dllStart + 2; i < lines.length; i++) {
        const line = lines[i].trim();
        if (!line || line.includes('Summary')) break;
        if (line.endsWith('.dll')) {
          console.log(`  - ${line}`);
          
          // 检查DLL是否可以被找到
          const nodeDir = path.dirname(nodeFilePath);
          const dllInSameDir = path.join(nodeDir, line);
          
          if (fs.existsSync(dllInSameDir)) {
            console.log(`    ✓ 找到: ${dllInSameDir}`);
          } else {
            console.log(`    ✗ 未找到在: ${dllInSameDir}`);
            console.log(`    ℹ 可能在系统PATH中，或需要复制到此目录`);
          }
        }
      }
    }
  } catch (dumpbinErr) {
    console.log('⚠ dumpbin 不可用（需要安装 Visual Studio）');
    console.log('提示：您可以使用 Dependencies Walker 或 dumpbin 查看依赖');
  }
} catch (err) {
  console.error('检查依赖时出错:', err.message);
}

// 4. 尝试加载模块看具体错误
console.log('\n🧪 尝试加载模块...\n');
try {
  require(nodeFilePath);
  console.log('✓ 模块加载成功！');
} catch (err) {
  console.log('✗ 模块加载失败:');
  console.log(`  错误: ${err.message}\n`);
  
  // 5. 提供解决方案
  console.log('━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━');
  console.log('🔧 可能的解决方案：\n');
  
  console.log('1️⃣  检查是否缺少 Visual C++ Redistributable');
  console.log('   下载地址：https://aka.ms/vs/17/release/vc_redist.x64.exe');
  console.log('   （如果是32位Node.js，使用 vc_redist.x86.exe）\n');
  
  console.log('2️⃣  复制依赖的DLL到 .node 文件同目录');
  console.log(`   目标目录：${path.dirname(nodeFilePath)}\n`);
  
  console.log('3️⃣  如果项目使用了 xlnt 等库，需要复制相关DLL');
  console.log('   常见依赖：xlnt.dll, zlib.dll, libexpat.dll 等\n');
  
  console.log('4️⃣  重新编译模块（确保Node.js版本匹配）');
  console.log('   进入模块目录运行：npm rebuild\n');
  
  console.log('5️⃣  检查Node.js架构是否匹配');
  console.log(`   当前Node.js架构：${process.arch}`);
  console.log(`   当前Node.js版本：${process.version}\n`);
  
  console.log('━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━');
}

// 6. 检查Node.js信息
console.log('\n💻 Node.js 环境信息：');
console.log(`  版本: ${process.version}`);
console.log(`  架构: ${process.arch}`);
console.log(`  平台: ${process.platform}`);

// 7. 检查模块是否是预编译的
const moduleDir = path.dirname(path.dirname(path.dirname(nodeFilePath)));
const packageJsonPath = path.join(moduleDir, 'package.json');

if (fs.existsSync(packageJsonPath)) {
  console.log('\n📋 模块信息：');
  try {
    const pkg = JSON.parse(fs.readFileSync(packageJsonPath, 'utf8'));
    console.log(`  名称: ${pkg.name}`);
    console.log(`  版本: ${pkg.version}`);
    if (pkg.binary) {
      console.log('  ✓ 支持预编译二进制文件');
    }
  } catch (e) {
    // 忽略
  }
}

