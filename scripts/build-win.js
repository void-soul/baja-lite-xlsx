'use strict';

/**
 * One-shot Windows build for baja-lite-xlsx.
 *
 *   node scripts/build-win.js
 *
 * Why this exists: the MSBuild projects node-gyp generates do not pin a
 * Windows SDK version, and the VS 2022 17.14 toolset then defaults to
 * 10.0.26100.0 -- which fails with MSB8036 on machines whose installed SDK
 * is older or newer (this machine has 10.0.22621 / 10.0.28000 under
 * D:\Windows Kits\10). The script configures, retargets every generated
 * project to the newest INSTALLED SDK, drops stale debug info (LNK1103) and
 * builds.
 *
 * Requirements: VCPKG_ROOT set (xlnt + libzip installed), Python and MSVC
 * Build Tools present. Run from the repository root.
 */

const fs = require('fs');
const path = require('path');
const { execFileSync } = require('child_process');

const ROOT = path.join(__dirname, '..');
const SDK_VERSION_RE = /^10\.0\.\d+\.\d+$/;

function sh(cmd, args) {
  // gyp's msvs generator decodes child-process output with the locale codec
  // (GBK on Chinese Windows) and dies with UnicodeDecodeError on UTF-8 bytes;
  // PYTHONUTF8 makes Python decode as UTF-8 instead.
  execFileSync(cmd, args, {
    cwd: ROOT,
    stdio: 'inherit',
    env: { ...process.env, PYTHONUTF8: '1', PYTHONIOENCODING: 'utf-8' }
  });
}

// Invoke node-gyp through node directly: spawning npx.cmd needs a shell on
// modern Node (EINVAL otherwise), and this keeps the helper shell-free.
function nodeGyp(args) {
  const entry = path.join(ROOT, 'node_modules', 'node-gyp', 'bin', 'node-gyp.js');
  sh(process.execPath, [entry, ...args]);
}

function registryKitsRoots() {
  // The SDK does not have to live under Program Files: this machine keeps it
  // at D:\Windows Kits\10, recorded in the registry. Ask the registry first.
  const keys = [
    'HKLM\\SOFTWARE\\WOW6432Node\\Microsoft\\Windows Kits\\Installed Roots',
    'HKLM\\SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots'
  ];
  const roots = [];
  for (const key of keys) {
    try {
      const out = execFileSync('reg', ['query', key, '/v', 'KitsRoot10'], {
        stdio: ['ignore', 'pipe', 'ignore']
      }).toString();
      const match = /KitsRoot10\s+REG_SZ\s+(.+)/i.exec(out);
      if (match) roots.push(match[1].trim());
    } catch (err) { /* key or value absent */ }
  }
  return roots;
}

function installedSdks() {
  const roots = [
    ...registryKitsRoots(),
    'C:\\Program Files (x86)\\Windows Kits\\10',
    'C:\\Program Files\\Windows Kits\\10',
    process.env.WINDOWS_SDK_ROOT
  ].filter(Boolean);

  const versions = new Set();
  for (const root of roots) {
    const include = path.join(root, 'Include');
    let entries = [];
    try {
      entries = fs.readdirSync(include);
    } catch (err) {
      continue; // root not present
    }
    for (const entry of entries) {
      if (SDK_VERSION_RE.test(entry)) versions.add(entry);
    }
  }
  return [...versions].sort((a, b) => {
    const pa = a.split('.').map(Number);
    const pb = b.split('.').map(Number);
    for (let i = 0; i < 4; i++) {
      if (pa[i] !== pb[i]) return pa[i] - pb[i];
    }
    return 0;
  });
}

function findVcxprojs(dir, out) {
  let entries = [];
  try {
    entries = fs.readdirSync(dir, { withFileTypes: true });
  } catch (err) {
    return out;
  }
  for (const entry of entries) {
    const full = path.join(dir, entry.name);
    if (entry.isDirectory()) {
      // Only the repository's own node_modules is uninteresting; the copy
      // under build/ holds the node-addon-api MSBuild project, which must be
      // retargeted too.
      if (full === path.join(ROOT, 'node_modules')) continue;
      findVcxprojs(full, out);
    } else if (entry.name.endsWith('.vcxproj')) {
      out.push(full);
    }
  }
  return out;
}

function main() {
  if (!process.env.VCPKG_ROOT) {
    console.error('ERROR: VCPKG_ROOT is not set (needs xlnt + libzip installed).');
    process.exit(1);
  }
  if (process.platform !== 'win32') {
    console.error('This helper is Windows-only; use "npm run build" elsewhere.');
    process.exit(1);
  }

  const sdks = installedSdks();
  if (sdks.length === 0) {
    console.error('ERROR: no Windows SDK found under the usual roots. ' +
      'Install one (VS BuildTools: "Windows 11 SDK" component) and retry.');
    process.exit(1);
  }
  const sdk = sdks[sdks.length - 1];
  console.log(`Installed Windows SDKs: ${sdks.join(', ')} -> retargeting to ${sdk}`);

  nodeGyp(['configure']);

  const projects = findVcxprojs(path.join(ROOT, 'build'), []);
  const pattern = /<WindowsTargetPlatformVersion>[^<]*<\/WindowsTargetPlatformVersion>/g;
  let patched = 0;
  for (const project of projects) {
    const before = fs.readFileSync(project, 'utf8');
    const after = before.replace(pattern, `<WindowsTargetPlatformVersion>${sdk}</WindowsTargetPlatformVersion>`);
    if (after !== before) {
      fs.writeFileSync(project, after, 'utf8');
      patched++;
      console.log(`  patched ${path.relative(ROOT, project)}`);
    }
  }
  if (patched === 0) {
    console.error('ERROR: no project pinned a WindowsTargetPlatformVersion; ' +
      'did "node-gyp configure" run?');
    process.exit(1);
  }

  // Stale .pdb/.ipdb/.iobj from an interrupted build make the linker fail
  // with LNK1103 ("debug info corrupt"); drop them first.
  const release = path.join(ROOT, 'build', 'Release');
  for (const name of ['baja_xlsx.pdb', 'baja_xlsx.ipdb', 'baja_xlsx.iobj']) {
    try {
      fs.unlinkSync(path.join(release, name));
    } catch (err) { /* not present */ }
  }

  nodeGyp(['build']);
  console.log(`\nBuild OK (SDK ${sdk}). Remember the runtime DLLs next to ` +
    `build/Release/baja_xlsx.node: see scripts/package-dlls.js ` +
    `(vcpkg bin: bz2/fmt/xlnt/z/zip).`);
}

main();
