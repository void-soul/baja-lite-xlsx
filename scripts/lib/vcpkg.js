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
const REQUIRED_DLLS = ['xlnt.dll', 'zip.dll'];

// DLLs copied when available but not strictly required (the exact set of
// transitive runtime dependencies depends on the vcpkg port features).
const OPTIONAL_DLLS = ['zlib1.dll', 'zlib.dll', 'bz2.dll', 'fmt.dll', 'liblzma.dll', 'zstd.dll'];

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

/**
 * Every DLL in the vcpkg bin directory.
 *
 * The hardcoded list above is not enough: what libzip/xlnt pull in at
 * runtime depends on the vcpkg port features (zlib vs zlib1, liblzma,
 * zstd, ...). Shipping the whole bin directory is the only future-proof
 * way to avoid "The specified module could not be found" (ERR_DLOPEN_FAILED).
 */
function listBinDlls() {
  const dir = findBinDir();
  if (!dir) return [];
  try {
    return fs.readdirSync(dir).filter((f) => f.toLowerCase().endsWith('.dll'));
  } catch (err) {
    return [];
  }
}

module.exports = {
  REQUIRED_DLLS,
  OPTIONAL_DLLS,
  candidateBinDirs,
  findBinDir,
  findDll,
  listBinDlls
};
