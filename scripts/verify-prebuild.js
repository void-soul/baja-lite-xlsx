'use strict';

/**
 * Cross-platform verification of the prebuild archives in `prebuilds/`.
 *
 * Asserts that every .tar.gz contains the compiled .node module (and, on
 * Windows, the runtime DLLs), so a broken archive can never be published.
 *
 * Usage: node scripts/verify-prebuild.js [dir=prebuilds]
 */

const fs = require('fs');
const path = require('path');
const os = require('os');
const { execFileSync } = require('child_process');

const dir = path.resolve(process.argv[2] || 'prebuilds');

if (!fs.existsSync(dir)) {
  console.error(`ERROR: ${dir} does not exist`);
  process.exit(1);
}

const archives = fs.readdirSync(dir).filter((f) => f.endsWith('.tar.gz'));

if (archives.length === 0) {
  console.error(`ERROR: no .tar.gz archives found in ${dir}`);
  process.exit(1);
}

const requireDlls = os.platform() === 'win32';
let failures = 0;

for (const archive of archives.sort()) {
  const archivePath = path.join(dir, archive);
  const extractDir = fs.mkdtempSync(path.join(os.tmpdir(), 'baja-verify-'));
  try {
    execFileSync('tar', ['-xzf', archivePath, '-C', extractDir], { stdio: 'pipe' });

    const walk = (d, acc = []) => {
      for (const entry of fs.readdirSync(d, { withFileTypes: true })) {
        const full = path.join(d, entry.name);
        if (entry.isDirectory()) walk(full, acc);
        else acc.push(full);
      }
      return acc;
    };
    const files = walk(extractDir);
    const nodeFiles = files.filter((f) => f.endsWith('.node'));
    const dllFiles = files.filter((f) => f.toLowerCase().endsWith('.dll'));

    const problems = [];
    if (nodeFiles.length === 0) problems.push('no .node module');
    if (requireDlls && dllFiles.length === 0) problems.push('no runtime DLLs');

    const dllNote = dllFiles.length > 0 ? ` + ${dllFiles.length} dll` : '';
    if (problems.length > 0) {
      failures++;
      console.error(`FAIL ${archive}: ${problems.join(', ')}`);
    } else {
      console.log(
        `OK   ${archive}: ${nodeFiles.map((f) => path.basename(f)).join(', ')}${dllNote}`
      );
    }
  } catch (err) {
    failures++;
    console.error(`FAIL ${archive}: ${err.message}`);
  } finally {
    fs.rmSync(extractDir, { recursive: true, force: true });
  }
}

if (failures > 0) {
  console.error(`${failures} archive(s) failed verification`);
  process.exit(1);
}
console.log(`All ${archives.length} archive(s) verified`);
