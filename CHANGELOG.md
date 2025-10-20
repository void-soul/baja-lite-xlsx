# Changelog

All notable changes to this project will be documented in this file.

## [1.0.0] - 2025-10-20

### Added
- Initial release of baja-lite-xlsx native module
- Read Excel (.xlsx) files using xlnt C++ library
- Extract embedded images from Excel files using libzip
- Get image position information (sheet, row, column coordinates)
- Support for Windows, Linux, and macOS
- Comprehensive error handling
- JavaScript API wrapper
- Test suite and examples

### Features
- `readExcel(filepath)` - Read complete Excel data including sheets, images, and positions
- `extractImages(filepath)` - Extract only images from Excel file
- Returns images as Node.js Buffers for easy processing
- High-performance native C++ implementation

### Documentation
- Complete README with API reference
- Installation guide for all platforms
- Usage examples
- Troubleshooting section


