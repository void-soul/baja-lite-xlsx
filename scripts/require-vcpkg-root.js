'use strict';

/**
 * Called from binding.gyp: prints $VCPKG_ROOT or exits with a clear,
 * actionable error (AUDIT-20260917-027). Previously an unset variable
 * expanded to the literal "%VCPKG_ROOT%" and produced unreadable
 * compiler errors.
 */

const root = process.env.VCPKG_ROOT;

if (!root) {
  console.error('ERROR: VCPKG_ROOT environment variable is not set.');
  console.error('');
  console.error('This project builds against xlnt + libzip provided by vcpkg.');
  console.error('Fix:');
  console.error('  1. Install vcpkg: https://github.com/microsoft/vcpkg');
  console.error('  2. Install dependencies (triplet per platform):');
  console.error('       Windows:      vcpkg install xlnt:x64-windows zip:x64-windows');
  console.error('       Linux:        vcpkg install xlnt:x64-linux zip:x64-linux');
  console.error('       macOS (ARM):  vcpkg install xlnt:arm64-osx zip:arm64-osx');
  console.error('  3. Set the environment variable, e.g.:');
  console.error('       set VCPKG_ROOT=C:\\vcpkg      (cmd)');
  console.error('       $env:VCPKG_ROOT = "C:\\vcpkg"  (PowerShell)');
  console.error('       export VCPKG_ROOT=~/vcpkg     (bash)');
  console.error('  4. Rebuild: npm run build');
  process.exit(1);
}

process.stdout.write(root);
