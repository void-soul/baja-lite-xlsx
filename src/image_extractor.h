#ifndef IMAGE_EXTRACTOR_H
#define IMAGE_EXTRACTOR_H

#include <string>
#include <vector>
#include <map>

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

class ImageExtractor {
public:
    ImageExtractor();
    ~ImageExtractor();
    
    // Extract images by directly reading the .xlsx ZIP file
    bool extractFromXlsx(const std::string& xlsxPath,
                        std::vector<ImageInfo>& outImages,
                        std::vector<DrawingAnchor>& outAnchors);
    
    // Get cell image mappings (WPS Excel format)
    const std::vector<CellImageInfo>& getCellImageMappings() const { return cellImageMappings_; }
    
    std::string getLastError() const { return lastError_; }
    
private:
    std::string lastError_;
    std::vector<CellImageInfo> cellImageMappings_;  // WPS Excel cell image ID to filename mapping
    
    // Helper to read file from ZIP
    bool readFileFromZip(void* zipArchive, const std::string& filename, 
                        std::vector<uint8_t>& outData);
    
    // Parse drawing XML to get image positions
    bool parseDrawingXml(const std::string& xmlContent,
                        const std::string& sheetName,
                        const std::map<std::string, std::string>& rIdToImageMap,
                        std::vector<DrawingAnchor>& outAnchors);
    
    // Parse cellimages.xml (WPS Excel embedded images)
    bool parseCellImagesXml(const std::string& xmlContent,
                           const std::map<std::string, std::string>& rIdToImageMap,
                           std::vector<CellImageInfo>& outCellImages);
    
    // Parse relationship XML to map rId to image filenames
    std::map<std::string, std::string> parseRelationships(const std::string& xmlContent);
    
    // Get content type from content types XML
    std::string getContentType(const std::string& extension);
};

} // namespace baja_xlsx

#endif // IMAGE_EXTRACTOR_H


