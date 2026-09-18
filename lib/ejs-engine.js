'use strict';

/**
 * The ejsExcel-compatible template engine.
 *
 * Cell text may contain `<%...%>` markers that are evaluated as real
 * JavaScript, with the current data, position and helper functions in scope:
 *
 *   <%=expr%>   emit the value as cell text
 *   <%~expr%>   emit the value as a number/Date so the cell's number format
 *               applies (dates become Excel serials)
 *   <%#expr%>   expr evaluates to a formula string ("=SUM(A1,A2)"); pair it
 *               with <%~result%> to also store the cached value, which is what
 *               keeps WPS from showing 0 until the cell is recalculated
 *   <%stmt%>    run for side effects (_mergeCellFn_, _img_, ...)
 *
 * Control markers shape the output rows:
 *
 *   <%forRow item,i in expr%>        the row containing the marker repeats
 *                                    once per item of expr
 *   <%forRBegin item,i in expr%>     the rows through the matching
 *                                    <%forREnd%> repeat per item
 *   <%forCell key in expr%>          the cell containing the marker repeats
 *                                    horizontally, once per item
 *   <%ifCBegin cond%> ... <%ifCEnd%> the rows in between are emitted only
 *                                    when cond is truthy
 *
 * In-scope identifiers: `_data_` (the values the caller passed; index by sheet
 * number when it is an array), `_row`, `_col`, `_rc`, `_charPlus_`,
 * `_charToNum_`, `_mergeCellFn_`, `_dataValidation_`, `_outlineLevel_`,
 * `_img_`, `_qrcode_`, `_str2Xml_` and every loop variable of the enclosing
 * for* marker.
 *
 * The engine never touches zip files itself: the host supplies an IO adapter
 * ({ readParts, build }), which in production is backed by the native addon
 * and in tests by an in-memory zip. Cached template parts are immutable; every
 * edit is written into a fresh part array.
 */

const fs = require('fs');

// ---------------------------------------------------------------------------
// Small XML / reference helpers
// ---------------------------------------------------------------------------

function escapeXml(text) {
  return String(text)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;');
}

function escapeXmlAttribute(text) {
  return escapeXml(text).replace(/"/g, '&quot;');
}

const ENTITY = { amp: '&', lt: '<', gt: '>', quot: '"', apos: "'" };

function unescapeXml(text) {
  return String(text).replace(/&(#x?[0-9a-fA-F]+|[a-zA-Z]+);/g, (all, body) => {
    if (body[0] === '#') {
      const code = body[1] === 'x' || body[1] === 'X'
        ? parseInt(body.slice(2), 16)
        : parseInt(body.slice(1), 10);
      return Number.isFinite(code) ? String.fromCodePoint(code) : all;
    }
    const named = ENTITY[body.toLowerCase()];
    return named === undefined ? all : named;
  });
}

function colToLetters(column) {
  let value = column;
  let out = '';
  while (value > 0) {
    const rem = (value - 1) % 26;
    out = String.fromCharCode(65 + rem) + out;
    value = Math.floor((value - 1) / 26);
  }
  return out;
}

function lettersToCol(letters) {
  let value = 0;
  for (const ch of letters) {
    value = value * 26 + (ch.charCodeAt(0) - 64);
  }
  return value;
}

// `_charPlus_`: "F" + 3 -> "I"
function charPlus(letters, offset) {
  return colToLetters(lettersToCol(String(letters)) + Number(offset));
}

function charToNum(letters) {
  return lettersToCol(String(letters));
}

// Excel serial, 1900 date system, matching the native writer's convention.
function dateToSerial(date) {
  const ms = Date.UTC(
    date.getFullYear(), date.getMonth(), date.getDate(),
    date.getHours(), date.getMinutes(), date.getSeconds(),
    date.getMilliseconds()
  );
  return (ms - Date.UTC(1899, 11, 30)) / 86400000;
}

function isNumericString(value) {
  return typeof value === 'string' && value.trim() !== '' && !Number.isNaN(Number(value));
}

function templateError(message) {
  const e = new Error(message);
  e.code = 'TEMPLATE_ERROR';
  return e;
}

// ---------------------------------------------------------------------------
// Package-level parsing (workbook, relationships, shared strings)
// ---------------------------------------------------------------------------

function partText(parts, name) {
  const part = parts.find((p) => p.name === name);
  return part ? part.data.toString('utf8') : null;
}

// "worksheets/sheet1.xml" (relative to xl/), "/xl/.../x.xml",
// "../drawings/drawing1.xml" (relative to the referencing part's folder).
function resolvePartName(basePart, target) {
  if (!target) return null;
  let clean = target.replace(/\\/g, '/').split('?')[0];
  if (clean.startsWith('/')) return clean.slice(1);
  const baseDir = basePart.includes('/') ? basePart.slice(0, basePart.lastIndexOf('/')) : '';
  const segments = baseDir === '' ? [] : baseDir.split('/');
  for (const segment of clean.split('/')) {
    if (segment === '' || segment === '.') continue;
    if (segment === '..') {
      segments.pop();
      continue;
    }
    segments.push(segment);
  }
  clean = segments.join('/');
  return clean.startsWith('xl/') ? clean : null;
}

function parseRels(xml) {
  const map = new Map();
  if (!xml) return map;
  const re = /<Relationship\b[^>]*>/g;
  let match;
  while ((match = re.exec(xml)) !== null) {
    const id = /\bId="([^"]*)"/.exec(match[0]);
    const target = /\bTarget="([^"]*)"/.exec(match[0]);
    if (id && target) map.set(id[1], target[1]);
  }
  return map;
}

