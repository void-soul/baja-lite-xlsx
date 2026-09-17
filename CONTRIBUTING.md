# Contributing

Thanks for considering a contribution to baja-lite-xlsx!

## Development setup

Prerequisites:

- Node.js >= 16
- Python 3 (node-gyp)
- C++17 toolchain (MSVC on Windows, gcc/clang elsewhere)
- [vcpkg](https://github.com/microsoft/vcpkg) with `VCPKG_ROOT` set
  (Windows builds link against `xlnt` and `libzip` from vcpkg)

```bash
# Windows: install dependencies
vcpkg install xlnt:x64-windows zip:x64-windows

# Build + copy runtime DLLs
npm run build:dev

# Run the test suite
npm test
```

On Linux/macOS install `xlnt`/`libzip` from the system package manager
(see README "Building from source") and run `npm run build`.

## Guidelines

- Keep `index.js` free of business logic: input normalization and option
  validation only. Attachment decisions belong in `src/xlsx_reader.cpp`.
- N-API boundary (`src/addon.cpp`) must stay type-conversion only.
- XML scanning must go through `src/xml_parsers.*` helpers — no hardcoded
  character offsets.
- Every error path must produce either a coded error (`err.code`) or a
  warning; silent `catch (...)` blocks are not acceptable.
- Add tests for new behavior in `test/test.js`.
- Update `CHANGELOG.md` (unreleased section) and `index.d.ts` together with
  any API change.

## Releasing

1. Bump the version in `package.json` and update `CHANGELOG.md`.
2. `npm run prebuild` to create platform archives, then `npm run checksums`
   and publish `prebuilds/checksums.txt` alongside the archives.
3. Tag the release on GitHub so `prebuild-install` can find the assets.
