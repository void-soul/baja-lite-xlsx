#include <napi.h>
#include "xlsx_reader.h"

using namespace Napi;
using namespace baja_xlsx;

// Helper function to create image object
Object createImageObject(Env env, const ImageData& img) {
    Object imgObj = Object::New(env);
    imgObj.Set("name", String::New(env, img.name));
    imgObj.Set("type", String::New(env, img.type));
    
    Buffer<uint8_t> buffer = Buffer<uint8_t>::Copy(env,
        img.data.data(),
        img.data.size());
    imgObj.Set("data", buffer);
    
    return imgObj;
}

// Helper function to convert C++ vector to JS array
Array sheetsToArray(Env env, const std::vector<SheetData>& sheets, 
                    const std::vector<ImageData>& images,
                    const std::vector<ImagePosition>& positions,
                    const std::vector<CellImageMapping>& cellImageMappings) {
    Array result = Array::New(env, sheets.size());
    
    for (size_t i = 0; i < sheets.size(); ++i) {
        Object sheetObj = Object::New(env);
        sheetObj.Set("name", String::New(env, sheets[i].name));
        
        Array dataArray = Array::New(env, sheets[i].data.size());
        for (size_t row = 0; row < sheets[i].data.size(); ++row) {
            Array rowArray = Array::New(env, sheets[i].data[row].size());
            for (size_t col = 0; col < sheets[i].data[row].size(); ++col) {
                const std::string& cellValue = sheets[i].data[row][col];
                
                // Check if this is an embedded image cell marker
                // Format: __IMAGE_CELL__ or __IMAGE_CELL__:ID_xxx (WPS Excel)
                if (cellValue.find("__IMAGE_CELL__") == 0) {
                    bool imageFound = false;
                    std::string targetImageName;
                    
                    // Check if this is WPS Excel format with embedded ID
                    if (cellValue.find("__IMAGE_CELL__:") == 0) {
                        // Extract image ID (e.g., "ID_C6F9C8CE7BB34DB9B1BB9835C5297155")
                        std::string imageId = cellValue.substr(15); // Skip "__IMAGE_CELL__:"
                        
                        // Find the image name using cellImageMappings
                        for (const auto& mapping : cellImageMappings) {
                            if (mapping.imageId == imageId) {
                                targetImageName = mapping.imageName;
                                break;
                            }
                        }
                    } else {
                        // Standard Excel format - find by position
                        for (const auto& pos : positions) {
                            if (pos.sheetName == sheets[i].name &&
                                pos.fromRow == static_cast<int>(row) &&
                                pos.fromCol == static_cast<int>(col)) {
                                targetImageName = pos.imageName;
                                break;
                            }
                        }
                    }
                    
                    // Find the image by name
                    if (!targetImageName.empty()) {
                        for (const auto& img : images) {
                            if (img.name == targetImageName) {
                                rowArray.Set(col, createImageObject(env, img));
                                imageFound = true;
                                break;
                            }
                        }
                    }
                    
                    // If image not found, set empty string
                    if (!imageFound) {
                        rowArray.Set(col, String::New(env, ""));
                    }
                } else {
                    // Normal cell - set string value
                    rowArray.Set(col, String::New(env, cellValue));
                }
            }
            dataArray.Set(row, rowArray);
        }
        
        // After filling all cells, process floating images
        // Floating images are added to cells based on their top-left position
        for (const auto& pos : positions) {
            if (pos.sheetName != sheets[i].name) {
                continue;
            }
            
            // Check if this is a floating image (not embedded)
            // Embedded images have fromRow == toRow and fromCol == toCol
            bool isEmbedded = (pos.fromRow == pos.toRow && pos.fromCol == pos.toCol);
            
            if (!isEmbedded) {
                // This is a floating image
                // Only attach to the top-left cell
                int targetRow = pos.fromRow;
                int targetCol = pos.fromCol;
                
                // Check if row and col are valid
                if (targetRow >= 0 && targetRow < static_cast<int>(sheets[i].data.size())) {
                    if (targetCol >= 0 && targetCol < static_cast<int>(sheets[i].data[targetRow].size())) {
                        // Find the image data
                        bool found = false;
                        
                        // Try exact match first
                        for (const auto& img : images) {
                            if (img.name == pos.imageName) {
                                // Get the row array
                                Array rowArray = dataArray.Get(targetRow).As<Array>();
                                Value currentValue = rowArray.Get(targetCol);
                                
                                // Check if this cell already has an image
                                if (currentValue.IsObject()) {
                                    Object currentObj = currentValue.As<Object>();
                                    if (currentObj.Has("data") && currentObj.Get("data").IsBuffer()) {
                                        // Cell already has an image, convert to array
                                        Array imgArray;
                                        if (currentObj.IsArray()) {
                                            imgArray = currentObj.As<Array>();
                                        } else {
                                            imgArray = Array::New(env, 1);
                                            imgArray.Set(uint32_t(0), currentObj);
                                        }
                                        // Add new image
                                        imgArray.Set(imgArray.Length(), createImageObject(env, img));
                                        rowArray.Set(targetCol, imgArray);
                                    } else {
                                        // Not an image object, replace with image
                                        rowArray.Set(targetCol, createImageObject(env, img));
                                    }
                                } else {
                                    // No image yet, set it
                                    rowArray.Set(targetCol, createImageObject(env, img));
                                }
                                
                                found = true;
                                break;
                            }
                        }
                        
                        // If exact match fails, try fuzzy match
                        if (!found) {
                            for (const auto& img : images) {
                                if (!pos.imageName.empty() && !img.name.empty() &&
                                    (img.name.find(pos.imageName) != std::string::npos ||
                                     pos.imageName.find(img.name) != std::string::npos)) {
                                    
                                    Array rowArray = dataArray.Get(targetRow).As<Array>();
                                    Value currentValue = rowArray.Get(targetCol);
                                    
                                    if (currentValue.IsObject()) {
                                        Object currentObj = currentValue.As<Object>();
                                        if (currentObj.Has("data") && currentObj.Get("data").IsBuffer()) {
                                            Array imgArray;
                                            if (currentObj.IsArray()) {
                                                imgArray = currentObj.As<Array>();
                                            } else {
                                                imgArray = Array::New(env, 1);
                                                imgArray.Set(uint32_t(0), currentObj);
                                            }
                                            imgArray.Set(imgArray.Length(), createImageObject(env, img));
                                            rowArray.Set(targetCol, imgArray);
                                        } else {
                                            rowArray.Set(targetCol, createImageObject(env, img));
                                        }
                                    } else {
                                        rowArray.Set(targetCol, createImageObject(env, img));
                                    }
                                    
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }
        
        sheetObj.Set("data", dataArray);
        result.Set(i, sheetObj);
    }
    
    return result;
}

Array imagesToArray(Env env, const std::vector<ImageData>& images) {
    Array result = Array::New(env, images.size());
    
    for (size_t i = 0; i < images.size(); ++i) {
        Object imgObj = Object::New(env);
        imgObj.Set("name", String::New(env, images[i].name));
        imgObj.Set("type", String::New(env, images[i].type));
        
        Buffer<uint8_t> buffer = Buffer<uint8_t>::Copy(env, 
            images[i].data.data(), 
            images[i].data.size());
        imgObj.Set("data", buffer);
        
        result.Set(i, imgObj);
    }
    
    return result;
}

Array positionsToArray(Env env, const std::vector<ImagePosition>& positions) {
    Array result = Array::New(env, positions.size());
    
    for (size_t i = 0; i < positions.size(); ++i) {
        Object posObj = Object::New(env);
        posObj.Set("image", String::New(env, positions[i].imageName));
        posObj.Set("sheet", String::New(env, positions[i].sheetName));
        
        Object fromObj = Object::New(env);
        fromObj.Set("col", Number::New(env, positions[i].fromCol));
        fromObj.Set("row", Number::New(env, positions[i].fromRow));
        posObj.Set("from", fromObj);
        
        Object toObj = Object::New(env);
        toObj.Set("col", Number::New(env, positions[i].toCol));
        toObj.Set("row", Number::New(env, positions[i].toRow));
        posObj.Set("to", toObj);
        
        result.Set(i, posObj);
    }
    
    return result;
}

// ReadExcel function - reads complete Excel data
Value ReadExcel(const CallbackInfo& info) {
    Env env = info.Env();
    
    if (info.Length() < 1 || !info[0].IsString()) {
        TypeError::New(env, "String expected for filepath").ThrowAsJavaScriptException();
        return env.Null();
    }
    
    std::string filepath = info[0].As<String>().Utf8Value();
    
    XlsxReader reader;
    ExcelData data = reader.readExcel(filepath);
    
    if (!reader.getLastError().empty()) {
        Error::New(env, reader.getLastError()).ThrowAsJavaScriptException();
        return env.Null();
    }
    
    Object result = Object::New(env);
    result.Set("sheets", sheetsToArray(env, data.sheets, data.images, data.imagePositions, data.cellImageMappings));
    result.Set("images", imagesToArray(env, data.images));
    result.Set("imagePositions", positionsToArray(env, data.imagePositions));
    
    return result;
}

// ExtractImages function - only extracts images
Value ExtractImages(const CallbackInfo& info) {
    Env env = info.Env();
    
    if (info.Length() < 1 || !info[0].IsString()) {
        TypeError::New(env, "String expected for filepath").ThrowAsJavaScriptException();
        return env.Null();
    }
    
    std::string filepath = info[0].As<String>().Utf8Value();
    
    XlsxReader reader;
    if (!reader.load(filepath)) {
        Error::New(env, reader.getLastError()).ThrowAsJavaScriptException();
        return env.Null();
    }
    
    std::vector<ImageData> images = reader.extractImages();
    
    if (!reader.getLastError().empty()) {
        Error::New(env, reader.getLastError()).ThrowAsJavaScriptException();
        return env.Null();
    }
    
    return imagesToArray(env, images);
}

// Initialize the addon
Object Init(Env env, Object exports) {
    exports.Set("readExcel", Function::New(env, ReadExcel));
    exports.Set("extractImages", Function::New(env, ExtractImages));
    return exports;
}

NODE_API_MODULE(baja_xlsx, Init)


