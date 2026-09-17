/**
 * Quick manual smoke test.
 * Reads examples/sample2.xlsx and prints the resulting rows.
 * The automated suite lives in test/test.js (npm test).
 */

const { readTableAsJSON } = require('../index');
const path = require('path');

console.log('=== baja-lite-xlsx smoke test ===\n');

const testFile = path.join(__dirname, 'sample2.xlsx');

// A Buffer works just as well as a path.
const result = readTableAsJSON(testFile, {
  headerRow: 0,
  headerMap: { '名称': 'name', '年龄': 'age' }
});

console.log(result);
