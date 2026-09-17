/**
 * Postinstall script: verify that the DLL files required by the prebuilt
 * Windows binary are in place, and print actionable guidance otherwise.
 * vcpkg paths come from scripts/lib/vcpkg.js (single source of truth).
 */

const fs = require('fs');
const path = require('path');
const os = require('os');
const vcpkg = require('./lib/vcpkg');

const platform = os.platform();

if (platform !== 'win32') {
  console.log('Non-Windows platform, skipping DLL check');
  process.exit(0);
}

console.log('Checking Windows DLL dependencies...\n');

const releaseDir = path.join(__dirname, '..', 'build', 'Release');
const nodeFile = path.join(releaseDir, 'baja_xlsx.node');

if (!fs.existsSync(nodeFile)) {
  console.log('Native module not built yet, skipping DLL check');
  console.log('(This is normal while a prebuild is being downloaded or before compiling)\n');
  process.exit(0);
}

const missingDlls = [];
const foundDlls = [];

vcpkg.REQUIRED_DLLS.forEach((dllName) => {
  const dllPath = path.join(releaseDir, dllName);
  if (fs.existsSync(dllPath)) {
    foundDlls.push(dllName);
    console.log(`+ ${dllName} - found`);
  } else {
    missingDlls.push(dllName);
  }
});

vcpkg.OPTIONAL_DLLS.forEach((dllName) => {
  const dllPath = path.join(releaseDir, dllName);
  if (fs.existsSync(dllPath)) {
    foundDlls.push(dllName);
    console.log(`+ ${dllName} - found (optional)`);
  }
});

console.log('');

if (missingDlls.length === 0) {
  console.log('All required DLL files are in place.\n');
} else {
  console.warn('WARNING: missing DLL files:\n');
  missingDlls.forEach((dll) => console.warn(`  - ${dll}`));
  console.warn('\nThis may prevent the module from loading.\n');

  console.log('How to fix:\n');
  console.log('1. Install the Visual C++ Redistributable:');
  console.log('   https://aka.ms/vs/17/release/vc_redist.x64.exe\n');

  const binDir = vcpkg.findBinDir();
  if (binDir) {
    console.log('2. Copy the DLLs manually:');
    console.log(`   from: ${binDir}`);
    console.log(`   to:   ${releaseDir}\n`);
  } else {
    console.log('2. Install xlnt via vcpkg (set VCPKG_ROOT first):');
    console.log('   vcpkg install xlnt:x64-windows\n');
  }
}

// Optional load test.
if (process.env.BAJA_XLSX_TEST_LOAD) {
  console.log('Testing module load...\n');
  try {
    require(nodeFile);
    console.log('Module loaded successfully!\n');
  } catch (err) {
    console.error('Module failed to load:');
    console.error(`   ${err.message}\n`);
  }
}
