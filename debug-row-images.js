/**
 * 调试行图片分配
 */

const { readTableAsJSON } = require('./index');
const addon = require('./build/Release/baja_xlsx.node');
const path = require('path');

const excelFile = path.resolve('./test/sample.xlsx');

console.log('=== 调试行图片分配 ===\n');

// 1. 先看看原始数据
const rawData = addon.readExcel(excelFile);

console.log('1. 原始数据:');
console.log('   Images:', rawData.images.length);
rawData.images.forEach((img, i) => {
  console.log(`     ${i}: ${img.name} (${img.data.length} bytes)`);
});

console.log('\n   ImagePositions:', rawData.imagePositions.length);
rawData.imagePositions.forEach((pos, i) => {
  console.log(`     ${i}: ${pos.image} at sheet="${pos.sheet}" from=(${pos.from.col}, ${pos.from.row}) to=(${pos.to.col}, ${pos.to.row})`);
});

// 2. 获取Sheet数据
console.log('\n2. Sheet1 数据:');
const sheet1 = rawData.sheets[0];
console.log('   名称:', sheet1.name);
console.log('   行数:', sheet1.data.length);
sheet1.data.forEach((row, i) => {
  const excelRow = i + 1;
  console.log(`   Excel行${excelRow} (索引${i}): [${row.join(', ')}]`);
});

// 3. 测试readTableAsJSON
console.log('\n3. 使用readTableAsJSON:');
try {
  const data = readTableAsJSON(excelFile, {
    headerRow: 0,
    headerMap: {
      '名称': 'name',
      '年龄': 'age',
      ' 备注': 'note'
    }
  });
  
  console.log('   结果行数:', data.length);
  data.forEach((row, i) => {
    console.log(`\n   数据行 ${i} (Excel行${i + 2}，因为跳过了表头):`);
    console.log(`     完整对象:`, JSON.stringify(row, (key, value) => {
      if (value && value.type === 'Buffer') {
        return `<Buffer ${value.data.length} bytes>`;
      }
      return value;
    }, 2));
  });
} catch (error) {
  console.error('   错误:', error.message);
  console.error(error.stack);
}

