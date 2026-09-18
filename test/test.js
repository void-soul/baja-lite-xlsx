'use strict';

/**
 * Minimal regression suite for baja-lite-xlsx
 * (AUDIT-20260917-011: the package previously had zero tests).
 *
 * Run with: npm test   (requires the native addon to be built first,
 * i.e. "npm run build" with VCPKG_ROOT set, or a downloaded prebuild).
 */

const assert = require('assert');
const fs = require('fs');
const path = require('path');

const {
  readTableAsJSON,
  readTableAsJSONAsync,
  writeTableAsJSON,
  writeTableAsJSONAsync,
  updateCells,
  updateCellsAsync,
  renderTemplate,
  renderTemplateAsync
} = require('..');

const fixturesDir = path.join(__dirname, '..', 'examples');
const FIXTURES = {
  sample: path.join(fixturesDir, 'sample.xlsx'),
  sample2: path.join(fixturesDir, 'sample2.xlsx'),
  wps: path.join(fixturesDir, 'test-wps.xlsx')
};

let passed = 0;
let skipped = 0;

function test(name, fixtureKey, fn) {
  const fixture = fixtureKey ? FIXTURES[fixtureKey] : null;
  if (fixtureKey && !fs.existsSync(fixture)) {
    console.log(`SKIP ${name} (missing fixture: ${path.basename(fixture)})`);
    skipped++;
    return;
  }
  try {
    fn(fixture);
    passed++;
    console.log(`PASS ${name}`);
  } catch (err) {
    console.error(`FAIL ${name}`);
    console.error(err && err.stack ? err.stack : err);
    process.exitCode = 1;
  }
}

function testAsync(name, fixtureKey, fn) {
  const fixture = fixtureKey ? FIXTURES[fixtureKey] : null;
  if (fixtureKey && !fs.existsSync(fixture)) {
    console.log(`SKIP ${name} (missing fixture: ${path.basename(fixture)})`);
    skipped++;
    return Promise.resolve();
  }
  return fn(fixture).then(
    () => {
      passed++;
      console.log(`PASS ${name}`);
    },
    (err) => {
      console.error(`FAIL ${name}`);
      console.error(err && err.stack ? err.stack : err);
      process.exitCode = 1;
    }
  );
}

// ---------------------------------------------------------------------------
// Input validation & error codes
// ---------------------------------------------------------------------------

test('missing file -> FILE_NOT_FOUND', null, () => {
  assert.throws(
    () => readTableAsJSON('definitely-not-here-12345.xlsx'),
    (err) => err.code === 'FILE_NOT_FOUND'
  );
});

test('invalid input -> INVALID_INPUT', null, () => {
  assert.throws(
    () => readTableAsJSON(42),
    (err) => err.code === 'INVALID_INPUT'
  );
  assert.throws(
    () => readTableAsJSON(null),
    (err) => err.code === 'INVALID_INPUT'
  );
});

test('negative headerRow -> INVALID_OPTIONS', 'sample', () => {
  assert.throws(
    () => readTableAsJSON(FIXTURES.sample, { headerRow: -1 }),
    (err) => err.code === 'INVALID_OPTIONS'
  );
});

test('non-integer headerRow -> INVALID_OPTIONS', 'sample', () => {
  assert.throws(
    () => readTableAsJSON(FIXTURES.sample, { headerRow: 1.5 }),
    (err) => err.code === 'INVALID_OPTIONS'
  );
});

test('bad skipRows -> INVALID_OPTIONS', 'sample', () => {
  assert.throws(
    () => readTableAsJSON(FIXTURES.sample, { skipRows: ['x'] }),
    (err) => err.code === 'INVALID_OPTIONS'
  );
});

// ---------------------------------------------------------------------------
// Basic reading
// ---------------------------------------------------------------------------

test('returns array of row objects', 'sample', (fixture) => {
  const rows = readTableAsJSON(fixture);
  assert.ok(Array.isArray(rows), 'rows should be an array');
  assert.ok(rows.length > 0, 'rows should not be empty');
  assert.equal(typeof rows[0], 'object');
});

test('sheetName option works', 'sample', (fixture) => {
  const names = new Set();
  // Try reading with a wrong name first.
  assert.throws(
    () => readTableAsJSON(fixture, { sheetName: '__NOPE__' }),
    (err) => err.code === 'SHEET_NOT_FOUND'
  );
  void names;
});

test('maxRows caps the output', 'sample', (fixture) => {
  const all = readTableAsJSON(fixture);
  const capped = readTableAsJSON(fixture, { maxRows: 2 });
  assert.ok(capped.length <= 2, `expected <= 2 rows, got ${capped.length}`);
  assert.ok(all.length >= capped.length);
});

test('maxCols caps each row', 'sample', (fixture) => {
  const capped = readTableAsJSON(fixture, { maxCols: 1 });
  for (const row of capped) {
    assert.ok(Object.keys(row).length <= 1);
  }
});

