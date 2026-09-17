# Changelog

## 1.0.16 (2026-09-17) — audit remediation

Security & robustness:

- Reject ZIP entries with no declared size or above the size caps
  (256 MB generic / 128 MB media) — zip-bomb protection.
- Add `maxRows` / `maxCols` read caps with warning-reported truncation.
- Remove the floating-image substring "fuzzy match" that could attach the
  wrong image to a cell.
- Base64 heuristic now requires ZIP "PK" magic after decoding; explicit
  `inputEncoding: 'base64'` option added.
- SHA-256 checksum generation for prebuild archives (`npm run checksums`).

Fixes:

- WPS workbooks with proprietary relationship types (e.g.
  `http://www.wps.cn/officeDocument/2020/cellImage`) and/or backslash ZIP
  entry separators no longer fail with
  `xlnt::exception: key not found in container`: loading is retried through
  a sanitized copy that normalizes entry names to `/` and strips
  non-standard relationship types / content-type overrides (reported via
  `warnings`); images are still extracted from the original file, and image
  extraction itself now tolerates backslash entry names.
- Number cells no longer forced to 6 fixed decimals (shortest round-trip
  formatting); date cells render as deterministic `YYYY-MM-DD[ HH:MM:SS]`.
- Drawing anchors are mapped to their sheets via `workbook.xml` +
  relationships (was hardcoded "Sheet1": images misattached/missing on
  multi-sheet workbooks).
- Image extraction failure no longer discards sheet data (fail-open with
  warnings); all "silently swallowed" error paths now record warnings.
- XML scanning rewritten without hardcoded offsets; tolerant of attribute
  order, quote style and whitespace; malformed anchors are skipped with a
  warning instead of silently attaching to A1.
- Copy of each image shared across cells (one Buffer per image, not per cell).
- `fs.existsSync` race window narrowed; coded errors everywhere
  (`err.code`); friendly `headerRow` validation errors.
- Temp files for Buffer/base64 input are cleaned up from crashed runs.

API:

- New `readTableAsJSONAsync`: non-blocking parse on the libuv thread pool
  (Promise). `readTableAsJSON` remains synchronous.
- Removed the dead native `extractImages` export and empty placeholder
  methods (they always returned `[]` silently).

Engineering:

- npm publishing automated in the release workflow via npm Trusted
  Publishing (OIDC, no stored token); runs only after the GitHub Release
  assets exist and skips versions already on the registry.
- `package.json` `files` whitelist added so the npm tarball ships only the
  JS binding, C++ sources, build scripts and docs (no stale prebuild
  archives or example workbooks).
- Examples rewritten against the real API (previously crashed on import).
- README rewritten to match the actual API.
- `npm test` now runs a real regression suite (`test/test.js`).
- vcpkg lookup unified in `scripts/lib/vcpkg.js`; `VCPKG_ROOT` is validated
  with a clear error during builds.
- CI workflow (lint/typecheck, Windows native build + test) with a
  tag-triggered `release` job: builds Windows prebuilds (napi + electron,
  DLLs bundled), verifies checksums and publishes them to the matching
  GitHub Release so `prebuild-install` resolves on install.
- `binding.gyp` Linux/macOS sections now consume `VCPKG_ROOT` instead of
  hardcoded `/usr/local` paths.

## 1.0.15

Previous release (public history not reconstructed in this file).
