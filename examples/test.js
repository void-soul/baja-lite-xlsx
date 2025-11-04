const { readTableAsJSON } = require('../index');
const path = require('path');
const fs = require('fs');

console.log('=== Baja-XLSX Native Module Test ===\n');


const testFile = path.join(__dirname, 'sample2.xlsx');
// testFile也可以是buffer
const result = readTableAsJSON(testFile, {
headerRow: 0,
headerMap: {'名称': 'name','年龄': 'age'}
});
console.log(result);

