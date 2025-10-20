/**
 * JSON API 使用示例
 * 演示如何使用新的高级API读取Excel并返回JSON数组
 */

const { readTableAsJSON, getSheetNames } = require('../index');
const fs = require('fs');
const path = require('path');

console.log('=== JSON API 使用示例 ===\n');

// 示例Excel文件路径
const excelFile = './test/sample.xlsx';

// 检查文件是否存在
if (!fs.existsSync(excelFile)) {
  console.log('⚠️  测试文件不存在:', excelFile);
  console.log('请创建一个包含以下内容的Excel文件:');
  console.log('');
  console.log('Sheet1:');
  console.log('┌────────┬──────┬─────────┬────────┐');
  console.log('│ 名称   │ 年龄 │ photo1  │ 备注   │');
  console.log('├────────┼──────┼─────────┼────────┤');
  console.log('│ 张三   │ 25   │ [图片]  │ 员工   │');
  console.log('│ 李四   │ 30   │         │ 经理   │');
  console.log('│ 王五   │ 28   │ [图片]  │ 员工   │');
  console.log('└────────┴──────┴─────────┴────────┘');
  console.log('');
  console.log('提示：在photo1列的某些行添加图片');
  process.exit(0);
}

try {
  // ========================================
  // 示例 1: 获取所有Sheet名称
  // ========================================
  console.log('【示例 1】获取所有Sheet名称\n');
  
  const sheetNames = getSheetNames(excelFile);
  console.log('Sheet名称列表:', sheetNames);
  console.log('\n' + '='.repeat(50) + '\n');
  
  // ========================================
  // 示例 2: 基本用法 - 使用默认选项
  // ========================================
  console.log('【示例 2】基本用法 - 默认选项\n');
  
  const data1 = readTableAsJSON(excelFile);
  console.log('读取结果（默认选项）:');
  console.log(JSON.stringify(data1, (key, value) => {
    // Buffer对象太大，只显示类型
    if (value && value.type === 'Buffer') {
      return `<Buffer ${value.data.length} bytes>`;
    }
    return value;
  }, 2));
  console.log(`共 ${data1.length} 行数据`);
  console.log('\n' + '='.repeat(50) + '\n');
  
  // ========================================
  // 示例 3: 使用表头映射
  // ========================================
  console.log('【示例 3】使用表头映射\n');
  
  const data2 = readTableAsJSON(excelFile, {
    headerRow: 0,
    headerMap: {
      '名称': 'name',
      '年龄': 'age',
      '备注': 'note'
      // photo1 没有映射，保持原名
    }
  });
  
  console.log('使用表头映射后的数据:');
  data2.forEach((row, index) => {
    console.log(`\n行 ${index + 1}:`);
    console.log(`  name: ${row.name || ''}`);
    console.log(`  age: ${row.age || ''}`);
    console.log(`  note: ${row.note || ''}`);
    
    // 图片会自动根据列名（photo1）添加
    if (row.photo1 && typeof row.photo1 === 'object' && row.photo1.data) {
      console.log(`  photo1:`);
      console.log(`    - name: ${row.photo1.name}`);
      console.log(`    - type: ${row.photo1.type}`);
      console.log(`    - data: Buffer (${row.photo1.data.length} bytes)`);
    } else {
      console.log(`  photo1: 无`);
    }
  });
  console.log('\n' + '='.repeat(50) + '\n');
  
  // ========================================
  // 示例 4: 跳过指定行
  // ========================================
  console.log('【示例 4】跳过指定行\n');
  
  const data3 = readTableAsJSON(excelFile, {
    headerRow: 0,
    skipRows: [1], // 跳过第2行（索引1）
    headerMap: {
      '名称': 'name',
      '年龄': 'age'
    }
  });
  
  console.log('跳过索引1的行后:');
  console.log(JSON.stringify(data3.map(r => ({ 
    name: r.name, 
    age: r.age,
    hasPhoto: !!r.photo1 
  })), null, 2));
  console.log(`剩余 ${data3.length} 行数据`);
  console.log('\n' + '='.repeat(50) + '\n');
  
  // ========================================
  // 示例 5: 保存图片到文件
  // ========================================
  console.log('【示例 5】保存图片到文件\n');
  
  const data4 = readTableAsJSON(excelFile, {
    headerMap: {
      '名称': 'name'
    }
  });
  
  const outputDir = path.join(__dirname, 'output', 'photos');
  if (!fs.existsSync(outputDir)) {
    fs.mkdirSync(outputDir, { recursive: true });
  }
  
  let savedCount = 0;
  data4.forEach((row, index) => {
    // 遍历所有可能的图片列
    Object.keys(row).forEach(key => {
      if (row[key] && typeof row[key] === 'object' && row[key].data) {
        const img = row[key];
        const filename = `${row.name || 'row' + (index + 1)}_${key}_${img.name}`;
        const outputPath = path.join(outputDir, filename);
        fs.writeFileSync(outputPath, img.data);
        console.log(`  保存: ${filename} (${img.data.length} bytes)`);
        savedCount++;
      }
    });
  });
  
  console.log(`\n共保存 ${savedCount} 张图片到: ${outputDir}`);
  console.log('\n' + '='.repeat(50) + '\n');
  
  // ========================================
  // 示例 6: 使用 Buffer 读取
  // ========================================
  console.log('【示例 6】使用 Buffer 读取\n');
  
  const fileBuffer = fs.readFileSync(excelFile);
  const data5 = readTableAsJSON(fileBuffer, {
    headerMap: {
      '名称': 'name',
      '年龄': 'age'
    }
  });
  
  console.log('使用 Buffer 读取:');
  console.log(`共 ${data5.length} 行数据`);
  console.log('\n' + '='.repeat(50) + '\n');
  
  // ========================================
  // 示例 7: 使用 base64 读取
  // ========================================
  console.log('【示例 7】使用 base64 读取\n');
  
  const base64String = fileBuffer.toString('base64');
  const data6 = readTableAsJSON(base64String, {
    headerRow: 0
  });
  
  console.log('使用 base64 字符串读取:');
  console.log(`共 ${data6.length} 行数据`);
  console.log('\n' + '='.repeat(50) + '\n');
  
  // ========================================
  // 示例 8: 处理多列图片
  // ========================================
  console.log('【示例 8】处理多列图片\n');
  console.log('提示：如果Excel有多个图片列（如photo1, photo2），图片会自动添加到对应列名的属性中');
  console.log('');
  console.log('示例数据结构:');
  console.log('{');
  console.log('  name: "张三",');
  console.log('  age: "25",');
  console.log('  photo1: { data: Buffer, name: "img1.png", type: "image/png" },');
  console.log('  photo2: { data: Buffer, name: "img2.jpg", type: "image/jpeg" }');
  console.log('}');
  console.log('\n' + '='.repeat(50) + '\n');
  
  console.log('✓ 所有示例执行完成!');
  
} catch (error) {
  console.error('\n✗ 错误:', error.message);
  console.error(error.stack);
  process.exit(1);
}