test('headerRow beyond data -> HEADER_ROW_OUT_OF_RANGE', 'sample', (fixture) => {
  assert.throws(
    () => readTableAsJSON(FIXTURES.sample, { headerRow: 999999 }),
    (err) => err.code === 'HEADER_ROW_OUT_OF_RANGE'
  );
});

test('skipRows removes rows', 'sample', (fixture) => {
  const rows = readTableAsJSON(fixture, { skipRows: [1] });
  const rows2 = readTableAsJSON(fixture, { skipRows: [1, 2] });
  assert.equal(rows.length, rows2.length + 1);
});

test('headerMap renames columns', 'sample', (fixture) => {
  const rows = readTableAsJSON(fixture);
  const first = rows[0] || {};
  const keys = Object.keys(first);
  if (keys.length === 0) return; // nothing to map
  const renamed = readTableAsJSON(fixture, {
    headerMap: { [keys[0]]: '__renamed__' }
  });
  assert.ok('__renamed__' in (renamed[0] || {}));
});

// ---------------------------------------------------------------------------
// Column projection (P1-2) and image opt-out (P0-3)
// ---------------------------------------------------------------------------

test('columns projection keeps only the requested column', 'sample', (fixture) => {
  const all = readTableAsJSON(fixture);
  const keys = Object.keys(all[0] || {});
  if (keys.length === 0) return; // no headers to project on
  const projected = readTableAsJSON(fixture, { columns: [keys[0]] });
  assert.equal(projected.length, all.length, 'row count must not change');
  for (const row of projected) {
    assert.ok(Object.keys(row).length <= 1, 'only one column expected');
    assert.ok(keys[0] in row, `expected key "${keys[0]}"`);
  }
});

test('columns projection accepts Excel references', 'sample', (fixture) => {
  const projected = readTableAsJSON(fixture, { columns: ['A'] });
  assert.ok(Array.isArray(projected));
  for (const row of projected) {
    assert.ok(Object.keys(row).length <= 1);
  }
});

test('unknown column -> INVALID_OPTIONS', 'sample', (fixture) => {
  assert.throws(
    () => readTableAsJSON(fixture, { columns: ['__NO_SUCH_COLUMN__'] }),
    (err) => err.code === 'INVALID_OPTIONS'
  );
});

test('bad columns type -> INVALID_OPTIONS', 'sample', (fixture) => {
  assert.throws(
    () => readTableAsJSON(fixture, { columns: 'A' }),
    (err) => err.code === 'INVALID_OPTIONS'
  );
});

test('includeImages false still returns every row', 'sample', (fixture) => {
  const withoutImages = readTableAsJSON(fixture, { includeImages: false });
  const withImages = readTableAsJSON(fixture);
  assert.equal(withoutImages.length, withImages.length);
});

test('bad includeImages -> INVALID_OPTIONS', 'sample', (fixture) => {
  assert.throws(
    () => readTableAsJSON(fixture, { includeImages: 'no' }),
    (err) => err.code === 'INVALID_OPTIONS'
  );
});

// ---------------------------------------------------------------------------
// Write: full sheet write (mode 1)
// ---------------------------------------------------------------------------

const outputDir = path.join(__dirname, 'output');

function ensureOutputDir() {
  fs.mkdirSync(outputDir, { recursive: true });
}

test('writeTableAsJSON returns a Buffer that reads back', null, () => {
  const rows = [
    { name: 'Ada', amount: 1200, active: true },
    { name: 'Alan', amount: 980.5, active: false }
  ];
  const buffer = writeTableAsJSON(rows, { sheetName: 'Data' });
  assert.ok(Buffer.isBuffer(buffer), 'expected a Buffer');
  // Numbers stay numbers (no "1200.0"), booleans keep their Excel type.
  assert.deepEqual(readTableAsJSON(buffer), [
    { name: 'Ada', amount: '1200', active: 'true' },
    { name: 'Alan', amount: '980.5', active: 'false' }
  ]);
});

test('column config drives header text, order and number formats', null, () => {
  const rows = [{ amount: 1234.5, name: 'Ada' }];
  const buffer = writeTableAsJSON(rows, {
    sheetName: '报表',
    columns: {
      name: { header: '姓名', width: 16 },
      amount: { header: '金额', numberFormat: '#,##0.00' }
    }
  });

  assert.deepEqual(readTableAsJSON(buffer), [{ 姓名: 'Ada', 金额: '1234.5' }]);
  assert.equal(readTableAsJSON(buffer, { sheetName: '报表' }).length, 1);
  assert.throws(
    () => readTableAsJSON(buffer, { sheetName: 'nope' }),
    (err) => err.code === 'SHEET_NOT_FOUND'
  );
});

