/**
 * Basic usage example.
 * Fixture: examples/sample.xlsx (adjust sheetName/headerMap to your file).
 */

const { readTableAsJSON } = require('../index');
const path = require('path');

const excelFile = path.join(__dirname, 'sample.xlsx');

try {
  const data = readTableAsJSON(excelFile, { includeWarnings: true });

  console.log(`Rows: ${data.rows.length}`);
  data.rows.slice(0, 5).forEach((row, i) => {
    console.log(`Row ${i + 1}:`, row);
  });

  if (data.warnings.length > 0) {
    console.log('\nWarnings:');
    data.warnings.forEach((w) => console.log(`  - ${w}`));
  }
} catch (err) {
  console.error(`Error [${err.code || 'UNKNOWN'}]:`, err.message);
}
