#include "image_extractor.h"
#include <zip.h>
#include <algorithm>
#include <sstream>
#include <cstring>

namespace baja_xlsx {

ImageExtractor::ImageExtractor() {
}

ImageExtractor::~ImageExtractor() {
}

std::string ImageExtractor::getContentType(const std::string& extension) {
    std::string ext = extension;
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    if (ext == ".png") return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".gif") return "image/gif";
    if (ext == ".bmp") return "image/bmp";
    if (ext == ".emf") return "image/x-emf";
    if (ext == ".wmf") return "image/x-wmf";
    
    return "application/octet-stream";
}

bool ImageExtractor::readFileFromZip(void* zipArchive, const std::string& filename, 
                                     std::vector<uint8_t>& outData) {
    zip_t* za = static_cast<zip_t*>(zipArchive);
    
    // Find file in archive
    zip_int64_t index = zip_name_locate(za, filename.c_str(), 0);
    if (index < 0) {
        return false;
    }
    
    // Get file stats
    struct zip_stat sb;
    if (zip_stat_index(za, index, 0, &sb) != 0) {
        return false;
    }
    
    // Read file
    zip_file_t* zf = zip_fopen_index(za, index, 0);
    if (!zf) {
        return false;
    }
    
    outData.resize(sb.size);
    zip_int64_t bytesRead = zip_fread(zf, outData.data(), sb.size);
    zip_fclose(zf);
    
    return bytesRead == static_cast<zip_int64_t>(sb.size);
}

std::map<std::string, std::string> ImageExtractor::parseRelationships(const std::string& xmlContent) {
    std::map<std::string, std::string> rIdMap;
    
    // Parse XML like: <Relationship Id="rId1" Type="..." Target="../media/image1.png"/>
    size_t pos = 0;
    while ((pos = xmlContent.find("<Relationship", pos)) != std::string::npos) {
        size_t endPos = xmlContent.find("/>", pos);
        if (endPos == std::string::npos) {
            endPos = xmlContent.find("</Relationship>", pos);
        }
        if (endPos == std::string::npos) break;
        
        std::string relXml = xmlContent.substr(pos, endPos - pos);
        
        // Extract Id
        std::string rId;
        size_t idPos = relXml.find("Id=\"");
        if (idPos != std::string::npos) {
            size_t idEnd = relXml.find("\"", idPos + 4);
            if (idEnd != std::string::npos) {
                rId = relXml.substr(idPos + 4, idEnd - idPos - 4);
            }
        }
        
        // Extract Target
        std::string target;
        size_t targetPos = relXml.find("Target=\"");
        if (targetPos != std::string::npos) {
            size_t targetEnd = relXml.find("\"", targetPos + 8);
            if (targetEnd != std::string::npos) {
                target = relXml.substr(targetPos + 8, targetEnd - targetPos - 8);
                
                // Extract just the filename from path like "../media/image1.png"
                size_t lastSlash = target.find_last_of('/');
                if (lastSlash != std::string::npos) {
                    target = target.substr(lastSlash + 1);
                }
            }
        }
        
        if (!rId.empty() && !target.empty()) {
            rIdMap[rId] = target;
        }
        
        pos = endPos;
    }
    
    return rIdMap;
}