test('writeTableAsJSON writes to a file and reports bytes', null, () => {
  ensureOutputDir();
  const target = path.join(outputDir, 'write-basic.xlsx');
  if (fs.existsSync(target)) fs.unlinkSync(target);

  const summary = writeTableAsJSON([{ a: 1, b: 'x' }], { output: target });
  assert.ok(fs.existsSync(target), 'output file must exist');
  assert.ok(summary.bytes > 0);
  assert.equal(summary.rowCount, 1);
  assert.deepEqual(readTableAsJSON(target), [{ a: '1', b: 'x' }]);
});

test('array rows with an explicit column list', null, () => {
  const buffer = writeTableAsJSON([[1, 'a'], [2, 'b']], {
    columns: [{ header: 'num' }, { header: 'text' }]
  });
  assert.deepEqual(readTableAsJSON(buffer), [
    { num: '1', text: 'a' },
    { num: '2', text: 'b' }
  ]);
});

test('includeHeader false leaves only data rows', null, () => {
  const buffer = writeTableAsJSON([[1, 2], [3, 4]], {
    columns: [{ header: 'A' }, { header: 'B' }],
    includeHeader: false
  });
  // The reader consumes row 0 as its header, so one row must remain.
  assert.equal(readTableAsJSON(buffer).length, 1);
});

test('Date values become real Excel dates', null, () => {
  const buffer = writeTableAsJSON([{ when: new Date(2026, 0, 15) }], {
    columns: { when: { header: 'when', numberFormat: 'yyyy-mm-dd' } }
  });
  assert.deepEqual(readTableAsJSON(buffer), [{ when: '2026-01-15' }]);
});

test('bad write specs -> INVALID_OPTIONS', null, () => {
  assert.throws(() => writeTableAsJSON('nope', {}), (err) => err.code === 'INVALID_OPTIONS');
  assert.throws(() => writeTableAsJSON([], {}), (err) => err.code === 'INVALID_OPTIONS');
  assert.throws(() => writeTableAsJSON([{ a: 1 }], { columns: 5 }), (err) => err.code === 'INVALID_OPTIONS');
  assert.throws(() => writeTableAsJSON([{ a: 1 }], { sheetName: 7 }), (err) => err.code === 'INVALID_OPTIONS');
});

// ---------------------------------------------------------------------------
// Write: template workbook (mode 1b) and cell patching (mode 2)
// ---------------------------------------------------------------------------

test('writeTableAsJSON with a template replaces the sheet data', 'sample', (fixture) => {
  const buffer = writeTableAsJSON([{ alpha: 'x', beta: 42 }], { template: fixture });
  assert.ok(Buffer.isBuffer(buffer));
  assert.deepEqual(readTableAsJSON(buffer), [{ alpha: 'x', beta: '42' }]);
});

test('a Buffer works as the template', 'sample', (fixture) => {
  const bytes = fs.readFileSync(fixture);
  const buffer = writeTableAsJSON([{ a: 1 }], { template: bytes });
  assert.deepEqual(readTableAsJSON(buffer), [{ a: '1' }]);
});

test('updateCells rewrites one cell and leaves the rest alone', 'sample', (fixture) => {
  const before = readTableAsJSON(fixture);
  assert.ok(before.length > 0, 'fixture needs at least one data row');
  const keys = Object.keys(before[0]);
  assert.ok(keys.length > 0, 'fixture needs at least one column');

  const buffer = updateCells({ template: fixture, updates: [{ cell: 'A2', value: 'REPLACED' }] });
  const after = readTableAsJSON(buffer);

  assert.equal(after.length, before.length, 'row count must not change');
  assert.equal(after[0][keys[0]], 'REPLACED');
  if (keys.length > 1) {
    assert.equal(after[0][keys[1]], before[0][keys[1]], 'other cells must stay untouched');
  }
});

test('updateCells writes numbers as numbers', 'sample', (fixture) => {
  const buffer = updateCells({ template: fixture, updates: [{ cell: 'A2', value: 1234.5 }] });
  const after = readTableAsJSON(buffer);
  assert.equal(after[0][Object.keys(after[0])[0]], '1234.5');
});

test('updateCells applies a number format when asked', 'sample', (fixture) => {
  const buffer = updateCells({
    template: fixture,
    updates: [{ cell: 'A2', value: 0.25, numberFormat: '0.00%' }]
  });
  const after = readTableAsJSON(buffer);
  assert.equal(after[0][Object.keys(after[0])[0]], '0.25');
});

test('updateCells writes a date with an automatic format', 'sample', (fixture) => {
  const buffer = updateCells({
    template: fixture,
    updates: [{ cell: 'A2', value: new Date(2026, 0, 15) }]
  });
  const after = readTableAsJSON(buffer);
  assert.equal(after[0][Object.keys(after[0])[0]], '2026-01-15');
});

test('updateCells can clear a cell', 'sample', (fixture) => {
  const buffer = updateCells({ template: fixture, updates: [{ cell: 'A2', value: null }] });
  const after = readTableAsJSON(buffer);
  assert.equal(after[0][Object.keys(after[0])[0]], '');
});

