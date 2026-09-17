const path = require('path');
const fs = require('fs');
const os = require('os');
const crypto = require('crypto');

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
// Temporary files for Buffer / base64 input
// ---------------------------------------------------------------------------

const TEMP_FILE_PATTERN = /^excel-[0-9a-f]{32}\.xlsx$/;
const TEMP_MAX_AGE_MS = 24 * 60 * 60 * 1000;

// Best-effort cleanup of temp files left behind by earlier crashed runs
// (AUDIT-20260917-036).
function cleanupOldTempFiles() {
  try {
    const tempDir = os.tmpdir();
    const now = Date.now();
    for (const name of fs.readdirSync(tempDir)) {
      if (!TEMP_FILE_PATTERN.test(name)) continue;
      const full = path.join(tempDir, name);
      try {
        const stats = fs.statSync(full);
        if (now - stats.mtimeMs > TEMP_MAX_AGE_MS) {
          fs.unlinkSync(full);
        }
      } catch (err) {
        // ignore individual entry failures
      }
    }
  } catch (err) {
    // cleanup is best-effort only
  }
}

function writeTempFile(buffer) {
  cleanupOldTempFiles();
  const tempFile = path.join(
    os.tmpdir(),
    `excel-${crypto.randomBytes(16).toString('hex')}.xlsx`
  );
  fs.writeFileSync(tempFile, buffer);
  return tempFile;
}

function removeTempFile(filepath) {
  try {
    fs.unlinkSync(filepath);
  } catch (err) {
    if (process.env.DEBUG) {
      console.warn(`[baja-lite-xlsx] failed to remove temp file ${filepath}: ${err.message}`);
    }
  }
}

function looksLikeBase64(input) {
  return input.length > 500 &&
         /^[A-Za-z0-9+/=\s]+$/.test(input) &&
         (input.startsWith('UEs') || input.startsWith('PK') ||
          input.includes('AAAA') || input.includes('////'));
}

/**
 * Normalizes the input (path | Buffer | base64) into a file path.
 * @private
 */
function prepareFilePath(input, inputEncoding) {
  if (Buffer.isBuffer(input)) {
    const filepath = writeTempFile(input);
    return { filepath, cleanup: () => removeTempFile(filepath) };
  }

  if (typeof input === 'string') {
    let buffer = null;

    if (inputEncoding === 'base64') {
      buffer = Buffer.from(input, 'base64');
    } else if (inputEncoding !== undefined) {
      throw makeError(
        'INVALID_INPUT',
        `Unsupported inputEncoding "${inputEncoding}" (expected "base64" or undefined)`
      );
    } else if (looksLikeBase64(input)) {
      // Heuristic path (AUDIT-20260917-025): decoded bytes must carry the
      // ZIP "PK" magic; otherwise fail with an explicit, actionable error
      // instead of misinterpreting the input.
      buffer = Buffer.from(input, 'base64');
      if (buffer.length < 4 || buffer[0] !== 0x50 || buffer[1] !== 0x4b) {
        throw makeError(
          'INVALID_INPUT',
          'Input string looks like base64 but does not decode to an xlsx (ZIP "PK") header. ' +
          'Pass { inputEncoding: "base64" } explicitly, or provide a file path / Buffer.'
        );
      }
    }

    if (buffer) {
      const filepath = writeTempFile(buffer);
      return { filepath, cleanup: () => removeTempFile(filepath) };
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
    return { filepath: absolutePath, cleanup: () => {} };
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

  return {
    sheetName: sheetName === undefined ? null : sheetName,
    headerRow,
    skipRows,
    headerMap,
    maxRows,
    maxCols,
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

  const headers = sheetData[headerRow];
  const mappedHeaders = headers.map((header) => headerMap[header] || header);
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

function runNative(filepath, opts) {
  return addon.readExcel(filepath, opts.maxRows, opts.maxCols);
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
 * @param {boolean} [options.includeWarnings=false] - Return { rows, warnings }.
 * @returns {Array<Object>|{rows: Array<Object>, warnings: string[]}}
 */
function readTableAsJSON(input, options = {}) {
  if (input === null || input === undefined || input === '') {
    throw makeError('INVALID_INPUT', 'Input is required (filepath, Buffer, or base64 string)');
  }

  const opts = validateOptions(options);
  const { filepath, cleanup } = prepareFilePath(input, options.inputEncoding);

  try {
    const nativeResult = runNative(filepath, opts);
    return transformToRows(nativeResult, opts);
  } catch (err) {
    if (!err.code) {
      err.code = 'PARSE_ERROR';
    }
    throw err;
  } finally {
    cleanup();
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
  const { filepath, cleanup } = prepareFilePath(input, options.inputEncoding);

  try {
    let nativeResult;
    try {
      nativeResult = await addon.readExcelAsync(filepath, opts.maxRows, opts.maxCols);
    } catch (err) {
      if (!err.code) {
        err.code = 'PARSE_ERROR';
      }
      throw err;
    }
    return transformToRows(nativeResult, opts);
  } finally {
    cleanup();
  }
}

module.exports = {
  readTableAsJSON,
  readTableAsJSONAsync
};
