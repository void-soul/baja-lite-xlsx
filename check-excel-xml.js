const fs = require('fs');
const path = require('path');
const { exec } = require('child_process');
const util = require('util');
const execPromise = util.promisify(exec);

const excelPath = path.resolve('./test/sample.xlsx');
const tempDir = './temp-excel-extract';

// 清理并创建临时目录
if (fs.existsSync(tempDir)) {
  fs.rmSync(tempDir, { recursive: true });
}
fs.mkdirSync(tempDir, { recursive: true });

// 复制文件为zip
const zipPath = path.join(tempDir, 'sample.zip');
fs.copyFileSync(excelPath, zipPath);

console.log('正在解压Excel文件...\n');

// 使用tar命令解压 (Windows 10+内置)
exec(`tar -xf "${zipPath}" -C "${tempDir}"`, (error) => {
  if (error) {
    console.error('解压失败，尝试其他方法:', error.message);
    
    // 尝试使用PowerShell
    const psCmd = `Expand-Archive -Path "${zipPath}" -DestinationPath "${tempDir}/extracted" -Force`;
    exec(`powershell -Command "${psCmd}"`, (error2) => {
      if (error2) {
        console.error('PowerShell解压也失败:', error2.message);
        return;
      }
      showXMLContent(path.join(tempDir, 'extracted'));
    });
  } else {
    showXMLContent(tempDir);
  }
});

function showXMLContent(extractDir) {
  console.log('=== Drawing XML ===\n');
  
  const drawingPath = path.join(extractDir, 'xl/drawings/drawing1.xml');
  if (fs.existsSync(drawingPath)) {
    const content = fs.readFileSync(drawingPath, 'utf8');
    console.log(content);
    console.log('\n');
    
    // 解析关键信息
    const fromMatch = content.match(/<xdr:from>[\s\S]*?<xdr:col>(\d+)<\/xdr:col>[\s\S]*?<xdr:row>(\d+)<\/xdr:row>/);
    const toMatch = content.match(/<xdr:to>[\s\S]*?<xdr:col>(\d+)<\/xdr:col>[\s\S]*?<xdr:row>(\d+)<\/xdr:row>/);
    
    if (fromMatch) {
      console.log(`From: col=${fromMatch[1]}, row=${fromMatch[2]}`);
    }
    if (toMatch) {
      console.log(`To: col=${toMatch[1]}, row=${toMatch[2]}`);
    }
  } else {
    console.log('drawing1.xml 不存在');
  }
  
  console.log('\n=== Drawing Relationships ===\n');
  
  const relsPath = path.join(extractDir, 'xl/drawings/_rels/drawing1.xml.rels');
  if (fs.existsSync(relsPath)) {
    const content = fs.readFileSync(relsPath, 'utf8');
    console.log(content);
  } else {
    console.log('drawing1.xml.rels 不存在');
  }
}

