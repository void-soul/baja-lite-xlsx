#ifndef IMAGE_EXTRACTOR_H
#define IMAGE_EXTRACTOR_H

#include <string>
#include <vector>
#include <map>
#include <zip.h>

namespace baja_xlsx {

struct ImageInfo {
    std::string filename;
    std::vector<uint8_t> data;
    std::string contentType;
};

struct DrawingAnchor {
    std::string imageName;
    std::string sheetName;
    int fromCol;
    int fromRow;
    int toCol;
    int toRow;
};

// WPS Excel embedded image info (from cellimages.xml)
struct CellImageInfo {
    std::string imageId;      // e.g., "ID_C6F9C8CE7BB34DB9B1BB9835C5297155"
    std::string imageName;    // e.g., "image1.png"
};

// Orchestrates media/anchor extraction from the .xlsx package.
// Previously a 506-line god file (AUDIT-20260917-015); ZIP I/O and XML
// parsing now live in zip_reader / xml_parsers.
class ImageExtractor {
public:
    // Extracts images, anchors and WPS cell-image mappings.
    // Non-fatal problems are appended to `warnings` instead of being
    // silently swallowed (AUDIT-20260917-009).
    bool extractFromXlsx(const std::string& xlsxPath,
                         std::vector<ImageInfo>& outImages,
                         std::vector<DrawingAnchor>& outAnchors,
                         std::vector<CellImageInfo>& outCellImages,
                         std::vector<std::string>& warnings);

    std::string getLastError() const { return lastError_; }

private:
    std::string lastError_;

    std::string getContentType(const std::string& extension);

    // Builds drawing base name ("drawing1") -> sheet title mapping from
    // xl/workbook.xml + workbook/worksheet relationships
    // (AUDIT-20260917-005: replaces the hardcoded "Sheet1").
    std::map<std::string, std::string> buildDrawingSheetMap(
        zip_t* za, std::vector<std::string>& warnings);

    void parseDrawingXml(const std::string& xmlContent,
                         const std::string& sheetName,
                         const std::map<std::string, std::string>& rIdToImageMap,
                         std::vector<DrawingAnchor>& outAnchors,
                         std::vector<std::string>& warnings);

    void parseCellImagesXml(const std::string& xmlContent,
                            const std::map<std::string, std::string>& rIdToImageMap,
                            std::vector<CellImageInfo>& outCellImages,
                            std::vector<std::string>& warnings);
};

} // namespace baja_xlsx

#endif // IMAGE_EXTRACTOR_H
