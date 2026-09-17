# Baja-Lite-XLSX

[![npm version](https://img.shields.io/npm/v/baja-lite-xlsx.svg)](https://www.npmjs.com/package/baja-lite-xlsx)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

High-performance Node.js native module (N-API) for reading Excel `.xlsx` tables and
extracting embedded images, powered by [xlnt](https://github.com/tfussell/xlnt) + libzip.

## Features

- Read sheet data as JSON rows with one call (`readTableAsJSON` / `readTableAsJSONAsync`)
- File path, `Buffer` and base64 input
- Image extraction: floating images (`twoCellAnchor`), embedded images
  (`oneCellAnchor`) and WPS `DISPIMG`/`cellimages.xml` embedded images
- Non-blocking async variant (libuv thread pool) for servers and Electron
- Read caps (`maxRows` / `maxCols`) and a per-entry size cap against hostile files
- Coded errors (`err.code`) and non-fatal diagnostics (`warnings`)
- Prebuilt binaries for Windows / Linux / macOS (N-API 8, Electron 34)

> All cell values are returned as **strings** (including numbers and dates,
> formatted as `YYYY-MM-DD[ HH:MM:SS]`); image cells return image objects.

## Install

```bash
npm install baja-lite-xlsx
```

The module downloads a prebuilt binary automatically. Windows prebuilds bundle
the required `xlnt.dll` / `zlib1.dll` etc.

## Quick start

```typescript
import { readTableAsJSON } from 'baja-lite-xlsx';
import * as fs from 'fs';

// From a file path
const rows = readTableAsJSON('data.xlsx', {
  sheetName: 'Sheet1',       // default: first sheet
  headerRow: 0,              // header row index (0-based)
  skipRows: [1, 2],          // rows to skip (0-based)
  headerMap: { '名称': 'name', '年龄': 'age' }
});

// From a Buffer
const rows2 = readTableAsJSON(fs.readFileSync('data.xlsx'));

// From base64 (recommended: pass the option explicitly)
const rows3 = readTableAsJSON(base64String, { inputEncoding: 'base64' });

// Non-fatal diagnostics (skipped images, truncated sheets, ...)
const { rows: rows4, warnings } = readTableAsJSON('data.xlsx', { includeWarnings: true });
```

> Note: strings are treated as file paths. When a string matches the base64
> heuristic (long, base64 charset) it must decode to a ZIP "PK" header or an
> `INVALID_INPUT` error is thrown — pass `{ inputEncoding: 'base64' }` explicitly.

## API

### readTableAsJSON(input, options?)

Synchronous. Blocks the calling thread while parsing — for large files or
Electron UIs prefer `readTableAsJSONAsync`.

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `sheetName` | `string` | first sheet | Sheet to read |
| `headerRow` | `number` | `0` | Header row index (0-based, non-negative) |
| `skipRows` | `number[]` | `[]` | Row indices to skip (0-based) |
| `headerMap` | `Record<string,string>` | `{}` | Rename headers in the output objects |
| `inputEncoding` | `'base64'` | – | Force base64 interpretation of string input |
| `maxRows` | `number` | `0` | Cap on rows read per sheet (0 = no cap); excess is truncated and reported via `warnings` |
| `maxCols` | `number` | `0` | Cap on columns read per sheet (0 = no cap) |
| `includeWarnings` | `boolean` | `false` | Return `{ rows, warnings }` instead of `rows` |

Returns `Array<Record<string, string | ImageDataObject | ImageDataObject[]>>`.

Image cells contain `{ data: Buffer, name: string, type: string }` — or an
array of such objects when several images attach to the same cell.

### readTableAsJSONAsync(input, options?)

Same options and results, returns a `Promise`. Parsing runs on the libuv
thread pool.

### Image attachment rules

- **WPS Excel** (`=DISPIMG("ID_...")` formulas): the image referenced by the
  cell's ID is attached to that cell.
- **Embedded images** (standard Excel, `oneCellAnchor`): attached to the
  anchor cell.
- **Floating images** (`twoCellAnchor`): attached to the top-left cell of the
  anchor range.

Unresolvable cases (missing media, unknown relationship ids, drawings that
cannot be mapped to a sheet) do not fail the read; they are reported in
`warnings` when `includeWarnings: true`.

### Errors

Every thrown error carries a machine-readable `code`:

| Code | Meaning |
|------|---------|
| `ADDON_NOT_FOUND` | Native binary missing (run `npm install` / `npm run build`) |
| `FILE_NOT_FOUND` | Input path does not exist |
| `FILE_OPEN_FAILED` | File exists but could not be opened/parsed as a workbook |
| `INVALID_INPUT` | Unsupported input type / base64 without ZIP magic |
| `INVALID_OPTIONS` | Options failed validation (e.g. negative `headerRow`) |
| `NO_SHEETS` | Workbook has no sheets |
| `SHEET_NOT_FOUND` | `options.sheetName` does not match any sheet |
| `HEADER_ROW_OUT_OF_RANGE` | `headerRow` beyond available rows |
| `PARSE_ERROR` / `READ_FAILED` | Workbook parsing failed |

### Resource limits

- Single ZIP entries larger than 256 MB are rejected (protection against
  zip bombs); media entries are capped at 128 MB.
- `maxRows` / `maxCols` limit how much of a sheet is materialized.
- Sheets whose declared dimensions exceed the caps are truncated and a
  warning is recorded.

## Building from source

All platforms use [vcpkg](https://github.com/microsoft/vcpkg) with the
`xlnt` and `libzip` ports, and require the `VCPKG_ROOT` environment variable
(the build fails with an explicit error when it is missing):

```bash
# 1. Install dependencies (triplet per platform)
vcpkg install xlnt:x64-windows zip:x64-windows   # Windows
vcpkg install xlnt:x64-linux zip:x64-linux       # Linux
vcpkg install xlnt:arm64-osx zip:arm64-osx       # macOS (Apple Silicon)

# 2. Set VCPKG_ROOT, e.g. PowerShell:
$env:VCPKG_ROOT = "C:\vcpkg"        # bash: export VCPKG_ROOT=~/vcpkg

# 3. Build + copy runtime DLLs (Windows; plain `npm run build` elsewhere)
npm run build:dev

# 4. Run tests
npm test
```

Note: on Linux/macOS the build links against the vcpkg triplet libraries
shown above; the old `/usr/local` / brew / apt paths are no longer used.

## CI & releases

- Push to `master` / PRs: lint + typecheck, Windows native build, `npm test`.
- Push a tag `v*` (must match `package.json` `version`): the release
  workflow
  1. builds the Windows prebuild archives (napi + electron, DLLs bundled),
  2. runs the tests and generates `prebuilds/checksums.txt`,
  3. publishes the archives to the matching GitHub Release — this is what
     `prebuild-install` downloads during `npm install`,
  4. publishes the package itself to npm (`npm publish --provenance`).
- Linux/macOS prebuilds are not produced yet; users on those platforms
  build from source (see above).

### Releasing a new version

```bash
# 1. bump the version (must equal the future tag, minus the leading "v")
npm version patch --no-git-tag-version     # or minor / major
# 2. update CHANGELOG.md, commit, push
git commit -am "chore: release v1.0.17"
git push origin master
# 3. tag -> triggers build, GitHub Release and npm publish
git tag -a v1.0.17 -m "v1.0.17" && git push origin v1.0.17
```

### npm publishing setup (one-time)

The workflow uses npm **Trusted Publishing** (OIDC), so no `NPM_TOKEN`
secret is stored in the repository:

1. Open https://www.npmjs.com/package/baja-lite-xlsx → *Settings* →
   *Trusted Publisher* → *GitHub Actions*.
2. Fill in: Organization/user `void-soul`, Repository `baja-lite-xlsx`,
   Workflow filename `prebuild.yml`, Environment (leave empty).
3. Save. The next `v*` tag push can publish without any token.

Prefer a token instead? Add a repository secret `NPM_TOKEN` (npm *Access
Token* with publish rights) and replace the publish step with:

```yaml
      - run: npm publish --access public
        env:
          NODE_AUTH_TOKEN: ${{ secrets.NPM_TOKEN }}
```

The published tarball is limited to `index.js`, `index.d.ts`, `src/`,
`scripts/`, `binding.gyp` and the docs (see `package.json` `files`): binaries
are never bundled into the npm package — they are downloaded from the GitHub
Release, keeping the tarball small and always matching the release assets.

## Troubleshooting (Windows)

If the module fails to load with DLL errors:

1. Install the [VC++ Redistributable](https://aka.ms/vs/17/release/vc_redist.x64.exe)
2. Or copy the DLLs from `%VCPKG_ROOT%\installed\x64-windows\bin` into `build/Release`
   (`node scripts/package-dlls.js` does this for you)
3. Set `BAJA_XLSX_TEST_LOAD=1` and re-run the install step to test loading

## Security notes

- This library parses untrusted files: keep it updated; use `maxRows` /
  `maxCols` when consuming third-party files.
- Prebuild archives are hashed with SHA-256 (`npm run checksums` produces
  `prebuilds/checksums.txt`); verify hashes when consuming release artifacts.

## License

MIT — see [LICENSE](LICENSE).
