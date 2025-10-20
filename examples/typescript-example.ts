/**
 * TypeScript usage example for baja-lite-xlsx
 * 
 * To run this example:
 *   npm install -g ts-node typescript
 *   ts-node examples/typescript-example.ts
 */

import { readExcel, extractImages, ExcelData, ImageData } from '../index';
import * as fs from 'fs';
import * as path from 'path';

// Example 1: Read Excel with TypeScript types
function example1(): void {
  console.log('Example 1: Reading Excel with TypeScript\n');

  try {
    const data: ExcelData = readExcel('./test/sample.xlsx');

    // Type-safe access to sheets
    data.sheets.forEach((sheet) => {
      console.log(`Sheet: ${sheet.name}`);
      console.log(`  Rows: ${sheet.data.length}`);

      // Access cells with type safety
      if (sheet.data.length > 0) {
        const firstRow: string[] = sheet.data[0];
        console.log(`  Columns: ${firstRow.length}`);
        console.log(`  Headers: ${firstRow.join(', ')}`);
      }
    });

    // Type-safe access to images
    console.log(`\nImages: ${data.images.length}`);
    data.images.forEach((img: ImageData) => {
      console.log(`  - ${img.name}: ${img.data.length} bytes (${img.type})`);
    });

    // Type-safe access to positions
    data.imagePositions.forEach((pos) => {
      console.log(`\nImage: ${pos.image}`);
      console.log(`  Sheet: ${pos.sheet}`);
      console.log(`  From: (${pos.from.col}, ${pos.from.row})`);
      console.log(`  To: (${pos.to.col}, ${pos.to.row})`);
    });
  } catch (error) {
    console.error('Error:', (error as Error).message);
  }
}

// Example 2: Extract and process images
function example2(): void {
  console.log('\n\nExample 2: Extracting Images\n');

  try {
    const images: ImageData[] = extractImages('./test/sample.xlsx');

    images.forEach((img: ImageData, idx: number) => {
      // Save image with type information
      const outputPath = path.join('./examples/output', img.name || `image_${idx}.bin`);

      // Create directory if needed
      const dir = path.dirname(outputPath);
      if (!fs.existsSync(dir)) {
        fs.mkdirSync(dir, { recursive: true });
      }

      // Write image buffer
      fs.writeFileSync(outputPath, img.data);
      console.log(`✓ Saved: ${outputPath}`);
      console.log(`  Size: ${img.data.length} bytes`);
      console.log(`  Type: ${img.type}`);
    });
  } catch (error) {
    console.error('Error:', (error as Error).message);
  }
}

// Example 3: Process specific data with type safety
function example3(): void {
  console.log('\n\nExample 3: Type-Safe Data Processing\n');

  try {
    const data: ExcelData = readExcel('./test/sample.xlsx');

    // Find sheet by name
    const targetSheet = data.sheets.find((s) => s.name === 'Sheet1');

    if (targetSheet) {
      console.log(`Processing sheet: ${targetSheet.name}`);

      // Process rows with type safety
      targetSheet.data.forEach((row: string[], rowIdx: number) => {
        row.forEach((cell: string, colIdx: number) => {
          if (cell && cell.toLowerCase().includes('total')) {
            console.log(`Found "total" at (${rowIdx}, ${colIdx}): ${cell}`);
          }
        });
      });

      // Calculate statistics
      const totalRows = targetSheet.data.length;
      const totalCols = targetSheet.data[0]?.length || 0;
      const totalCells = totalRows * totalCols;

      console.log(`\nStatistics:`);
      console.log(`  Rows: ${totalRows}`);
      console.log(`  Columns: ${totalCols}`);
      console.log(`  Total cells: ${totalCells}`);
    } else {
      console.log('Sheet1 not found');
    }
  } catch (error) {
    console.error('Error:', (error as Error).message);
  }
}

// Run all examples
if (require.main === module) {
  example1();
  example2();
  example3();
}

export { example1, example2, example3 };


