/**
 * 调试图片数据结构
 */

const addon = require('./build/Release/baja_xlsx.node');
const path = require('path');

const excelFile = path.resolve('./test/sample.xlsx');

console.log('=== 调试图片数据 ===\n');

try {
  const data = addon.readExcel(excelFile);
  
  console.log('1. Images 数组:');
  console.log('   数量:', data.images.length);
  data.images.forEach((img, i) => {
    console.log(`\n   图片 ${i}:`);
    console.log(`     name: "${img.name}"`);
    console.log(`     type: "${img.type}"`);
    console.log(`     data: Buffer(${img.data ? img.data.length : 0} bytes)`);
  });
  
  console.log('\n2. ImagePositions 数组:');
  console.log('   数量:', data.imagePositions.length);
  data.imagePositions.forEach((pos, i) => {
    console.log(`\n   位置 ${i}:`);
    console.log(`     image: "${pos.image}"`);
    console.log(`     sheet: "${pos.sheet}"`);
    console.log(`     from: { col: ${pos.from.col}, row: ${pos.from.row} }`);
    console.log(`     to: { col: ${pos.to.col}, row: ${pos.to.row} }`);
  });
  
  console.log('\n3. 匹配测试:');
  data.imagePositions.forEach((pos, i) => {
    const matchedImage = data.images.find(img => 
      img.name === pos.image || 
      img.name.includes(pos.image) || 
      pos.image.includes(img.name)
    );
    
    console.log(`\n   位置 ${i} (image="${pos.image}"):`);
    if (matchedImage) {
      console.log(`     ✓ 找到匹配: ${matchedImage.name}`);
    } else {
      console.log(`     ✗ 未找到匹配`);
      console.log(`     可用的图片名称:`, data.images.map(img => img.name));
    }
  });
  
} catch (error) {
  console.error('错误:', error.message);
  console.error(error.stack);
}