bool ImageExtractor::parseDrawingXml(const std::string& xmlContent,
                                     const std::string& sheetName,
                                     const std::map<std::string, std::string>& rIdToImageMap,
                                     std::vector<DrawingAnchor>& outAnchors) {
    // This is a simplified parser - for production, use a proper XML library
    // like pugixml or rapidxml
    
    // Parse twoCellAnchor (floating images)
    size_t pos = 0;
    while ((pos = xmlContent.find("<xdr:twoCellAnchor", pos)) != std::string::npos) {
        DrawingAnchor anchor;
        anchor.sheetName = sheetName;
        
        // Find the closing tag
        size_t endPos = xmlContent.find("</xdr:twoCellAnchor>", pos);
        if (endPos == std::string::npos) break;
        
        std::string anchorXml = xmlContent.substr(pos, endPos - pos);
        
        // Parse from coordinates
        size_t fromPos = anchorXml.find("<xdr:from>");
        if (fromPos != std::string::npos) {
            size_t fromEnd = anchorXml.find("</xdr:from>", fromPos);
            if (fromEnd == std::string::npos) fromEnd = anchorXml.length();
            
            std::string fromSection = anchorXml.substr(fromPos, fromEnd - fromPos);
            
            size_t colPos = fromSection.find("<xdr:col>");
            size_t rowPos = fromSection.find("<xdr:row>");
            
            if (colPos != std::string::npos && rowPos != std::string::npos) {
                size_t colEnd = fromSection.find("</xdr:col>", colPos);
                size_t rowEnd = fromSection.find("</xdr:row>", rowPos);
                
                if (colEnd != std::string::npos && rowEnd != std::string::npos) {
                    std::string colStr = fromSection.substr(colPos + 9, colEnd - colPos - 9);  // <xdr:col> is 9 chars
                    std::string rowStr = fromSection.substr(rowPos + 9, rowEnd - rowPos - 9);  // <xdr:row> is 9 chars
                    
                    try {
                        anchor.fromCol = std::stoi(colStr);
                        anchor.fromRow = std::stoi(rowStr);
                    } catch (const std::exception& e) {
                        anchor.fromCol = 0;
                        anchor.fromRow = 0;
                    }
                }
            }
        }
        
        // Parse to coordinates
        size_t toPos = anchorXml.find("<xdr:to>");
        if (toPos != std::string::npos) {
            size_t toEnd = anchorXml.find("</xdr:to>", toPos);
            if (toEnd == std::string::npos) toEnd = anchorXml.length();
            
            std::string toSection = anchorXml.substr(toPos, toEnd - toPos);
            
            size_t colPos = toSection.find("<xdr:col>");
            size_t rowPos = toSection.find("<xdr:row>");
            
            if (colPos != std::string::npos && rowPos != std::string::npos) {
                size_t colEnd = toSection.find("</xdr:col>", colPos);
                size_t rowEnd = toSection.find("</xdr:row>", rowPos);
                
                if (colEnd != std::string::npos && rowEnd != std::string::npos) {
                    std::string colStr = toSection.substr(colPos + 9, colEnd - colPos - 9);  // <xdr:col> is 9 chars
                    std::string rowStr = toSection.substr(rowPos + 9, rowEnd - rowPos - 9);  // <xdr:row> is 9 chars
                    try {
                        anchor.toCol = std::stoi(colStr);
                        anchor.toRow = std::stoi(rowStr);
                    } catch (...) {
                        anchor.toCol = 0;
                        anchor.toRow = 0;
                    }
                }
            }
        }
        
        // Try to find image reference (rId)
        size_t embedPos = anchorXml.find("r:embed=\"");
        if (embedPos != std::string::npos) {
            size_t quoteEnd = anchorXml.find("\"", embedPos + 9);
            if (quoteEnd != std::string::npos) {
                std::string rId = anchorXml.substr(embedPos + 9, quoteEnd - embedPos - 9);
                
                // Map rId to actual image filename
                auto it = rIdToImageMap.find(rId);
                if (it != rIdToImageMap.end()) {
                    anchor.imageName = it->second;
                } else {
                    // Fallback to rId if mapping not found
                    anchor.imageName = rId;
                }
            }
        }
        
        outAnchors.push_back(anchor);
        pos = endPos;
    }
    
    // Parse oneCellAnchor (embedded/cell-based images)
    // These images are anchored to a single cell
    pos = 0;
    while ((pos = xmlContent.find("<xdr:oneCellAnchor", pos)) != std::string::npos) {
        DrawingAnchor anchor;
        anchor.sheetName = sheetName;
        
        // Find the closing tag
        size_t endPos = xmlContent.find("</xdr:oneCellAnchor>", pos);
        if (endPos == std::string::npos) break;
        
        std::string anchorXml = xmlContent.substr(pos, endPos - pos);
        
        // Parse from coordinates
        size_t fromPos = anchorXml.find("<xdr:from>");
        if (fromPos != std::string::npos) {
            size_t fromEnd = anchorXml.find("</xdr:from>", fromPos);
            if (fromEnd == std::string::npos) fromEnd = anchorXml.length();
            
            std::string fromSection = anchorXml.substr(fromPos, fromEnd - fromPos);
            
            size_t colPos = fromSection.find("<xdr:col>");
            size_t rowPos = fromSection.find("<xdr:row>");
            
            if (colPos != std::string::npos && rowPos != std::string::npos) {
                size_t colEnd = fromSection.find("</xdr:col>", colPos);
                size_t rowEnd = fromSection.find("</xdr:row>", rowPos);
                
                if (colEnd != std::string::npos && rowEnd != std::string::npos) {
                    std::string colStr = fromSection.substr(colPos + 9, colEnd - colPos - 9);
                    std::string rowStr = fromSection.substr(rowPos + 9, rowEnd - rowPos - 9);
                    
                    try {
                        anchor.fromCol = std::stoi(colStr);
                        anchor.fromRow = std::stoi(rowStr);
                        // For oneCellAnchor, to is the same as from (embedded image)
                        anchor.toCol = anchor.fromCol;
                        anchor.toRow = anchor.fromRow;
                    } catch (const std::exception& e) {
                        anchor.fromCol = 0;
                        anchor.fromRow = 0;
                        anchor.toCol = 0;
                        anchor.toRow = 0;
                    }
                }
            }
        }
        
        // Try to find image reference (rId)
        size_t embedPos = anchorXml.find("r:embed=\"");
        if (embedPos != std::string::npos) {
            size_t quoteEnd = anchorXml.find("\"", embedPos + 9);
            if (quoteEnd != std::string::npos) {
                std::string rId = anchorXml.substr(embedPos + 9, quoteEnd - embedPos - 9);
                
                // Map rId to actual image filename
                auto it = rIdToImageMap.find(rId);
                if (it != rIdToImageMap.end()) {
                    anchor.imageName = it->second;
                } else {
                    // Fallback to rId if mapping not found
                    anchor.imageName = rId;
                }
            }
        }
        
        if (!anchor.imageName.empty()) {
            outAnchors.push_back(anchor);
        }
        pos = endPos;
    }
    
    return true;
}

