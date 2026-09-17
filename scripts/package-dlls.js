/**
 * Copy vcpkg DLL files into build/Release.
 * vcpkg lookup now lives in scripts/lib/vcpkg.js (single source of truth).
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

console.log(`Copying DLLs from ${sourceDir}...`);

let copiedCount = 0;

[...vcpkg.REQUIRED_DLLS, ...vcpkg.OPTIONAL_DLLS].forEach((dllName) => {
  const sourcePath = path.join(sourceDir, dllName);
  const targetPath = path.join(buildDir, dllName);

  if (fs.existsSync(sourcePath)) {
    try {
      fs.copyFileSync(sourcePath, targetPath);
      console.log(`  + ${dllName}`);
      copiedCount++;
    } catch (err) {
      console.error(`  x ${dllName} - copy failed: ${err.message}`);
    }
  } else {
    console.log(`  ! ${dllName} - not found in vcpkg bin`);
  }
});

console.log(`Copied ${copiedCount} DLL files`);
