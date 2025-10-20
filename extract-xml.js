/**
 * 提取并显示Excel ZIP中的XML文件
 */

const AdmZip = require('adm-zip');
const path = require('path');

const excelFile = path.resolve('./test/sample.xlsx');
const zip = new AdmZip(excelFile);

console.log('=== Excel ZIP 文件结构 ===\n');

// 列出所有文件
console.log('所有文件:');
zip.getEntries().forEach(entry => {
  if (entry.entryName.includes('drawing') || entry.entryName.includes('media')) {
    console.log(`  ${entry.entryName} (${entry.header.size} bytes)`);
  }
});

console.log('\n=== Drawing XML ===\n');
const drawingEntry = zip.getEntry('xl/drawings/drawing1.xml');
if (drawingEntry) {
  const content = drawingEntry.getData().toString('utf8');
  console.log(content);
}

console.log('\n=== Drawing Relationships ===\n');
const relsEntry = zip.getEntry('xl/drawings/_rels/drawing1.xml.rels');
if (relsEntry) {
  const content = relsEntry.getData().toString('utf8');
  console.log(content);
}

