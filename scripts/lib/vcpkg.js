'use strict';

/**
 * Single source of truth for locating the vcpkg binary directory and the
 * DLL set that must ship with the Windows build
 * (AUDIT-20260917-026 / AUDIT-20260917-014).
 *
 * Previously this logic was duplicated (and inconsistent) across
 * package-dlls.js, pack-dlls-into-prebuild.js, postinstall.js and
 * package.json.
 */

const fs = require('fs');
const path = require('path');

// DLLs required for the native module to load on Windows.
const REQUIRED_DLLS = ['xlnt.dll', 'zlib1.dll'];

// DLLs copied when available but not strictly required.
const OPTIONAL_DLLS = ['bz2.dll', 'fmt.dll', 'zip.dll', 'zlib.dll'];

// DLLs embedded into Windows prebuild archives.
const PACKAGED_DLLS = ['xlnt.dll', 'zlib1.dll', 'bz2.dll', 'fmt.dll', 'zip.dll'];

// Candidate vcpkg "installed/<triplet>/bin" directories, best first.
function candidateBinDirs() {
  const dirs = [];
  if (process.env.VCPKG_ROOT) {
    dirs.push(path.join(process.env.VCPKG_ROOT, 'installed', 'x64-windows', 'bin'));
  }
  return dirs;
}

// First candidate directory that exists on disk.
function findBinDir() {
  for (const dir of candidateBinDirs()) {
    if (fs.existsSync(dir)) {
      return dir;
    }
  }
  return null;
}

function findDll(dllName) {
  for (const dir of candidateBinDirs()) {
    const p = path.join(dir, dllName);
    if (fs.existsSync(p)) {
      return p;
    }
  }
  return null;
}

module.exports = {
  REQUIRED_DLLS,
  OPTIONAL_DLLS,
  PACKAGED_DLLS,
  candidateBinDirs,
  findBinDir,
  findDll
};
