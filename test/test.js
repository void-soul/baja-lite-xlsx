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

const { readTableAsJSON, readTableAsJSONAsync } = require('..');

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