// Worksheet parts in workbook order: [{ name, part }]
function parseSheets(workbookXml, relsXml) {
  const relationships = parseRels(relsXml);
  const sheets = [];
  const re = /<sheet\b[^>]*>/g;
  let match;
  while ((match = re.exec(workbookXml)) !== null) {
    const name = /\bname="([^"]*)"/.exec(match[0]);
    const rid = /\br:id="([^"]*)"/.exec(match[0]);
    if (!name || !rid) continue;
    const target = relationships.get(rid[1]);
    const part = target ? resolvePartName('xl/workbook.xml', target) : null;
    if (part) sheets.push({ name: unescapeXml(name[1]), part });
  }
  return sheets;
}

function parseSharedStrings(xml) {
  const strings = [];
  if (!xml) return strings;
  const items = xml.match(/<si\b[^>]*(?:\/>|>[\s\S]*?<\/si>)/g) || [];
  for (const item of items) {
    let text = '';
    const runs = item.match(/<t\b[^>]*(?:\/>|>[\s\S]*?<\/t>)/g) || [];
    for (const run of runs) {
      if (run.endsWith('/>')) continue;
      const open = run.indexOf('>');
      text += unescapeXml(run.slice(open + 1, run.lastIndexOf('</t>')));
    }
    strings.push(text);
  }
  return strings;
}

// ---------------------------------------------------------------------------
// Sheet XML splitting
// ---------------------------------------------------------------------------

const ROW_RE = /<row\b[^>]*(?:\/>|>[\s\S]*?<\/row>)/g;
const CELL_RE = /<c\b[^>]*(?:\/>|>[\s\S]*?<\/c>)/g;

function tagAttrs(openTag) {
  const attrs = {};
  const re = /([A-Za-z_:][\w:.-]*)="([^"]*)"/g;
  let match;
  while ((match = re.exec(openTag)) !== null) {
    attrs[match[1]] = match[2];
  }
  return attrs;
}

function splitSheet(xml) {
  const openStart = xml.indexOf('<sheetData');
  if (openStart < 0) return { head: xml, rows: [], tail: '', hasSheetData: false };

  const openEnd = xml.indexOf('>', openStart);
  if (openEnd < 0) return { head: xml, rows: [], tail: '', hasSheetData: false };

  if (xml[openEnd - 1] === '/') {
    return { head: xml.slice(0, openStart), rows: [], tail: xml.slice(openEnd + 1), hasSheetData: true };
  }

  const closeStart = xml.indexOf('</sheetData>', openEnd);
  if (closeStart < 0) return { head: xml, rows: [], tail: '', hasSheetData: false };

  const body = xml.slice(openEnd + 1, closeStart);
  const rows = [];
  let match;
  ROW_RE.lastIndex = 0;
  while ((match = ROW_RE.exec(body)) !== null) {
    rows.push(match[0]);
  }
  return { head: xml.slice(0, openStart), rows, tail: xml.slice(closeStart), hasSheetData: true };
}

function parseRow(rowXml) {
  const openEnd = rowXml.indexOf('>');
  const selfClosing = rowXml[openEnd - 1] === '/';
  const openTag = rowXml.slice(0, selfClosing ? openEnd : openEnd + 1);
  const body = selfClosing ? '' : rowXml.slice(openEnd + 1, rowXml.length - 6); // minus </row>
  const attrs = tagAttrs(openTag);

  const cells = [];
  if (!selfClosing) {
    let match;
    CELL_RE.lastIndex = 0;
    while ((match = CELL_RE.exec(body)) !== null) {
      const cellXml = match[0];
      const cellOpenEnd = cellXml.indexOf('>');
      const cellSelfClosing = cellXml[cellOpenEnd - 1] === '/';
      const cellOpenTag = cellXml.slice(0, cellSelfClosing ? cellOpenEnd : cellOpenEnd + 1);
      const ref = /\br="([A-Za-z]+)(\d+)"/.exec(cellOpenTag);
      cells.push({
        xml: cellXml,
        openTag: cellOpenTag,
        selfClosing: cellSelfClosing,
        letters: ref ? ref[1] : '',
        body: cellSelfClosing ? '' : cellXml.slice(cellOpenEnd + 1, cellXml.length - 4),
        attrs: tagAttrs(cellOpenTag)
      });
    }
  }
  return { openTag, selfClosing, attrs, cells };
}

function cellDisplayText(cell, sharedStrings) {
  const t = cell.attrs.t;
  if (t === 's') {
    const v = /<v\b[^>]*>([\s\S]*?)<\/v>/.exec(cell.body);
    if (!v) return '';
    const index = Number(v[1]);
    return Number.isInteger(index) ? (sharedStrings[index] || '') : '';
  }
  if (t === 'inlineStr' || cell.body.includes('<is')) {
    let text = '';
    const runs = cell.body.match(/<t\b[^>]*(?:\/>|>[\s\S]*?<\/t>)/g) || [];
    for (const run of runs) {
      if (run.endsWith('/>')) continue;
      const open = run.indexOf('>');
      text += unescapeXml(run.slice(open + 1, run.lastIndexOf('</t>')));
    }
    return text;
  }
  const v = /<v\b[^>]*>([\s\S]*?)<\/v>/.exec(cell.body);
  return v ? unescapeXml(v[1]) : '';
}

