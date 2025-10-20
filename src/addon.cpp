#include <napi.h>
#include "xlsx_reader.h"

using namespace Napi;
using namespace baja_xlsx;

// Helper function to convert C++ vector to JS array
Array sheetsToArray(Env env, const std::vector<SheetData>& sheets) {
    Array result = Array::New(env, sheets.size());
    
    for (size_t i = 0; i < sheets.size(); ++i) {
        Object sheetObj = Object::New(env);
        sheetObj.Set("name", String::New(env, sheets[i].name));
        
        Array dataArray = Array::New(env, sheets[i].data.size());
        for (size_t row = 0; row < sheets[i].data.size(); ++row) {
            Array rowArray = Array::New(env, sheets[i].data[row].size());
            for (size_t col = 0; col < sheets[i].data[row].size(); ++col) {
                rowArray.Set(col, String::New(env, sheets[i].data[row][col]));
            }
            dataArray.Set(row, rowArray);
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
    result.Set("sheets", sheetsToArray(env, data.sheets));
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


