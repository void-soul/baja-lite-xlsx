/**
 * Embedded image example.
 * Shows how embedded images (=DISPIMG formulas) are returned.
 */

const { readTableAsJSON } = require('../index');
const fs = require('fs');
const path = require('path');

console.log('=== Embedded images example ===\n');

const excelFile = path.join(__dirname, 'sample.xlsx');

if (!fs.existsSync(excelFile)) {
  console.log('Fixture not found:', excelFile);
  console.log('');
  console.log('How to try this example:');
  console.log('1. Prepare an .xlsx with e.g. employee data and photos.');
  console.log('2. Insert pictures in either of these ways:');
  console.log('   - floating image: drag the image near a cell');
  console.log('   - embedded image: Insert > Pictures > This Device, then "Place in Cell"');
  console.log('3. Embedded pictures appear as formulas in Excel: =DISPIMG("ID_...", 1)');
  console.log('4. This library converts both kinds into the same object shape.');
  console.log('');
  console.log('Expected sheet layout:');
  console.log('+--------+------+---------+--------+');
  console.log('| name   | age  | photo1  | note   |');
  console.log('+--------+------+---------+--------+');
  console.log('| Alice  | 25   | [image] | staff  |  <- embedded image');
  console.log('| Bob    | 30   | [image] | manager|  <- floating image');
  console.log('| Carol  | 28   |         | staff  |  <- no image');
  console.log('+--------+------+---------+--------+');
  process.exit(0);
}

try {
  const data = readTableAsJSON(excelFile, {
    headerRow: 0,
    headerMap: {
      '名称': 'name',
      '年龄': 'age'
      // photo1 keeps its original header
    }
  });

  console.log(`Read ${data.length} row(s)\n`);

  data.forEach((row, index) => {
    console.log(`[row ${index + 1}]`);
    console.log(`  name: ${row.name || '(empty)'}`);
    console.log(`  age:  ${row.age || '(empty)'}`);

    const photo = row.photo1;
    if (photo && typeof photo === 'object' && photo.data) {
      console.log('  photo1: recognized');
      console.log(`    - file: ${photo.name}`);
      console.log(`    - type: ${photo.type}`);
      console.log(`    - size: ${photo.data.length} bytes`);

      const outputDir = path.join(__dirname, 'output', 'embedded-photos');
      fs.mkdirSync(outputDir, { recursive: true });

      const filename = `${row.name || 'row' + (index + 1)}_${photo.name}`;
      fs.writeFileSync(path.join(outputDir, filename), photo.data);
      console.log(`    - saved: ${filename}`);
    } else if (typeof photo === 'string' && photo.includes('DISPIMG')) {
      // Seeing this means the embedded image was NOT converted (a bug).
      console.log('  photo1: NOT converted (still a formula)');
      console.log(`    - formula: ${photo}`);
    } else {
      console.log(`  photo1: ${photo || 'none'}`);
    }
    console.log('');
  });

  const withPhotos = data.filter(
    (row) => row.photo1 && typeof row.photo1 === 'object' && row.photo1.data
  ).length;

  console.log('=== Summary ===');
  console.log(`rows:       ${data.length}`);
  console.log(`with photo: ${withPhotos}`);
  console.log(`no photo:   ${data.length - withPhotos}`);

  if (withPhotos > 0) {
    console.log('\nImages saved to: ./examples/output/embedded-photos/');
  }
} catch (error) {
  console.error(`\nError [${error.code || 'UNKNOWN'}]:`, error.message);
  process.exit(1);
}
