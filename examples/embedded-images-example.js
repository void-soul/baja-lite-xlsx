/**
 * 嵌入式图片处理示例
 * 演示如何处理Excel中的嵌入式图片（=DISPIMG公式）
 */

const { readTableAsJSON } = require('../index');
const fs = require('fs');
const path = require('path');

console.log('=== 嵌入式图片处理示例 ===\n');

// 使用示例文件
const excelFile = './test/sample.xlsx';

// 检查文件是否存在
if (!fs.existsSync(excelFile)) {
  console.log('⚠️  测试文件不存在:', excelFile);
  console.log('');
  console.log('使用说明：');
  console.log('1. 准备一个Excel文件，包含员工信息和照片');
  console.log('2. 在Excel中，可以使用两种方式插入图片：');
  console.log('   - 浮动图片：直接拖拽图片到单元格附近');
  console.log('   - 嵌入式图片：使用"插入 > 图片 > 此设备"，选择"嵌入单元格"');
  console.log('3. 嵌入式图片在Excel中会显示为公式：=DISPIMG("ID_...", 1)');
  console.log('4. 本库会自动识别并转换两种类型的图片为统一格式');
  console.log('');
  console.log('Excel 示例结构：');
  console.log('┌────────┬──────┬─────────┬────────┐');
  console.log('│ 名称   │ 年龄 │ photo1  │ 备注   │');
  console.log('├────────┼──────┼─────────┼────────┤');
  console.log('│ 张三   │ 25   │ [图片]  │ 员工   │  <- 嵌入式图片');
  console.log('│ 李四   │ 30   │ [图片]  │ 经理   │  <- 浮动图片');
  console.log('│ 王五   │ 28   │         │ 员工   │  <- 无图片');
  console.log('└────────┴──────┴─────────┴────────┘');
  process.exit(0);
}

try {
  // 读取Excel数据
  const data = readTableAsJSON(excelFile, {
    headerRow: 0,
    headerMap: {
      '名称': 'name',
      '年龄': 'age'
      // photo1 保持原名
    }
  });
  
  console.log(`读取到 ${data.length} 行数据\n`);
  
  // 处理每一行数据
  data.forEach((row, index) => {
    console.log(`【行 ${index + 1}】`);
    console.log(`  姓名: ${row.name || '(空)'}`);
    console.log(`  年龄: ${row.age || '(空)'}`);
    
    // 检查是否有图片
    if (row.photo1) {
      if (typeof row.photo1 === 'object' && row.photo1.data) {
        // 图片已正确转换为对象
        console.log(`  照片: ✓ 已识别`);
        console.log(`    - 文件名: ${row.photo1.name}`);
        console.log(`    - 类型: ${row.photo1.type}`);
        console.log(`    - 大小: ${row.photo1.data.length} bytes`);
        
        // 保存图片到文件
        const outputDir = path.join(__dirname, 'output', 'embedded-photos');
        if (!fs.existsSync(outputDir)) {
          fs.mkdirSync(outputDir, { recursive: true });
        }
        
        const filename = `${row.name || 'row' + (index + 1)}_${row.photo1.name}`;
        const outputPath = path.join(outputDir, filename);
        fs.writeFileSync(outputPath, row.photo1.data);
        console.log(`    - 已保存: ${filename}`);
      } else if (typeof row.photo1 === 'string' && row.photo1.includes('DISPIMG')) {
        // 如果显示这个，说明嵌入式图片没有正确转换（bug）
        console.log(`  照片: ✗ 未转换（仍为公式）`);
        console.log(`    - 公式: ${row.photo1}`);
        console.log(`    - ⚠️  这可能是一个bug，嵌入式图片应该被自动转换`);
      } else {
        console.log(`  照片: ${row.photo1}`);
      }
    } else {
      console.log(`  照片: 无`);
    }
    
    console.log('');
  });
  
  // 统计
  const withPhotos = data.filter(row => 
    row.photo1 && typeof row.photo1 === 'object' && row.photo1.data
  ).length;
  
  console.log('=== 统计 ===');
  console.log(`总行数: ${data.length}`);
  console.log(`有照片的行: ${withPhotos}`);
  console.log(`无照片的行: ${data.length - withPhotos}`);
  
  if (withPhotos > 0) {
    console.log('\n✓ 图片已保存到: ./examples/output/embedded-photos/');
  }
  
} catch (error) {
  console.error('\n✗ 错误:', error.message);
  console.error(error.stack);
  process.exit(1);
}

