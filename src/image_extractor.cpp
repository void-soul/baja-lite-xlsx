#include "image_extractor.h"
#include "zip_reader.h"
#include "xml_parsers.h"
#include <algorithm>
#include <cctype>

namespace baja_xlsx {

namespace {

// XML parts are small relative to media; cap them well below the media cap.
const size_t kMaxXmlBytes = 16u * 1024u * 1024u;

// Conservative per-media-entry cap used for image extraction.
const size_t kMaxMediaBytes = 128u * 1024u * 1024u;

std::string toLowerAscii(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

// OPC requires '/' as the ZIP entry separator, but some producers (notably
// WPS) write '\\'. Normalize for pattern matching; the raw name is still
// used for zip lookups, which must match the archive byte-for-byte.
std::string normalizeEntryName(const std::string& name) {
    std::string out = name;
    std::replace(out.begin(), out.end(), '\\', '/');
    return out;
}

// Reads an entry addressed by its canonical ('/') path even when the
// archive stored it with a different separator.
bool readFileByName(zip_t* za, const std::string& wanted,
                    std::vector<uint8_t>& out, size_t maxBytes,
                    std::string& error) {
    if (zipio::readFile(za, wanted, out, maxBytes, error)) {
        return true;
    }
    const zip_int64_t numEntries = zip_get_num_entries(za, 0);
    for (zip_int64_t i = 0; i < numEntries; ++i) {
        const char* entryName = zip_get_name(za, i, 0);
        if (!entryName) continue;
        if (normalizeEntryName(entryName) == wanted) {
            return zipio::readFile(za, entryName, out, maxBytes, error);
        }
    }
    error.clear();
    return false;
}

} // namespace

std::string ImageExtractor::getContentType(const std::string& extension) {
    const std::string ext = toLowerAscii(extension);
    if (ext == ".png") return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".gif") return "image/gif";
    if (ext == ".bmp") return "image/bmp";
    if (ext == ".emf") return "image/x-emf";
    if (ext == ".wmf") return "image/x-wmf";
    return "application/octet-stream";
}

std::map<std::string, std::string> ImageExtractor::buildDrawingSheetMap(
    zip_t* za, std::vector<std::string>& warnings) {

    std::map<std::string, std::string> drawingToSheet;
    std::vector<uint8_t> data;
    std::string err;

    // 1) xl/workbook.xml: sheet name + r:id
    if (!readFileByName(za, "xl/workbook.xml", data, kMaxXmlBytes, err)) {
        warnings.push_back("workbook.xml missing; drawings cannot be mapped to sheets");
        return drawingToSheet;
    }
    const std::string workbookXml(data.begin(), data.end());

    std::map<std::string, std::string> ridToName;
    size_t pos = 0;
    while ((pos = xmlp::findTagOpen(workbookXml, "sheet", pos)) != std::string::npos) {
        std::string name;
        std::string rid;
        xmlp::getAttribute(workbookXml, pos, "name", name);
        xmlp::getAttribute(workbookXml, pos, "r:id", rid);
        if (!name.empty() && !rid.empty()) {
            ridToName[rid] = name;
        }
        pos += 6; // past "<sheet"
    }

    // 2) xl/_rels/workbook.xml.rels: rId -> "worksheets/sheetN.xml"
    if (!readFileByName(za, "xl/_rels/workbook.xml.rels", data, kMaxXmlBytes, err)) {
        warnings.push_back("workbook.xml.rels missing; drawings cannot be mapped to sheets");
        return drawingToSheet;
    }
    const std::string workbookRels(data.begin(), data.end());
    const std::map<std::string, std::string> ridToTarget =
        xmlp::parseRelationships(workbookRels);

    std::map<std::string, std::string> sheetTargetToName; // "sheet1.xml" -> title
    for (const auto& kv : ridToTarget) {
        auto nameIt = ridToName.find(kv.first);
        if (nameIt != ridToName.end()) {
            sheetTargetToName[kv.second] = nameIt->second;
        }
    }

    // 3) xl/worksheets/_rels/sheetN.xml.rels: drawingN.xml -> sheet title
    const zip_int64_t numEntries = zip_get_num_entries(za, 0);
    for (zip_int64_t i = 0; i < numEntries; ++i) {
        const char* entryName = zip_get_name(za, i, 0);
        if (!entryName) continue;
        const std::string rawName(entryName);
        const std::string filename = normalizeEntryName(rawName);
        if (filename.find("xl/worksheets/_rels/") != 0) continue;
        if (filename.find(".xml.rels") == std::string::npos) continue;

        std::vector<uint8_t> relData;
        if (!zipio::readFile(za, rawName, relData, kMaxXmlBytes, err)) {
            warnings.push_back("Failed to read worksheet relationships: " + filename);
            continue;
        }
        const std::string relContent(relData.begin(), relData.end());

        const size_t slash = filename.find_last_of('/');
        const std::string base = (slash != std::string::npos)
                                     ? filename.substr(slash + 1) : filename;
        const size_t extPos = base.find(".xml.rels");
        const std::string sheetFile =
            (extPos != std::string::npos) ? base.substr(0, extPos) : base; // "sheet1.xml"

        auto targetIt = sheetTargetToName.find(sheetFile);
        if (targetIt == sheetTargetToName.end()) continue;
        const std::string& sheetName = targetIt->second;

        const std::map<std::string, std::string> rels =
            xmlp::parseRelationships(relContent);
        for (const auto& kv : rels) {
            if (kv.second.find("drawing") == std::string::npos) continue;
            const size_t dot = kv.second.find(".xml");
            const std::string drawingBase =
                (dot != std::string::npos) ? kv.second.substr(0, dot) : kv.second;
            drawingToSheet[drawingBase] = sheetName;
        }
    }

    return drawingToSheet;
}

void ImageExtractor::parseDrawingXml(const std::string& xmlContent,
                                     const std::string& sheetName,
                                     const std::map<std::string, std::string>& rIdToImageMap,
                                     std::vector<DrawingAnchor>& outAnchors,
                                     std::vector<std::string>& warnings) {
    struct AnchorSpec {
        const char* tag;
        bool floating;
    };
    const AnchorSpec specs[2] = {
        { "twoCellAnchor", true },
        { "oneCellAnchor", false },
    };

    for (const auto& spec : specs) {
        const std::string closing = std::string("</") + spec.tag + ">";
        size_t pos = 0;
        while ((pos = xmlp::findTagOpen(xmlContent, spec.tag, pos)) != std::string::npos) {
            const size_t endPos = xmlContent.find(closing, pos);
            if (endPos == std::string::npos) break;

            auto skipAnchor = [&]() {
                pos = endPos + closing.size();
            };

            DrawingAnchor anchor;
            anchor.sheetName = sheetName;
            anchor.fromCol = 0;
            anchor.fromRow = 0;
            anchor.toCol = 0;
            anchor.toRow = 0;

            // from coordinates (required)
            const size_t fromTag = xmlp::findTagOpen(xmlContent, "xdr:from", pos);
            if (fromTag == std::string::npos || fromTag >= endPos) {
                warnings.push_back(std::string("Missing <xdr:from> in ") +
                                   spec.tag + " for sheet '" + sheetName + "': skipped");
                skipAnchor();
                continue;
            }
            std::string colStr;
            std::string rowStr;
            if (!xmlp::getElementText(xmlContent, "xdr:col", fromTag, colStr) ||
                !xmlp::getElementText(xmlContent, "xdr:row", fromTag, rowStr)) {
                warnings.push_back(std::string("Incomplete <xdr:from> in ") +
                                   spec.tag + " for sheet '" + sheetName + "': skipped");
                skipAnchor();
                continue;
            }
            int col = 0;
            int row = 0;
            if (!xmlp::parseNonNegativeInt(colStr, col) ||
                !xmlp::parseNonNegativeInt(rowStr, row)) {
                warnings.push_back(std::string("Invalid anchor coordinates in ") +
                                   spec.tag + " for sheet '" + sheetName + "': skipped");
                skipAnchor();
                continue;
            }
            anchor.fromCol = col;
            anchor.fromRow = row;

            if (spec.floating) {
                const size_t toTag = xmlp::findTagOpen(xmlContent, "xdr:to", pos);
                std::string toColStr;
                std::string toRowStr;
                int toCol = 0;
                int toRow = 0;
                if (toTag != std::string::npos && toTag < endPos &&
                    xmlp::getElementText(xmlContent, "xdr:col", toTag, toColStr) &&
                    xmlp::getElementText(xmlContent, "xdr:row", toTag, toRowStr) &&
                    xmlp::parseNonNegativeInt(toColStr, toCol) &&
                    xmlp::parseNonNegativeInt(toRowStr, toRow)) {
                    anchor.toCol = toCol;
                    anchor.toRow = toRow;
                } else {
                    warnings.push_back(std::string("Missing/invalid <xdr:to> in ") +
                                       spec.tag + " for sheet '" + sheetName +
                                       "': treated as single-cell");
                    anchor.toCol = col;
                    anchor.toRow = row;
                }
            } else {
                anchor.toCol = col; // oneCellAnchor: embedded image
                anchor.toRow = row;
            }

            // Image reference
            std::string rId;
            if (xmlp::getAttributeInRange(xmlContent, pos, endPos, "r:embed", rId) &&
                !rId.empty()) {
                auto it = rIdToImageMap.find(rId);
                if (it != rIdToImageMap.end()) {
                    anchor.imageName = it->second;
                } else {
                    anchor.imageName = rId;
                    warnings.push_back("Unknown relationship id '" + rId +
                                       "' in drawing for sheet '" + sheetName + "'");
                }
            } else {
                warnings.push_back(std::string("No r:embed image reference in ") +
                                   spec.tag + " for sheet '" + sheetName + "'");
            }

            if (!anchor.imageName.empty()) {
                outAnchors.push_back(anchor);
            }
            pos = endPos + closing.size();
        }
    }
}

void ImageExtractor::parseCellImagesXml(const std::string& xmlContent,
                                        const std::map<std::string, std::string>& rIdToImageMap,
                                        std::vector<CellImageInfo>& outCellImages,
                                        std::vector<std::string>& warnings) {
    const std::string opening = "etc:cellImage";
    const std::string closing = "</etc:cellImage>";
    size_t pos = 0;
    while ((pos = xmlp::findTagOpen(xmlContent, opening, pos)) != std::string::npos) {
        const size_t endPos = xmlContent.find(closing, pos);
        if (endPos == std::string::npos) break;

        CellImageInfo cellImg;
        std::string rId;
        xmlp::getAttributeInRange(xmlContent, pos, endPos, "name", cellImg.imageId);
        xmlp::getAttributeInRange(xmlContent, pos, endPos, "r:embed", rId);

        auto it = rIdToImageMap.find(rId);
        if (!rId.empty() && it != rIdToImageMap.end()) {
            cellImg.imageName = it->second;
        }

        if (!cellImg.imageId.empty() && !cellImg.imageName.empty()) {
            outCellImages.push_back(cellImg);
        } else {
            warnings.push_back("WPS cellimages.xml entry missing id or image mapping: skipped");
        }
        pos = endPos + closing.size();
    }
}

bool ImageExtractor::extractFromXlsx(const std::string& xlsxPath,
                                     std::vector<ImageInfo>& outImages,
                                     std::vector<DrawingAnchor>& outAnchors,
                                     std::vector<CellImageInfo>& outCellImages,
                                     std::vector<std::string>& warnings) {
    std::string err;
    zip_t* za = zipio::openReadOnly(xlsxPath, err);
    if (!za) {
        lastError_ = err;
        return false;
    }

    // Pass 1: relationship documents.
    std::map<std::string, std::map<std::string, std::string>> drawingRelsMap; // "drawing1" -> rId map
    std::map<std::string, std::string> cellImagesRelsMap; // WPS cellimages rels
    std::map<std::string, std::string> drawingToSheet =
        buildDrawingSheetMap(za, warnings);

    zip_int64_t numEntries = zip_get_num_entries(za, 0);
    for (zip_int64_t i = 0; i < numEntries; ++i) {
        const char* entryName = zip_get_name(za, i, 0);
        if (!entryName) continue;
        const std::string rawName(entryName);
        const std::string filename = normalizeEntryName(rawName);

        if (filename.find("xl/drawings/_rels/") == 0 &&
            filename.find(".xml.rels") != std::string::npos) {
            std::vector<uint8_t> xmlData;
            if (zipio::readFile(za, rawName, xmlData, kMaxXmlBytes, err)) {
                const std::string xmlContent(xmlData.begin(), xmlData.end());
                const size_t lastSlash = filename.find_last_of('/');
                const std::string baseName = (lastSlash != std::string::npos)
                    ? filename.substr(lastSlash + 1) : filename;
                const size_t relsPos = baseName.find(".xml.rels");
                const std::string drawingBase = (relsPos != std::string::npos)
                    ? baseName.substr(0, relsPos) : baseName;
                drawingRelsMap[drawingBase] = xmlp::parseRelationships(xmlContent);
            } else {
                warnings.push_back("Failed to read drawing relationships: " + filename +
                                   " (" + err + ")");
            }
        } else if (filename == "xl/_rels/cellimages.xml.rels") {
            std::vector<uint8_t> xmlData;
            if (zipio::readFile(za, rawName, xmlData, kMaxXmlBytes, err)) {
                const std::string xmlContent(xmlData.begin(), xmlData.end());
                cellImagesRelsMap = xmlp::parseRelationships(xmlContent);
            } else {
                warnings.push_back("Failed to read cellimages relationships: " + err);
            }
        }
    }

    // Pass 2: media, drawing XML, cellimages XML.
    for (zip_int64_t i = 0; i < numEntries; ++i) {
        const char* entryName = zip_get_name(za, i, 0);
        if (!entryName) continue;
        const std::string rawName(entryName);
        const std::string filename = normalizeEntryName(rawName);

        if (filename.find("xl/media/") == 0) {
            ImageInfo img;
            if (zipio::readFile(za, rawName, img.data, kMaxMediaBytes, err)) {
                if (img.data.empty()) {
                    warnings.push_back("Empty media entry skipped: " + filename);
                    continue;
                }
                const size_t lastSlash = filename.find_last_of('/');
                img.filename = (lastSlash != std::string::npos)
                    ? filename.substr(lastSlash + 1) : filename;
                const size_t dotPos = img.filename.find_last_of('.');
                img.contentType = (dotPos != std::string::npos)
                    ? getContentType(img.filename.substr(dotPos))
                    : "application/octet-stream";
                outImages.push_back(std::move(img));
            } else {
                warnings.push_back("Failed to read media entry '" + filename +
                                   "': " + err);
            }
            continue;
        }

        if (filename.find("xl/drawings/drawing") == 0 &&
            filename.find(".xml") != std::string::npos &&
            filename.find(".rels") == std::string::npos) {
            std::vector<uint8_t> xmlData;
            if (!zipio::readFile(za, rawName, xmlData, kMaxXmlBytes, err)) {
                warnings.push_back("Failed to read drawing XML: " + filename + " (" + err + ")");
                continue;
            }
            const std::string xmlContent(xmlData.begin(), xmlData.end());
            const size_t lastSlash = filename.find_last_of('/');
            const std::string baseName = (lastSlash != std::string::npos)
                ? filename.substr(lastSlash + 1) : filename;
            const size_t xmlPos = baseName.find(".xml");
            const std::string drawingBase = (xmlPos != std::string::npos)
                ? baseName.substr(0, xmlPos) : baseName;

            std::map<std::string, std::string> rIdMap;
            auto relsIt = drawingRelsMap.find(drawingBase);
            if (relsIt != drawingRelsMap.end()) {
                rIdMap = relsIt->second;
            } else {
                warnings.push_back("No relationships found for drawing '" +
                                   drawingBase + "'; image refs may not resolve");
            }

            std::string sheetName;
            auto sheetIt = drawingToSheet.find(drawingBase);
            if (sheetIt != drawingToSheet.end()) {
                sheetName = sheetIt->second;
            } else {
                warnings.push_back("Drawing '" + drawingBase +
                                   "' not mapped to any sheet; anchors left unattached");
            }

            parseDrawingXml(xmlContent, sheetName, rIdMap, outAnchors, warnings);
            continue;
        }

        if (filename == "xl/cellimages.xml") {
            std::vector<uint8_t> xmlData;
            if (zipio::readFile(za, rawName, xmlData, kMaxXmlBytes, err)) {
                const std::string xmlContent(xmlData.begin(), xmlData.end());
                parseCellImagesXml(xmlContent, cellImagesRelsMap, outCellImages, warnings);
            } else {
                warnings.push_back("Failed to read cellimages.xml: " + err);
            }
        }
    }

    zipio::close(za);
    return true;
}

} // namespace baja_xlsx
