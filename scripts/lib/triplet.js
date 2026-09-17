'use strict';

/**
 * vcpkg triplet for the current (or given) platform/arch.
 *
 * Kept in one place so binding.gyp, the link-flag generator and the CI
 * workflows cannot drift apart.
 */

function tripletFor(platform = process.platform, arch = process.arch) {
  if (platform === 'win32') {
    return arch === 'arm64' ? 'arm64-windows' : 'x64-windows';
  }
  if (platform === 'darwin') {
    return arch === 'arm64' ? 'arm64-osx' : 'x64-osx';
  }
  if (platform === 'linux') {
    return arch === 'arm64' ? 'arm64-linux' : 'x64-linux';
  }
  throw new Error(`Unsupported platform: ${platform}`);
}

module.exports = { tripletFor };