test('updateCells writes to a file when asked', 'sample', (fixture) => {
  ensureOutputDir();
  const target = path.join(outputDir, 'patched.xlsx');
  if (fs.existsSync(target)) fs.unlinkSync(target);
  const summary = updateCells({
    template: fixture,
    updates: [{ cell: 'A2', value: 'FILE' }],
    output: target
  });
  assert.ok(fs.existsSync(target));
  assert.equal(summary.cells, 1);
  assert.ok(summary.bytes > 0);
  assert.equal(readTableAsJSON(target)[0][Object.keys(readTableAsJSON(target)[0])[0]], 'FILE');
});

test('bad update specs -> INVALID_OPTIONS', null, () => {
  assert.throws(
    () => updateCells({ updates: [{ cell: 'A1', value: 1 }] }),
    (err) => err.code === 'INVALID_OPTIONS'
  );
  assert.throws(
    () => updateCells({ template: 'x.xlsx', updates: [] }),
    (err) => err.code === 'INVALID_OPTIONS'
  );
  assert.throws(
    () => updateCells({ template: 'x.xlsx', updates: [{ value: 1 }] }),
    (err) => err.code === 'INVALID_OPTIONS'
  );
});

test('a missing template reports a coded error', null, () => {
  assert.throws(
    () => updateCells({ template: 'definitely-not-here-77.xlsx', updates: [{ cell: 'A1', value: 1 }] }),
    (err) => err.code === 'FILE_OPEN_FAILED' || err.code === 'FILE_NOT_FOUND'
  );
  assert.throws(
    () => writeTableAsJSON([{ a: 1 }], { template: 'definitely-not-here-88.xlsx' }),
    (err) => err.code === 'FILE_OPEN_FAILED' || err.code === 'FILE_NOT_FOUND'
  );
});

// ---------------------------------------------------------------------------
// Write: template rendering (mode 3)
// ---------------------------------------------------------------------------

let templateCounter = 0;

// Templates are built with our own writer, so the tests exercise the full
// round trip: write a template, render it, read the result back.
function makeTemplate(rows, columns) {
  ensureOutputDir();
  templateCounter += 1;
  const target = path.join(outputDir, `template-${templateCounter}.xlsx`);
  writeTableAsJSON(rows, { columns, includeHeader: false, output: target });
  return target;
}

test('renderTemplate fills placeholders', null, () => {
  const template = makeTemplate(
    [['label', 'value'], ['Report ${title}', '${total}']],
    [{}, {}]
  );
  const buffer = renderTemplate({ title: 'Q1', total: 42 }, { template });
  assert.deepEqual(readTableAsJSON(buffer), [{ label: 'Report Q1', value: '42' }]);
});

test('renderTemplate repeats rows for {{#each}}', null, () => {
  const template = makeTemplate(
    [
      ['name', 'amount'],
      ['{{#each items}}', ''],
      ['${name}', '${amount}'],
      ['{{/each}}', '']
    ],
    [{}, {}]
  );
  const buffer = renderTemplate(
    {
      items: [
        { name: 'a', amount: 1 },
        { name: 'b', amount: 2 },
        { name: 'c', amount: 3 }
      ]
    },
    { template }
  );
  assert.deepEqual(readTableAsJSON(buffer), [
    { name: 'a', amount: '1' },
    { name: 'b', amount: '2' },
    { name: 'c', amount: '3' }
  ]);
});

test('renderTemplate supports @index and shorthand ${.}', null, () => {
  const template = makeTemplate(
    [['no', 'name'], ['{{#each items}}', ''], ['${@index}', '${.}'], ['{{/each}}', '']],
    [{}, {}]
  );
  const filled = renderTemplate({ items: ['x', 'y'] }, { template });
  assert.deepEqual(readTableAsJSON(filled), [{ no: '0', name: 'x' }, { no: '1', name: 'y' }]);

  const empty = renderTemplate({ items: [] }, { template });
  assert.deepEqual(readTableAsJSON(empty), []);
});

test('renderTemplate keeps the template number format for dates', null, () => {
  const template = makeTemplate([['when'], ['${when}']], [{ numberFormat: 'yyyy-mm-dd' }]);
  const buffer = renderTemplate({ when: new Date(2026, 0, 15) }, { template });
  assert.deepEqual(readTableAsJSON(buffer), [{ when: '2026-01-15' }]);
});

test('renderTemplate steps out of a loop with ../', null, () => {
  const template = makeTemplate(
    [
      ['title', 'name'],
      ['{{#each items}}', ''],
      ['${../title}', '${name}'],
      ['{{/each}}', '']
    ],
    [{}, {}]
  );
  const buffer = renderTemplate({ title: 'T', items: [{ name: 'a' }] }, { template });
  assert.deepEqual(readTableAsJSON(buffer), [{ title: 'T', name: 'a' }]);
});