bool ImageExtractor::parseCellImagesXml(const std::string& xmlContent,
                                        const std::map<std::string, std::string>& rIdToImageMap,
                                        std::vector<CellImageInfo>& outCellImages) {
    // Parse WPS Excel cellimages.xml format
    // Example:
    // <etc:cellImage>
    //   <xdr:pic>
    //     <xdr:nvPicPr>
    //       <xdr:cNvPr id="2" name="ID_C6F9C8CE7BB34DB9B1BB9835C5297155" />
    //     </xdr:nvPicPr>
    //     <xdr:blipFill>
    //       <a:blip r:embed="rId1" />
    //     </xdr:blipFill>
    //   </xdr:pic>
    // </etc:cellImage>
    
    size_t pos = 0;
    while ((pos = xmlContent.find("<etc:cellImage>", pos)) != std::string::npos) {
        size_t endPos = xmlContent.find("</etc:cellImage>", pos);
        if (endPos == std::string::npos) break;
        
        std::string cellImageXml = xmlContent.substr(pos, endPos - pos);
        
        CellImageInfo cellImg;
        
        // Extract image ID from name attribute
        size_t namePos = cellImageXml.find("name=\"");
        if (namePos != std::string::npos) {
            size_t nameEnd = cellImageXml.find("\"", namePos + 6);
            if (nameEnd != std::string::npos) {
                cellImg.imageId = cellImageXml.substr(namePos + 6, nameEnd - namePos - 6);
            }
        }
        
        // Extract rId from r:embed attribute
        size_t embedPos = cellImageXml.find("r:embed=\"");
        if (embedPos != std::string::npos) {
            size_t embedEnd = cellImageXml.find("\"", embedPos + 9);
            if (embedEnd != std::string::npos) {
                std::string rId = cellImageXml.substr(embedPos + 9, embedEnd - embedPos - 9);
                
                // Map rId to actual image filename
                auto it = rIdToImageMap.find(rId);
                if (it != rIdToImageMap.end()) {
                    cellImg.imageName = it->second;
                }
            }
        }
        
        // Only add if we have both ID and image name
        if (!cellImg.imageId.empty() && !cellImg.imageName.empty()) {
            outCellImages.push_back(cellImg);
        }
        
        pos = endPos;
    }
    
    return true;
}

