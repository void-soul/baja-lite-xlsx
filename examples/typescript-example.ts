/**
 * TypeScript usage example for baja-lite-xlsx.
 * Types come from index.d.ts; every entry point is async.
 */

import { readTableAsJSON, writeTableAsJSON, updateCells, ImageDataObject } from 'baja-lite-xlsx';
import * as fs from 'fs';

type CellValue = string | number | boolean | Date | ImageDataObject | ImageDataObject[];
type Row = Record<string, CellValue>;

function isImage(value: CellValue): value is ImageDataObject {
  return !!value && typeof value === 'object' && !Array.isArray(value) &&
    Buffer.isBuffer((value as ImageDataObject).data);
}

async function main(): Promise<void> {
  // 1. Read: the overload with includeWarnings gives { rows, warnings }.
  const { rows, warnings } = await readTableAsJSON('./sample.xlsx', {
    sheetName: 'Sheet1',
    headerRow: 0,
    includeWarnings: true
  });
  console.log(`Rows: ${rows.length}`);

  rows.forEach((row: Row, i: number) => {
    for (const [key, value] of Object.entries(row)) {
      if (isImage(value)) {
        console.log(`Row ${i + 1} [${key}]: image ${value.name} (${value.data.length} bytes)`);
      }
    }
  });
  if (warnings.length > 0) {
    console.warn('Warnings:', warnings);
  }

  // 2. Typed read: real numbers / booleans / Dates instead of strings.
  const typed = await readTableAsJSON('./sample.xlsx', {
    engine: 'xml',
    values: 'typed',
    columns: ['amount']
  });
  console.log(`Typed rows: ${typed.length}`);

  // 3. Write, then chain: the Buffer from one call feeds the next as sourceFile.
  const bytes: Buffer = await writeTableAsJSON(rows, { sheetName: 'Data' });
  const appended: Buffer = await writeTableAsJSON([{ note: 'appended' }], {
    sourceFile: bytes,
    sheetName: 'Data',
    columns: { note: { header: 'Note' } }
  });

  // 4. Patch single cells of the Buffer produced above.
  const patched: Buffer = await updateCells({
    sourceFile: appended,
    updates: [{ cell: 'B2', value: 1234.5, numberFormat: '#,##0.00' }]
  });
  fs.writeFileSync('./out.xlsx', patched);
}

main().catch((err: NodeJS.ErrnoException) => {
  console.error(`Error [${err.code || 'UNKNOWN'}]: ${err.message}`);
  process.exit(1);
});
