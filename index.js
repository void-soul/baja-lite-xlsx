const path = require('path');
const fs = require('fs');

// ---------------------------------------------------------------------------
// Native addon loading
// ---------------------------------------------------------------------------

function loadAddon() {
  let releaseErr = null;
  try {
    return require('./build/Release/baja_xlsx.node');
  } catch (err) {
    releaseErr = err;
  }
  try {
    return require('./build/Debug/baja_xlsx.node');
  } catch (debugErr) {
    // Surface the ORIGINAL load error (e.g. missing DLL name from Windows)
    // instead of hiding it behind a generic message.
    const detail = [releaseErr, debugErr]
      .filter(Boolean)
      .map((e) => (e && e.message) || String(e))
      .join(' | ');
    const e = new Error(
      'Native addon failed to load. Original errors: ' + detail + '\n' +
      'Hints: file missing -> run "npm install" or "npm run build"; ' +
      'module/DLL missing -> copy the required vcpkg DLLs next to baja_xlsx.node ' +
      '(see scripts/package-dlls.js) or install the VC++ Redistributable.'
    );
    e.code = 'ADDON_LOAD_FAILED';
    e.cause = releaseErr;
    throw e;
  }
}

const addon = loadAddon();

// ---------------------------------------------------------------------------
// Error helper: every error carries a machine-readable `code`
// (AUDIT-20260917-039).
// ---------------------------------------------------------------------------

function makeError(code, message) {
  const e = new Error(message);
  e.code = code;
  return e;
}

// ---------------------------------------------------------------------------
// Input normalization
// ---------------------------------------------------------------------------

function looksLikeBase64(input) {
  return input.length > 500 &&
         /^[A-Za-z0-9+/=\s]+$/.test(input) &&
         (input.startsWith('UEs') || input.startsWith('PK') ||
          input.includes('AAAA') || input.includes('////'));
}

/**
 * Normalizes the input (path | Buffer | base64) into either a file path or the
 * workbook bytes. Buffers are parsed from memory, so nothing is written to
 * disk (P0-4).
 * @private
 */
function prepareInput(input, inputEncoding) {
  if (Buffer.isBuffer(input)) {
    return { buffer: input };
  }

  if (typeof input === 'string') {
    if (inputEncoding === 'base64') {
      return { buffer: Buffer.from(input, 'base64') };
    }

    if (inputEncoding !== undefined) {
      throw makeError(
        'INVALID_INPUT',
        `Unsupported inputEncoding "${inputEncoding}" (expected "base64" or undefined)`
      );
    }

    if (looksLikeBase64(input)) {
      // Heuristic path (AUDIT-20260917-025): decoded bytes must carry the
      // ZIP "PK" magic; otherwise fail with an explicit, actionable error
      // instead of misinterpreting the input.
      const buffer = Buffer.from(input, 'base64');
      if (buffer.length < 4 || buffer[0] !== 0x50 || buffer[1] !== 0x4b) {
        throw makeError(
          'INVALID_INPUT',
          'Input string looks like base64 but does not decode to an xlsx (ZIP "PK") header. ' +
          'Pass { inputEncoding: "base64" } explicitly, or provide a file path / Buffer.'
        );
      }
      return { buffer };
    }

    // Treat as file path. fs.statSync inside try/catch gives a coded,
    // friendly error; the residual TOCTOU window is inherent and harmless
    // here (AUDIT-20260917-037).
    const absolutePath = path.isAbsolute(input) ? input : path.resolve(input);
    try {
      fs.statSync(absolutePath);
    } catch (err) {
      throw makeError('FILE_NOT_FOUND', `File not found: ${absolutePath}`);
    }
    return { filepath: absolutePath };
  }

  throw makeError('INVALID_INPUT', 'Input must be a file path (string), Buffer, or base64 string');
}

// ---------------------------------------------------------------------------
// Options validation (AUDIT-20260917-024)
// ---------------------------------------------------------------------------

