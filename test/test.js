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
  writeTableAsJSON,
  updateCells,
  renderTemplate
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

function test(name, fixtureKey, fn) {
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

test('missing file -> FILE_NOT_FOUND', null,async () => {
  await assert.rejects(
    async () => await readTableAsJSON('definitely-not-here-12345.xlsx'),
    (err) => err.code === 'FILE_NOT_FOUND'
  );
});

test('invalid input -> INVALID_INPUT', null,async () => {
  await assert.rejects(
    async () => await readTableAsJSON(42),
    (err) => err.code === 'INVALID_INPUT'
  );
  await assert.rejects(
    async () => await readTableAsJSON(null),
    (err) => err.code === 'INVALID_INPUT'
  );
});

test('negative headerRow -> INVALID_OPTIONS', 'sample',async () => {
  await assert.rejects(
    async () => await readTableAsJSON(FIXTURES.sample, { headerRow: -1 }),
    (err) => err.code === 'INVALID_OPTIONS'
  );
});

test('non-integer headerRow -> INVALID_OPTIONS', 'sample',async () => {
  await assert.rejects(
    async () => await readTableAsJSON(FIXTURES.sample, { headerRow: 1.5 }),
    (err) => err.code === 'INVALID_OPTIONS'
  );
});

test('bad skipRows -> INVALID_OPTIONS', 'sample',async () => {
  await assert.rejects(
    async () => await readTableAsJSON(FIXTURES.sample, { skipRows: ['x'] }),
    (err) => err.code === 'INVALID_OPTIONS'
  );
});

// ---------------------------------------------------------------------------
// Basic reading
// ---------------------------------------------------------------------------

test('returns array of row objects', 'sample',async (fixture) => {
  const rows = await readTableAsJSON(fixture);
  assert.ok(Array.isArray(rows), 'rows should be an array');
  assert.ok(rows.length > 0, 'rows should not be empty');
  assert.equal(typeof rows[0], 'object');
});

test('sheetName option works', 'sample',async (fixture) => {
  const names = new Set();
  // Try reading with a wrong name first.
  await assert.rejects(
    async () => await readTableAsJSON(fixture, { sheetName: '__NOPE__' }),
    (err) => err.code === 'SHEET_NOT_FOUND'
  );
  void names;
});

test('maxRows caps the output', 'sample',async (fixture) => {
  const all = await readTableAsJSON(fixture);
  const capped = await readTableAsJSON(fixture, { maxRows: 2 });
  assert.ok(capped.length <= 2, `expected <= 2 rows, got ${capped.length}`);
  assert.ok(all.length >= capped.length);
});

test('maxCols caps each row', 'sample',async (fixture) => {
  const capped = await readTableAsJSON(fixture, { maxCols: 1 });
  for (const row of capped) {
    assert.ok(Object.keys(row).length <= 1);
  }
});

test('headerRow beyond data -> HEADER_ROW_OUT_OF_RANGE', 'sample',async (fixture) => {
  await assert.rejects(
    async () => await readTableAsJSON(FIXTURES.sample, { headerRow: 999999 }),
    (err) => err.code === 'HEADER_ROW_OUT_OF_RANGE'
  );
});

test('skipRows removes rows', 'sample',async (fixture) => {
  const rows = await readTableAsJSON(fixture, { skipRows: [1] });
  const rows2 = await readTableAsJSON(fixture, { skipRows: [1, 2] });
  assert.equal(rows.length, rows2.length + 1);
});

test('headerMap renames columns', 'sample',async (fixture) => {
  const rows = await readTableAsJSON(fixture);
  const first = rows[0] || {};
  const keys = Object.keys(first);
  if (keys.length === 0) return; // nothing to map
  const renamed = await readTableAsJSON(fixture, {
    headerMap: { [keys[0]]: '__renamed__' }
  });
  assert.ok('__renamed__' in (renamed[0] || {}));
});

// ---------------------------------------------------------------------------
// Column projection (P1-2) and image opt-out (P0-3)
// ---------------------------------------------------------------------------

test('columns projection keeps only the requested column', 'sample',async (fixture) => {
  const all = await readTableAsJSON(fixture);
  const keys = Object.keys(all[0] || {});
  if (keys.length === 0) return; // no headers to project on
  const projected = await readTableAsJSON(fixture, { columns: [keys[0]] });
  assert.equal(projected.length, all.length, 'row count must not change');
  for (const row of projected) {
    assert.ok(Object.keys(row).length <= 1, 'only one column expected');
    assert.ok(keys[0] in row, `expected key "${keys[0]}"`);
  }
});

test('columns projection accepts Excel references', 'sample',async (fixture) => {
  const projected = await readTableAsJSON(fixture, { columns: ['A'] });
  assert.ok(Array.isArray(projected));
  for (const row of projected) {
    assert.ok(Object.keys(row).length <= 1);
  }
});

test('unknown column -> INVALID_OPTIONS', 'sample',async (fixture) => {
  await assert.rejects(
    async () => await readTableAsJSON(fixture, { columns: ['__NO_SUCH_COLUMN__'] }),
    (err) => err.code === 'INVALID_OPTIONS'
  );
});

test('bad columns type -> INVALID_OPTIONS', 'sample',async (fixture) => {
  await assert.rejects(
    async () => await readTableAsJSON(fixture, { columns: 'A' }),
    (err) => err.code === 'INVALID_OPTIONS'
  );
});

test('includeImages false still returns every row', 'sample',async (fixture) => {
  const withoutImages = await readTableAsJSON(fixture, { includeImages: false });
  const withImages = await readTableAsJSON(fixture);
  assert.equal(withoutImages.length, withImages.length);
});

test('bad includeImages -> INVALID_OPTIONS', 'sample',async (fixture) => {
  await assert.rejects(
    async () => await readTableAsJSON(fixture, { includeImages: 'no' }),
    (err) => err.code === 'INVALID_OPTIONS'
  );
});

// ---------------------------------------------------------------------------
// Read engine (P3): reading the sheet straight from the package must produce
// exactly what the xlnt path produces
// ---------------------------------------------------------------------------

// Option sets that exercise the caps, projection and header handling.
const ENGINE_OPTION_SETS = [
  {},
  { includeImages: false },
  { maxRows: 3 },
  { maxCols: 2 },
  { maxRows: 3, maxCols: 2 },
  { headerRow: 1 }
];

for (const [label, fixtureKey] of [['sample', 'sample'], ['sample2', 'sample2'], ['wps', 'wps']]) {
  test(`engine xml matches xlnt on ${label}`, fixtureKey, async (fixture) => {
    for (const extra of ENGINE_OPTION_SETS) {
      const viaXlnt = await readTableAsJSON(fixture, { ...extra });
      const viaXml = await readTableAsJSON(fixture, { ...extra, engine: 'xml' });
      assert.deepEqual(viaXml, viaXlnt, `options ${JSON.stringify(extra)}`);
    }
  });

  test(`engine xml projection matches xlnt on ${label}`, fixtureKey, async (fixture) => {
    const probe = await readTableAsJSON(fixture, { includeImages: false });
    const keys = Object.keys(probe[0] || {});
    if (keys.length === 0) return;

    const requested = keys.length > 1 ? [keys[0], keys[keys.length - 1]] : [keys[0]];
    const viaXlnt = await readTableAsJSON(fixture, { columns: requested, includeImages: false });
    const viaXml = await readTableAsJSON(fixture, { columns: requested, includeImages: false, engine: 'xml' });
    assert.deepEqual(viaXml, viaXlnt);
  });
}

test('engine xml streams the same rows as xlnt', 'sample', async (fixture) => {
  const collect = async (engine) => {
    const rows = [];
    const summary = await readTableAsJSON(fixture, {
      engine,
      includeImages: false,
      batchSize: 2,
      onBatch: (batch) => { rows.push(...batch); }
    });
    return { rows, rowCount: summary.rowCount };
  };

  const viaXlnt = await collect('xlnt');
  const viaXml = await collect('xml');
  assert.equal(viaXml.rowCount, viaXlnt.rowCount);
  assert.deepEqual(viaXml.rows, viaXlnt.rows);
});

test('engine xml reports the same errors as xlnt', 'sample',async (fixture) => {
  // A missing sheet falls back to the xlnt path, which words the error.
  await assert.rejects(
    async () => await readTableAsJSON(fixture, { engine: 'xml', sheetName: '__NOPE__' }),
    (err) => err.code === 'SHEET_NOT_FOUND'
  );
  // An unknown column is an options error, whichever engine reads the file.
  await assert.rejects(
    async () => await readTableAsJSON(fixture, { engine: 'xml', columns: ['__NO_SUCH_COLUMN__'] }),
    (err) => err.code === 'INVALID_OPTIONS'
  );
});

test('bad engine -> INVALID_OPTIONS', 'sample',async (fixture) => {
  await assert.rejects(
    async () => await readTableAsJSON(fixture, { engine: 'fast' }),
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

test('writeTableAsJSON returns a Buffer that reads back', null,async () => {
  const rows = [
    { name: 'Ada', amount: 1200, active: true },
    { name: 'Alan', amount: 980.5, active: false }
  ];
  const buffer = await writeTableAsJSON(rows, { sheetName: 'Data' });
  assert.ok(Buffer.isBuffer(buffer), 'expected a Buffer');
  // Numbers stay numbers (no "1200.0"), booleans keep their Excel type.
  assert.deepEqual(await readTableAsJSON(buffer), [
    { name: 'Ada', amount: '1200', active: 'true' },
    { name: 'Alan', amount: '980.5', active: 'false' }
  ]);
});

test('column config drives header text, order and number formats', null,async () => {
  const rows = [{ amount: 1234.5, name: 'Ada' }];
  const buffer = await writeTableAsJSON(rows, {
    sheetName: '报表',
    columns: {
      name: { header: '姓名', width: 16 },
      amount: { header: '金额', numberFormat: '#,##0.00' }
    }
  });

  assert.deepEqual(await readTableAsJSON(buffer), [{ 姓名: 'Ada', 金额: '1234.5' }]);
  assert.equal(await readTableAsJSON(buffer, { sheetName: '报表' }).length, 1);
  await assert.rejects(
    async () => await readTableAsJSON(buffer, { sheetName: 'nope' }),
    (err) => err.code === 'SHEET_NOT_FOUND'
  );
});

test('writeTableAsJSON writes to a file and reports bytes', null,async () => {
  ensureOutputDir();
  const target = path.join(outputDir, 'write-basic.xlsx');
  if (fs.existsSync(target)) fs.unlinkSync(target);

  const summary = await writeTableAsJSON([{ a: 1, b: 'x' }], { output: target });
  assert.ok(fs.existsSync(target), 'output file must exist');
  assert.ok(summary.bytes > 0);
  assert.equal(summary.rowCount, 1);
  assert.deepEqual(await readTableAsJSON(target), [{ a: '1', b: 'x' }]);
});

test('array rows with an explicit column list', null,async () => {
  const buffer = await writeTableAsJSON([[1, 'a'], [2, 'b']], {
    columns: [{ header: 'num' }, { header: 'text' }]
  });
  assert.deepEqual(await readTableAsJSON(buffer), [
    { num: '1', text: 'a' },
    { num: '2', text: 'b' }
  ]);
});

test('includeHeader false leaves only data rows', null,async () => {
  const buffer = await writeTableAsJSON([[1, 2], [3, 4]], {
    columns: [{ header: 'A' }, { header: 'B' }],
    includeHeader: false
  });
  // The reader consumes row 0 as its header, so one row must remain.
  assert.equal(await readTableAsJSON(buffer).length, 1);
});

test('Date values become real Excel dates', null,async () => {
  const buffer = await writeTableAsJSON([{ when: new Date(2026, 0, 15) }], {
    columns: { when: { header: 'when', numberFormat: 'yyyy-mm-dd' } }
  });
  assert.deepEqual(await readTableAsJSON(buffer), [{ when: '2026-01-15' }]);
});

test('bad write specs -> INVALID_OPTIONS', null,async () => {
  await assert.rejects(async () => await writeTableAsJSON('nope', {}), (err) => err.code === 'INVALID_OPTIONS');
  await assert.rejects(async () => await writeTableAsJSON([], {}), (err) => err.code === 'INVALID_OPTIONS');
  await assert.rejects(async () => await writeTableAsJSON([{ a: 1 }], { columns: 5 }), (err) => err.code === 'INVALID_OPTIONS');
  await assert.rejects(async () => await writeTableAsJSON([{ a: 1 }], { sheetName: 7 }), (err) => err.code === 'INVALID_OPTIONS');
});

// ---------------------------------------------------------------------------
// Write: template workbook (mode 1b) and cell patching (mode 2)
// ---------------------------------------------------------------------------

test('writeTableAsJSON with a template replaces the sheet data', 'sample',async (fixture) => {
  const buffer = await writeTableAsJSON([{ alpha: 'x', beta: 42 }], { sourceFile: fixture });
  assert.ok(Buffer.isBuffer(buffer));
  assert.deepEqual(await readTableAsJSON(buffer), [{ alpha: 'x', beta: '42' }]);
});

test('a Buffer works as the template', 'sample',async (fixture) => {
  const bytes = fs.readFileSync(fixture);
  const buffer = await writeTableAsJSON([{ a: 1 }], { sourceFile: bytes });
  assert.deepEqual(await readTableAsJSON(buffer), [{ a: '1' }]);
});

test('updateCells rewrites one cell and leaves the rest alone', 'sample',async (fixture) => {
  const before = await readTableAsJSON(fixture);
  assert.ok(before.length > 0, 'fixture needs at least one data row');
  const keys = Object.keys(before[0]);
  assert.ok(keys.length > 0, 'fixture needs at least one column');

  const buffer = await updateCells({ sourceFile: fixture, updates: [{ cell: 'A2', value: 'REPLACED' }] });
  const after = await readTableAsJSON(buffer);

  assert.equal(after.length, before.length, 'row count must not change');
  assert.equal(after[0][keys[0]], 'REPLACED');
  if (keys.length > 1) {
    assert.equal(after[0][keys[1]], before[0][keys[1]], 'other cells must stay untouched');
  }
});

test('updateCells writes numbers as numbers', 'sample',async (fixture) => {
  const buffer = await updateCells({ sourceFile: fixture, updates: [{ cell: 'A2', value: 1234.5 }] });
  const after = await readTableAsJSON(buffer);
  assert.equal(after[0][Object.keys(after[0])[0]], '1234.5');
});

test('updateCells applies a number format when asked', 'sample',async (fixture) => {
  const buffer = await updateCells({
    sourceFile: fixture,
    updates: [{ cell: 'A2', value: 0.25, numberFormat: '0.00%' }]
  });
  const after = await readTableAsJSON(buffer);
  assert.equal(after[0][Object.keys(after[0])[0]], '0.25');
});

test('updateCells writes a date with an automatic format', 'sample',async (fixture) => {
  const buffer = await updateCells({
    sourceFile: fixture,
    updates: [{ cell: 'A2', value: new Date(2026, 0, 15) }]
  });
  const after = await readTableAsJSON(buffer);
  assert.equal(after[0][Object.keys(after[0])[0]], '2026-01-15');
});

test('updateCells can clear a cell', 'sample',async (fixture) => {
  const buffer = await updateCells({ sourceFile: fixture, updates: [{ cell: 'A2', value: null }] });
  const after = await readTableAsJSON(buffer);
  assert.equal(after[0][Object.keys(after[0])[0]], '');
});

test('updateCells writes to a file when asked', 'sample',async (fixture) => {
  ensureOutputDir();
  const target = path.join(outputDir, 'patched.xlsx');
  if (fs.existsSync(target)) fs.unlinkSync(target);
  const summary = await updateCells({
    sourceFile: fixture,
    updates: [{ cell: 'A2', value: 'FILE' }],
    output: target
  });
  assert.ok(fs.existsSync(target));
  assert.equal(summary.cells, 1);
  assert.ok(summary.bytes > 0);
  assert.equal(await readTableAsJSON(target)[0][Object.keys(await readTableAsJSON(target)[0])[0]], 'FILE');
});

test('bad update specs -> INVALID_OPTIONS', null,async () => {
  await assert.rejects(
    async () => await updateCells({ updates: [{ cell: 'A1', value: 1 }] }),
    (err) => err.code === 'INVALID_OPTIONS'
  );
  await assert.rejects(
    async () => await updateCells({ sourceFile: 'x.xlsx', updates: [] }),
    (err) => err.code === 'INVALID_OPTIONS'
  );
  await assert.rejects(
    async () => await updateCells({ sourceFile: 'x.xlsx', updates: [{ value: 1 }] }),
    (err) => err.code === 'INVALID_OPTIONS'
  );
});

test('a missing template reports a coded error', null,async () => {
  await assert.rejects(
    async () => await updateCells({ sourceFile: 'definitely-not-here-77.xlsx', updates: [{ cell: 'A1', value: 1 }] }),
    (err) => err.code === 'FILE_OPEN_FAILED' || err.code === 'FILE_NOT_FOUND'
  );
  await assert.rejects(
    async () => await writeTableAsJSON([{ a: 1 }], { sourceFile: 'definitely-not-here-88.xlsx' }),
    (err) => err.code === 'FILE_OPEN_FAILED' || err.code === 'FILE_NOT_FOUND'
  );
});

// ---------------------------------------------------------------------------
// Write: template rendering (mode 3)
// ---------------------------------------------------------------------------

let templateCounter = 0;

// Templates are built with our own writer, so the tests exercise the full
// round trip: write a template, render it, read the result back.
async function makeTemplate(rows, columns) {
  ensureOutputDir();
  templateCounter += 1;
  const target = path.join(outputDir, `template-${templateCounter}.xlsx`);
  await writeTableAsJSON(rows, { columns, includeHeader: false, output: target });
  return target;
}

test('renderTemplate fills placeholders', null,async () => {
  const template = await makeTemplate(
    [['label', 'value'], ['Report ${title}', '${total}']],
    [{}, {}]
  );
  const buffer = await renderTemplate({ title: 'Q1', total: 42 }, { template });
  assert.deepEqual(await readTableAsJSON(buffer), [{ label: 'Report Q1', value: '42' }]);
});

test('renderTemplate repeats rows for {{#each}}', null,async () => {
  const template = await makeTemplate(
    [
      ['name', 'amount'],
      ['{{#each items}}', ''],
      ['${name}', '${amount}'],
      ['{{/each}}', '']
    ],
    [{}, {}]
  );
  const buffer = await renderTemplate(
    {
      items: [
        { name: 'a', amount: 1 },
        { name: 'b', amount: 2 },
        { name: 'c', amount: 3 }
      ]
    },
    { template }
  );
  assert.deepEqual(await readTableAsJSON(buffer), [
    { name: 'a', amount: '1' },
    { name: 'b', amount: '2' },
    { name: 'c', amount: '3' }
  ]);
});

test('renderTemplate supports @index and shorthand ${.}', null,async () => {
  const template = await makeTemplate(
    [['no', 'name'], ['{{#each items}}', ''], ['${@index}', '${.}'], ['{{/each}}', '']],
    [{}, {}]
  );
  const filled = await renderTemplate({ items: ['x', 'y'] }, { template });
  assert.deepEqual(await readTableAsJSON(filled), [{ no: '0', name: 'x' }, { no: '1', name: 'y' }]);

  const empty = await renderTemplate({ items: [] }, { template });
  assert.deepEqual(await readTableAsJSON(empty), []);
});

test('renderTemplate keeps the template number format for dates', null,async () => {
  const template = await makeTemplate([['when'], ['${when}']], [{ numberFormat: 'yyyy-mm-dd' }]);
  const buffer = await renderTemplate({ when: new Date(2026, 0, 15) }, { template });
  assert.deepEqual(await readTableAsJSON(buffer), [{ when: '2026-01-15' }]);
});

test('renderTemplate steps out of a loop with ../', null,async () => {
  const template = await makeTemplate(
    [
      ['title', 'name'],
      ['{{#each items}}', ''],
      ['${../title}', '${name}'],
      ['{{/each}}', '']
    ],
    [{}, {}]
  );
  const buffer = await renderTemplate({ title: 'T', items: [{ name: 'a' }] }, { template });
  assert.deepEqual(await readTableAsJSON(buffer), [{ title: 'T', name: 'a' }]);
});

test('unknown placeholder -> TEMPLATE_ERROR, strict false -> empty', null,async () => {
  const template = await makeTemplate([['a'], ['${nope}']], [{}]);
  await assert.rejects(
    async () => await renderTemplate({ other: 1 }, { template }),
    (err) => err.code === 'TEMPLATE_ERROR'
  );
  const lenient = await renderTemplate({ other: 1 }, { template, strict: false });
  assert.deepEqual(await readTableAsJSON(lenient), [{ a: '' }]);
});

test('an unclosed {{#each}} -> TEMPLATE_ERROR', null,async () => {
  const template = await makeTemplate([['a'], ['{{#each items}}'], ['${.}']], [{}]);
  await assert.rejects(
    async () => await renderTemplate({ items: ['x'] }, { template }),
    (err) => err.code === 'TEMPLATE_ERROR'
  );
});

test('renderTemplate writes to a file when asked', null,async () => {
  const template = await makeTemplate([['a'], ['${v}']], [{}]);
  ensureOutputDir();
  const target = path.join(outputDir, 'rendered.xlsx');
  if (fs.existsSync(target)) fs.unlinkSync(target);
  const summary = await renderTemplate({ v: 'ok' }, { template, output: target });
  assert.ok(fs.existsSync(target));
  assert.ok(summary.bytes > 0);
  assert.ok(Array.isArray(summary.sheets));
  assert.deepEqual(await readTableAsJSON(target), [{ a: 'ok' }]);
});

test('cache: true renders exactly what the uncached path renders', null,async () => {
  const template = await makeTemplate(
    [
      ['name', 'amount'],
      ['{{#each items}}', ''],
      ['${name}', '${amount}'],
      ['{{/each}}', '']
    ],
    [{}, {}]
  );
  const values = { items: [{ name: 'a', amount: 1 }, { name: 'b', amount: 2 }] };

  const plain = await renderTemplate(values, { template });
  const firstCached = await renderTemplate(values, { template, cache: true });   // fills the cache
  const secondCached = await renderTemplate(values, { template, cache: true });  // served from it

  assert.ok(plain.equals(firstCached), 'cached bytes must match the uncached render');
  assert.ok(firstCached.equals(secondCached), 'cache hits must stay identical');
  assert.deepEqual(await readTableAsJSON(secondCached), [
    { name: 'a', amount: '1' },
    { name: 'b', amount: '2' }
  ]);
});

test('the cache drops an entry when the template is rewritten', null,async () => {
  ensureOutputDir();
  const target = path.join(outputDir, 'cache-invalidate.xlsx');

  await writeTableAsJSON([['label'], ['A ${v}']], {
    columns: [{}], includeHeader: false, output: target
  });
  const first = await readTableAsJSON(await renderTemplate({ v: 'x' }, { template: target, cache: true }));
  assert.deepEqual(first, [{ label: 'A x' }]);

  // Same path, different layout: the identity (size + mtime) changes, so the
  // stale entry must not be reused.
  await writeTableAsJSON([['other'], ['B ${v}'], ['C']], {
    columns: [{}], includeHeader: false, output: target
  });
  const second = await readTableAsJSON(await renderTemplate({ v: 'y' }, { template: target, cache: true }));
  assert.deepEqual(second, [{ other: 'B y' }, { other: 'C' }]);
});

// Excel stores cell text in sharedStrings and references it by index, so a
// template authored in Excel has no marker in its sheet XML at all. This fixture
// was produced by docs/local/cachecheck.cpp from a package our own writer made
// (see that file), which keeps it loadable by xlnt as well.
const SHARED_TEMPLATE = path.join(__dirname, 'fixtures', 'template-shared-strings.xlsx');

test('renderTemplate handles markers stored in sharedStrings', null,async () => {
  const buffer = await renderTemplate(
    {
      customer: 'ACME & Co',
      items: [{ name: 'Widget', qty: 3 }, { name: 'Gadget', qty: 5 }]
    },
    { template: SHARED_TEMPLATE }
  );

  assert.deepEqual(await readTableAsJSON(buffer, { headerRow: 0 }), [
    { Item: 'Invoice for ACME & Co', Qty: '' },
    { Item: 'Widget', Qty: '3' },
    { Item: 'Gadget', Qty: '5' }
  ]);
});

test('cache: true also works for shared-string templates', null,async () => {
  const values = { customer: 'Cached', items: [{ name: 'only', qty: 7 }] };
  const plain = await renderTemplate(values, { template: SHARED_TEMPLATE });
  const firstCached = await renderTemplate(values, { template: SHARED_TEMPLATE, cache: true });
  const secondCached = await renderTemplate(values, { template: SHARED_TEMPLATE, cache: true });

  assert.ok(plain.equals(firstCached), 'cache miss must match the uncached render');
  assert.ok(firstCached.equals(secondCached), 'cache hit must match too');

  const rows = await readTableAsJSON(secondCached, { headerRow: 0 });
  assert.equal(rows[0].Item, 'Invoice for Cached');
  assert.deepEqual(rows[1], { Item: 'only', Qty: '7' });
});

test('bad cache option -> INVALID_OPTIONS', null,async () => {
  const template = await makeTemplate([['a'], ['${v}']], [{}]);
  await assert.rejects(
    async () => await renderTemplate({ v: 1 }, { template, cache: 'yes' }),
    (err) => err.code === 'INVALID_OPTIONS'
  );
});

test('bad render specs -> INVALID_OPTIONS', null,async () => {
  await assert.rejects(async () => await renderTemplate('nope', {}), (err) => err.code === 'INVALID_OPTIONS');
  await assert.rejects(async () => await renderTemplate({}, {}), (err) => err.code === 'INVALID_OPTIONS');
  await assert.rejects(
    async () => await renderTemplate({ a: [{ 'bad.key': 1 }] }, { template: 'x.xlsx' }),
    (err) => err.code === 'INVALID_OPTIONS'
  );
});

// ---------------------------------------------------------------------------
// Write: asynchronous twins (rows are snapshotted, the work runs off-thread)
// ---------------------------------------------------------------------------

// (removed with the synchronous API: async IS the API)

test('writeTableAsJSON writes the file it is asked to', null, async () => {
  ensureOutputDir();
  const target = path.join(outputDir, 'async-write.xlsx');
  if (fs.existsSync(target)) fs.unlinkSync(target);

  const summary = await writeTableAsJSON([{ a: 1, b: 'x' }], { output: target });
  assert.ok(fs.existsSync(target));
  assert.equal(summary.rowCount, 1);
  assert.equal(summary.sheetName, 'Sheet1');
  assert.deepEqual(await readTableAsJSON(target), [{ a: '1', b: 'x' }]);
});

test('writeTableAsJSON rejects with a coded error', null, async () => {
  await assert.rejects(
    async () => await writeTableAsJSON([{ a: 1 }], { sourceFile: 'no-such-file.xlsx' }),
    (err) => err.code === 'FILE_OPEN_FAILED'
  );
  await assert.rejects(
    async () => await writeTableAsJSON('nope', {}),
    (err) => err.code === 'INVALID_OPTIONS'
  );
});

// (removed with the synchronous API: async IS the API)

// (removed with the synchronous API: async IS the API)

test('a large async write keeps the event loop running', null, async () => {
  const rows = [];
  for (let i = 0; i < 20000; i++) {
    rows.push({ id: i, name: `row ${i}`, amount: i * 1.5, flag: i % 2 === 0 });
  }

  let ticks = 0;
  const timer = setInterval(() => { ticks += 1; }, 1);
  try {
    const buffer = await writeTableAsJSON(rows, { sheetName: 'Big' });
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

test('writeTableAsJSON accepts a generator', null,async () => {
  const rows = collectRows(50);
  const fromArray = await writeTableAsJSON(rows, { columns: STREAM_COLUMNS });
  const fromGenerator = await writeTableAsJSON(rowGenerator(50), { columns: STREAM_COLUMNS });

  assert.equal(fromGenerator.length, fromArray.length);
  assert.deepEqual(await readTableAsJSON(fromGenerator), await readTableAsJSON(fromArray));
});

test('a generator spanning several internal batches matches the array write', null,async () => {
  // More rows than one append batch, so the multi-batch path is exercised.
  const count = 25000;
  const rows = collectRows(count);

  const fromArray = await writeTableAsJSON(rows, { columns: STREAM_COLUMNS, sheetName: 'Big' });
  const fromGenerator = await writeTableAsJSON(rowGenerator(count), {
    columns: STREAM_COLUMNS, sheetName: 'Big'
  });

  const expected = await readTableAsJSON(fromArray);
  assert.equal(expected.length, count);
  assert.deepEqual(await readTableAsJSON(fromGenerator), expected);
});

test('a generator can fill a template sheet', null,async () => {
  const template = await makeTemplate([['old'], ['data']], [{}]);

  const buffer = await writeTableAsJSON(rowGenerator(3), {
    template,
    sheetName: 'Sheet1',
    columns: [
      { header: 'name', key: 'name' },
      { header: 'amount', key: 'amount' }
    ]
  });
  assert.ok(Buffer.isBuffer(buffer));

  const rows = await readTableAsJSON(buffer, { headerRow: 0 });
  assert.equal(rows.length, 3);
  assert.deepEqual(rows[0], { name: 'row 1', amount: '0' });
  assert.deepEqual(rows[2], { name: 'row 3', amount: '2.5' });
});

test('iterable input requires explicit columns', null,async () => {
  await assert.rejects(
    async () => await writeTableAsJSON(rowGenerator(1), {}),
    (err) => err.code === 'INVALID_OPTIONS'
  );
});

test('a non-iterable input is rejected', null,async () => {
  for (const bad of [42, 'rows', null, undefined, {}, new Date()]) {
    await assert.rejects(
      async () => await writeTableAsJSON(bad, { columns: STREAM_COLUMNS }),
      (err) => err.code === 'INVALID_OPTIONS',
      `expected ${String(bad)} to be rejected`
    );
  }
});

test('an error thrown by the generator propagates', null,async () => {
  function* broken() {
    yield { id: 1, name: 'ok', amount: 1, when: null };
    throw new Error('source exploded');
  }
  await assert.rejects(async () => await writeTableAsJSON(broken(), { columns: STREAM_COLUMNS }), /source exploded/);
});

test('writeTableAsJSON accepts a generator and an async iterable', null, async () => {
  const rows = collectRows(30);
  const expected = await readTableAsJSON(
    await writeTableAsJSON(rows, { columns: STREAM_COLUMNS })
  );

  const fromGenerator = await writeTableAsJSON(rowGenerator(30), {
    columns: STREAM_COLUMNS
  });
  assert.deepEqual(await readTableAsJSON(fromGenerator), expected);

  async function* asyncRows() {
    for (const row of rows) {
      yield row;
    }
  }
  const fromAsyncGenerator = await writeTableAsJSON(asyncRows(), {
    columns: STREAM_COLUMNS
  });
  assert.deepEqual(await readTableAsJSON(fromAsyncGenerator), expected);
});

test('streaming writes keep the event loop running', null, async () => {
  let ticks = 0;
  const timer = setInterval(() => { ticks += 1; }, 1);
  try {
    const buffer = await writeTableAsJSON(rowGenerator(25000), {
      columns: STREAM_COLUMNS,
      sheetName: 'Streamed'
    });
    assert.ok(Buffer.isBuffer(buffer));
  } finally {
    clearInterval(timer);
  }
  assert.ok(ticks > 0, `expected timers to fire while streaming, got ${ticks}`);
});

test('a streamed write reports its row count when writing to a file', null, async () => {
  ensureOutputDir();
  const target = path.join(outputDir, 'streamed.xlsx');
  if (fs.existsSync(target)) fs.unlinkSync(target);

  const summary = await writeTableAsJSON(rowGenerator(100), {
    columns: STREAM_COLUMNS,
    output: target
  });
  assert.ok(fs.existsSync(target));
  assert.equal(summary.rowCount, 100);
  assert.equal(summary.sheetName, 'Sheet1');
  assert.equal(await readTableAsJSON(target).length, 100);
});

// ---------------------------------------------------------------------------
// Streaming batches (P2-1)
// ---------------------------------------------------------------------------

test('onBatch delivers every row', 'sample',async (fixture) => {
  const all = await readTableAsJSON(fixture);
  const seen = [];
  const result = await readTableAsJSON(fixture, {
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

test('batchSize splits the read into several batches', 'sample',async (fixture) => {
  const all = await readTableAsJSON(fixture);
  let batches = 0;
  let largest = 0;
  await readTableAsJSON(fixture, {
    batchSize: 1,
    onBatch: (rows) => { batches++; largest = Math.max(largest, rows.length); }
  });
  assert.ok(batches >= all.length, `expected at least ${all.length} batches, got ${batches}`);
  assert.ok(largest <= 1, 'batchSize must cap rows per callback');
});

test('onBatch reports warnings too', 'sample',async (fixture) => {
  const result = await readTableAsJSON(fixture, { onBatch: () => {} });
  assert.ok(Array.isArray(result.warnings));
});

test('bad onBatch -> INVALID_OPTIONS', 'sample',async (fixture) => {
  await assert.rejects(
    async () => await readTableAsJSON(fixture, { onBatch: 'nope' }),
    (err) => err.code === 'INVALID_OPTIONS'
  );
});

// (removed with the synchronous API: async IS the API)

// ---------------------------------------------------------------------------
// Buffer / base64 input
// ---------------------------------------------------------------------------

test('Buffer input matches file input', 'sample',async (fixture) => {
  const fromFile = await readTableAsJSON(fixture);
  const fromBuffer = await readTableAsJSON(fs.readFileSync(fixture));
  assert.deepEqual(
    JSON.parse(JSON.stringify(fromBuffer)),
    JSON.parse(JSON.stringify(fromFile))
  );
});

test('explicit base64 input works', 'sample',async (fixture) => {
  const b64 = fs.readFileSync(fixture).toString('base64');
  const rows = await readTableAsJSON(b64, { inputEncoding: 'base64' });
  assert.ok(Array.isArray(rows));
});

test('base64-like garbage -> INVALID_INPUT (PK magic check)', null,async () => {
  const fake = 'A'.repeat(600); // passes charset heuristic, not a ZIP
  await assert.rejects(
    async () => await readTableAsJSON(fake),
    (err) => err.code === 'INVALID_INPUT'
  );
});

// ---------------------------------------------------------------------------
// Async API
// ---------------------------------------------------------------------------

test('async matches sync result', 'sample', async (fixture) => {
  const sync = await readTableAsJSON(fixture);
  const async = await readTableAsJSON(fixture);
  assert.deepEqual(
    JSON.parse(JSON.stringify(async)),
    JSON.parse(JSON.stringify(sync))
  );
});

test('async rejects with coded errors', null, async () => {
  await assert.rejects(
    await readTableAsJSON('definitely-not-here-67890.xlsx'),
    (err) => err.code === 'FILE_NOT_FOUND'
  );
});

// ---------------------------------------------------------------------------
// Second fixture / WPS (structure-level checks)
// ---------------------------------------------------------------------------

test('second fixture reads', 'sample2',async (fixture) => {
  const rows = await readTableAsJSON(fixture);
  assert.ok(Array.isArray(rows));
});

test('WPS fixture reads without throwing', 'wps',async (fixture) => {
  const result = await readTableAsJSON(fixture, { includeWarnings: true });
  const rows = Array.isArray(result) ? result : result.rows;
  assert.ok(Array.isArray(rows));
});

// ---------------------------------------------------------------------------
// v2: appending, multi-sheet writes, ejsExcel templates and typed reads
// ---------------------------------------------------------------------------

test('every public write/read entry point returns a Promise', null,async () => {
  assert.ok(writeTableAsJSON([{ a: 1 }]) instanceof Promise, 'writeTableAsJSON');
  assert.ok(readTableAsJSON('whatever.xlsx') instanceof Promise, 'readTableAsJSON');
  await Promise.allSettled([writeTableAsJSON([{ a: 1 }]), readTableAsJSON('whatever.xlsx')]);
});

test('sourceFile appends rows and chains on the returned Buffer', null,async () => {
  const base = await writeTableAsJSON([{ name: 'a', qty: 1 }], { sheetName: 'Data' });
  assert.ok(Buffer.isBuffer(base));

  const more = await writeTableAsJSON([{ name: 'b', qty: 2 }], {
    sourceFile: base, sheetName: 'Data'
  });
  const withMeta = await writeTableAsJSON([{ label: 'L1' }], {
    sourceFile: more, sheetName: 'Meta'
  });

  assert.deepEqual(await readTableAsJSON(withMeta, { sheetName: 'Data' }), [
    { name: 'a', qty: '1' },
    { name: 'b', qty: '2' }
  ]);
  assert.deepEqual(await readTableAsJSON(withMeta, { sheetName: 'Meta' }), [
    { label: 'L1' }
  ]);
});

test('append: false replaces the sheet, header modes behave', null,async () => {
  const base = await writeTableAsJSON([{ a: 1 }], { sheetName: 'S' });

  const replaced = await writeTableAsJSON([{ a: 9 }, { a: 10 }], {
    sourceFile: base, sheetName: 'S', append: false
  });
  assert.deepEqual(await readTableAsJSON(replaced), [{ a: '9' }, { a: '10' }]);

  // An explicit includeHeader appends a header row even onto existing data.
  const withHeader = await writeTableAsJSON([{ a: 11 }], {
    sourceFile: replaced, sheetName: 'S', includeHeader: true
  });
  assert.deepEqual(await readTableAsJSON(withHeader), [
    { a: '9' }, { a: '10' }, { a: 'a' }, { a: '11' }
  ]);
});

test('sourceFile works with file paths too', null,async () => {
  ensureOutputDir();
  const target = path.join(outputDir, 'append-file.xlsx');
  if (fs.existsSync(target)) fs.unlinkSync(target);

  await writeTableAsJSON([{ a: 1 }], { output: target });
  await writeTableAsJSON([{ a: 2 }], { sourceFile: target, output: target });
  assert.deepEqual(await readTableAsJSON(target), [{ a: '1' }, { a: '2' }]);
});

test('renderTemplate evaluates ejsExcel-style markers', null,async () => {
  // Row 2 emits a number through ~, row 3 a dynamic formula with a cached value.
  const template = await makeTemplate(
    [['label'], ['<%~_data_.total%>'], ['<%#"=SUM(A1,A2)"%><%~_data_.cached%>']],
    [{}, {}, {}]);
  const buffer = await renderTemplate({ total: 7, cached: 9 }, { template });
  const rows = await readTableAsJSON(buffer, { headerRow: 0 });
  assert.equal(rows[0].label, '7');

  // A fresh render with different values produces a fresh package: the
  // formula keeps its cached value (which is what stops WPS showing 0).
  const again = await renderTemplate({ total: 1, cached: 2 }, { template });
  assert.ok(Buffer.isBuffer(again) && again.length > 0);
});

test('renderTemplate forRow loops rebase rows and cells', null,async () => {
  const template = await makeTemplate(
    [
      ['hdrA', 'hdrB'],
      ['<%forRow it,i in _data_.items%>', '<%=i%>: <%=it.name%>']
    ],
    [{}, {}]);
  const buffer = await renderTemplate(
    { items: [{ name: 'x' }, { name: 'y' }] }, { template });
  const rows = await readTableAsJSON(buffer, { headerRow: 0 });
  assert.deepEqual(rows, [
    { hdrA: '', hdrB: '0: x' },
    { hdrA: '', hdrB: '1: y' }
  ]);
});

test('renderTemplate merges cells and groups rows on request', null,async () => {
  const template = await makeTemplate(
    [
      ['<%_mergeCellFn_("A1:B1")%><%="merged"%>', ''],
      ['<%="a0"%><%_outlineLevel_(1)%>', '']
    ],
    [{}, {}]);
  const buffer = await renderTemplate({}, { template });
  const rows = await readTableAsJSON(buffer, { headerRow: 0 });
  assert.equal(rows[0].merged, 'a0');
});

test('renderTemplate renders every sheet with _data_[i]', null,async () => {
  const first = await makeTemplate([['who'], ['<%=_data_[0].who%>']], [{}, {}]);
  const twoSheets = await writeTableAsJSON(
    [['<%=_data_[1].tag%>']],
    { columns: [{}], includeHeader: false, sourceFile: first, sheetName: 'S2' });

  const buffer = await renderTemplate([{ who: 'one' }, { tag: 'two' }], { template: twoSheets });
  assert.deepEqual(await readTableAsJSON(buffer, { sheetName: 'Sheet1', headerRow: 0 }),
    [{ who: 'one' }]);
  assert.deepEqual(await readTableAsJSON(buffer, { sheetName: 'S2', headerRow: 0 }),
    [{ tag: 'two' }]);
});

test('values: typed returns numbers, booleans and Dates (engine xml)', null,async () => {
  const workbook = await writeTableAsJSON(
    [{ name: 'a', amount: 2.5, when: new Date(2026, 0, 15), flag: true }],
    {
      sheetName: 'T',
      columns: {
        name: {},
        amount: { numberFormat: '#,##0.00' },
        when: { numberFormat: 'yyyy-mm-dd' },
        flag: {}
      }
    });

  const rows = await readTableAsJSON(workbook, { engine: 'xml', values: 'typed' });
  assert.equal(rows[0].name, 'a');
  assert.equal(rows[0].amount, 2.5);
  assert.ok(rows[0].when instanceof Date);
  assert.equal(rows[0].when.getTime(), new Date(2026, 0, 15).getTime());
  assert.equal(rows[0].flag, true);

  // The string engine output is unchanged.
  const strings = await readTableAsJSON(workbook, { engine: 'xml' });
  assert.equal(rows[0].amount, 2.5);
  assert.equal(typeof strings[0].amount, 'string');

  // typed needs the direct reader.
  await assert.rejects(
    async () => await readTableAsJSON(workbook, { engine: 'xlnt', values: 'typed' }),
    (err) => err.code === 'INVALID_OPTIONS'
  );
  await assert.rejects(
    async () => await readTableAsJSON(workbook, { values: 'nope' }),
    (err) => err.code === 'INVALID_OPTIONS'
  );
});

test('values: typed streams typed batches too', null,async () => {
  const workbook = await writeTableAsJSON([{ n: 1 }, { n: 2 }], { sheetName: 'S' });
  const batches = [];
  await readTableAsJSON(workbook, {
    engine: 'xml', values: 'typed', includeImages: false, batchSize: 1,
    onBatch: (batch) => batches.push(batch)
  });
  assert.equal(batches.flat().map((r) => r.n).join(','), '1,2');
});

// ---------------------------------------------------------------------------

Promise.all(pendingChecks).then(() => {
  console.log(`\n${passed} passed, ${skipped} skipped, ${process.exitCode ? 'FAILED' : 'all OK'}`);
});
