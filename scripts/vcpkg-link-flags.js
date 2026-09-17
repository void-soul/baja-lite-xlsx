'use strict';

/**
 * Emits the linker flags needed to link the native addon against the
 * vcpkg-provided xlnt + libzip on Linux/macOS.
 *
 * Why enumerate every archive: on those platforms vcpkg builds static
 * libraries, so xlnt/libzip additionally need their own dependencies
 * (zlib, bzip2, liblzma, zstd, fmt, fast-float, libstudxml, ...). Listing
 * the whole lib directory inside a linker group (Linux) or with -all_load
 * (macOS) resolves them without hardcoding a fragile dependency list.
 *
 * Usage: node scripts/vcpkg-link-flags.js   ->  -L... -Wl,--start-group ...
 */

const fs = require('fs');
const path = require('path');
const { tripletFor } = require('./lib/triplet');

const root = process.env.VCPKG_ROOT;
if (!root) {
  console.error('ERROR: VCPKG_ROOT is not set (see scripts/require-vcpkg-root.js).');
  process.exit(1);
}

const triplet = tripletFor();
const libDir = path.join(root, 'installed', triplet, 'lib');

if (!fs.existsSync(libDir)) {
  console.error(`ERROR: vcpkg lib directory not found: ${libDir}`);
  process.exit(1);
}

const archives = fs.readdirSync(libDir).filter((f) => f.endsWith('.a')).sort();

if (archives.length === 0) {
  console.error(`ERROR: no static libraries found in ${libDir}`);
  process.exit(1);
}

if (process.platform === 'linux') {
  const names = archives.map((a) => '-l' + a.replace(/^lib/, '').replace(/\.a$/, ''));
  process.stdout.write(
    [`-L${libDir}`, '-Wl,--start-group', ...names, '-Wl,--end-group'].join(' ')
  );
} else if (process.platform === 'darwin') {
  const paths = archives.map((a) => path.join(libDir, a));
  process.stdout.write(['-Wl,-all_load', ...paths, `-L${libDir}`].join(' '));
} else {
  // Windows uses explicit .lib paths in binding.gyp.
  process.stdout.write('');
}
