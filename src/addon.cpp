#include <napi.h>
#include "xlsx_reader.h"

#include <cmath>
#include <map>
#include <string>
#include <utility>
#include <vector>

using namespace Napi;
using namespace baja_xlsx;

namespace {

// ---------------------------------------------------------------------------
// Error helpers: lastError_ travels as "CODE|message" so consumers can
// branch on err.code (AUDIT-20260917-039).
// ---------------------------------------------------------------------------

Error makeCodedError(Env env, const std::string& lastError) {
    std::string code = "PARSE_ERROR";
    std::string message = lastError;
    const size_t sep = lastError.find('|');
    if (sep != std::string::npos) {
        code = lastError.substr(0, sep);
        message = lastError.substr(sep + 1);
    }
    Error err = Error::New(env, message);
    err.Value().Set("code", String::New(env, code));
    return err;
}

Value failWith(Env env, const std::string& lastError) {
    makeCodedError(env, lastError).ThrowAsJavaScriptException();
    return env.Null();
}

// With NAPI_DISABLE_CPP_EXCEPTIONS, failed N-API construction returns empty
// handles instead of raising; bail out of long conversion loops as soon as
// a pending exception is observed (AUDIT-20260917-018).
bool hasPendingException(Env env) {
    return env.IsExceptionPending();
}

// ---------------------------------------------------------------------------
// Value conversion (type conversion only; attachment decisions are made in
// XlsxReader::readExcel, AUDIT-20260917-034).
// ---------------------------------------------------------------------------

// One shared Buffer per image index (AUDIT-20260917-017: no per-cell copies).
Object createImageObject(Env env, const ImageData& img,
                         std::map<int, Value>& bufferCache, int imageIndex) {
    auto cached = bufferCache.find(imageIndex);
    if (cached != bufferCache.end()) {
        return cached->second.As<Object>();
    }

    Object imgObj = Object::New(env);
    imgObj.Set("name", String::New(env, img.name));
    imgObj.Set("type", String::New(env, img.type));
    Buffer<uint8_t> buffer = Buffer<uint8_t>::Copy(env,
        img.data.data(),
        static_cast<size_t>(img.data.size()));
    imgObj.Set("data", buffer);
    bufferCache[imageIndex] = imgObj;
    return imgObj;
}

Value cellToJsValue(Env env, const CellValue& cell,
                    const std::vector<ImageData>& images,
                    std::map<int, Value>& bufferCache) {
    if (cell.imageIndices.empty()) {
        return String::New(env, cell.text);
    }
    if (cell.imageIndices.size() == 1) {
        const int idx = cell.imageIndices[0];
        if (idx >= 0 && idx < static_cast<int>(images.size())) {
            return createImageObject(env, images[idx], bufferCache, idx);
        }
        return String::New(env, cell.text);
    }

    Array imgArray = Array::New(env, cell.imageIndices.size());
    uint32_t out = 0;
    for (int idx : cell.imageIndices) {
        if (idx >= 0 && idx < static_cast<int>(images.size())) {
            imgArray.Set(out++, createImageObject(env, images[idx], bufferCache, idx));
        }
    }
    return imgArray;
}

Array stringArray(Env env, const std::vector<std::string>& values) {
    Array result = Array::New(env, values.size());
    for (size_t i = 0; i < values.size(); ++i) {
        result.Set(static_cast<uint32_t>(i), String::New(env, values[i]));
    }
    return result;
}

// Takes a non-const ExcelData so each row can be moved out while it is
// converted: the C++ copy is released row by row instead of staying fully
// resident next to the JS result (P0-5).
Array sheetsToArray(Env env, ExcelData& data,
                    std::map<int, Value>& bufferCache) {
    Array result = Array::New(env, data.sheets.size());

    for (size_t i = 0; i < data.sheets.size(); ++i) {
        if (hasPendingException(env)) return result;

        SheetData& sheet = data.sheets[i];
        Object sheetObj = Object::New(env);
        sheetObj.Set("name", String::New(env, sheet.name));

        Array dataArray = Array::New(env, sheet.data.size());
        for (size_t row = 0; row < sheet.data.size(); ++row) {
            if (hasPendingException(env)) return result;

            std::vector<CellValue> rowData = std::move(sheet.data[row]);
            Array rowArray = Array::New(env, rowData.size());
            for (size_t col = 0; col < rowData.size(); ++col) {
                rowArray.Set(static_cast<uint32_t>(col),
                             cellToJsValue(env, rowData[col], data.images, bufferCache));
            }
            dataArray.Set(static_cast<uint32_t>(row), rowArray);
        }

        // Present only for column projection: the resolved header texts in the
        // same order as the projected columns.
        if (!sheet.headers.empty()) {
            sheetObj.Set("headers", stringArray(env, sheet.headers));
        }
        sheetObj.Set("data", dataArray);
        result.Set(static_cast<uint32_t>(i), sheetObj);
    }

    data.sheets.clear();
    return result;
}

Array imagesToArray(Env env, const std::vector<ImageData>& images,
                    std::map<int, Value>& bufferCache) {
    Array result = Array::New(env, images.size());
    for (size_t i = 0; i < images.size(); ++i) {
        if (hasPendingException(env)) return result;
        result.Set(static_cast<uint32_t>(i),
                   createImageObject(env, images[i], bufferCache, static_cast<int>(i)));
    }
    return result;
}

Array positionsToArray(Env env, const std::vector<ImagePosition>& positions) {
    Array result = Array::New(env, positions.size());
    for (size_t i = 0; i < positions.size(); ++i) {
        if (hasPendingException(env)) return result;

        const ImagePosition& pos = positions[i];
        Object posObj = Object::New(env);
        posObj.Set("image", String::New(env, pos.imageName));
        posObj.Set("sheet", String::New(env, pos.sheetName));

        Object fromObj = Object::New(env);
        fromObj.Set("col", Number::New(env, pos.fromCol));
        fromObj.Set("row", Number::New(env, pos.fromRow));
        posObj.Set("from", fromObj);

        Object toObj = Object::New(env);
        toObj.Set("col", Number::New(env, pos.toCol));
        toObj.Set("row", Number::New(env, pos.toRow));
        posObj.Set("to", toObj);

        result.Set(static_cast<uint32_t>(i), posObj);
    }
    return result;
}

Array warningsToArray(Env env, const std::vector<std::string>& warnings) {
    Array result = Array::New(env, warnings.size());
    for (size_t i = 0; i < warnings.size(); ++i) {
        result.Set(static_cast<uint32_t>(i), String::New(env, warnings[i]));
    }
    return result;
}

Object buildResult(Env env, ExcelData& data) {
    std::map<int, Value> bufferCache;
    Object result = Object::New(env);
    result.Set("sheets", sheetsToArray(env, data, bufferCache));
    result.Set("images", imagesToArray(env, data.images, bufferCache));
    result.Set("imagePositions", positionsToArray(env, data.imagePositions));
    result.Set("warnings", warningsToArray(env, data.warnings));
    return result;
}

// Reads one optional non-negative integer option; missing/null keeps default.
bool readNumberOption(Env env, const Object& opts, const char* name, size_t& out) {
    out = 0;
    if (!opts.Has(name)) return true;
    Value v = opts.Get(name);
    if (v.IsUndefined() || v.IsNull()) return true;
    if (!v.IsNumber()) {
        TypeError::New(env, std::string("options.") + name + " must be a number")
            .ThrowAsJavaScriptException();
        return false;
    }
    const double value = v.As<Number>().DoubleValue();
    if (!(value >= 0) || value != std::floor(value)) {
        TypeError::New(env, std::string("options.") + name + " must be a non-negative integer")
            .ThrowAsJavaScriptException();
        return false;
    }
    out = static_cast<size_t>(value);
    return true;
}

// Input is either a file path or the workbook bytes. Buffer input is parsed
// straight from memory (P0-4), so Buffer / base64 callers never pay for a
// temporary file round-trip.
bool readInputArgument(const CallbackInfo& info, std::string& filepath,
                       std::vector<uint8_t>& bytes, bool& fromMemory) {
    Env env = info.Env();
    fromMemory = false;

    if (info.Length() < 1) {
        TypeError::New(env, "Input expected (file path or Buffer)")
            .ThrowAsJavaScriptException();
        return false;
    }
    if (info[0].IsString()) {
        filepath = info[0].As<String>().Utf8Value();
        return true;
    }
    if (info[0].IsBuffer()) {
        Buffer<uint8_t> buffer = info[0].As<Buffer<uint8_t>>();
        bytes.assign(buffer.Data(), buffer.Data() + buffer.Length());
        fromMemory = true;
        return true;
    }

    TypeError::New(env, "Input must be a file path (string) or a Buffer")
        .ThrowAsJavaScriptException();
    return false;
}

// Reads the options object: { sheetName, headerRow, maxRows, maxCols,
// includeImages, columns }. Everything is optional, so a caller that only
// needs one option still writes `{ sheetName: 'Sheet2' }`.
bool parseReadOptions(const CallbackInfo& info, size_t index, ReadOptions& out) {
    Env env = info.Env();
    if (info.Length() <= index || !info[index].IsObject()) {
        return true; // no options at all -> defaults
    }

    Object opts = info[index].As<Object>();

    if (opts.Has("sheetName")) {
        Value v = opts.Get("sheetName");
        if (!v.IsUndefined() && !v.IsNull()) {
            if (!v.IsString()) {
                TypeError::New(env, "options.sheetName must be a string")
                    .ThrowAsJavaScriptException();
                return false;
            }
            out.sheetName = v.As<String>().Utf8Value();
        }
    }

    size_t numeric = 0;
    if (!readNumberOption(env, opts, "headerRow", numeric)) return false;
    out.headerRow = numeric;
    if (!readNumberOption(env, opts, "maxRows", numeric)) return false;
    out.maxRows = numeric;
    if (!readNumberOption(env, opts, "maxCols", numeric)) return false;
    out.maxCols = numeric;

    if (opts.Has("includeImages")) {
        Value v = opts.Get("includeImages");
        if (!v.IsUndefined() && !v.IsNull()) {
            if (!v.IsBoolean()) {
                TypeError::New(env, "options.includeImages must be a boolean")
                    .ThrowAsJavaScriptException();
                return false;
            }
            out.includeImages = v.As<Boolean>().Value();
        }
    }

    if (opts.Has("columns")) {
        Value v = opts.Get("columns");
        if (!v.IsUndefined() && !v.IsNull()) {
            if (!v.IsArray()) {
                TypeError::New(env, "options.columns must be an array of strings")
                    .ThrowAsJavaScriptException();
                return false;
            }
            Array arr = v.As<Array>();
            const uint32_t length = arr.Length();
            out.columns.reserve(length);
            for (uint32_t i = 0; i < length; ++i) {
                Value item = arr.Get(i);
                if (!item.IsString()) {
                    TypeError::New(env, "options.columns must contain only strings")
                        .ThrowAsJavaScriptException();
                    return false;
                }
                out.columns.push_back(item.As<String>().Utf8Value());
            }
        }
    }

    return true;
}

} // namespace

