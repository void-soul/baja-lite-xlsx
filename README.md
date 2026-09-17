# Baja-Lite-XLSX

[![npm version](https://img.shields.io/npm/v/baja-lite-xlsx.svg)](https://www.npmjs.com/package/baja-lite-xlsx)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

**English** | [简体中文](./README-CN.md)

High-performance Node.js native module (N-API) for reading Excel `.xlsx` tables and
extracting embedded images, built on [xlnt](https://github.com/tfussell/xlnt) and libzip.

## Features

- Read a sheet as JSON rows with a single call: `readTableAsJSON` / `readTableAsJSONAsync`
- Input as file path, `Buffer`, or base64 string
- Image extraction: floating images (`twoCellAnchor`), embedded images
  (`oneCellAnchor`) and WPS `DISPIMG` / `cellimages.xml` images
- Robust against real-world files: WPS workbooks with proprietary relationship
  types or backslash ZIP entry names are loaded through a sanitized copy
- Non-blocking async variant running on the libuv thread pool
- Read caps (`maxRows` / `maxCols`) and per-entry size limits against hostile files
- Coded errors (`err.code`) plus non-fatal diagnostics (`warnings`)
- Prebuilt binaries for Windows / Linux / macOS as **N-API v8**: one binary per
  platform+architecture covers every Node.js >= 16 and every Electron version

> Every cell value is returned as a **string** (numbers and dates included;
> dates are formatted as `YYYY-MM-DD[ HH:MM:SS]`). Cells containing pictures
> return image objects instead.

## Install

```bash
npm install baja-lite-xlsx
```

The matching prebuilt binary is downloaded during install. When no prebuild is
available for your platform, the package compiles from source (see
[Building from source](#building-from-source)); on Windows the prebuild ships
the required runtime DLLs.

## Quick start

```javascript
const { readTableAsJSON, readTableAsJSONAsync } = require('baja-lite-xlsx');
const fs = require('fs');

// From a file path
const rows = readTableAsJSON('data.xlsx', {
  sheetName: 'Sheet1',                                  // default: first sheet
  headerRow: 0,                                         // header row (0-based)
  skipRows: [1, 2],                                     // rows to skip
  headerMap: { 'name': 'fullName', 'age': 'age' }       // rename headers
});

// From a Buffer
const rows2 = readTableAsJSON(fs.readFileSync('data.xlsx'));

// From base64 (pass the option explicitly; the heuristic requires ZIP magic)
const rows3 = readTableAsJSON(base64String, { inputEncoding: 'base64' });

// Non-fatal diagnostics (unattached images, truncated sheets, ...)
const { rows: rows4, warnings } = readTableAsJSON('data.xlsx', { includeWarnings: true });

// Async: parsing runs off the event loop
const rows5 = await readTableAsJSONAsync('big.xlsx', { maxRows: 100000 });
```

## API

### readTableAsJSON(input, options?)

Synchronous. Blocks the calling thread while parsing — prefer
`readTableAsJSONAsync` for large files, servers, or Electron UIs.

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `sheetName` | `string` | first sheet | Sheet to read |
| `headerRow` | `number` | `0` | Header row index (0-based, non-negative) |
| `skipRows` | `number[]` | `[]` | Row indices to skip (0-based) |
| `headerMap` | `Record<string,string>` | `{}` | Rename headers in the output objects |
| `inputEncoding` | `'base64'` | – | Force base64 interpretation of string input |
| `maxRows` | `number` | `0` | Cap on rows read per sheet (0 = no cap); excess rows are truncated and reported via `warnings` |
| `maxCols` | `number` | `0` | Cap on columns read per sheet (0 = no cap) |
| `includeWarnings` | `boolean` | `false` | Return `{ rows, warnings }` instead of the rows array |

Returns `Array<Record<string, string | ImageDataObject | ImageDataObject[]>>`.

An image cell contains `{ data: Buffer, name: string, type: string }` — or an
array of such objects when several images attach to one cell.

### readTableAsJSONAsync(input, options?)

Same options and result, returns a `Promise`. Parsing runs on the libuv thread
pool, so the event loop (and any Electron UI) stays responsive.

### Image attachment rules

- **WPS embedded images** (`=DISPIMG("ID_...")` formulas): attached by image ID.
- **Embedded images** (standard Excel, `oneCellAnchor`): attached to the anchor cell.
- **Floating images** (`twoCellAnchor`): attached to the top-left cell of the anchor range.

Unresolvable cases (missing media, unknown relationship ids, drawings that
cannot be mapped to a sheet) never fail the read; they appear in `warnings`
when `includeWarnings` is enabled.

### Errors

Every thrown error carries a machine-readable `code`:

| Code | Meaning |
|------|---------|
| `ADDON_LOAD_FAILED` / `ADDON_NOT_FOUND` | Native binary missing or its runtime DLLs are missing |
| `FILE_NOT_FOUND` | Input path does not exist |
| `FILE_OPEN_FAILED` | File exists but could not be parsed as a workbook |
| `INVALID_INPUT` | Unsupported input type / base64 without ZIP magic |
| `INVALID_OPTIONS` | Options failed validation (e.g. negative `headerRow`) |
| `NO_SHEETS` | Workbook has no sheets |
| `SHEET_NOT_FOUND` | `options.sheetName` matches no sheet |
| `HEADER_ROW_OUT_OF_RANGE` | `headerRow` beyond the available rows |
| `PARSE_ERROR` / `READ_FAILED` | Workbook parsing failed |

### Resource limits

- ZIP entries without a declared size, or above 256 MB, are rejected; media
  entries are capped at 128 MB (protection against zip bombs).
- `maxRows` / `maxCols` limit how much of a sheet is materialized.
- Truncation is always reported as a warning, never silently applied.

## Building from source

All platforms use [vcpkg](https://github.com/microsoft/vcpkg) with the `xlnt`
and `libzip` ports and require the `VCPKG_ROOT` environment variable (the build
fails with an explicit error when it is missing):

```bash
# 1. Install dependencies (triplet per platform)
vcpkg install xlnt:x64-windows libzip:x64-windows   # Windows x64
vcpkg install xlnt:x64-linux   libzip:x64-linux     # Linux x64
vcpkg install xlnt:arm64-osx   libzip:arm64-osx     # macOS (Apple Silicon)

# 2. Point the build at vcpkg
#    Windows:  set VCPKG_ROOT=C:\vcpkg
#    bash:     export VCPKG_ROOT=~/vcpkg

# 3. Build (Windows also copies the runtime DLLs)
npm run build:dev     # or: npm run build

# 4. Run the tests
npm test
```

## CI & releases

- Push to `master` / pull requests: lint + typecheck, native build, `npm test`.
- Push a tag `v*` (must equal `package.json` `version`): the release workflow
  1. builds the N-API prebuild archives for **win32-x64, linux-x64,
     linux-arm64, darwin-arm64, darwin-x64** (Windows archives bundle the
     runtime DLLs),
  2. runs the tests and verifies every archive contains the `.node` module,
  3. publishes the archives plus `checksums.txt` to the matching GitHub Release,
  4. publishes the package to npm with provenance (`npm publish --provenance`).

Only the N-API v8 binary is produced per platform: the ABI is stable, so
per-Node/per-Electron-version builds are unnecessary.

### Releasing a new version

```bash
# 1. bump the version (must equal the future tag without the leading "v")
npm version patch --no-git-tag-version     # or minor / major
# 2. update CHANGELOG.md, commit in English, push
git commit -am "chore: release v1.0.17"
git push origin master
# 3. tag -> builds, GitHub Release, npm publish
git tag -a v1.0.17 -m "v1.0.17" && git push origin v1.0.17
```

### npm publishing setup (one-time)

The workflow uses npm **Trusted Publishing** (OIDC), so no `NPM_TOKEN` secret
is stored in the repository:

1. Open <https://www.npmjs.com/package/baja-lite-xlsx> → *Settings* →
   *Trusted Publisher* → *GitHub Actions*.
2. Fill in: Organization/user `void-soul`, Repository `baja-lite-xlsx`,
   Workflow filename `prebuild.yml`, Environment (leave empty).
3. Save. The next `v*` tag can publish without any token.

Prefer a token? Add a repository secret `NPM_TOKEN` and replace the publish
step with:

```yaml
      - run: npm publish --access public
        env:
          NODE_AUTH_TOKEN: ${{ secrets.NPM_TOKEN }}
```

The published tarball only contains `index.js`, `index.d.ts`, `src/`,
`scripts/`, `binding.gyp` and the docs (see `package.json` `files`): binaries
are never bundled into the npm package — they come from the GitHub Release,
keeping the tarball small and always matching the release assets.

## Troubleshooting (Windows)

If the module fails to load:

1. Install the [VC++ Redistributable](https://aka.ms/vs/17/release/vc_redist.x64.exe).
2. Copy the DLLs from `%VCPKG_ROOT%\installed\x64-windows\bin` next to
   `baja_xlsx.node` — `node scripts/package-dlls.js` does this for you.
3. Set `BAJA_XLSX_TEST_LOAD=1` and re-run the install step to see the loader error.

## Security notes

- The library parses untrusted files: keep it updated and use `maxRows` /
  `maxCols` when consuming third-party workbooks.
- Prebuild archives are hashed with SHA-256 (`npm run checksums` writes
  `prebuilds/checksums.txt`); verify hashes when consuming release artifacts.

## License

MIT — see [LICENSE](LICENSE).