function validateOptions(options) {
  if (options === null || typeof options !== 'object' || Array.isArray(options)) {
    throw makeError('INVALID_OPTIONS', 'options must be an object');
  }
  const {
    sheetName = null,
    headerRow = 0,
    skipRows = [],
    headerMap = {},
    inputEncoding,
    maxRows = 0,
    maxCols = 0,
    includeImages = true,
    columns = [],
    includeWarnings = false
  } = options;

  if (sheetName !== null && sheetName !== undefined && typeof sheetName !== 'string') {
    throw makeError('INVALID_OPTIONS', 'options.sheetName must be a string');
  }
  if (!Number.isInteger(headerRow) || headerRow < 0) {
    throw makeError('INVALID_OPTIONS', `options.headerRow must be a non-negative integer, got ${headerRow}`);
  }
  if (!Array.isArray(skipRows)) {
    throw makeError('INVALID_OPTIONS', 'options.skipRows must be an array of row indices');
  }
  for (const r of skipRows) {
    if (!Number.isInteger(r) || r < 0) {
      throw makeError('INVALID_OPTIONS', `options.skipRows must contain non-negative integers, got ${r}`);
    }
  }
  if (headerMap === null || typeof headerMap !== 'object' || Array.isArray(headerMap)) {
    throw makeError('INVALID_OPTIONS', 'options.headerMap must be an object');
  }
  if (maxRows !== undefined && !Number.isInteger(maxRows) || maxRows < 0) {
    throw makeError('INVALID_OPTIONS', `options.maxRows must be a non-negative integer, got ${maxRows}`);
  }
  if (maxCols !== undefined && !Number.isInteger(maxCols) || maxCols < 0) {
    throw makeError('INVALID_OPTIONS', `options.maxCols must be a non-negative integer, got ${maxCols}`);
  }
  if (typeof includeWarnings !== 'boolean') {
    throw makeError('INVALID_OPTIONS', 'options.includeWarnings must be a boolean');
  }
  if (typeof includeImages !== 'boolean') {
    throw makeError('INVALID_OPTIONS', 'options.includeImages must be a boolean');
  }
  if (!Array.isArray(columns)) {
    throw makeError('INVALID_OPTIONS', 'options.columns must be an array of column names or references');
  }
  for (const column of columns) {
    if (typeof column !== 'string' || column.trim() === '') {
      throw makeError('INVALID_OPTIONS', 'options.columns must contain non-empty strings');
    }
  }

  return {
    sheetName: sheetName === undefined ? null : sheetName,
    headerRow,
    skipRows,
    headerMap,
    maxRows,
    maxCols,
    includeImages,
    columns,
    includeWarnings
  };
}

// ---------------------------------------------------------------------------
// Sheet data -> JSON rows
// ---------------------------------------------------------------------------

function transformToRows(nativeResult, opts) {
  const { sheetName, headerRow, skipRows, headerMap, includeWarnings } = opts;

  let targetSheet;
  if (sheetName) {
    targetSheet = nativeResult.sheets.find((sheet) => sheet.name === sheetName);
    if (!targetSheet) {
      throw makeError('SHEET_NOT_FOUND', `Sheet "${sheetName}" not found`);
    }
  } else {
    if (nativeResult.sheets.length === 0) {
      throw makeError('NO_SHEETS', 'Excel file has no sheets');
    }
    targetSheet = nativeResult.sheets[0];
  }

  const sheetData = targetSheet.data;

  if (sheetData.length <= headerRow) {
    throw makeError(
      'HEADER_ROW_OUT_OF_RANGE',
      `Header row index ${headerRow} is out of range (${sheetData.length} rows available)`
    );
  }

  // With column projection the native layer reports the resolved header
  // texts; otherwise the header row itself provides them.
  const headerSource =
    (targetSheet.headers && targetSheet.headers.length) ? targetSheet.headers : sheetData[headerRow];
  const mappedHeaders = headerSource.map((header) => headerMap[header] || header);
  const skipRowsSet = new Set([headerRow, ...skipRows]);

  const rows = [];
  for (let rowIndex = 0; rowIndex < sheetData.length; rowIndex++) {
    if (skipRowsSet.has(rowIndex)) continue;

    const row = sheetData[rowIndex];
    const rowObj = {};
    for (let colIndex = 0; colIndex < mappedHeaders.length; colIndex++) {
      const header = mappedHeaders[colIndex];
      const value = row[colIndex] !== undefined ? row[colIndex] : '';
      if (header) {
        rowObj[header] = value;
      }
    }
    rows.push(rowObj);
  }

  if (includeWarnings) {
    return { rows, warnings: nativeResult.warnings || [] };
  }
  return rows;
}