bool ImageExtractor::extractFromXlsx(const std::string& xlsxPath,
                                     std::vector<ImageInfo>& outImages,
                                     std::vector<DrawingAnchor>& outAnchors) {
    int errorp;
    zip_t* za = zip_open(xlsxPath.c_str(), ZIP_RDONLY, &errorp);
    
    if (!za) {
        zip_error_t error;
        zip_error_init_with_code(&error, errorp);
        lastError_ = std::string("Failed to open XLSX file as ZIP: ") + zip_error_strerror(&error);
        zip_error_fini(&error);
        return false;
    }
    
    // Get number of files in archive
    zip_int64_t numEntries = zip_get_num_entries(za, 0);
    
    // Clear previous cell image mappings
    cellImageMappings_.clear();
    
    // First pass: Parse relationship files to build rId mappings
    std::map<std::string, std::map<std::string, std::string>> drawingRelsMap;
    std::map<std::string, std::string> cellImagesRelsMap;  // WPS Excel cellimages.xml relationships
    
    for (zip_int64_t i = 0; i < numEntries; i++) {
        const char* name = zip_get_name(za, i, 0);
        if (!name) continue;
        
        std::string filename(name);
        
        // Parse drawing relationship files (for floating images)
        if (filename.find("xl/drawings/_rels/") == 0 && filename.find(".xml.rels") != std::string::npos) {
            std::vector<uint8_t> xmlData;
            if (readFileFromZip(za, filename, xmlData)) {
                try {
                    std::string xmlContent(xmlData.begin(), xmlData.end());
                    std::map<std::string, std::string> rIdMap = parseRelationships(xmlContent);
                    
                    // Extract drawing number (e.g., "drawing1.xml.rels" -> "drawing1")
                    size_t lastSlash = filename.find_last_of('/');
                    std::string baseName = (lastSlash != std::string::npos) 
                        ? filename.substr(lastSlash + 1) 
                        : filename;
                    size_t xmlRelsPos = baseName.find(".xml.rels");
                    if (xmlRelsPos != std::string::npos) {
                        baseName = baseName.substr(0, xmlRelsPos);
                    }
                    
                    drawingRelsMap[baseName] = rIdMap;
                } catch (...) {
                    // Ignore parsing errors
                }
            }
        }
        
        // Parse cellimages relationship file (for WPS Excel embedded images)
        if (filename == "xl/_rels/cellimages.xml.rels") {
            std::vector<uint8_t> xmlData;
            if (readFileFromZip(za, filename, xmlData)) {
                try {
                    std::string xmlContent(xmlData.begin(), xmlData.end());
                    cellImagesRelsMap = parseRelationships(xmlContent);
                } catch (...) {
                    // Ignore parsing errors
                }
            }
        }
    }
    
    // Second pass: Extract images and parse drawing XML files
    for (zip_int64_t i = 0; i < numEntries; i++) {
        const char* name = zip_get_name(za, i, 0);
        if (!name) continue;
        
        std::string filename(name);
        
        // Check if it's in the media directory
        if (filename.find("xl/media/") == 0) {
            ImageInfo img;
            
            if (readFileFromZip(za, filename, img.data)) {
                // Extract just the filename without path
                size_t lastSlash = filename.find_last_of('/');
                img.filename = (lastSlash != std::string::npos) 
                    ? filename.substr(lastSlash + 1) 
                    : filename;
                
                // Skip invalid images (empty name or empty data)
                if (img.filename.empty() || img.data.empty()) {
                    continue;
                }
                
                // Determine content type from extension
                size_t dotPos = img.filename.find_last_of('.');
                if (dotPos != std::string::npos) {
                    img.contentType = getContentType(img.filename.substr(dotPos));
                } else {
                    img.contentType = "application/octet-stream";
                }
                
                outImages.push_back(img);
            }
        }
        
        // Parse drawing XML files for image positions
        if (filename.find("xl/drawings/drawing") == 0 && 
            filename.find(".xml") != std::string::npos &&
            filename.find(".rels") == std::string::npos) {
            std::vector<uint8_t> xmlData;
            if (readFileFromZip(za, filename, xmlData)) {
                try {
                    std::string xmlContent(xmlData.begin(), xmlData.end());
                    
                    // Extract drawing number
                    size_t lastSlash = filename.find_last_of('/');
                    std::string baseName = (lastSlash != std::string::npos) 
                        ? filename.substr(lastSlash + 1) 
                        : filename;
                    size_t xmlPos = baseName.find(".xml");
                    if (xmlPos != std::string::npos) {
                        baseName = baseName.substr(0, xmlPos);
                    }
                    
                    // Get corresponding rId mapping
                    std::map<std::string, std::string> rIdMap;
                    auto it = drawingRelsMap.find(baseName);
                    if (it != drawingRelsMap.end()) {
                        rIdMap = it->second;
                    }
                    
                    // Extract sheet name from filename (simplified)
                    std::string sheetName = "Sheet1"; // Default, should be mapped from relationships
                    
                    parseDrawingXml(xmlContent, sheetName, rIdMap, outAnchors);
                } catch (...) {
                    // Ignore XML parsing errors
                }
            }
        }
        
        // Parse cellimages.xml for WPS Excel embedded images
        if (filename == "xl/cellimages.xml") {
            std::vector<uint8_t> xmlData;
            if (readFileFromZip(za, filename, xmlData)) {
                try {
                    std::string xmlContent(xmlData.begin(), xmlData.end());
                    
                    // Use the dedicated cellimages.xml.rels mapping (WPS Excel)
                    parseCellImagesXml(xmlContent, cellImagesRelsMap, cellImageMappings_);
                } catch (...) {
                    // Ignore XML parsing errors
                }
            }
        }
    }
    
    zip_close(za);
    return true;
}

} // namespace baja_xlsx