test('unknown placeholder -> TEMPLATE_ERROR, strict false -> empty', null, () => {
  const template = makeTemplate([['a'], ['${nope}']], [{}]);
  assert.throws(
    () => renderTemplate({ other: 1 }, { template }),
    (err) => err.code === 'TEMPLATE_ERROR'
  );
  const lenient = renderTemplate({ other: 1 }, { template, strict: false });
  assert.deepEqual(readTableAsJSON(lenient), [{ a: '' }]);
});

test('an unclosed {{#each}} -> TEMPLATE_ERROR', null, () => {
  const template = makeTemplate([['a'], ['{{#each items}}'], ['${.}']], [{}]);
  assert.throws(
    () => renderTemplate({ items: ['x'] }, { template }),
    (err) => err.code === 'TEMPLATE_ERROR'
  );
});

test('renderTemplate writes to a file when asked', null, () => {
  const template = makeTemplate([['a'], ['${v}']], [{}]);
  ensureOutputDir();
  const target = path.join(outputDir, 'rendered.xlsx');
  if (fs.existsSync(target)) fs.unlinkSync(target);
  const summary = renderTemplate({ v: 'ok' }, { template, output: target });
  assert.ok(fs.existsSync(target));
  assert.ok(summary.bytes > 0);
  assert.ok(Array.isArray(summary.sheets));
  assert.deepEqual(readTableAsJSON(target), [{ a: 'ok' }]);
});

test('cache: true renders exactly what the uncached path renders', null, () => {
  const template = makeTemplate(
    [
      ['name', 'amount'],
      ['{{#each items}}', ''],
      ['${name}', '${amount}'],
      ['{{/each}}', '']
    ],
    [{}, {}]
  );
  const values = { items: [{ name: 'a', amount: 1 }, { name: 'b', amount: 2 }] };

  const plain = renderTemplate(values, { template });
  const firstCached = renderTemplate(values, { template, cache: true });   // fills the cache
  const secondCached = renderTemplate(values, { template, cache: true });  // served from it

  assert.ok(plain.equals(firstCached), 'cached bytes must match the uncached render');
  assert.ok(firstCached.equals(secondCached), 'cache hits must stay identical');
  assert.deepEqual(readTableAsJSON(secondCached), [
    { name: 'a', amount: '1' },
    { name: 'b', amount: '2' }
  ]);
});

test('the cache drops an entry when the template is rewritten', null, () => {
  ensureOutputDir();
  const target = path.join(outputDir, 'cache-invalidate.xlsx');

  writeTableAsJSON([['label'], ['A ${v}']], {
    columns: [{}], includeHeader: false, output: target
  });
  const first = readTableAsJSON(renderTemplate({ v: 'x' }, { template: target, cache: true }));
  assert.deepEqual(first, [{ label: 'A x' }]);

  // Same path, different layout: the identity (size + mtime) changes, so the
  // stale entry must not be reused.
  writeTableAsJSON([['other'], ['B ${v}'], ['C']], {
    columns: [{}], includeHeader: false, output: target
  });
  const second = readTableAsJSON(renderTemplate({ v: 'y' }, { template: target, cache: true }));
  assert.deepEqual(second, [{ other: 'B y' }, { other: 'C' }]);
});

// Excel stores cell text in sharedStrings and references it by index, so a
// template authored in Excel has no marker in its sheet XML at all. This fixture
// was produced by docs/local/cachecheck.cpp from a package our own writer made
// (see that file), which keeps it loadable by xlnt as well.
const SHARED_TEMPLATE = path.join(__dirname, 'fixtures', 'template-shared-strings.xlsx');

test('renderTemplate handles markers stored in sharedStrings', null, () => {
  const buffer = renderTemplate(
    {
      customer: 'ACME & Co',
      items: [{ name: 'Widget', qty: 3 }, { name: 'Gadget', qty: 5 }]
    },
    { template: SHARED_TEMPLATE }
  );

  assert.deepEqual(readTableAsJSON(buffer, { headerRow: 0 }), [
    { Item: 'Invoice for ACME & Co', Qty: '' },
    { Item: 'Widget', Qty: '3' },
    { Item: 'Gadget', Qty: '5' }
  ]);
});

test('cache: true also works for shared-string templates', null, () => {
  const values = { customer: 'Cached', items: [{ name: 'only', qty: 7 }] };
  const plain = renderTemplate(values, { template: SHARED_TEMPLATE });
  const firstCached = renderTemplate(values, { template: SHARED_TEMPLATE, cache: true });
  const secondCached = renderTemplate(values, { template: SHARED_TEMPLATE, cache: true });

  assert.ok(plain.equals(firstCached), 'cache miss must match the uncached render');
  assert.ok(firstCached.equals(secondCached), 'cache hit must match too');

  const rows = readTableAsJSON(secondCached, { headerRow: 0 });
  assert.equal(rows[0].Item, 'Invoice for Cached');
  assert.deepEqual(rows[1], { Item: 'only', Qty: '7' });
});

