const { readExcel, extractImages } = require('../index');
const path = require('path');
const fs = require('fs');

console.log('=== Baja-XLSX Native Module Test ===\n');

const testFile = path.join(__dirname, 'sample.xlsx');

// Check if test file exists
if (!fs.existsSync(testFile)) {
  console.error('Error: test/sample.xlsx not found');
  console.log('Please create a sample.xlsx file in the test directory with some data and images');
  process.exit(1);
}

try {
  console.log('1. Testing readExcel()...');
  const result = readExcel(testFile);
  
  console.log(`\n   Sheets found: ${result.sheets.length}`);
  result.sheets.forEach((sheet, idx) => {
    console.log(`   - Sheet ${idx + 1}: "${sheet.name}" (${sheet.data.length} rows)`);
    if (sheet.data.length > 0) {
      console.log(`     First row has ${sheet.data[0].length} columns`);
      console.log(`     Sample data: ${JSON.stringify(sheet.data[0].slice(0, 3))}`);
    }
  });
  
  console.log(`\n   Images found: ${result.images.length}`);
  result.images.forEach((img, idx) => {
    console.log(`   - Image ${idx + 1}: ${img.name} (${img.data.length} bytes, type: ${img.type})`);
  });
  
  console.log(`\n   Image positions found: ${result.imagePositions.length}`);
  result.imagePositions.forEach((pos, idx) => {
    console.log(`   - Position ${idx + 1}: ${pos.image} in sheet "${pos.sheet}"`);
    console.log(`     From: col ${pos.from.col}, row ${pos.from.row}`);
    console.log(`     To: col ${pos.to.col}, row ${pos.to.row}`);
  });
  
  console.log('\n2. Testing extractImages()...');
  const images = extractImages(testFile);
  console.log(`   Extracted ${images.length} images`);
  
  // Save images to output directory
  const outputDir = path.join(__dirname, 'output');
  if (!fs.existsSync(outputDir)) {
    fs.mkdirSync(outputDir);
  }
  
  images.forEach((img, idx) => {
    const outputPath = path.join(outputDir, img.name || `image_${idx + 1}.bin`);
    fs.writeFileSync(outputPath, img.data);
    console.log(`   Saved: ${outputPath}`);
  });
  
  console.log('\n✓ All tests completed successfully!');
  
} catch (error) {
  console.error('\n✗ Test failed:');
  console.error(error.message);
  console.error(error.stack);
  process.exit(1);
}


