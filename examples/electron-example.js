/**
 * Electron usage example.
 *
 * Use readTableAsJSONAsync in the renderer/main process so parsing runs on
 * the libuv thread pool and the UI never blocks (AUDIT-20260917-007).
 */
const { readTableAsJSONAsync } = require('baja-lite-xlsx');
const path = require('path');

async function loadExcelFile(filePath) {
  const rows = await readTableAsJSONAsync(filePath, {
    headerRow: 0,
    includeWarnings: true
  });

  if (rows.warnings.length > 0) {
    console.warn('Excel warnings:', rows.warnings);
  }

  return rows.rows; // plain JSON-serializable rows for the renderer
}

// Renderer usage:
//   const data = await loadExcelFile(path.join(__dirname, 'sample.xlsx'));
//   renderTable(data);

void path;
void loadExcelFile;
