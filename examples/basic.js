const { readExcel, extractImages } = require('../index');
const fs = require('fs');
const path = require('path');

// Example 1: Read complete Excel file
console.log('Example 1: Reading complete Excel file\n');

try {
  const data = readExcel('./test/sample.xlsx');
  
  // Access sheets
  console.log(`Found ${data.sheets.length} sheet(s):`);
  data.sheets.forEach(sheet => {
    console.log(`\nSheet: ${sheet.name}`);
    console.log(`Rows: ${sheet.data.length}`);
    
    // Print first few rows
    sheet.data.slice(0, 5).forEach((row, rowIdx) => {
      console.log(`Row ${rowIdx + 1}:`, row);
    });
  });
  
  // Access images
  console.log(`\nFound ${data.images.length} image(s):`);
  data.images.forEach(img => {
    console.log(`- ${img.name}: ${img.data.length} bytes (${img.type})`);
  });
  
  // Access image positions
  console.log(`\nImage positions:`);
  data.imagePositions.forEach(pos => {
    console.log(`- ${pos.image} in sheet "${pos.sheet}"`);
    console.log(`  Position: (${pos.from.col},${pos.from.row}) to (${pos.to.col},${pos.to.row})`);
  });
  
} catch (error) {
  console.error('Error:', error.message);
}

// Example 2: Extract only images
console.log('\n\nExample 2: Extracting images only\n');

try {
  const images = extractImages('./test/sample.xlsx');
  
  console.log(`Extracted ${images.length} image(s):`);
  
  images.forEach((img, idx) => {
    const filename = img.name || `image_${idx + 1}.bin`;
    const outputPath = path.join('./examples/output', filename);
    
    // Create output directory if it doesn't exist
    const dir = path.dirname(outputPath);
    if (!fs.existsSync(dir)) {
      fs.mkdirSync(dir, { recursive: true });
    }
    
    // Save image
    fs.writeFileSync(outputPath, img.data);
    console.log(`✓ Saved ${filename} (${img.data.length} bytes)`);
  });
  
} catch (error) {
  console.error('Error:', error.message);
}

// Example 3: Process specific cell data
console.log('\n\nExample 3: Processing specific cells\n');

try {
  const data = readExcel('./test/sample.xlsx');
  
  if (data.sheets.length > 0) {
    const firstSheet = data.sheets[0];
    console.log(`Processing sheet: ${firstSheet.name}`);
    
    // Access specific cell (e.g., A1 = row 0, col 0)
    if (firstSheet.data.length > 0 && firstSheet.data[0].length > 0) {
      console.log(`Cell A1: ${firstSheet.data[0][0]}`);
    }
    
    // Find cells with specific content
    firstSheet.data.forEach((row, rowIdx) => {
      row.forEach((cell, colIdx) => {
        if (cell && cell.toLowerCase().includes('total')) {
          console.log(`Found "total" at row ${rowIdx + 1}, col ${colIdx + 1}: ${cell}`);
        }
      });
    });
  }
  
} catch (error) {
  console.error('Error:', error.message);
}


