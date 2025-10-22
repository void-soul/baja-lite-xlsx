/**
 * 测试预编译包的完整性
 * 验证预编译包是否包含所有必需的 DLL 文件
 * 
 * 使用方法：
 * npm run prebuild
 * node scripts/test-prebuild-package.js
 */

const fs = require('fs');
const path = require('path');
const { execSync } = require('child_process');
const os = require('os');

console.log('━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━');
console.log('🧪 测试预编译包完整性');
console.log('━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n');

const platform = os.platform();
const prebuildsDir = path.join(__dirname, '..', 'prebuilds');

// 检查 prebuilds 目录是否存在
if (!fs.existsSync(prebuildsDir)) {
  console.error('❌ prebuilds 目录不存在');
  console.error('   请先运行: npm run prebuild\n');
  process.exit(1);
}

// 获取所有 .tar.gz 文件
const tarFiles = fs.readdirSync(prebuildsDir).filter(f => f.endsWith('.tar.gz'));

if (tarFiles.length === 0) {
  console.error('❌ 没有找到预编译包');
  console.error('   请先运行: npm run prebuild\n');
  process.exit(1);
}

console.log(`📦 找到 ${tarFiles.length} 个预编译包:\n`);

let allTestsPassed = true;

// 测试每个预编译包
for (const tarFile of tarFiles) {
  console.log(`━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━`);
  console.log(`\n📄 测试: ${tarFile}\n`);
  
  const tarPath = path.join(prebuildsDir, tarFile);
  const stats = fs.statSync(tarPath);
  const sizeKB = (stats.size / 1024).toFixed(1);
  
  console.log(`   大小: ${sizeKB} KB`);
  
  // 创建临时解压目录
  const tempDir = path.join(__dirname, '..', 'temp_test_' + Date.now());
  fs.mkdirSync(tempDir, { recursive: true });
  
  try {
    // 解压
    console.log('\n🔓 解压预编译包...');
    
    if (platform === 'win32') {
      execSync(`tar -xzf "${tarPath}" -C "${tempDir}"`, { stdio: 'pipe' });
    } else {
      execSync(`tar -xzf "${tarPath}" -C "${tempDir}"`, { stdio: 'pipe' });
    }
    
    // 列出内容
    console.log('\n📂 包内容:');
    
    const listFiles = (dir, prefix = '') => {
      const files = fs.readdirSync(dir);
      const result = [];
      
      files.forEach(file => {
        const fullPath = path.join(dir, file);
        const stats = fs.statSync(fullPath);
        
        if (stats.isDirectory()) {
          console.log(`   ${prefix}📁 ${file}/`);
          listFiles(fullPath, prefix + '  ');
        } else {
          const sizeKB = (stats.size / 1024).toFixed(1);
          const ext = path.extname(file);
          const icon = ext === '.node' ? '🔷' : ext === '.dll' ? '📚' : '📄';
          console.log(`   ${prefix}${icon} ${file} (${sizeKB} KB)`);
          result.push(file);
        }
      });
      
      return result;
    };
    
    const allFiles = [];
    const walk = (dir) => {
      const files = fs.readdirSync(dir);
      files.forEach(file => {
        const fullPath = path.join(dir, file);
        if (fs.statSync(fullPath).isDirectory()) {
          walk(fullPath);
        } else {
          allFiles.push(file);
        }
      });
    };
    
    listFiles(tempDir);
    walk(tempDir);
    
    // 验证必需文件（仅 Windows）
    if (tarFile.includes('win32')) {
      console.log('\n🔍 验证 Windows 依赖...\n');
      
      const requiredFiles = {
        'baja_xlsx.node': { required: true, found: false },
        'xlnt.dll': { required: true, found: false },
        'zlib1.dll': { required: true, found: false },
        'zlib.dll': { required: false, found: false },
        'bz2.dll': { required: false, found: false },
        'fmt.dll': { required: false, found: false },
        'zip.dll': { required: false, found: false },
      };
      
      // 检查每个文件
      allFiles.forEach(file => {
        if (requiredFiles[file]) {
          requiredFiles[file].found = true;
        }
      });
      
      // 验证必需文件
      let hasErrors = false;
      
      console.log('必需文件:');
      if (requiredFiles['baja_xlsx.node'].found) {
        console.log('   ✅ baja_xlsx.node - 已找到');
      } else {
        console.log('   ❌ baja_xlsx.node - 未找到');
        hasErrors = true;
      }
      
      if (requiredFiles['xlnt.dll'].found) {
        console.log('   ✅ xlnt.dll - 已找到');
      } else {
        console.log('   ❌ xlnt.dll - 未找到');
        hasErrors = true;
      }
      
      // zlib 需要至少有一个
      const hasZlib = requiredFiles['zlib1.dll'].found || requiredFiles['zlib.dll'].found;
      if (hasZlib) {
        const zlibName = requiredFiles['zlib1.dll'].found ? 'zlib1.dll' : 'zlib.dll';
        console.log(`   ✅ ${zlibName} - 已找到`);
      } else {
        console.log('   ❌ zlib1.dll / zlib.dll - 未找到');
        hasErrors = true;
      }
      
      // 可选的 DLL
      console.log('\n可选文件（xlnt 依赖）:');
      if (requiredFiles['bz2.dll'].found) {
        console.log('   ✅ bz2.dll - 已找到');
      }
      if (requiredFiles['fmt.dll'].found) {
        console.log('   ✅ fmt.dll - 已找到');
      }
      if (requiredFiles['zip.dll'].found) {
        console.log('   ✅ zip.dll - 已找到');
      }
      
      console.log('');
      
      if (hasErrors) {
        console.log(`❌ ${tarFile} - 验证失败\n`);
        allTestsPassed = false;
      } else {
        console.log(`✅ ${tarFile} - 验证通过\n`);
      }
    } else {
      // 非 Windows 平台
      console.log('\n✅ 非 Windows 平台，跳过 DLL 检查\n');
    }
    
  } catch (err) {
    console.error(`\n❌ 测试失败: ${err.message}\n`);
    allTestsPassed = false;
  } finally {
    // 清理临时目录
    if (fs.existsSync(tempDir)) {
      fs.rmSync(tempDir, { recursive: true, force: true });
    }
  }
}

console.log('━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n');

if (allTestsPassed) {
  console.log('✅ 所有预编译包验证通过！');
  console.log('📦 可以安全发布到 GitHub Release\n');
  process.exit(0);
} else {
  console.log('❌ 部分预编译包验证失败');
  console.log('🔧 请检查并修复问题后再发布\n');
  console.log('提示：');
  console.log('  1. 确保运行了 npm run copy-dlls');
  console.log('  2. 检查 build/Release/ 目录是否包含所有 DLL');
  console.log('  3. 重新运行 npm run prebuild\n');
  process.exit(1);
}