// Only the requested sheet, columns and (optionally) images are ever touched
// on the native side; everything the caller does not ask for is skipped there
// instead of being filtered afterwards.
function nativeOptions(opts) {
  return {
    sheetName: opts.sheetName === null ? undefined : opts.sheetName,
    headerRow: opts.headerRow,
    maxRows: opts.maxRows,
    maxCols: opts.maxCols,
    includeImages: opts.includeImages,
    columns: opts.columns.length ? opts.columns : undefined
  };
}

function runNative(prepared, opts) {
  return prepared.buffer
    ? addon.readExcel(prepared.buffer, nativeOptions(opts))
    : addon.readExcel(prepared.filepath, nativeOptions(opts));
}

function runNativeAsync(prepared, opts) {
  return prepared.buffer
    ? addon.readExcelAsync(prepared.buffer, nativeOptions(opts))
    : addon.readExcelAsync(prepared.filepath, nativeOptions(opts));
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/**
 * Read an Excel table and return it as a JSON array.
 *
 * @param {string|Buffer} input - File path, Buffer, or base64 string
 *   (when passing base64 explicitly, set options.inputEncoding = 'base64').
 * @param {Object} [options]
 * @param {string} [options.sheetName] - Sheet to read (default: first sheet).
 * @param {number} [options.headerRow=0] - Header row index (0-based).
 * @param {number[]} [options.skipRows=[]] - Row indices to skip (0-based).
 * @param {Object<string,string>} [options.headerMap={}] - Header renames.
 * @param {string} [options.inputEncoding] - Force input interpretation: 'base64'.
 * @param {number} [options.maxRows=0] - Cap on rows read per sheet (0 = no cap).
 * @param {number} [options.maxCols=0] - Cap on columns read per sheet (0 = no cap).
 * @param {boolean} [options.includeImages=true] - false skips the whole image
 *   pipeline (no second pass over the archive, no media decompression).
 * @param {string[]} [options.columns=[]] - Read only these columns, given as
 *   header texts ("Amount") or Excel references ("B", "C:E").
 * @param {boolean} [options.includeWarnings=false] - Return { rows, warnings }.
 * @returns {Array<Object>} One object per data row, keyed by the header texts
 *   (after headerMap). Cell values are strings, except cells holding pictures,
 *   which contain { data: Buffer, name, type } (or an array of them).
 *   With includeWarnings: true it returns { rows, warnings } instead.
 */
function readTableAsJSON(input, options = {}) {
  if (input === null || input === undefined || input === '') {
    throw makeError('INVALID_INPUT', 'Input is required (filepath, Buffer, or base64 string)');
  }

  const opts = validateOptions(options);
  const prepared = prepareInput(input, options.inputEncoding);

  try {
    const nativeResult = runNative(prepared, opts);
    return transformToRows(nativeResult, opts);
  } catch (err) {
    if (!err.code) {
      err.code = 'PARSE_ERROR';
    }
    throw err;
  }
}

/**
 * Async variant of readTableAsJSON: parsing runs on the libuv thread pool
 * so the event loop (and any Electron UI) is not blocked.
 *
 * @param {string|Buffer} input - See readTableAsJSON.
 * @param {Object} [options] - See readTableAsJSON.
 * @returns {Promise<Array<Object>|{rows: Array<Object>, warnings: string[]}>}
 */
async function readTableAsJSONAsync(input, options = {}) {
  if (input === null || input === undefined || input === '') {
    throw makeError('INVALID_INPUT', 'Input is required (filepath, Buffer, or base64 string)');
  }

  const opts = validateOptions(options);
  const prepared = prepareInput(input, options.inputEncoding);

  let nativeResult;
  try {
    nativeResult = await runNativeAsync(prepared, opts);
  } catch (err) {
    if (!err.code) {
      err.code = 'PARSE_ERROR';
    }
    throw err;
  }
  return transformToRows(nativeResult, opts);
}

module.exports = {
  readTableAsJSON,
  readTableAsJSONAsync
};
