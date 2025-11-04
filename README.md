# baja-lite-xlsx

A lightweight, high-performance Node.js library for reading Excel files with automatic image extraction support. Built with native C++ for maximum speed.

[中文文档](./README.zh-CN.md)

## Features

- ✅ **Read Excel tables as JSON** - Simple API to convert Excel to JavaScript objects
- ✅ **Automatic image extraction** - Supports both floating and embedded images
- ✅ **WPS Excel support** - Fully compatible with WPS Office embedded images (DISPIMG formula)
- ✅ **High performance** - Native C++ implementation using xlnt library
- ✅ **TypeScript support** - Full type definitions included
- ✅ **Multiple input formats** - File path, Buffer, or base64 string
- ✅ **Cross-platform** - Prebuilt binaries for Windows, Linux, macOS

## Installation

```bash
npm install baja-lite-xlsx
```

The package includes prebuilt binaries. If a binary isn't available for your platform, it will automatically compile from source (requires build tools).

## Quick Start

```javascript
const { readTableAsJSON } = require('baja-lite-xlsx');
const fs = require('fs');

// Read Excel file as JSON
const data = readTableAsJSON('./sample.xlsx', {
  headerRow: 0,
  headerMap: {
    '姓名': 'name',
    '年龄': 'age',
    '照片': 'photo'
  }
});

// Access data
data.forEach(row => {
  console.log(row.name, row.age);
  
  // Images are automatically extracted as { data: Buffer, name, type }
  if (row.photo && row.photo.data) {
    fs.writeFileSync(`${row.name}.png`, row.photo.data);
  }
});
```

## API Reference

### `readTableAsJSON(input, options)`

Reads an Excel file and returns an array of objects.

**Parameters:**
- `input` (string | Buffer): Excel file path, Buffer, or base64 string
- `options` (object):
  - `sheetName` (string): Sheet name to read (defaults to first sheet)
  - `headerRow` (number): Header row index, 0-based (default: 0)
  - `skipRows` (number[]): Row indices to skip
  - `headerMap` (object): Map original headers to new property names

**Returns:** `Array<Object>` - Each object represents a row

**Image Support:**
- Floating images (twoCellAnchor) ✅
- Embedded images - Standard Excel (oneCellAnchor) ✅
- Embedded images - WPS Excel (DISPIMG formula) ✅

All images are returned as:
```javascript
{
  data: Buffer,      // Image binary data
  name: 'image.png', // Filename
  type: 'image/png'  // MIME type
}
```

## TypeScript Usage

```typescript
import { readTableAsJSON, ImageDataObject } from 'baja-lite-xlsx';

interface Employee {
  name: string;
  age: string;
  photo: string | ImageDataObject;
}

const data = readTableAsJSON('./sample.xlsx', {
  headerMap: {
    '姓名': 'name',
    '年龄': 'age',
    '照片': 'photo'
  }
}) as Employee[];
```

## Build from Source

### Prerequisites

#### Windows
```bash
# 1. Install Visual Studio Build Tools 2019+
# Download from: https://visualstudio.microsoft.com/downloads/

# 2. Install vcpkg
cd C:\
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat

# 3. Install dependencies
.\vcpkg install xlnt:x64-windows
.\vcpkg install libzip:x64-windows

# 4. Set environment variable
set VCPKG_ROOT=C:\vcpkg
```

Or use the automated script:
```bash
check-vcpkg.bat
```

#### Linux
```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install build-essential
sudo apt-get install libxlnt-dev libzip-dev

# Fedora/RHEL
sudo dnf install gcc-c++ make
sudo dnf install xlnt-devel libzip-devel
```

#### macOS
```bash
# Install Xcode Command Line Tools
xcode-select --install

# Install dependencies via Homebrew
brew install xlnt libzip
```

### Compile

```bash
# Clone repository
git clone https://github.com/void-soul/baja-lite-xlsx.git
cd baja-lite-xlsx

# Install dependencies
npm install

# Build native module
npm run build

# Or use the local build script (Windows)
build-local.bat
```

## Create Prebuilt Packages

For library maintainers who want to create prebuilt binaries:

### Windows
```bash
# Run the prebuild script
scripts\create-prebuilds.bat

# This will:
# 1. Clean old builds
# 2. Compile the native module
# 3. Copy DLL files
# 4. Create .tar.gz packages for N-API and Electron
# 5. Package DLL files into the archives
```

The prebuilt packages will be in the `prebuilds/` directory.

## Publishing

### Publish to npm

```bash
# 1. Update version in package.json
npm version patch  # or minor, major

# 2. Build prebuilt packages
scripts\create-prebuilds.bat

# 3. Publish to npm
npm publish
```

### Publish to GitHub Releases

The library uses GitHub Releases to host prebuilt binaries. The workflow:

1. **Create prebuilt packages locally:**
   ```bash
   scripts\create-prebuilds.bat
   ```

2. **Create a git tag:**
   ```bash
   git tag v1.0.16
   git push origin v1.0.16
   ```

3. **Upload to GitHub Releases:**
   - Go to: https://github.com/void-soul/baja-lite-xlsx/releases
   - Click "Create a new release"
   - Select your tag (e.g., v1.0.16)
   - Upload files from `prebuilds/` directory
   - Publish release

4. **GitHub Actions (Automated):**

   You can also use GitHub Actions to automate the build and release process. Create `.github/workflows/release.yml`:

   ```yaml
   name: Build and Release
   
   on:
     push:
       tags:
         - 'v*'
   
   jobs:
     build-windows:
       runs-on: windows-latest
       steps:
         - uses: actions/checkout@v3
         - uses: actions/setup-node@v3
           with:
             node-version: '20'
         
         - name: Setup vcpkg
           run: |
             git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg
             C:\vcpkg\bootstrap-vcpkg.bat
             C:\vcpkg\vcpkg install xlnt:x64-windows libzip:x64-windows
           
         - name: Build
           run: |
             set VCPKG_ROOT=C:\vcpkg
             npm install
             npm run prebuild
           
         - name: Upload Release Assets
           uses: actions/upload-release-asset@v1
           env:
             GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}
           with:
             upload_url: ${{ github.event.release.upload_url }}
             asset_path: ./prebuilds/*.tar.gz
             asset_name: prebuild-${{ matrix.os }}.tar.gz
             asset_content_type: application/gzip
   ```

## Examples

See the `examples/` directory for more usage examples:
- `basic.js` - Basic usage
- `json-api.js` - JSON API with image extraction
- `advanced.js` - Advanced features
- `typescript-example.ts` - TypeScript example

## License

MIT

## Author

DEDEDE

## Repository

https://github.com/void-soul/baja-lite-xlsx
