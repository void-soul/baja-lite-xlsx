# baja-lite-xlsx

A native Node.js module for reading Excel (.xlsx) files and extracting images using the xlnt C++ library.

[English](README.md) | [简体中文](README.zh-CN.md)

## Features

- 📊 Read Excel (.xlsx) files natively with high performance
- 🖼️ Extract embedded images from Excel files
- 📍 Get image position information (sheet, row, column)
- 🚀 Native C++ implementation using xlnt library
- 💾 Returns images as Node.js Buffers for easy processing

## Prerequisites

Before installing this module, you need to have the following installed:

### Windows

1. **Node.js** (>= 16.0.0)
2. **Python 3.x** (required by node-gyp)
3. **Visual Studio 2019 or newer** with C++ build tools
4. **vcpkg** for C++ package management
5. **xlnt library** installed via vcpkg:

```bash
# Install vcpkg (if not already installed)
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat

# Install xlnt
.\vcpkg install xlnt:x64-windows

# Integrate vcpkg with Visual Studio (optional)
.\vcpkg integrate install
```

**Note**: Update the `binding.gyp` file with your vcpkg installation path if it's not at `C:/vcpkg`.

### Linux

```bash
# Install build tools
sudo apt-get install build-essential python3

# Install xlnt (may need to build from source)
git clone https://github.com/tfussell/xlnt.git
cd xlnt
mkdir build && cd build
cmake ..
make
sudo make install
```

### macOS

```bash
# Install Xcode Command Line Tools
xcode-select --install

# Install xlnt via Homebrew
brew install xlnt
```

## Installation

```bash
npm install
```

This will automatically build the native addon using node-gyp.

If you encounter build errors, try:

```bash
npm run clean
npm run build
```

## Usage

### Reading Complete Excel File

```javascript
const { readExcel } = require('baja-lite-xlsx');

const data = readExcel('./sample.xlsx');

// Access sheets
console.log(data.sheets[0].name);     // Sheet name
console.log(data.sheets[0].data);     // 2D array of cell values

// Access images
console.log(data.images[0].name);     // Image filename
console.log(data.images[0].data);     // Buffer containing image data
console.log(data.images[0].type);     // MIME type (e.g., 'image/png')

// Access image positions
console.log(data.imagePositions[0].sheet);    // Sheet name
console.log(data.imagePositions[0].from);     // { col: 2, row: 5 }
console.log(data.imagePositions[0].to);       // { col: 4, row: 10 }
```

### Extracting Only Images

```javascript
const { extractImages } = require('baja-lite-xlsx');
const fs = require('fs');

const images = extractImages('./sample.xlsx');

images.forEach(img => {
  fs.writeFileSync(img.name, img.data);
  console.log(`Saved ${img.name}`);
});
```

## API Reference

### readExcel(filepath)

Reads an Excel file and returns all data including sheets, images, and image positions.

**Parameters:**
- `filepath` (string): Path to the .xlsx file

**Returns:**
```javascript
{
  sheets: [
    {
      name: string,           // Sheet name
      data: string[][]        // 2D array of cell values
    }
  ],
  images: [
    {
      name: string,           // Image filename
      data: Buffer,           // Image data as Buffer
      type: string            // MIME type (e.g., 'image/png')
    }
  ],
  imagePositions: [
    {
      image: string,          // Image filename
      sheet: string,          // Sheet name
      from: {                 // Top-left position
        col: number,
        row: number
      },
      to: {                   // Bottom-right position
        col: number,
        row: number
      }
    }
  ]
}
```

### extractImages(filepath)

Extracts only images from an Excel file.

**Parameters:**
- `filepath` (string): Path to the .xlsx file

**Returns:**
```javascript
[
  {
    name: string,             // Image filename
    data: Buffer,             // Image data as Buffer
    type: string              // MIME type
  }
]
```

## Examples

See the `examples/` directory for more usage examples:

```bash
node examples/basic.js
```

## Testing

Create a `test/sample.xlsx` file with some data and images, then run:

```bash
npm test
```

## Limitations

- Currently only supports .xlsx format (not .xls)
- Image extraction depends on xlnt library capabilities
- Some advanced Excel features may not be supported

**Note on Image Extraction**: The xlnt library has limited built-in support for image extraction. For full image extraction functionality with position information, you may need to extend the implementation to directly parse the .xlsx ZIP structure and relationship XML files.

## Building from Source

```bash
# Clean previous builds
npm run clean

# Build the native module
npm run build

# Or install and build
npm install
```

## Troubleshooting

### Build Fails on Windows

1. Ensure Visual Studio C++ build tools are installed
2. Check that vcpkg path in `binding.gyp` is correct
3. Verify xlnt is installed: `vcpkg list | findstr xlnt`

### Build Fails on Linux/macOS

1. Ensure xlnt is installed and in the library path
2. Update include paths in `binding.gyp` if needed
3. Check compiler supports C++17

### Module Not Found Error

Make sure you've run `npm install` or `npm run build` before using the module.

## License

MIT

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## Author

Created for high-performance Excel processing in Node.js applications.