// ---------------------------------------------------------------------------
// Synchronous entry point
// ---------------------------------------------------------------------------

Value ReadExcel(const CallbackInfo& info) {
    Env env = info.Env();

    std::string filepath;
    std::vector<uint8_t> bytes;
    bool fromMemory = false;
    if (!readInputArgument(info, filepath, bytes, fromMemory)) {
        return env.Null();
    }

    ReadOptions options;
    if (!parseReadOptions(info, 1, options)) {
        return env.Null();
    }

    XlsxReader reader;
    ExcelData data = fromMemory ? reader.readExcel(bytes, options)
                                : reader.readExcel(filepath, options);

    if (!reader.getLastError().empty()) {
        return failWith(env, reader.getLastError());
    }
    if (hasPendingException(env)) {
        return env.Null();
    }

    return buildResult(env, data);
}

// ---------------------------------------------------------------------------
// Asynchronous entry point: returns a Promise; parsing runs on the libuv
// thread pool so Electron UIs and Node servers are not blocked
// (AUDIT-20260917-007).
// ---------------------------------------------------------------------------

class ReadExcelWorker : public Napi::AsyncWorker {
public:
    ReadExcelWorker(Napi::Env env, std::string filepath, std::vector<uint8_t> bytes,
                    bool fromMemory, const ReadOptions& options)
        : Napi::AsyncWorker(env),
          deferred_(Napi::Promise::Deferred::New(env)),
          filepath_(std::move(filepath)),
          bytes_(std::move(bytes)),
          fromMemory_(fromMemory),
          options_(options) {}

