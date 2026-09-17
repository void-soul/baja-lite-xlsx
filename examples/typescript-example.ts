/**
 * TypeScript usage example for baja-lite-xlsx.
 * Types come from index.d.ts.
 */

import { readTableAsJSON, readTableAsJSONAsync, ImageDataObject } from 'baja-lite-xlsx';
import * as fs from 'fs';

interface UserRow {
  [key: string]: string | ImageDataObject | ImageDataObject[];
}

function isImage(v: string | ImageDataObject | ImageDataObject[] | undefined): v is ImageDataObject {
  return !!v && typeof v === 'object' && !Array.isArray(v) && Buffer.isBuffer((v as ImageDataObject).data);
}

// Synchronous read
const rows = readTableAsJSON('./sample.xlsx') as UserRow[];
console.log(`Rows: ${rows.length}`);

// Async read with diagnostics
async function main(): Promise<void> {
  const { rows, warnings } = (await readTableAsJSONAsync('./sample.xlsx', {
    sheetName: 'Sheet1',
    headerRow: 0,
    includeWarnings: true
  })) as { rows: UserRow[]; warnings: string[] };

  rows.forEach((row, i) => {
    for (const [key, value] of Object.entries(row)) {
      if (isImage(value)) {
        console.log(`Row ${i + 1} [${key}]: image ${value.name} (${value.data.length} bytes)`);
      }
    }
  });

  if (warnings.length > 0) {
    console.warn('Warnings:', warnings);
  }
}

main().catch((err: NodeJS.ErrnoException) => {
  console.error(`Error [${err.code || 'UNKNOWN'}]: ${err.message}`);
  process.exit(1);
});
