#include <napi.h>
#include "xlsx_reader.h"

#include <map>
#include <string>

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

Array sheetsToArray(Env env, const ExcelData& data,
                    std::map<int, Value>& bufferCache) {
    Array result = Array::New(env, data.sheets.size());

    for (size_t i = 0; i < data.sheets.size(); ++i) {
        if (hasPendingException(env)) return result;

        const SheetData& sheet = data.sheets[i];
        Object sheetObj = Object::New(env);
        sheetObj.Set("name", String::New(env, sheet.name));

        Array dataArray = Array::New(env, sheet.data.size());
        for (size_t row = 0; row < sheet.data.size(); ++row) {
            if (hasPendingException(env)) return result;

            const std::vector<CellValue>& rowData = sheet.data[row];
            Array rowArray = Array::New(env, rowData.size());
            for (size_t col = 0; col < rowData.size(); ++col) {
                rowArray.Set(static_cast<uint32_t>(col),
                             cellToJsValue(env, rowData[col], data.images, bufferCache));
            }
            dataArray.Set(static_cast<uint32_t>(row), rowArray);
        }

        sheetObj.Set("data", dataArray);
        result.Set(static_cast<uint32_t>(i), sheetObj);
    }

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

Object buildResult(Env env, const ExcelData& data) {
    std::map<int, Value> bufferCache;
    Object result = Object::New(env);
    result.Set("sheets", sheetsToArray(env, data, bufferCache));
    result.Set("images", imagesToArray(env, data.images, bufferCache));
    result.Set("imagePositions", positionsToArray(env, data.imagePositions));
    result.Set("warnings", warningsToArray(env, data.warnings));
    return result;
}

// Reads optional numeric arguments (maxRows, maxCols).
bool readCapArgument(const CallbackInfo& info, size_t index, size_t& out) {
    out = 0;
    if (info.Length() <= index || info[index].IsUndefined() || info[index].IsNull()) {
        return true;
    }
    if (!info[index].IsNumber()) {
        TypeError::New(info.Env(), "maxRows/maxCols must be numbers")
            .ThrowAsJavaScriptException();
        return false;
    }
    const double v = info[index].As<Number>().DoubleValue();
    if (!(v >= 0) || v != std::floor(v)) {
        TypeError::New(info.Env(), "maxRows/maxCols must be non-negative integers")
            .ThrowAsJavaScriptException();
        return false;
    }
    out = static_cast<size_t>(v);
    return true;
}

} // namespace

// ---------------------------------------------------------------------------
// Synchronous entry point
// ---------------------------------------------------------------------------

Value ReadExcel(const CallbackInfo& info) {
    Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsString()) {
        TypeError::New(env, "String expected for filepath").ThrowAsJavaScriptException();
        return env.Null();
    }

    size_t maxRows = 0;
    size_t maxCols = 0;
    if (!readCapArgument(info, 1, maxRows) || !readCapArgument(info, 2, maxCols)) {
        return env.Null();
    }

    std::string filepath = info[0].As<String>().Utf8Value();

    XlsxReader reader;
    ExcelData data = reader.readExcel(filepath, maxRows, maxCols);

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
    ReadExcelWorker(Napi::Env env, std::string filepath, size_t maxRows, size_t maxCols)
        : Napi::AsyncWorker(env),
          deferred_(Napi::Promise::Deferred::New(env)),
          filepath_(std::move(filepath)),
          maxRows_(maxRows),
          maxCols_(maxCols) {}

    Napi::Promise Promise() { return deferred_.Promise(); }

    void Execute() override {
        XlsxReader reader;
        data_ = reader.readExcel(filepath_, maxRows_, maxCols_);
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
    size_t maxRows_;
    size_t maxCols_;
    ExcelData data_;
};

Value ReadExcelAsync(const CallbackInfo& info) {
    Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsString()) {
        TypeError::New(env, "String expected for filepath").ThrowAsJavaScriptException();
        return env.Null();
    }

    size_t maxRows = 0;
    size_t maxCols = 0;
    if (!readCapArgument(info, 1, maxRows) || !readCapArgument(info, 2, maxCols)) {
        return env.Null();
    }

    auto* worker = new ReadExcelWorker(env,
                                       info[0].As<String>().Utf8Value(),
                                       maxRows, maxCols);
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
