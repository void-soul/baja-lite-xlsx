'use strict';

/**
 * Generates SHA-256 checksums for prebuild archives (AUDIT-20260917-016).
 *
 * Publish flow:
 *   1. npm run prebuild        (create archives)
 *   2. npm run checksums       (write prebuilds/checksums.txt)
 *   3. upload archives + checksums.txt to the GitHub release
 *
 * Consumers of the source install can verify a downloaded archive against
 * checksums.txt. Enforcing verification at install time requires wiring
 * `--sha256` into the prebuild-install invocation once the release
 * pipeline publishes the checksum file.
 */

const fs = require('fs');
const path = require('path');
const crypto = require('crypto');

const prebuildsDir = path.join(__dirname, '..', 'prebuilds');

if (!fs.existsSync(prebuildsDir)) {
  console.error('prebuilds directory does not exist. Run "npm run prebuild" first.');
  process.exit(1);
}

const files = fs.readdirSync(prebuildsDir)
  .filter((f) => f.endsWith('.tar.gz') || f.endsWith('.zip'));

if (files.length === 0) {
  console.error('No prebuild archives found in prebuilds/');
  process.exit(1);
}

const lines = [];
for (const file of files.sort()) {
  const content = fs.readFileSync(path.join(prebuildsDir, file));
  const hash = crypto.createHash('sha256').update(content).digest('hex');
  lines.push(`${hash}  ${file}`);
  console.log(`${hash}  ${file}`);
}

const outPath = path.join(prebuildsDir, 'checksums.txt');
fs.writeFileSync(outPath, lines.join('\n') + '\n');
console.log(`\nWrote ${outPath}`);
console.log('Upload checksums.txt together with the archives to the GitHub release,');
console.log('and record the hashes when wiring --sha256 into prebuild-install.');