    Napi::Promise Promise() { return deferred_.Promise(); }

    void Execute() override {
        XlsxReader reader;
        data_ = reader.readExcel(filepath_, options_);
        if (!reader.getLastError().empty()) {
            SetError(reader.getLastError());
        }
    }

    void OnOK() override {
        Napi::Env env = Env();
        if (env.IsExceptionPending()) {
            deferred_.Reject(env.GetAndClearPendingException().Value());
            return;
        }
        try {
            deferred_.Resolve(buildResult(env, data_));
        } catch (...) {
            deferred_.Reject(env.GetAndClearPendingException().Value());
        }
    }

    void OnError(const Napi::Error& e) override {
        deferred_.Reject(makeCodedError(Env(), e.Message()).Value());
    }

private:
    Napi::Promise::Deferred deferred_;
    std::string filepath_;
    std::vector<uint8_t> bytes_;
    bool fromMemory_;
    ReadOptions options_;
    ExcelData data_;
};

Value ReadExcelAsync(const CallbackInfo& info) {
    Env env = info.Env();

    std::string filepath;
    std::vector<uint8_t> bytes;
    bool fromMemory = false;
    if (!readInputArgument(info, filepath, bytes, fromMemory)) {
        return env.Null();
    }

    ReadOptions options;
    if (!parseReadOptions(info, 1, options)) {
        return env.Null();
    }

    auto* worker = new ReadExcelWorker(env, std::move(filepath), std::move(bytes),
                                       fromMemory, options);
    auto promise = worker->Promise();
    worker->Queue();
    return promise;
}

// ---------------------------------------------------------------------------
// Init. The former native `extractImages` export is removed: it was a
// placeholder that always returned [] silently (AUDIT-20260917-012).
// ---------------------------------------------------------------------------

Object Init(Env env, Object exports) {
    exports.Set("readExcel", Function::New(env, ReadExcel));
    exports.Set("readExcelAsync", Function::New(env, ReadExcelAsync));
    return exports;
}

NODE_API_MODULE(baja_xlsx, Init)
