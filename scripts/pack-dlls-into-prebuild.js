/**
 * Adds DLL files to existing Windows prebuild archives.
 * Uses execFileSync (AUDIT-20260917-035: no shell string interpolation)
 * and the shared vcpkg helper (AUDIT-20260917-026).
 */

const fs = require('fs');
const path = require('path');
const os = require('os');
const { execFileSync } = require('child_process');
const vcpkg = require('./lib/vcpkg');

console.log('Adding DLL files to Windows prebuild archives...');

const prebuildsDir = path.join(__dirname, '..', 'prebuilds');
const buildReleaseDir = path.join(__dirname, '..', 'build', 'Release');

// Ensure the runtime DLLs are present in build/Release first.
// Every DLL from the vcpkg bin directory is shipped: transitive runtime
// dependencies vary with the vcpkg port features (see scripts/lib/vcpkg.js).
if (os.platform() === 'win32') {
  const binDlls = vcpkg.listBinDlls();
  let copiedCount = 0;
  for (const dllName of binDlls) {
    const targetPath = path.join(buildReleaseDir, dllName);
    if (fs.existsSync(targetPath)) {
      copiedCount++;
      continue;
    }
    const sourcePath = vcpkg.findDll(dllName);
    if (sourcePath) {
      try {
        fs.copyFileSync(sourcePath, targetPath);
        console.log(`  + ${dllName} - copied`);
        copiedCount++;
      } catch (err) {
        console.error(`  x ${dllName} - copy failed: ${err.message}`);
      }
    }
  }
  console.log(`Prepared ${copiedCount} DLL file(s) from vcpkg bin`);
}

if (!fs.existsSync(prebuildsDir)) {
  console.error('prebuilds directory does not exist');
  process.exit(1);
}

const tarFiles = fs.readdirSync(prebuildsDir)
  .filter((f) => f.endsWith('.tar.gz') && f.includes('win32'));

if (tarFiles.length === 0) {
  console.log('No Windows prebuild archives found');
  process.exit(0);
}

tarFiles.forEach((tarFile) => {
  console.log(`\nProcessing: ${tarFile}`);

  const tarPath = path.join(prebuildsDir, tarFile);
  const tempDir = path.join(os.tmpdir(), `prebuild-repack-${Date.now()}`);

  try {
    fs.mkdirSync(tempDir, { recursive: true });

    // 1. Extract existing archive (execFileSync: no shell, no quoting issues).
    execFileSync('tar', ['-xzf', tarPath, '-C', tempDir], { stdio: 'pipe' });

    // 2. Add every DLL that the Release build ships with.
    const targetDir = path.join(tempDir, 'build', 'Release');
    fs.mkdirSync(targetDir, { recursive: true });

    const releaseDlls = fs.existsSync(buildReleaseDir)
      ? fs.readdirSync(buildReleaseDir).filter((f) => f.toLowerCase().endsWith('.dll'))
      : [];

    let addedCount = 0;
    for (const dllName of releaseDlls) {
      const sourcePath = path.join(buildReleaseDir, dllName);
      fs.copyFileSync(sourcePath, path.join(targetDir, dllName));
      const stats = fs.statSync(path.join(targetDir, dllName));
      console.log(`  + ${dllName} (${(stats.size / 1024).toFixed(1)} KB)`);
      addedCount++;
    }
    console.log(`Added ${addedCount} DLL file(s)`);

    // 3. Repack.
    fs.unlinkSync(tarPath);
    const cwd = process.cwd();
    process.chdir(tempDir);
    try {
      execFileSync('tar', ['-czf', tarPath, 'build'], { stdio: 'pipe' });
    } finally {
      process.chdir(cwd);
    }

    const newStats = fs.statSync(tarPath);
    console.log(`New archive size: ${(newStats.size / 1024).toFixed(1)} KB`);
    console.log(`Done: ${tarFile}`);
  } catch (err) {
    console.error(`Failed processing ${tarFile}: ${err.message}`);
  } finally {
    if (fs.existsSync(tempDir)) {
      fs.rmSync(tempDir, { recursive: true, force: true });
    }
  }
});

console.log('\nAll Windows prebuild archives updated. Run: npm run test:prebuild');
console.log('NOTE: test:prebuild was removed; use test/test.js instead.');
