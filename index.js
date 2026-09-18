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
    onBatch = null,
    batchSize = 50000,
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
  if (onBatch !== null && onBatch !== undefined && typeof onBatch !== 'function') {
    throw makeError('INVALID_OPTIONS', 'options.onBatch must be a function');
  }
  if (!Number.isInteger(batchSize) || batchSize <= 0) {
    throw makeError('INVALID_OPTIONS', `options.batchSize must be a positive integer, got ${batchSize}`);
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
    onBatch,
    batchSize,
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

  // Cells arrive from the native layer as small integers: >= 0 indexes the
  // shared string pool, < 0 references an image (P1-1). Decoding here means
  // repeated values all point at the same V8 string.
  const pool = nativeResult.strings || [];
  const images = nativeResult.images || [];
  const decode = (value) => {
    if (typeof value === 'number') {
      return value < 0 ? images[-value - 1] : pool[value];
    }
    if (Array.isArray(value)) {
      return value.map(decode);
    }
    return value;
  };

  // With column projection the native layer reports the resolved header
  // texts; otherwise the header row itself provides them (also encoded, so
  // decode first).
  const rawHeaders =
    (targetSheet.headers && targetSheet.headers.length) ? targetSheet.headers : sheetData[headerRow];
  const headerSource = rawHeaders.map((header) => {
    const text = typeof header === 'number' ? decode(header) : header;
    return typeof text === 'string' ? text : '';
  });
  const mappedHeaders = headerSource.map((header) => headerMap[header] || header);
  const skipRowsSet = new Set([headerRow, ...skipRows]);

  const rows = [];
  for (let rowIndex = 0; rowIndex < sheetData.length; rowIndex++) {
    if (skipRowsSet.has(rowIndex)) continue;

    const row = sheetData[rowIndex];
    const rowObj = {};
    for (let colIndex = 0; colIndex < mappedHeaders.length; colIndex++) {
      const header = mappedHeaders[colIndex];
      const raw = row[colIndex];
      const value = raw === undefined || raw === null ? '' : decode(raw);
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
function nativeOptions(opts, streaming) {
  const options = {
    sheetName: opts.sheetName === null ? undefined : opts.sheetName,
    headerRow: opts.headerRow,
    maxRows: opts.maxRows,
    maxCols: opts.maxCols,
    includeImages: opts.includeImages,
    columns: opts.columns.length ? opts.columns : undefined
  };
  if (streaming) {
    options.batchSize = opts.batchSize;
  }
  return options;
}

// ---------------------------------------------------------------------------
// Streaming: rows are pushed to `onBatch` in batches instead of accumulating,
// so a million-row sheet never materializes (P2-1).
// ---------------------------------------------------------------------------

function createBatchHandler(opts) {
  const skipRowsSet = new Set([opts.headerRow, ...opts.skipRows]);
  let mappedHeaders = null;
  let rowIndex = 0;     // 0-based index of the first row of the current batch
  let delivered = 0;

  const handleBatch = (batch) => {
    // Whichever batch carries the header row defines the keys.
    if (!mappedHeaders) {
      const local = opts.headerRow - rowIndex;
      if (local >= 0 && local < batch.length) {
        const headers = batch[local].map((value) => (typeof value === 'string' ? value : ''));
        mappedHeaders = headers.map((header) => opts.headerMap[header] || header);
      }
    }

    const rows = [];
    for (let i = 0; i < batch.length; i++) {
      const globalIndex = rowIndex + i;
      if (globalIndex === opts.headerRow || skipRowsSet.has(globalIndex)) continue;
      if (!mappedHeaders) continue; // rows above the header row cannot be keyed

      const row = batch[i];
      const rowObj = {};
      for (let c = 0; c < mappedHeaders.length && c < row.length; c++) {
        const header = mappedHeaders[c];
        if (header) {
          rowObj[header] = row[c];
        }
      }
      rows.push(rowObj);
    }

    const startIndex = rowIndex;
    rowIndex += batch.length;
    if (rows.length > 0) {
      delivered += rows.length;
      opts.onBatch(rows, { startIndex, count: rows.length });
    }
  };

  return {
    handleBatch,
    // Keeps the buffered contract: a header row that never arrived is an
    // out-of-range header row, not silently zero rows.
    finish(warnings) {
      if (!mappedHeaders) {
        throw makeError(
          'HEADER_ROW_OUT_OF_RANGE',
          `Header row index ${opts.headerRow} is out of range (${rowIndex} rows available)`
        );
      }
      return { rowCount: delivered, warnings: warnings || [] };
    }
  };
}

function readStreamedSync(prepared, opts) {
  const handler = createBatchHandler(opts);
  const options = nativeOptions(opts, true);
  const result = prepared.buffer
    ? addon.readExcelBatched(prepared.buffer, options, handler.handleBatch)
    : addon.readExcelBatched(prepared.filepath, options, handler.handleBatch);
  return handler.finish(result.warnings);
}

async function readStreamedAsync(prepared, opts) {
  const handler = createBatchHandler(opts);
  const options = nativeOptions(opts, true);
  const result = await (prepared.buffer
    ? addon.readExcelBatchedAsync(prepared.buffer, options, handler.handleBatch)
    : addon.readExcelBatchedAsync(prepared.filepath, options, handler.handleBatch));
  return handler.finish(result.warnings);
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
// Write: full sheet write
// ---------------------------------------------------------------------------

function copyColumnExtras(target, config, label) {
  if (config.numberFormat !== undefined) {
    if (typeof config.numberFormat !== 'string') {
      throw makeError('INVALID_OPTIONS', `${label}.numberFormat must be a string`);
    }
    target.numberFormat = config.numberFormat;
  }
  if (config.align !== undefined) {
    if (!['left', 'center', 'right'].includes(config.align)) {
      throw makeError('INVALID_OPTIONS', `${label}.align must be "left", "center" or "right"`);
    }
    target.align = config.align;
  }
  if (config.width !== undefined) {
    if (typeof config.width !== 'number' || !(config.width > 0)) {
      throw makeError('INVALID_OPTIONS', `${label}.width must be a positive number`);
    }
    target.width = config.width;
  }
}

/**
 * Normalizes the many accepted `columns` shapes into the flat list of column
 * specs the native writer expects.
 * @private
 */
function normalizeWriteColumns(rows, columns) {
  if (columns === undefined || columns === null) {
    const first = rows[0];
    if (Array.isArray(first)) {
      throw makeError('INVALID_OPTIONS', 'options.columns is required when rows are arrays');
    }
    return Object.keys(first || {}).map((key) => ({ key, header: key }));
  }

  if (Array.isArray(columns)) {
    return columns.map((column, i) => {
      const label = `options.columns[${i}]`;
      if (column === null || typeof column !== 'object' || Array.isArray(column)) {
        throw makeError('INVALID_OPTIONS', `${label} must be an object`);
      }
      const spec = { header: typeof column.header === 'string' ? column.header : '' };
      if (typeof column.key === 'string' || Number.isInteger(column.key)) {
        spec.key = column.key;
      } else if (Number.isInteger(column.index)) {
        spec.index = column.index;
      } else {
        spec.index = i;
      }
      copyColumnExtras(spec, column, label);
      return spec;
    });
  }

  if (typeof columns === 'object') {
    return Object.keys(columns).map((key) => {
      const config = columns[key];
      const label = `options.columns.${key}`;
      if (config === null || typeof config !== 'object' || Array.isArray(config)) {
        throw makeError('INVALID_OPTIONS', `${label} must be an object`);
      }
      const spec = { key, header: typeof config.header === 'string' ? config.header : key };
      copyColumnExtras(spec, config, label);
      return spec;
    });
  }

  throw makeError('INVALID_OPTIONS', 'options.columns must be an object or an array');
}

/**
 * Writes a JSON array into a worksheet, returning the .xlsx bytes (or a
 * summary when `options.output` is given).
 *
 * Numbers are written as numbers, booleans as booleans and `Date` objects as
 * Excel dates, so the result stays computable in Excel instead of turning into
 * text.
 *
 * @param {Array<Object>|Array<Array>} rows - data rows.
 * @param {Object} [options]
 * @param {string} [options.sheetName='Sheet1'] - worksheet name.
 * @param {boolean} [options.includeHeader=true] - write a header row.
 * @param {boolean} [options.freezeHeader=false] - freeze the header row.
 * @param {Object|Array} [options.columns] - column spec: `{ prop: { header,
 *   numberFormat, align, width } }`, or an array of `{ key | index, header, ... }`.
 * @param {string} [options.output] - write the file natively instead of
 *   returning a Buffer; the result is then `{ bytes, rowCount, sheetName }`.
 * @param {number} [options.compression] - 0 (store) .. 9 (max).
 * @returns {Buffer|{bytes: number, rowCount: number, sheetName: string}}
 */
function writeTableAsJSON(rows, options = {}) {
  if (!Array.isArray(rows)) {
    throw makeError('INVALID_OPTIONS', 'rows must be an array');
  }
  if (options === null || typeof options !== 'object' || Array.isArray(options)) {
    throw makeError('INVALID_OPTIONS', 'options must be an object');
  }

  const {
    sheetName,
    includeHeader = true,
    freezeHeader = false,
    output,
    compression,
    columns,
    template
  } = options;

  if (rows.length === 0 && (columns === undefined || columns === null)) {
    throw makeError('INVALID_OPTIONS', 'rows must not be empty unless options.columns is given');
  }
  if (sheetName !== undefined && typeof sheetName !== 'string') {
    throw makeError('INVALID_OPTIONS', 'options.sheetName must be a string');
  }
  if (typeof includeHeader !== 'boolean') {
    throw makeError('INVALID_OPTIONS', 'options.includeHeader must be a boolean');
  }
  if (typeof freezeHeader !== 'boolean') {
    throw makeError('INVALID_OPTIONS', 'options.freezeHeader must be a boolean');
  }
  if (output !== undefined && typeof output !== 'string') {
    throw makeError('INVALID_OPTIONS', 'options.output must be a file path');
  }
  if (template !== undefined && template !== null &&
      typeof template !== 'string' && !Buffer.isBuffer(template)) {
    throw makeError('INVALID_OPTIONS', 'options.template must be a file path or a Buffer');
  }
  validateCompression(compression);

  const specs = normalizeWriteColumns(rows, columns);

  try {
    return addon.writeExcel(rows, {
      sheetName,
      includeHeader,
      freezeHeader,
      output: output === undefined ? undefined : path.resolve(output),
      compression,
      columns: specs,
      template: Buffer.isBuffer(template) || template === undefined || template === null
        ? template
        : path.resolve(template)
    });
  } catch (err) {
    if (!err.code) {
      err.code = 'WRITE_FAILED';
    }
    throw err;
  }
}

function validateCompression(compression) {
  if (compression !== undefined &&
      (!Number.isInteger(compression) || compression < 0 || compression > 9)) {
    throw makeError('INVALID_OPTIONS', 'options.compression must be an integer between 0 and 9');
  }
}

/**
 * Rewrites individual cells of an existing workbook and returns the result.
 *
 * Only the affected worksheets are regenerated; every other part of the package
 * is copied byte for byte, so untouched sheets, images and styles survive
 * exactly as they were. A cell keeps its existing style unless the update
 * supplies a `numberFormat`.
 *
 * @param {Object} spec
 * @param {string|Buffer} spec.template - Workbook to patch.
 * @param {Array<Object>} spec.updates - `{ sheet?, cell, value?, numberFormat? }`
 *   entries; `cell` is an A1 reference ("B7"), `sheet` defaults to the first.
 * @param {string} [spec.output] - Write the file natively instead of returning
 *   a Buffer; the result is then `{ bytes, cells }`.
 * @param {number} [spec.compression] - 0 (store) .. 9 (max).
 * @returns {Buffer|{bytes: number, cells: number}}
 */
function updateCells(spec = {}) {
  if (spec === null || typeof spec !== 'object' || Array.isArray(spec)) {
    throw makeError('INVALID_OPTIONS', 'updateCells expects an options object');
  }

  const { template, updates, output, compression } = spec;

  if (typeof template !== 'string' && !Buffer.isBuffer(template)) {
    throw makeError('INVALID_OPTIONS', 'spec.template is required (file path or Buffer)');
  }
  if (!Array.isArray(updates) || updates.length === 0) {
    throw makeError('INVALID_OPTIONS', 'spec.updates must be a non-empty array');
  }
  for (let i = 0; i < updates.length; i++) {
    const update = updates[i];
    if (update === null || typeof update !== 'object' || Array.isArray(update)) {
      throw makeError('INVALID_OPTIONS', `spec.updates[${i}] must be an object`);
    }
    if (typeof update.cell !== 'string' || update.cell.trim() === '') {
      throw makeError('INVALID_OPTIONS', `spec.updates[${i}].cell must be a reference like "B7"`);
    }
    if (update.sheet !== undefined && typeof update.sheet !== 'string') {
      throw makeError('INVALID_OPTIONS', `spec.updates[${i}].sheet must be a string`);
    }
    if (update.numberFormat !== undefined && typeof update.numberFormat !== 'string') {
      throw makeError('INVALID_OPTIONS', `spec.updates[${i}].numberFormat must be a string`);
    }
  }
  if (output !== undefined && typeof output !== 'string') {
    throw makeError('INVALID_OPTIONS', 'spec.output must be a file path');
  }
  validateCompression(compression);

  try {
    return addon.writeCells({
      template: Buffer.isBuffer(template) ? template : path.resolve(template),
      updates,
      output: output === undefined ? undefined : path.resolve(output),
      compression
    });
  } catch (err) {
    if (!err.code) {
      err.code = 'WRITE_FAILED';
    }
    throw err;
  }
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
    if (opts.onBatch) {
      return readStreamedSync(prepared, opts);
    }
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

  if (opts.onBatch) {
    try {
      return await readStreamedAsync(prepared, opts);
    } catch (err) {
      if (!err.code) {
        err.code = 'PARSE_ERROR';
      }
      throw err;
    }
  }

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
  readTableAsJSONAsync,
  writeTableAsJSON,
  updateCells
};