test('bad cache option -> INVALID_OPTIONS', null, () => {
  const template = makeTemplate([['a'], ['${v}']], [{}]);
  assert.throws(
    () => renderTemplate({ v: 1 }, { template, cache: 'yes' }),
    (err) => err.code === 'INVALID_OPTIONS'
  );
});

test('bad render specs -> INVALID_OPTIONS', null, () => {
  assert.throws(() => renderTemplate('nope', {}), (err) => err.code === 'INVALID_OPTIONS');
  assert.throws(() => renderTemplate({}, {}), (err) => err.code === 'INVALID_OPTIONS');
  assert.throws(
    () => renderTemplate({ a: [{ 'bad.key': 1 }] }, { template: 'x.xlsx' }),
    (err) => err.code === 'INVALID_OPTIONS'
  );
});

// ---------------------------------------------------------------------------
// Write: asynchronous twins (rows are snapshotted, the work runs off-thread)
// ---------------------------------------------------------------------------

testAsync('writeTableAsJSONAsync matches the sync result', null, async () => {
  const rows = [
    { name: 'a', amount: 1, when: new Date(2026, 0, 15) },
    { name: 'b', amount: 2.5, when: new Date(2026, 1, 1) }
  ];
  const options = {
    sheetName: 'Async',
    columns: {
      name: {},
      amount: { numberFormat: '#,##0.00' },
      when: { numberFormat: 'yyyy-mm-dd' }
    }
  };
  const sync = writeTableAsJSON(rows, options);
  const asynchronous = await writeTableAsJSONAsync(rows, options);
  assert.ok(Buffer.isBuffer(asynchronous), 'expected a Buffer');
  assert.ok(sync.equals(asynchronous), 'async bytes must equal sync bytes');
});

testAsync('writeTableAsJSONAsync writes the file it is asked to', null, async () => {
  ensureOutputDir();
  const target = path.join(outputDir, 'async-write.xlsx');
  if (fs.existsSync(target)) fs.unlinkSync(target);

  const summary = await writeTableAsJSONAsync([{ a: 1, b: 'x' }], { output: target });
  assert.ok(fs.existsSync(target));
  assert.equal(summary.rowCount, 1);
  assert.equal(summary.sheetName, 'Sheet1');
  assert.deepEqual(readTableAsJSON(target), [{ a: '1', b: 'x' }]);
});

testAsync('writeTableAsJSONAsync rejects with a coded error', null, async () => {
  await assert.rejects(
    () => writeTableAsJSONAsync([{ a: 1 }], { template: 'no-such-file.xlsx' }),
    (err) => err.code === 'FILE_OPEN_FAILED'
  );
  await assert.rejects(
    () => writeTableAsJSONAsync('nope', {}),
    (err) => err.code === 'INVALID_OPTIONS'
  );
});

testAsync('updateCellsAsync matches the sync result', null, async () => {
  ensureOutputDir();
  const template = path.join(outputDir, 'async-cells.xlsx');
  writeTableAsJSON([{ a: 1, b: 2 }], { output: template });

  const spec = {
    template,
    updates: [
      { cell: 'A1', value: 'patched' },
      { cell: 'B1', value: 42.5, numberFormat: '#,##0.00' }
    ]
  };
  const sync = updateCells(spec);
  const asynchronous = await updateCellsAsync(spec);
  assert.ok(sync.equals(asynchronous), 'async bytes must equal sync bytes');
  assert.deepEqual(readTableAsJSON(asynchronous), readTableAsJSON(sync));
});

testAsync('renderTemplateAsync matches the sync result', null, async () => {
  const template = makeTemplate([['label', 'name'], ['${title}', '${items.0.name}']], [{}, {}]);
  const values = { title: 'async', items: [{ name: 'first' }] };

  const sync = renderTemplate(values, { template });
  const asynchronous = await renderTemplateAsync(values, { template });
  assert.ok(sync.equals(asynchronous), 'async bytes must equal sync bytes');
  assert.deepEqual(readTableAsJSON(asynchronous), [
    { label: 'async', name: 'first' }
  ]);

  ensureOutputDir();
  const target = path.join(outputDir, 'async-render.xlsx');
  if (fs.existsSync(target)) fs.unlinkSync(target);
  const summary = await renderTemplateAsync(values, { template, output: target });
  assert.ok(fs.existsSync(target));
  assert.ok(summary.bytes > 0);
  assert.ok(Array.isArray(summary.sheets));

  await assert.rejects(
    () => renderTemplateAsync({}, { template }),
    (err) => err.code === 'TEMPLATE_ERROR'
  );
});

testAsync('a large async write keeps the event loop running', null, async () => {
  const rows = [];
  for (let i = 0; i < 20000; i++) {
    rows.push({ id: i, name: `row ${i}`, amount: i * 1.5, flag: i % 2 === 0 });
  }

  let ticks = 0;
  const timer = setInterval(() => { ticks += 1; }, 1);
  try {
    const buffer = await writeTableAsJSONAsync(rows, { sheetName: 'Big' });
    assert.ok(Buffer.isBuffer(buffer));
  } finally {
    clearInterval(timer);
  }
  // The snapshot is built on the calling thread, but generation, deflate and
  // assembly run in the thread pool -- the loop must have kept firing.
  assert.ok(ticks > 0, `expected timers to fire during the write, got ${ticks}`);
});

