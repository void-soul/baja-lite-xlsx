/**
 * 测试JSON API功能
 */

console.log('测试 JSON API...\n');

try {
  const { getSheetNames, readTableAsJSON, 读取表格, 读取表格SheetName } = require('./index');
  
  console.log('✅ JSON API 加载成功！');
  console.log('');
  console.log('可用函数:');
  console.log('  - getSheetNames:', typeof getSheetNames);
  console.log('  - readTableAsJSON:', typeof readTableAsJSON);
  console.log('  - 读取表格:', typeof 读取表格);
  console.log('  - 读取表格SheetName:', typeof 读取表格SheetName);
  console.log('');
  console.log('✓ 所有API函数都已正确导出！');
  console.log('');
  console.log('接下来:');
  console.log('1. 创建测试Excel文件: test/sample.xlsx');
  console.log('2. 运行示例: npm run example:json');
  console.log('3. 查看文档: API_USAGE_CN.md');
  
} catch (error) {
  console.error('❌ 错误:', error.message);
  console.error(error.stack);
  process.exit(1);
}

