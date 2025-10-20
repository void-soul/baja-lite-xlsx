// 快速测试脚本
console.log('测试 baja-lite-xlsx 模块加载...\n');

try {
  const { readExcel, extractImages } = require('./index');
  console.log('✅ 模块加载成功！');
  console.log('✅ readExcel 函数:', typeof readExcel);
  console.log('✅ extractImages 函数:', typeof extractImages);
  
  console.log('\n模块可以正常使用了！');
  console.log('\n接下来：');
  console.log('1. 在 test/ 目录创建一个 sample.xlsx 文件（包含一些数据和图片）');
  console.log('2. 运行: node test/test.js');
  console.log('3. 或查看示例: node examples/basic.js');
  
} catch (error) {
  console.error('❌ 错误:', error.message);
  process.exit(1);
}