// ---------------------------------------------------------------------------
// Markers
// ---------------------------------------------------------------------------

const MARKER_RE = /<%([=~#]?)([\s\S]*?)%>/g;
const CONTROL_NAMES = ['forRow', 'forRBegin', 'forREnd', 'forCell', 'ifCBegin', 'ifCEnd'];

// -> [{ type: 'lit', s }] | [{ type: 'code', kind, code, control }]
function parseSegments(text) {
  if (!text.includes('<%')) return null;
  const segments = [];
  let last = 0;
  MARKER_RE.lastIndex = 0;
  let match;
  while ((match = MARKER_RE.exec(text)) !== null) {
    if (match.index > last) {
      segments.push({ type: 'lit', s: text.slice(last, match.index) });
    }
    const symbol = match[1];
    const code = match[2];
    let control = null;
    if (symbol === '') {
      const trimmed = code.trim();
      const keyword = CONTROL_NAMES.find((name) => trimmed === name ||
        trimmed.startsWith(name + ' ') || trimmed.startsWith(name + '\t'));
      if (keyword) {
        control = parseControlMarker(keyword, trimmed.slice(keyword.length).trim());
      }
    }
    segments.push({
      type: 'code',
      kind: symbol === '=' ? 'expr' : symbol === '~' ? 'fmt' : symbol === '#' ? 'formula' : 'stmt',
      code,
      control
    });
    last = match.index + match[0].length;
  }
  if (last < text.length) {
    segments.push({ type: 'lit', s: text.slice(last) });
  }
  return segments;
}

function parseControlMarker(name, rest) {
  if (name === 'ifCEnd' || name === 'forREnd') return { name };
  if (name === 'ifCBegin') return { name, expr: rest };

  const m = /^([A-Za-z_$][\w$]*)(?:\s*,\s*([A-Za-z_$][\w$]*))?\s+in\s+([\s\S]+)$/.exec(rest);
  if (!m) {
    throw templateError(`Malformed ${name} marker: expected "${name} item,i in <expression>"`);
  }
  return { name, variable: m[1], indexVar: m[2] || null, expr: m[3] };
}

// ---------------------------------------------------------------------------
// Evaluation
// ---------------------------------------------------------------------------

// `with` gives expressions bare access to _data_, loop variables and helpers,
// exactly like the reference engine.
const evalCache = new Map();

function compileEval(source, isStatement) {
  const key = (isStatement ? 'S:' : 'E:') + source;
  let fn = evalCache.get(key);
  if (!fn) {
    fn = isStatement
      ? new Function('__ctx', `with (__ctx) {\n${source}\n}`)
      : new Function('__ctx', `with (__ctx) {\nreturn (${source});\n}`);
    if (evalCache.size > 20000) evalCache.clear();
    evalCache.set(key, fn);
  }
  return fn;
}

function makeContext(state) {
  return {
    _data_: state.data,
    _row: state.row,
    _col: state.col,
    _rc: state.col + state.row,
    _charPlus_: charPlus,
    _charToNum_: charToNum,
    _str2Xml_: escapeXml,
    _mergeCellFn_: (ref) => state.merges.push(String(ref)),
    _dataValidation_: (o) => {
      if (o && o.sqref) {
        state.validations.push({
          type: o.type || 'list',
          allowBlank: o.allowBlank === undefined ? '1' : String(o.allowBlank),
          showInputMessage: o.showInputMessage === undefined ? '1' : String(o.showInputMessage),
          showErrorMessage: o.showErrorMessage === undefined ? '1' : String(o.showErrorMessage),
          formula1: o.formula1 === undefined ? '' : String(o.formula1),
          sqref: String(o.sqref)
        });
      }
    },
    _outlineLevel_: (n) => {
      state.rowOutline = Math.max(0, Math.min(7, Number(n) || 0));
    },
    _img_: (o) => state.requestImage(o, false),
    _qrcode_: (o) => state.requestImage(o, true),
    __str: (v) => (v === undefined || v === null ? '' : String(v)),
    ...state.scope
  };
}

function describe(state) {
  return `${state.sheet} ${state.col}${state.row}`;
}

function evaluateExpr(expr, state) {
  try {
    return compileEval(expr, false)(makeContext(state));
  } catch (err) {
    if (err.code === 'TEMPLATE_ERROR') throw err;
    const e = templateError(
      `Template expression failed at ${describe(state)} (<%= ${expr} %>): ${err.message}`);
    e.cause = err;
    throw e;
  }
}

// Runs one cell's marker segments; returns { text, value, formula }.
function runCellSegments(segments, state) {
  const result = { text: '', value: undefined, formula: undefined };
  let body = '';
  for (const segment of segments) {
    if (segment.type === 'lit') {
      body += `__r.text+=${JSON.stringify(segment.s)};`;
      continue;
    }
    if (segment.control) continue; // control markers are consumed by the walker
    if (segment.kind === 'expr') body += `__r.text+=__str((${segment.code}));`;
    else if (segment.kind === 'fmt') body += `__r.value=(${segment.code});`;
    else if (segment.kind === 'formula') body += `__r.formula=(${segment.code});`;
    else body += `${segment.code}\n;`; // statement; the ; keeps pieces separated
  }
  if (body === '') return result;

  const fn = new Function('__ctx', '__r', `with (__ctx) {\n${body}\n}`);
  try {
    fn(makeContext(state), result);
  } catch (err) {
    if (err.code === 'TEMPLATE_ERROR') throw err;
    const e = templateError(
      `Template marker failed at ${describe(state)}: ${err.message}`);
    e.cause = err;
    throw e;
  }
  return result;
}

// ---------------------------------------------------------------------------
// Cell / row XML generation
// ---------------------------------------------------------------------------

function numberLike(value) {
  const num = typeof value === 'number' ? value : Number(value);
  return Number.isFinite(num) ? String(num) : '0';
}

// What goes inside the `<c>` element, plus the `t` attribute it needs.
function cellContent(result) {
  if (result.formula !== undefined) {
    let formula = String(result.formula);
    if (formula.startsWith('=')) formula = formula.slice(1);
    let xml = `<f>${escapeXml(formula)}</f>`;
    if (result.value !== undefined) {
      const value = result.value;
      if (value instanceof Date) xml += `<v>${dateToSerial(value)}</v>`;
      else if (typeof value === 'boolean') xml += `<v>${value ? 1 : 0}</v>`;
      else if (typeof value === 'number' || isNumericString(value)) xml += `<v>${numberLike(value)}</v>`;
      else xml += `<v>${escapeXml(String(value))}</v>`;
    }
    return { t: null, content: xml };
  }

  if (result.value !== undefined && result.text === '') {
    const value = result.value;
    if (value instanceof Date) return { t: null, content: `<v>${dateToSerial(value)}</v>` };
    if (typeof value === 'boolean') return { t: 'b', content: `<v>${value ? 1 : 0}</v>` };
    if (typeof value === 'number' || isNumericString(value)) {
      return { t: null, content: `<v>${numberLike(value)}</v>` };
    }
    if (value === null || value === undefined) return { t: null, content: '' };
    return {
      t: 'inlineStr',
      content: `<is><t xml:space="preserve">${escapeXml(String(value))}</t></is>`
    };
  }

  if (result.text !== '') {
    // A numeric result from a pure-expression cell stays a number, matching
    // the reference engine; mixed text/number cells go out as text.
    if (isNumericString(result.text)) {
      return { t: null, content: `<v>${numberLike(result.text)}</v>` };
    }
    return {
      t: 'inlineStr',
      content: `<is><t xml:space="preserve">${escapeXml(result.text)}</t></is>`
    };
  }
  return { t: null, content: '' };
}

function setRef(openTag, letters, rowNo) {
  return /\br="/.test(openTag)
    ? openTag.replace(/\br="[^"]*"/, `r="${letters}${rowNo}"`)
    : openTag.replace(/<c\b/, `<c r="${letters}${rowNo}"`);
}

// Rebuilds a `<c>` with a new reference, `t` attribute and content, keeping
// its style (`s`) and any other attributes.
function rebuildCell(cell, letters, rowNo, content) {
  let open = setRef(dropT(cell.openTag), letters, rowNo);
  if (content.t) {
    open = open.replace(/(\/?>)$/, ` t="${content.t}"$1`);
  }
  if (content.content === '') {
    return open.endsWith('/>') ? open : `${open.replace(/\s*>$/, '')}/>`;
  }
  if (open.endsWith('/>')) {
    return `${open.slice(0, -2)}>${content.content}</c>`;
  }
  return `${open}${content.content}</c>`;
}

function dropT(openTag) {
  return openTag.replace(/\st="[^"]*"/g, '');
}

function verbatimCell(cell, letters, rowNo) {
  const open = setRef(cell.openTag, letters, rowNo);
  if (cell.selfClosing) return open;
  return `${open}${cell.body}</c>`;
}

// ---------------------------------------------------------------------------
// The sheet walker
// ---------------------------------------------------------------------------

function rowControlMarker(row, sharedStrings) {
  for (const cell of row.cells) {
    const segments = parseSegments(cellDisplayText(cell, sharedStrings));
    if (!segments) continue;
    for (const segment of segments) {
      if (segment.type === 'code' && segment.control) {
        return { control: segment.control };
      }
    }
  }
  return null;
}

function findMatching(rows, sharedStrings, from, openName, closeName) {
  let depth = 0;
  for (let i = from; i < rows.length; i++) {
    for (const cell of rows[i].cells) {
      const segments = parseSegments(cellDisplayText(cell, sharedStrings));
      if (!segments) continue;
      for (const segment of segments) {
        if (segment.type !== 'code' || !segment.control) continue;
        if (segment.control.name === openName) depth++;
        else if (segment.control.name === closeName) {
          depth--;
          if (depth === 0) return i;
        }
      }
    }
  }
  return -1;
}

function toItems(value) {
  if (value === undefined || value === null) return [];
  if (Array.isArray(value)) return value;
  if (typeof value === 'object') {
    return Object.keys(value).map((key) => [key, value[key]]);
  }
  return [value];
}

function renderSheet(sheetXml, sheetName, sharedStrings, data, onImage) {
  const { head, rows, tail, hasSheetData } = splitSheet(sheetXml);

  const state = {
    sheet: sheetName,
    data,
    row: 0,
    col: 'A',
    merges: [],
    validations: [],
    images: [],
    rowOutline: null,
    scope: {},
    // _img_ / _qrcode_ land here with the position they were called at; the
    // hook validates and returns the request, which the sheet collects.
    requestImage: (opts, isQrcode) => {
      const request = onImage(opts, isQrcode, state.col, state.row);
      if (request) state.images.push(request);
    }
  };

  const parsedRows = rows.map((rowXml) => parseRow(rowXml));
  const outRows = [];

  // Emits one template row at the next output row number. Control markers are
  // skipped as content; their effects were applied by the walker.
  const emitRow = (parsed) => {
    const outRowNo = ++state.row;
    state.rowOutline = null;

    let colOffset = 0;
    const parts = [];
    for (const cell of parsed.cells) {
      const col = lettersToCol(cell.letters || 'A') + colOffset;
      const letters = colToLetters(col);
      state.col = letters;

      const segments = parseSegments(cellDisplayText(cell, sharedStrings));
      if (!segments) {
        parts.push(verbatimCell(cell, letters, outRowNo));
        continue;
      }

      // forCell: the cell repeats horizontally, one per item.
      const forCell = segments.find((s) => s.type === 'code' && s.control &&
        s.control.name === 'forCell');
      if (forCell) {
        const items = toItems(evaluateExpr(forCell.control.expr, state));
        const rest = segments.filter((s) => s !== forCell);
        const previousScope = state.scope;
        for (let i = 0; i < items.length; i++) {
          state.scope = { ...previousScope, [forCell.control.variable]: items[i] };
          if (forCell.control.indexVar !== null) {
            state.scope[forCell.control.indexVar] = i;
          }
          state.col = colToLetters(col + i);
          const result = runCellSegments(rest, state);
          parts.push(rebuildCell(cell, colToLetters(col + i), outRowNo, cellContent(result)));
        }
        state.scope = previousScope;
        state.col = letters;
        colOffset += Math.max(0, items.length - 1);
        continue;
      }

      const result = runCellSegments(segments, state);
      parts.push(rebuildCell(cell, letters, outRowNo, cellContent(result)));
    }

    let attrs = parsed.openTag.replace(/\s(?:r|spans)="[^"]*"/g, '');
    attrs = /\br="/.test(attrs)
      ? attrs.replace(/\br="[^"]*"/, `r="${outRowNo}"`)
      : attrs.replace(/<row\b/, `<row r="${outRowNo}"`);
    if (state.rowOutline !== null) {
      attrs = attrs.replace(/\soutlineLevel="[^"]*"/g, '');
      attrs = attrs.replace(/<row\b/, `<row outlineLevel="${state.rowOutline}"`);
    }

    if (parsed.selfClosing && parts.length === 0) {
      outRows.push(attrs.replace(/\s*\/>$/, '/>'));
      return;
    }
    const open = attrs.endsWith('/>') ? attrs.slice(0, -2) + '>' : attrs;
    outRows.push(`${open}${parts.join('')}</row>`);
  };

  // The walk: control markers govern the row stream. `suppress` names the
  // control pair of an enclosing block that is being re-walked (a truthy
  // ifCBegin region), so its own markers are not applied twice.
  const walk = (from, to, suppress) => {
    let i = from;
    while (i < to) {
      const parsed = parsedRows[i];
      const ctrl = rowControlMarker(parsed, sharedStrings);

      if (!ctrl || (suppress && (ctrl.control.name === suppress.open ||
          ctrl.control.name === suppress.close))) {
        emitRow(parsed);
        i++;
        continue;
      }

      if (ctrl.control.name === 'forRow') {
        const items = toItems(evaluateExpr(ctrl.control.expr, state));
        const previousScope = state.scope;
        for (let item = 0; item < items.length; item++) {
          state.scope = { ...previousScope, [ctrl.control.variable]: items[item] };
          if (ctrl.control.indexVar !== null) {
            state.scope[ctrl.control.indexVar] = item;
          }
          emitRow(parsed);
        }
        state.scope = previousScope;
        i++;
        continue;
      }

      if (ctrl.control.name === 'forRBegin') {
        const endRow = findMatching(parsedRows, sharedStrings, i, 'forRBegin', 'forREnd');
        if (endRow < 0) {
          throw templateError(
            `Unclosed <%forRBegin%> in sheet "${sheetName}" (no matching <%forREnd%>)`);
        }
        const items = toItems(evaluateExpr(ctrl.control.expr, state));
        const previousScope = state.scope;
        for (let item = 0; item < items.length; item++) {
          state.scope = { ...previousScope, [ctrl.control.variable]: items[item] };
          if (ctrl.control.indexVar !== null) {
            state.scope[ctrl.control.indexVar] = item;
          }
          for (let r = i; r <= endRow; r++) {
            emitRow(parsedRows[r]);
          }
        }
        state.scope = previousScope;
        i = endRow + 1;
        continue;
      }

      if (ctrl.control.name === 'ifCBegin') {
        const endRow = findMatching(parsedRows, sharedStrings, i, 'ifCBegin', 'ifCEnd');
        if (endRow < 0) {
          throw templateError(
            `Unclosed <%ifCBegin%> in sheet "${sheetName}" (no matching <%ifCEnd%>)`);
        }
        if (evaluateExpr(ctrl.control.expr, state)) {
          walk(i, endRow + 1, { open: 'ifCBegin', close: 'ifCEnd' });
        }
        i = endRow + 1;
        continue;
      }

      // A stray forCell/ifCEnd heading a row: emit the row normally.
      emitRow(parsed);
      i++;
    }
  };

  walk(0, parsedRows.length, null);

  let xml = head;
  if (hasSheetData) xml += '<sheetData>';
  xml += outRows.join('');
  if (hasSheetData) xml += tail;
  xml = applyMerges(xml, state.merges);
  xml = applyValidations(xml, state.validations);
  return { xml, images: state.images };
}

function insertBeforeFirst(xml, candidates, fragment) {
  for (const candidate of candidates) {
    const at = xml.indexOf(candidate);
    if (at >= 0) return xml.slice(0, at) + fragment + xml.slice(at);
  }
  return xml + fragment;
}

function applyMerges(xml, merges) {
  if (!merges.length) return xml;
  const items = merges.map((ref) => `<mergeCell ref="${escapeXmlAttribute(ref)}"/>`);
  const close = xml.indexOf('</mergeCells>');
  if (close >= 0) {
    const openStart = xml.lastIndexOf('<mergeCells', close);
    const countMatch = /\bcount="(\d+)"/.exec(xml.slice(openStart, close));
    const count = (countMatch ? Number(countMatch[1]) : 0) + items.length;
    let updated = xml.slice(0, close) + items.join('') + xml.slice(close);
    updated = updated.slice(0, openStart) +
      updated.slice(openStart, close).replace(/\bcount="\d+"/, `count="${count}"`) +
      updated.slice(close);
    return updated;
  }
  const fragment = `<mergeCells count="${items.length}">${items.join('')}</mergeCells>`;
  return insertBeforeFirst(xml,
    ['<phoneticPr', '<pageMargins', '<dataValidations', '<hyperlinks', '</worksheet>'], fragment);
}

function applyValidations(xml, validations) {
  if (!validations.length) return xml;
  const items = validations.map((v) => {
    const attrs = [
      `type="${escapeXmlAttribute(v.type)}"`,
      `allowBlank="${escapeXmlAttribute(v.allowBlank)}"`,
      `showInputMessage="${escapeXmlAttribute(v.showInputMessage)}"`,
      `showErrorMessage="${escapeXmlAttribute(v.showErrorMessage)}"`,
      `sqref="${escapeXmlAttribute(v.sqref)}"`
    ];
    if (v.formula1) attrs.push(`formula1="${escapeXmlAttribute(v.formula1)}"`);
    return `<dataValidation ${attrs.join(' ')}/>`;
  });
  const close = xml.indexOf('</dataValidations>');
  if (close >= 0) {
    return xml.slice(0, close) + items.join('') + xml.slice(close);
  }
  const fragment = `<dataValidations count="${items.length}">${items.join('')}</dataValidations>`;
  return insertBeforeFirst(xml,
    ['<hyperlinks', '<printOptions', '<pageMargins', '</worksheet>'], fragment);
}

// ---------------------------------------------------------------------------
// Images
// ---------------------------------------------------------------------------

const IMAGE_SIGNATURES = [
  { ext: 'png', test: (b) => b[0] === 0x89 && b[1] === 0x50 && b[2] === 0x4e && b[3] === 0x47 },
  { ext: 'jpeg', test: (b) => b[0] === 0xff && b[1] === 0xd8 && b[2] === 0xff },
  { ext: 'gif', test: (b) => b[0] === 0x47 && b[1] === 0x49 && b[2] === 0x46 },
  { ext: 'bmp', test: (b) => b[0] === 0x42 && b[1] === 0x4d }
];

function detectImageExtension(bytes) {
  for (const signature of IMAGE_SIGNATURES) {
    if (signature.test(bytes)) return signature.ext;
  }
  return null;
}

// imgPh accepts an http(s) URL, a data: URI, a local file path, a Buffer or a
// raw base64 string.
async function loadImageBytes(source) {
  if (Buffer.isBuffer(source)) return source;
  if (typeof source !== 'string' || source === '') {
    throw templateError('Image source must be a URL, file path, Buffer or base64 string');
  }
  if (/^https?:\/\//i.test(source)) {
    const response = await fetch(source);
    if (!response.ok) {
      throw templateError(`Downloading image ${source} failed: HTTP ${response.status}`);
    }
    return Buffer.from(await response.arrayBuffer());
  }
  if (/^data:/i.test(source)) {
    const comma = source.indexOf(',');
    if (comma < 0) throw templateError('Malformed data: URI for an image');
    return Buffer.from(source.slice(comma + 1), 'base64');
  }
  if (/^[A-Za-z0-9+/=\s]{100,}$/.test(source)) {
    return Buffer.from(source.replace(/\s/g, ''), 'base64');
  }
  return fs.promises.readFile(source);
}

async function renderQrcode(text, size) {
  let lib = null;
  try {
    lib = require('qrcode');
  } catch (err) {
    throw templateError(
      '_qrcode_ needs the optional package "qrcode"; install it with: npm install qrcode');
  }
  const width = Math.max(32, Math.min(1024, Number(size || 5) * 32));
  return lib.toBuffer(text, { width, margin: 1 });
}

// Adds one sheet's image requests to the package parts. Requires the template
// to already contain a drawing (the same constraint the reference engine
// has): we extend its structure instead of inventing one.
async function attachImages(sheetPart, requests, parts) {
  const sheetRelsName = sheetPart.replace(/([^/]+)$/, '_rels/$1.rels');
  const sheetRelsXml = partText(parts, sheetRelsName);
  if (!sheetRelsXml) {
    throw templateError(
      'Images require the template to contain at least one picture ' +
      `(no drawing relationship found for ${sheetPart})`);
  }

  let drawingPart = null;
  for (const target of parseRels(sheetRelsXml).values()) {
    const resolved = resolvePartName(sheetPart, target);
    if (resolved && /drawings\/drawing\d+\.xml$/.test(resolved)) {
      drawingPart = resolved;
      break;
    }
  }
  if (!drawingPart) {
    throw templateError(
      'Images require the template to contain at least one picture ' +
      `(no drawing part referenced by ${sheetPart})`);
  }

  const drawingXml = partText(parts, drawingPart);
  if (drawingXml === null) {
    throw templateError(`The drawing part ${drawingPart} is missing from the package`);
  }

  const drawingRelsName = drawingPart.replace(/([^/]+)$/, '_rels/$1.rels');
  let relsUpdated = partText(parts, drawingRelsName) ||
    '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>' +
    '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"/>';
  let drawingUpdated = drawingXml;

  let nextId = 1;
  for (const match of drawingXml.matchAll(/\bid="(\d+)"/g)) {
    nextId = Math.max(nextId, Number(match[1]) + 1);
  }
  let nextRel = 1;
  for (const id of parseRels(relsUpdated).keys()) {
    const n = /^rId(\d+)$/.exec(id);
    if (n) nextRel = Math.max(nextRel, Number(n[1]) + 1);
  }
  let nextMedia = 1;
  for (const part of parts) {
    const m = /^xl\/media\/image(\d+)\./.exec(part.name);
    if (m) nextMedia = Math.max(nextMedia, Number(m[1]) + 1);
  }

  for (const request of requests) {
    const bytes = request.qrcode
      ? await renderQrcode(request.source, request.size)
      : await loadImageBytes(request.source);
    const ext = detectImageExtension(bytes);
    if (!ext) {
      throw templateError('Unsupported image format (expected PNG, JPEG, GIF or BMP)');
    }

    const mediaName = `xl/media/image${nextMedia}.${ext}`;
    const relId = `rId${nextRel}`;
    parts.push({ name: mediaName, data: bytes });
    relsUpdated = relsUpdated.replace(
      '</Relationships>',
      `<Relationship Id="${relId}" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/image" Target="../media/image${nextMedia}.${ext}"/></Relationships>`
    );

    // DrawingML anchors are 0-based; the marker cell is the top-left corner
    // and cellNumAdd / rowNumAdd give the span.
    const anchor =
      '<xdr:twoCellAnchor editAs="oneCell">' +
      `<xdr:from><xdr:col>${lettersToCol(request.col) - 1}</xdr:col><xdr:colOff>0</xdr:colOff>` +
      `<xdr:row>${request.row - 1}</xdr:row><xdr:rowOff>0</xdr:rowOff></xdr:from>` +
      `<xdr:to><xdr:col>${lettersToCol(request.col) - 1 + request.cellNumAdd}</xdr:col>` +
      `<xdr:colOff>0</xdr:colOff><xdr:row>${request.row - 1 + request.rowNumAdd}</xdr:row>` +
      '<xdr:rowOff>0</xdr:rowOff></xdr:to>' +
      '<xdr:pic><xdr:nvPicPr>' +
      `<xdr:cNvPr id="${nextId}" name="${escapeXmlAttribute(request.name || `Image ${nextId}`)}"` +
      (request.descr ? ` descr="${escapeXmlAttribute(request.descr)}"` : '') +
      '/><xdr:cNvPicPr><a:picLocks noChangeAspect="1"/></xdr:cNvPicPr></xdr:nvPicPr>' +
      '<xdr:blipFill>' +
      '<a:blip xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships" ' +
      `r:embed="${relId}"/><a:stretch><a:fillRect/></a:stretch></xdr:blipFill>` +
      '<xdr:spPr><a:xfrm><a:off x="0" y="0"/><a:ext cx="0" cy="0"/></a:xfrm>' +
      '<a:prstGeom prst="rect"><a:avLst/></a:prstGeom></xdr:spPr></xdr:pic>' +
      '<xdr:clientData/></xdr:twoCellAnchor>';

    const close = drawingUpdated.lastIndexOf('</xdr:wsDr>');
    if (close < 0) {
      throw templateError(`The drawing part ${drawingPart} has no </xdr:wsDr> to extend`);
    }
    drawingUpdated = drawingUpdated.slice(0, close) + anchor + drawingUpdated.slice(close);

    nextId++;
    nextRel++;
    nextMedia++;
  }

  replacePart(parts, drawingPart, drawingUpdated);
  replacePart(parts, drawingRelsName, relsUpdated);

  // [Content_Types].xml needs a Default entry for every used extension.
  const typesXml = partText(parts, '[Content_Types].xml');
  if (typesXml !== null) {
    let updated = typesXml;
    const extensions = new Set();
    for (const part of parts) {
      const m = /^xl\/media\/image\d+\.(\w+)$/.exec(part.name);
      if (m) extensions.add(m[1]);
    }
    for (const ext of extensions) {
      if (!new RegExp(`<Default Extension="${ext}"`).test(updated)) {
        updated = updated.replace(
          '</Types>',
          `<Default Extension="${ext}" ContentType="image/${ext}"/></Types>`);
      }
    }
    replacePart(parts, '[Content_Types].xml', updated);
  }
}

function replacePart(parts, name, text) {
  const part = parts.find((p) => p.name === name);
  if (part) {
    part.data = Buffer.from(text, 'utf8');
  } else {
    parts.push({ name, data: Buffer.from(text, 'utf8') });
  }
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

// Bounded LRU over the READ parts of a Buffer-backed template (path-backed
// templates are re-read so a rewritten file is picked up immediately).
class PartsCache {
  constructor() {
    this.entries = new Map();
  }

  get(key) {
    const entry = this.entries.get(key);
    if (entry === undefined) return null;
    this.entries.delete(key);
    this.entries.set(key, entry); // promote
    return entry;
  }

  put(key, parts) {
    if (this.entries.size >= 8) {
      this.entries.delete(this.entries.keys().next().value);
    }
    this.entries.set(key, parts);
  }
}

const partsCache = new PartsCache();

function cacheIdentity(source) {
  if (Buffer.isBuffer(source)) {
    let hash = 2166136261;
    for (const byte of source) {
      hash = ((hash ^ byte) * 16777619) >>> 0;
    }
    return `bytes:${source.length}:${hash}`;
  }
  return null;
}

/**
 * Renders the `<%...%>` markers of a workbook.
 *
 * @param {string|Buffer} source template path or bytes
 * @param {Object|Array} values `_data_` for the expressions; index by sheet
 *   number (workbook order) when it is an array
 * @param {Object} options { sheetName?, compression? }
 * @param {Object} io { readParts(source) -> Promise<[{name, data}]>,
 *                      build(parts, compression) -> Promise<Buffer> }
 * @returns {Promise<{buffer: Buffer, sheets: string[]}>}
 */
async function renderEjsTemplate(source, values, options, io) {
  if (values === null || typeof values !== 'object' || typeof values === 'boolean' ||
      typeof values === 'number' || typeof values === 'string') {
    throw templateError('renderTemplate values must be an object or an array');
  }
  const only = options.sheetName || null;

  let parts;
  if (typeof source === 'string') {
    parts = await io.readParts(source);
  } else {
    const identity = cacheIdentity(source);
    parts = identity ? partsCache.get(identity) : null;
    if (!parts) {
      parts = await io.readParts(source);
      if (identity) partsCache.put(identity, parts);
    }
  }

  // WPS writes backslash entry names; normalize so lookups below are uniform.
  const normalized = parts.map((p) => ({ name: String(p.name).replace(/\\/g, '/'), data: p.data }));

  const workbookXml = partText(normalized, 'xl/workbook.xml');
  const relsXml = partText(normalized, 'xl/_rels/workbook.xml.rels');
  if (workbookXml === null || relsXml === null) {
    throw templateError('The template is not a valid workbook (workbook.xml or its rels missing)');
  }
  const sheets = parseSheets(workbookXml, relsXml);
  const sharedStrings = parseSharedStrings(partText(normalized, 'xl/sharedStrings.xml'));

  const selected = only ? sheets.filter((s) => s.name === only) : sheets;
  if (only && selected.length === 0) {
    const e = new Error(`Sheet "${only}" not found in the template`);
    e.code = 'SHEET_NOT_FOUND';
    throw e;
  }

  const outParts = normalized.map((p) => ({ name: p.name, data: p.data }));
  const rendered = [];

  for (const sheet of selected) {
    const xml = partText(outParts, sheet.part);
    if (xml === null) {
      throw templateError(`The worksheet part ${sheet.part} is missing from the package`);
    }
    // In a real package the markers are XML-escaped inside <t> (&lt;%...%&gt;);
    // the engine compares against both spellings before deciding to skip.
    if (!xml.includes('<%') && !xml.includes('&lt;%')) continue;

    // Every sheet evaluates against the WHOLE values object; the template
    // picks its slice with _data_[i] (ejsExcel semantics).
    const result = renderSheet(xml, sheet.name, sharedStrings, values, collectImage);
    replacePart(outParts, sheet.part, result.xml);

    if (result.images.length) {
      await attachImages(sheet.part, result.images, outParts);
    }
    rendered.push(sheet.name);
  }

  const buffer = await io.build(outParts, options.compression);
  return { buffer, sheets: rendered };
}

// Deferred image request: recorded synchronously at the marker (so col/row
// are the emitted position), downloaded/generated after the walk.
function collectImage(opts, isQrcode, col, row) {
  const source = isQrcode ? (opts && opts.text) : (opts && (opts.imgPh || opts.image || opts.src));
  if (!source) {
    throw templateError(
      isQrcode
        ? '_qrcode_ expects { text, size?, cellNumAdd?, rowNumAdd? }'
        : '_img_ expects { imgPh, cellNumAdd?, rowNumAdd? } where imgPh is a URL, file path, Buffer or base64'
    );
  }
  return {
    source,
    qrcode: isQrcode,
    size: isQrcode ? Number(opts.size || 5) : undefined,
    cellNumAdd: Math.max(1, Number(opts.cellNumAdd || (isQrcode ? 3 : 2))),
    rowNumAdd: Math.max(1, Number(opts.rowNumAdd || (isQrcode ? 3 : 2))),
    name: opts.cNvPrName,
    descr: opts.cNvPrDescr,
    col,
    row
  };
}

module.exports = {
  renderEjsTemplate,
  // exposed for tests
  parseSharedStrings,
  parseSheets,
  resolvePartName,
  splitSheet,
  colToLetters,
  lettersToCol,
  charPlus,
  dateToSerial
};