// ---------------------------------------------------------------------------
// Streaming writes: rows may be any iterable, so the table never has to exist
// in JS at once
// ---------------------------------------------------------------------------

const STREAM_COLUMNS = { id: {}, name: {}, amount: {}, when: { numberFormat: 'yyyy-mm-dd' } };

function* rowGenerator(count) {
  const when = new Date(2026, 0, 15);
  for (let i = 0; i < count; i++) {
    yield { id: i + 1, name: `row ${i + 1}`, amount: i * 1.25, when };
  }
}

function collectRows(count) {
  return Array.from(rowGenerator(count));
}

test('writeTableAsJSON accepts a generator', null, () => {
  const rows = collectRows(50);
  const fromArray = writeTableAsJSON(rows, { columns: STREAM_COLUMNS });
  const fromGenerator = writeTableAsJSON(rowGenerator(50), { columns: STREAM_COLUMNS });

  assert.equal(fromGenerator.length, fromArray.length);
  assert.deepEqual(readTableAsJSON(fromGenerator), readTableAsJSON(fromArray));
});

test('a generator spanning several internal batches matches the array write', null, () => {
  // More rows than one append batch, so the multi-batch path is exercised.
  const count = 25000;
  const rows = collectRows(count);

  const fromArray = writeTableAsJSON(rows, { columns: STREAM_COLUMNS, sheetName: 'Big' });
  const fromGenerator = writeTableAsJSON(rowGenerator(count), {
    columns: STREAM_COLUMNS, sheetName: 'Big'
  });

  const expected = readTableAsJSON(fromArray);
  assert.equal(expected.length, count);
  assert.deepEqual(readTableAsJSON(fromGenerator), expected);
});

test('a generator can fill a template sheet', null, () => {
  const template = makeTemplate([['old'], ['data']], [{}]);

  const buffer = writeTableAsJSON(rowGenerator(3), {
    template,
    sheetName: 'Sheet1',
    columns: [
      { header: 'name', key: 'name' },
      { header: 'amount', key: 'amount' }
    ]
  });
  assert.ok(Buffer.isBuffer(buffer));

  const rows = readTableAsJSON(buffer, { headerRow: 0 });
  assert.equal(rows.length, 3);
  assert.deepEqual(rows[0], { name: 'row 1', amount: '0' });
  assert.deepEqual(rows[2], { name: 'row 3', amount: '2.5' });
});

test('iterable input requires explicit columns', null, () => {
  assert.throws(
    () => writeTableAsJSON(rowGenerator(1), {}),
    (err) => err.code === 'INVALID_OPTIONS'
  );
});

test('a non-iterable input is rejected', null, () => {
  for (const bad of [42, 'rows', null, undefined, {}, new Date()]) {
    assert.throws(
      () => writeTableAsJSON(bad, { columns: STREAM_COLUMNS }),
      (err) => err.code === 'INVALID_OPTIONS',
      `expected ${String(bad)} to be rejected`
    );
  }
});

test('an error thrown by the generator propagates', null, () => {
  function* broken() {
    yield { id: 1, name: 'ok', amount: 1, when: null };
    throw new Error('source exploded');
  }
  assert.throws(() => writeTableAsJSON(broken(), { columns: STREAM_COLUMNS }), /source exploded/);
});

testAsync('writeTableAsJSONAsync accepts a generator and an async iterable', null, async () => {
  const rows = collectRows(30);
  const expected = readTableAsJSON(
    writeTableAsJSON(rows, { columns: STREAM_COLUMNS })
  );

  const fromGenerator = await writeTableAsJSONAsync(rowGenerator(30), {
    columns: STREAM_COLUMNS
  });
  assert.deepEqual(readTableAsJSON(fromGenerator), expected);

  async function* asyncRows() {
    for (const row of rows) {
      yield row;
    }
  }
  const fromAsyncGenerator = await writeTableAsJSONAsync(asyncRows(), {
    columns: STREAM_COLUMNS
  });
  assert.deepEqual(readTableAsJSON(fromAsyncGenerator), expected);
});

testAsync('streaming writes keep the event loop running', null, async () => {
  let ticks = 0;
  const timer = setInterval(() => { ticks += 1; }, 1);
  try {
    const buffer = await writeTableAsJSONAsync(rowGenerator(25000), {
      columns: STREAM_COLUMNS,
      sheetName: 'Streamed'
    });
    assert.ok(Buffer.isBuffer(buffer));
  } finally {
    clearInterval(timer);
  }
  assert.ok(ticks > 0, `expected timers to fire while streaming, got ${ticks}`);
});

