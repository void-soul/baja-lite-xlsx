/**
 * JSON API usage example (async + header mapping + images).
 * Fixture: examples/sample.xlsx
 */

const { readTableAsJSONAsync } = require('../index');
const fs = require('fs');
const path = require('path');

const excelFile = path.join(__dirname, 'sample.xlsx');

async function main() {
  if (!fs.existsSync(excelFile)) {
    console.log('Fixture not found:', excelFile);
    return;
  }

  // 1. Basic read (first sheet, first row as header)
  const rows = await readTableAsJSONAsync(excelFile);
  console.log(`Basic read: ${rows.length} rows`);

  // 2. Header mapping
  const first = rows[0] || {};
  const headerMap = {};
  Object.keys(first).forEach((k) => {
    headerMap[k] = k.trim().toLowerCase().replace(/\s+/g, '_');
  });
  const mapped = await readTableAsJSONAsync(excelFile, { headerMap });
  console.log('Mapped first row:', mapped[0]);

  // 3. Buffer + base64 input
  const buffer = fs.readFileSync(excelFile);
  const fromBuffer = await readTableAsJSONAsync(buffer);
  console.log(`Buffer read: ${fromBuffer.length} rows`);

  const fromBase64 = await readTableAsJSONAsync(buffer.toString('base64'), {
    inputEncoding: 'base64'
  });
  console.log(`Base64 read: ${fromBase64.length} rows`);

  // 4. Image cells (if the fixture contains images)
  const withWarnings = await readTableAsJSONAsync(excelFile, {
    headerMap,
    includeWarnings: true
  });
  withWarnings.rows.forEach((row, i) => {
    Object.entries(row).forEach(([key, value]) => {
      if (value && typeof value === 'object' && value.data) {
        console.log(`Row ${i + 1} [${key}]: image ${value.name} (${value.type})`);
      } else if (Array.isArray(value) && value[0] && value[0].data) {
        console.log(`Row ${i + 1} [${key}]: ${value.length} images`);
      }
    });
  });
  if (withWarnings.warnings.length) {
    console.log('Warnings:', withWarnings.warnings);
  }
}

main().catch((err) => {
  console.error(`Error [${err.code || 'UNKNOWN'}]:`, err.message);
  process.exit(1);
});
