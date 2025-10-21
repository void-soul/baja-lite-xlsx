# baja-lite-xlsx

A high-performance native Node.js module for reading Excel files and extracting embedded images, powered by xlnt C++ library.

[中文文档](./README.zh-CN.md) | English

## Features

- ✅ **Fast & Efficient** - Native C++ implementation for high performance
- ✅ **Excel Reading** - Read data from Excel files (.xlsx)
- ✅ **Image Extraction** - Extract embedded images with position information
- ✅ **Multiple Input Types** - Support file path, Buffer, and base64 string
- ✅ **Multi-Sheet Support** - Get sheet names and read from specific sheets
- ✅ **JSON Output** - Easy-to-use JSON API with automatic image attachment
- ✅ **TypeScript Support** - Full TypeScript type definitions included
- ✅ **Prebuilt Binaries** - Windows x64 prebuilt packages for Node.js 20+ and Electron 34+
- ✅ **Electron Support** - Works seamlessly in Electron applications

## Installation

### Windows + Node.js 20 / Electron 34 (Recommended)

For Windows x64 with Node.js 20+ or Electron 34+, prebuilt binaries are available:

```bash
npm install baja-lite-xlsx
```

✅ **No build tools required!** The precompiled package will be downloaded automatically.

**Supported prebuilt platforms:**
- Windows x64 + Node.js 20+
- Windows x64 + Electron 34+

### Other Environments

For other platforms or Node.js versions, the module will be compiled from source:

```bash
npm install baja-lite-xlsx
```

**Requirements for source compilation:**

<details>
<summary><strong>Windows (Node 16/18 or other versions)</strong></summary>

```bash
# Install build tools
npm install -g windows-build-tools

# Install vcpkg (if not already installed)
git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg
cd C:\vcpkg
.\bootstrap-vcpkg.bat

# Install xlnt library
.\vcpkg install xlnt:x64-windows

# Set environment variable
setx VCPKG_ROOT "C:\vcpkg"

# Now install the package
npm install baja-lite-xlsx
```

</details>

<details>
<summary><strong>Linux</strong></summary>

**Ubuntu/Debian:**
```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake git
npm install baja-lite-xlsx
```

**CentOS/RHEL:**
```bash
sudo yum groupinstall "Development Tools"
sudo yum install cmake git
npm install baja-lite-xlsx
```

</details>

<details>
<summary><strong>macOS</strong></summary>

```bash
# Install Xcode Command Line Tools
xcode-select --install

# Install the package
npm install baja-lite-xlsx
```

</details>

## Quick Start

```javascript
const { readTableAsJSON, getSheetNames } = require('baja-lite-xlsx');

// Get all sheet names
const sheets = getSheetNames('./data.xlsx');
console.log('Sheets:', sheets);

// Read Excel as JSON (with automatic image extraction)
const data = readTableAsJSON('./data.xlsx', {
  headerRow: 0,           // Header row index (default: 0)
  headerMap: {            // Map Chinese headers to English keys
    '姓名': 'name',
    '年龄': 'age',
    '照片': 'photo'
  }
});

console.log(data);
// Output:
// [
//   {
//     name: 'Alice',
//     age: '25',
//     photo: {
//       data: Buffer,           // Image binary data
//       name: 'photo1.png',     // Image filename
//       type: 'image/png'       // MIME type
//     }
//   },
//   ...
// ]
```

## API Documentation

### `getSheetNames(input)`

Get all sheet names from an Excel file.

**Parameters:**
- `input` (string | Buffer): Excel file path, Buffer, or base64 string

**Returns:**
- `string[]`: Array of sheet names

**Example:**
```javascript
const sheets = getSheetNames('./data.xlsx');
// ['Sheet1', 'Sheet2', 'Sheet3']
```

---

### `readTableAsJSON(input, options?)`

Read Excel data as JSON array with automatic image attachment.

**Parameters:**

- `input` (string | Buffer): Excel file path, Buffer, or base64 string
- `options` (object, optional):
  - `sheetName` (string): Sheet name to read (default: first sheet)
  - `headerRow` (number): Header row index (default: 0)
  - `headerMap` (object): Map headers to property names
  - `skipRows` (number[]): Row indices to skip

**Returns:**
- `object[]`: Array of row objects

**Example:**
```javascript
const data = readTableAsJSON('./data.xlsx', {
  headerRow: 0,
  headerMap: {
    '名称': 'name',
    '年龄': 'age',
    '照片': 'photo'
  },
  skipRows: [1, 2]  // Skip row 2 and 3
});
```

**Image Attachment:**

If your Excel has image columns (e.g., column header is `photo1`), images will be automatically attached to matching rows:

```javascript
{
  name: 'Alice',
  age: '25',
  photo1: {
    data: Buffer,           // Image binary data
    name: 'image1.png',     // Image filename
    type: 'image/png'       // MIME type (image/png, image/jpeg, etc.)
  }
}
```