testAsync('a streamed write reports its row count when writing to a file', null, async () => {
  ensureOutputDir();
  const target = path.join(outputDir, 'streamed.xlsx');
  if (fs.existsSync(target)) fs.unlinkSync(target);

  const summary = await writeTableAsJSONAsync(rowGenerator(100), {
    columns: STREAM_COLUMNS,
    output: target
  });
  assert.ok(fs.existsSync(target));
  assert.equal(summary.rowCount, 100);
  assert.equal(summary.sheetName, 'Sheet1');
  assert.equal(readTableAsJSON(target).length, 100);
});

// ---------------------------------------------------------------------------
// Streaming batches (P2-1)
// ---------------------------------------------------------------------------

test('onBatch delivers every row', 'sample', (fixture) => {
  const all = readTableAsJSON(fixture);
  const seen = [];
  const result = readTableAsJSON(fixture, {
    onBatch: (rows) => { seen.push(...rows); }
  });
  assert.equal(typeof result.rowCount, 'number');
  assert.equal(result.rowCount, seen.length);
  assert.equal(seen.length, all.length);
  assert.deepEqual(
    JSON.parse(JSON.stringify(seen)),
    JSON.parse(JSON.stringify(all))
  );
});

test('batchSize splits the read into several batches', 'sample', (fixture) => {
  const all = readTableAsJSON(fixture);
  let batches = 0;
  let largest = 0;
  readTableAsJSON(fixture, {
    batchSize: 1,
    onBatch: (rows) => { batches++; largest = Math.max(largest, rows.length); }
  });
  assert.ok(batches >= all.length, `expected at least ${all.length} batches, got ${batches}`);
  assert.ok(largest <= 1, 'batchSize must cap rows per callback');
});

test('onBatch reports warnings too', 'sample', (fixture) => {
  const result = readTableAsJSON(fixture, { onBatch: () => {} });
  assert.ok(Array.isArray(result.warnings));
});

test('bad onBatch -> INVALID_OPTIONS', 'sample', (fixture) => {
  assert.throws(
    () => readTableAsJSON(fixture, { onBatch: 'nope' }),
    (err) => err.code === 'INVALID_OPTIONS'
  );
});

testAsync('async onBatch matches the sync result', 'sample', async (fixture) => {
  const all = readTableAsJSON(fixture);
  const seen = [];
  const result = await readTableAsJSONAsync(fixture, {
    onBatch: (rows) => { seen.push(...rows); }
  });
  assert.equal(result.rowCount, all.length);
  assert.deepEqual(
    JSON.parse(JSON.stringify(seen)),
    JSON.parse(JSON.stringify(all))
  );
});

// ---------------------------------------------------------------------------
// Buffer / base64 input
// ---------------------------------------------------------------------------

test('Buffer input matches file input', 'sample', (fixture) => {
  const fromFile = readTableAsJSON(fixture);
  const fromBuffer = readTableAsJSON(fs.readFileSync(fixture));
  assert.deepEqual(
    JSON.parse(JSON.stringify(fromBuffer)),
    JSON.parse(JSON.stringify(fromFile))
  );
});

test('explicit base64 input works', 'sample', (fixture) => {
  const b64 = fs.readFileSync(fixture).toString('base64');
  const rows = readTableAsJSON(b64, { inputEncoding: 'base64' });
  assert.ok(Array.isArray(rows));
});

test('base64-like garbage -> INVALID_INPUT (PK magic check)', null, () => {
  const fake = 'A'.repeat(600); // passes charset heuristic, not a ZIP
  assert.throws(
    () => readTableAsJSON(fake),
    (err) => err.code === 'INVALID_INPUT'
  );
});

// ---------------------------------------------------------------------------
// Async API
// ---------------------------------------------------------------------------

testAsync('async matches sync result', 'sample', async (fixture) => {
  const sync = readTableAsJSON(fixture);
  const async = await readTableAsJSONAsync(fixture);
  assert.deepEqual(
    JSON.parse(JSON.stringify(async)),
    JSON.parse(JSON.stringify(sync))
  );
});

testAsync('async rejects with coded errors', null, async () => {
  await assert.rejects(
    readTableAsJSONAsync('definitely-not-here-67890.xlsx'),
    (err) => err.code === 'FILE_NOT_FOUND'
  );
});

// ---------------------------------------------------------------------------
// Second fixture / WPS (structure-level checks)
// ---------------------------------------------------------------------------

test('second fixture reads', 'sample2', (fixture) => {
  const rows = readTableAsJSON(fixture);
  assert.ok(Array.isArray(rows));
});

test('WPS fixture reads without throwing', 'wps', (fixture) => {
  const result = readTableAsJSON(fixture, { includeWarnings: true });
  const rows = Array.isArray(result) ? result : result.rows;
  assert.ok(Array.isArray(rows));
});

// ---------------------------------------------------------------------------

console.log(`\n${passed} passed, ${skipped} skipped, ${process.exitCode ? 'FAILED' : 'all OK'}`);
