const { readExcel, extractImages } = require('../index');
const fs = require('fs');
const path = require('path');

console.log('=== Advanced Usage Examples ===\n');

// Example 1: Export sheet data to CSV
function exportToCSV(sheetData, outputPath) {
  const csvContent = sheetData.data
    .map(row => row.map(cell => `"${cell}"`).join(','))
    .join('\n');
  
  fs.writeFileSync(outputPath, csvContent, 'utf8');
  console.log(`✓ Exported to CSV: ${outputPath}`);
}

// Example 2: Find cells by pattern
function findCellsByPattern(sheetData, pattern) {
  const results = [];
  const regex = new RegExp(pattern, 'i');
  
  sheetData.data.forEach((row, rowIdx) => {
    row.forEach((cell, colIdx) => {
      if (regex.test(cell)) {
        results.push({
          row: rowIdx,
          col: colIdx,
          value: cell
        });
      }
    });
  });
  
  return results;
}

// Example 3: Extract sheet to JSON
function sheetToJSON(sheetData, hasHeaders = true) {
  if (sheetData.data.length === 0) return [];
  
  if (hasHeaders) {
    const headers = sheetData.data[0];
    return sheetData.data.slice(1).map(row => {
      const obj = {};
      headers.forEach((header, idx) => {
        obj[header] = row[idx] || '';
      });
      return obj;
    });
  } else {
    return sheetData.data.map(row => {
      const obj = {};
      row.forEach((cell, idx) => {
        obj[`col_${idx}`] = cell;
      });
      return obj;
    });
  }
}

// Example 4: Get sheet statistics
function getSheetStatistics(sheetData) {
  const stats = {
    totalRows: sheetData.data.length,
    totalCols: 0,
    emptyCells: 0,
    filledCells: 0,
    uniqueValues: new Set()
  };
  
  sheetData.data.forEach(row => {
    if (row.length > stats.totalCols) {
      stats.totalCols = row.length;
    }
    
    row.forEach(cell => {
      if (cell === '') {
        stats.emptyCells++;
      } else {
        stats.filledCells++;
        stats.uniqueValues.add(cell);
      }
    });
  });
  
  stats.uniqueValuesCount = stats.uniqueValues.size;
  delete stats.uniqueValues; // Remove the set from output
  
  return stats;
}

// Example 5: Save images with metadata
function saveImagesWithMetadata(images, imagePositions, outputDir) {
  if (!fs.existsSync(outputDir)) {
    fs.mkdirSync(outputDir, { recursive: true });
  }
  
  const metadata = [];
  
  images.forEach((img, idx) => {
    // Save image
    const imgPath = path.join(outputDir, img.name || `image_${idx}.bin`);
    fs.writeFileSync(imgPath, img.data);
    
    // Find position info
    const positions = imagePositions.filter(p => p.image === img.name || p.image.includes(img.name));
    
    metadata.push({
      filename: img.name,
      path: imgPath,
      size: img.data.length,
      type: img.type,
      positions: positions
    });
  });
  
  // Save metadata JSON
  const metadataPath = path.join(outputDir, 'metadata.json');
  fs.writeFileSync(metadataPath, JSON.stringify(metadata, null, 2), 'utf8');
  
  return metadata;
}

// Run examples
try {
  const data = readExcel('./test/sample.xlsx');
  
  console.log('Example 1: Export to CSV');
  if (data.sheets.length > 0) {
    const csvPath = path.join('./examples/output', 'sheet1.csv');
    const dir = path.dirname(csvPath);
    if (!fs.existsSync(dir)) {
      fs.mkdirSync(dir, { recursive: true });
    }
    exportToCSV(data.sheets[0], csvPath);
  }
  
  console.log('\nExample 2: Find cells by pattern');
  if (data.sheets.length > 0) {
    const matches = findCellsByPattern(data.sheets[0], 'total|sum|amount');
    console.log(`Found ${matches.length} matching cells:`);
    matches.forEach(m => {
      console.log(`  (${m.row}, ${m.col}): ${m.value}`);
    });
  }
  
  console.log('\nExample 3: Convert sheet to JSON');
  if (data.sheets.length > 0) {
    const json = sheetToJSON(data.sheets[0], true);
    console.log(`Converted ${json.length} rows to JSON objects`);
    if (json.length > 0) {
      console.log('Sample:', JSON.stringify(json[0], null, 2));
    }
  }
  
  console.log('\nExample 4: Get sheet statistics');
  if (data.sheets.length > 0) {
    const stats = getSheetStatistics(data.sheets[0]);
    console.log('Statistics:', stats);
  }
  
  console.log('\nExample 5: Save images with metadata');
  if (data.images.length > 0) {
    const outputDir = path.join('./examples/output', 'images');
    const metadata = saveImagesWithMetadata(data.images, data.imagePositions, outputDir);
    console.log(`Saved ${metadata.length} images with metadata`);
    console.log(`Metadata file: ${path.join(outputDir, 'metadata.json')}`);
  } else {
    console.log('No images found in the Excel file');
  }
  
  console.log('\n✓ All advanced examples completed!');
  
} catch (error) {
  console.error('\n✗ Error:', error.message);
  process.exit(1);
}