**Saving Images:**
```javascript
const fs = require('fs');
const data = readTableAsJSON('./data.xlsx');

data.forEach((row, i) => {
  if (row.photo && row.photo.data) {
    fs.writeFileSync(`./output/photo_${i}.png`, row.photo.data);
  }
});
```

---

### Input Types

All functions support three input types:

**1. File Path:**
```javascript
readTableAsJSON('./data.xlsx');
```

**2. Buffer:**
```javascript
const fs = require('fs');
const buffer = fs.readFileSync('./data.xlsx');
readTableAsJSON(buffer);
```

**3. Base64 String:**
```javascript
const base64 = buffer.toString('base64');
readTableAsJSON(base64);
```

## Examples

See the [examples](./examples) directory for more detailed usage:

- [basic.js](./examples/basic.js) - Basic usage
- [json-api.js](./examples/json-api.js) - JSON API with all features
- [advanced.js](./examples/advanced.js) - Advanced usage
- [typescript-example.ts](./examples/typescript-example.ts) - TypeScript usage
- [electron-example.js](./examples/electron-example.js) - Electron application usage

Run examples:
```bash
npm run example          # Basic example
npm run example:json     # JSON API example
npm run example:advanced # Advanced example
```

## Developer Guide

### Local Development

**1. Clone the repository:**
```bash
git clone https://github.com/void-soul/baja-lite-xlsx.git
cd baja-lite-xlsx
```

**2. Install dependencies:**
```bash
npm install
```

**3. Set up build environment:**

See [Installation - Other Environments](#other-environments) for platform-specific build tools.

**4. Build the module:**
```bash
npm run build
```

**5. Run tests:**
```bash
npm test
```

### Publishing to npm

**1. Update version:**
```bash
npm version patch  # or minor, major
```

**2. Commit and push:**
```bash
git add .
git commit -m "chore: release v1.0.x"
git push origin master
```

**3. Create and push tag:**
```bash
git tag v1.0.x
git push origin v1.0.x
```

**4. Wait for GitHub Actions:**

The GitHub Actions workflow will automatically:
- ✅ Build precompiled binaries for Windows x64 + Node 20
- ✅ Build precompiled binaries for Windows x64 + Electron 34
- ✅ Create a GitHub Release
- ✅ Upload prebuilt packages to the release

**5. Publish to npm:**
```bash
npm publish
```

**Note:** The prebuilt binaries are hosted on GitHub Releases. Users with matching environments will automatically download them during `npm install`.

### GitHub Actions Workflow

The project uses GitHub Actions for automated builds:

- **Trigger:** On push of tags matching `v*` (e.g., `v1.0.8`)
- **Platforms:** Windows x64
- **Runtimes:** Node.js 20, Electron 34
- **Output:** Prebuilt packages uploaded to GitHub Releases

**Workflow file:** [`.github/workflows/prebuild.yml`](./.github/workflows/prebuild.yml)

## Technical Details

**Core Dependencies:**
- [xlnt](https://github.com/tfussell/xlnt) - C++ library for Excel file manipulation
- [libzip](https://libzip.org/) - ZIP file handling (Excel files are ZIP archives)
- [node-addon-api](https://github.com/nodejs/node-addon-api) - N-API C++ wrapper

**Build System:**
- `node-gyp` - Native addon build tool
- `prebuild` - Precompiled binary creation
- `prebuild-install` - Automatic precompiled binary installation

**Supported Platforms:**
- Windows (x64)
- Linux (x64, ARM64)
- macOS (x64, ARM64)

**Supported Runtimes:**
- Node.js 16.x+
- Node.js 18.x+
- Node.js 20.x+ (prebuilt binaries available for Windows x64)
- Electron 34.x+ (prebuilt binaries available for Windows x64)

## Troubleshooting

<details>
<summary><strong>Build fails on Windows</strong></summary>

1. Ensure Visual Studio 2019+ or Build Tools for Visual Studio is installed
2. Check that `vcpkg` is properly installed and `VCPKG_ROOT` environment variable is set
3. Verify xlnt is installed: `C:\vcpkg\vcpkg list`
4. Try rebuilding: `npm run clean && npm run build`

</details>

<details>
<summary><strong>Module not found after installation</strong></summary>

1. Check that the module is installed: `npm list baja-lite-xlsx`
2. Try reinstalling: `npm uninstall baja-lite-xlsx && npm install baja-lite-xlsx`
3. Clear npm cache: `npm cache clean --force`

</details>

<details>
<summary><strong>Prebuilt binary not found</strong></summary>

This is expected for non-Windows or non-Node 20 environments. The module will automatically fall back to building from source. Ensure you have the required build tools installed.

</details>

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## License

MIT License - see [LICENSE](./LICENSE) file for details.

## Author

DEDEDE

## Links

- [GitHub Repository](https://github.com/void-soul/baja-lite-xlsx)
- [npm Package](https://www.npmjs.com/package/baja-lite-xlsx)
- [Report Issues](https://github.com/void-soul/baja-lite-xlsx/issues)
- [Changelog](./CHANGELOG.md)
