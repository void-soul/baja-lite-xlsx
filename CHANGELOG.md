# Changelog

All notable changes to this project will be documented in this file.

## [1.0.7] - 2025-10-20

### Added
- ✅ New high-level JSON API: `readTableAsJSON()` and `getSheetNames()`
- ✅ Support for multiple input types: file path, Buffer, and base64 string
- ✅ Automatic image attachment to JSON rows based on column names
- ✅ Header mapping support for custom property names
- ✅ Skip rows functionality
- ✅ TypeScript type definitions for all APIs
- ✅ Prebuilt binaries for Windows x64 + Node 20
- ✅ GitHub Actions workflow for automated builds

### Changed
- 🔧 Improved image extraction with proper rId to filename mapping
- 🔧 Fixed image position parsing (column/row coordinates)
- 🔧 Enhanced error handling with try-catch blocks in C++ layer
- 🔧 Updated `binding.gyp` to use environment variables for vcpkg paths
- 📝 Completely rewritten documentation (README.md and README.zh-CN.md)

### Removed
- ❌ Chinese API aliases (`读取表格`, `读取表格SheetName`)
- ❌ Temporary and outdated documentation files

### Fixed
- 🐛 Fixed silent crash when parsing invalid image positions
- 🐛 Fixed image metadata extraction (name, type)
- 🐛 Fixed image position coordinates (fromCol, fromRow, toCol, toRow)
- 🐛 Fixed vcpkg path issues in GitHub Actions

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


