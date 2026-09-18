/**
 * Advanced usage: options, caps, diagnostics and image saving.
 * Fixture: examples/sample.xlsx
 */

const { readTableAsJSON } = require('../index');
const fs = require('fs');
const path = require('path');

const excelFile = path.join(__dirname, 'sample.xlsx');

async function main() {
  if (!fs.existsSync(excelFile)) {
    console.log('Fixture not found:', excelFile);
    return;
  }

  // 1. Read caps: truncate huge sheets deterministically.
  const capped = await readTableAsJSON(excelFile, { maxRows: 1000, maxCols: 50 });
  console.log(`Capped read: ${capped.length} rows`);

  // 2. Diagnostics: collect warnings for unattached images etc.
  const { rows, warnings } = await readTableAsJSON(excelFile, {
    includeWarnings: true
  });
  console.log(`Full read: ${rows.length} rows, ${warnings.length} warnings`);
  warnings.forEach((w) => console.log(`  - ${w}`));

  // 3. Save images found in cells.
  const outputDir = path.join(__dirname, 'output', 'photos');
  fs.mkdirSync(outputDir, { recursive: true });

  let saved = 0;
  rows.forEach((row, index) => {
    Object.entries(row).forEach(([key, value]) => {
      const images = Array.isArray(value)
        ? value.filter((v) => v && v.data)
        : (value && typeof value === 'object' && value.data ? [value] : []);
      images.forEach((img, n) => {
        const name = `${index}_${key}_${n}_${img.name}`;
        fs.writeFileSync(path.join(outputDir, name), img.data);
        saved++;
      });
    });
  });
  console.log(`Saved ${saved} image(s) to ${outputDir}`);
}

main().catch((err) => {
  console.error(`Error [${err.code || 'UNKNOWN'}]:`, err.message);
  process.exit(1);
});
