/**
 * Copy the vcpkg runtime DLLs into build/Release.
 *
 * Copies EVERY DLL from the vcpkg bin directory (see scripts/lib/vcpkg.js
 * for why a hardcoded list is not enough: libzip/xlnt transitive runtime
 * dependencies vary with port features and caused ERR_DLOPEN_FAILED).
 */

const fs = require('fs');
const path = require('path');
const os = require('os');
const vcpkg = require('./lib/vcpkg');

if (os.platform() !== 'win32') {
  console.log('Non-Windows system, skipping DLL copy');
  process.exit(0);
}

const buildDir = path.join(__dirname, '..', 'build', 'Release');

if (!fs.existsSync(buildDir)) {
  console.log('build/Release directory does not exist, skipping DLL copy');
  process.exit(0);
}

const sourceDir = vcpkg.findBinDir();

if (!sourceDir) {
  console.log('vcpkg bin directory not found (set VCPKG_ROOT), skipping DLL copy');
  process.exit(0);
}

const dlls = vcpkg.listBinDlls();

if (dlls.length === 0) {
  console.log(`No DLLs found in ${sourceDir}, skipping DLL copy`);
  process.exit(0);
}

console.log(`Copying ${dlls.length} DLL(s) from ${sourceDir}...`);

let copiedCount = 0;
for (const dllName of dlls) {
  const sourcePath = path.join(sourceDir, dllName);
  const targetPath = path.join(buildDir, dllName);
  try {
    fs.copyFileSync(sourcePath, targetPath);
    copiedCount++;
  } catch (err) {
    console.error(`  x ${dllName} - copy failed: ${err.message}`);
  }
}

console.log(`Copied ${copiedCount}/${dlls.length} DLL file(s)`);

const missingRequired = vcpkg.REQUIRED_DLLS.filter(
  (name) => !fs.existsSync(path.join(buildDir, name))
);
if (missingRequired.length > 0) {
  console.warn(`WARNING: required DLL(s) still missing: ${missingRequired.join(', ')}`);
}
