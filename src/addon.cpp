#include <napi.h>
#include "path_util.h"
#include "write_snapshot.h"
#include "xlsx_patch.h"
#include "xlsx_reader.h"
#include "xlsx_template.h"
#include "xlsx_writer.h"

#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <ctime>
#include <map>
#include <mutex>
#include <string>
#include <unordered_map>
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

// P1-1: every distinct cell value is emitted once into a string pool and
// cells carry the pool index instead of a fresh V8 string. Low-cardinality
// columns (status flags, categories, repeated codes) then allocate one string
// instead of one per row.
class StringPool {
public:
    uint32_t intern(const std::string& text) {
        auto it = index_.find(text);
        if (it != index_.end()) {
            return it->second;
        }
        const uint32_t id = static_cast<uint32_t>(values_.size());
        index_.emplace(text, id);
        values_.push_back(text);
        return id;
    }

    const std::vector<std::string>& values() const { return values_; }

private:
    std::unordered_map<std::string, uint32_t> index_;
    std::vector<std::string> values_;
};

// One shared image object per index: images travel as their own array and
// cells reference them, so no Buffer is ever copied per cell
// (AUDIT-20260917-017).
Object createImageObject(Env env, const ImageData& img) {
    Object imgObj = Object::New(env);
    imgObj.Set("name", String::New(env, img.name));
    imgObj.Set("type", String::New(env, img.type));
    Buffer<uint8_t> buffer = Buffer<uint8_t>::Copy(env,
        img.data.data(),
        static_cast<size_t>(img.data.size()));
    imgObj.Set("data", buffer);
    return imgObj;
}

// Cell encoding (decoded in index.js):
//   >= 0 -> index into the string pool
//   <  0 -> -(imageIndex + 1)
//   array -> several images on one cell
Value cellToJsIndex(Env env, const CellValue& cell,
                    const std::vector<ImageData>& images,
                    StringPool& pool) {
    if (cell.imageIndices.empty()) {
        return Number::New(env, static_cast<double>(pool.intern(cell.text)));
    }
    if (cell.imageIndices.size() == 1) {
        const int idx = cell.imageIndices[0];
        if (idx >= 0 && idx < static_cast<int>(images.size())) {
            return Number::New(env, -static_cast<double>(idx + 1));
        }
        return Number::New(env, static_cast<double>(pool.intern(cell.text)));
    }

    Array encoded = Array::New(env, cell.imageIndices.size());
    uint32_t out = 0;
    for (int idx : cell.imageIndices) {
        if (idx >= 0 && idx < static_cast<int>(images.size())) {
            encoded.Set(out++, Number::New(env, -static_cast<double>(idx + 1)));
        }
    }
    return encoded;
}

// Streaming batches carry real values (no cross-batch string pool, which
// would keep growing for the whole read) -- see P1-1 for the buffered path.
Value cellToJsValueDirect(Env env, const CellValue& cell,
                          const std::vector<ImageData>& images) {
    if (cell.imageIndices.empty()) {
        return String::New(env, cell.text);
    }
    if (cell.imageIndices.size() == 1) {
        const int idx = cell.imageIndices[0];
        if (idx >= 0 && idx < static_cast<int>(images.size())) {
            return createImageObject(env, images[idx]);
        }
        return String::New(env, cell.text);
    }
    Array arr = Array::New(env, cell.imageIndices.size());
    uint32_t out = 0;
    for (int idx : cell.imageIndices) {
        if (idx >= 0 && idx < static_cast<int>(images.size())) {
            arr.Set(out++, createImageObject(env, images[idx]));
        }
    }
    return arr;
}

Array rowsToJsArray(Env env, const std::vector<std::vector<CellValue>>& rows,
                    const std::vector<ImageData>& images) {
    Array result = Array::New(env, rows.size());
    for (size_t r = 0; r < rows.size(); ++r) {
        if (hasPendingException(env)) return result;
        const std::vector<CellValue>& row = rows[r];
        Array rowArray = Array::New(env, row.size());
        for (size_t c = 0; c < row.size(); ++c) {
            rowArray.Set(static_cast<uint32_t>(c),
                         cellToJsValueDirect(env, row[c], images));
        }
        result.Set(static_cast<uint32_t>(r), rowArray);
    }
    return result;
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
Array sheetsToArray(Env env, ExcelData& data, StringPool& pool) {
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
                             cellToJsIndex(env, rowData[col], data.images, pool));
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

Array imagesToArray(Env env, const std::vector<ImageData>& images) {
    Array result = Array::New(env, images.size());
    for (size_t i = 0; i < images.size(); ++i) {
        if (hasPendingException(env)) return result;
        result.Set(static_cast<uint32_t>(i), createImageObject(env, images[i]));
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
    StringPool pool;
    Object result = Object::New(env);
    result.Set("sheets", sheetsToArray(env, data, pool));
    result.Set("strings", stringArray(env, pool.values()));
    result.Set("images", imagesToArray(env, data.images));
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
// Streaming entry points (P2-1): rows are pushed to a JS callback in batches
// instead of accumulating, so memory stays flat on very large sheets.
// ---------------------------------------------------------------------------

const size_t kDefaultBatchSize = 50000;

bool readBatchArgument(const CallbackInfo& info, size_t& batchSize, Function& callback) {
    Env env = info.Env();

    batchSize = kDefaultBatchSize;
    if (info.Length() > 1 && info[1].IsObject()) {
        size_t requested = 0;
        if (!readNumberOption(env, info[1].As<Object>(), "batchSize", requested)) {
            return false;
        }
        if (requested > 0) {
            batchSize = requested;
        }
    }

    if (info.Length() < 3 || !info[2].IsFunction()) {
        TypeError::New(env, "Function expected for the batch callback")
            .ThrowAsJavaScriptException();
        return false;
    }
    callback = info[2].As<Function>();
    return true;
}

Object makeStreamResult(Env env, size_t rowCount, const std::vector<std::string>& warnings) {
    Object result = Object::New(env);
    result.Set("rowCount", Number::New(env, static_cast<double>(rowCount)));
    result.Set("warnings", warningsToArray(env, warnings));
    return result;
}

Value ReadExcelBatched(const CallbackInfo& info) {
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
    size_t batchSize = kDefaultBatchSize;
    Function callback;
    if (!readBatchArgument(info, batchSize, callback)) {
        return env.Null();
    }

    XlsxReader reader;
    ExcelData data;
    size_t rowCount = 0;

    RowBatchSink sink = [&](std::vector<std::vector<CellValue>>&& batch) -> bool {
        rowCount += batch.size();
        callback.Call({ rowsToJsArray(env, batch, data.images) });
        return !hasPendingException(env);
    };

    if (fromMemory) {
        reader.readExcelStreamed(bytes, options, sink, batchSize, data);
    } else {
        reader.readExcelStreamed(filepath, options, sink, batchSize, data);
    }

    if (!reader.getLastError().empty()) {
        return failWith(env, reader.getLastError());
    }
    return makeStreamResult(env, rowCount, data.warnings);
}

class ReadExcelBatchedWorker : public Napi::AsyncWorker {
public:
    struct BatchPayload {
        std::vector<std::vector<CellValue>> rows;
        ReadExcelBatchedWorker* worker;
    };

    ReadExcelBatchedWorker(Napi::Env env, std::string filepath, std::vector<uint8_t> bytes,
                           bool fromMemory, const ReadOptions& options, size_t batchSize,
                           const Napi::Function& callback)
        : Napi::AsyncWorker(env),
          deferred_(Napi::Promise::Deferred::New(env)),
          filepath_(std::move(filepath)),
          bytes_(std::move(bytes)),
          fromMemory_(fromMemory),
          options_(options),
          batchSize_(batchSize),
          tsf_(Napi::ThreadSafeFunction::New(env, callback, "baja_xlsx_batch", 0, 1)) {}

    Napi::Promise Promise() { return deferred_.Promise(); }

    void Execute() override {
        RowBatchSink sink = [this](std::vector<std::vector<CellValue>>&& batch) -> bool {
            rowCount_ += batch.size();
            {
                std::lock_guard<std::mutex> lock(mutex_);
                batchDelivered_ = false;
            }

            auto* payload = new BatchPayload{std::move(batch), this};
            const napi_status status = tsf_.BlockingCall(
                payload,
                [](Napi::Env env, Napi::Function cb, void* ctx) {
                    BatchPayload* item = static_cast<BatchPayload*>(ctx);
                    ReadExcelBatchedWorker* worker = item->worker;
                    Array rows = rowsToJsArray(env, item->rows, worker->data_.images);
                    delete item;
                    cb.Call({ rows });
                    worker->markBatchDelivered(env.IsExceptionPending());
                });
            if (status != napi_ok) {
                delete payload;
                return false;
            }

            // Wait for the main thread to run the callback before producing the
            // next batch: it keeps queueing bounded (backpressure) and
            // guarantees that no callback can fire after Execute returns --
            // AsyncWorker is destroyed right after OnOK/OnError.
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] { return batchDelivered_; });
            return !stopRequested_;
        };

        XlsxReader reader;
        if (fromMemory_) {
            reader.readExcelStreamed(bytes_, options_, sink, batchSize_, data_);
        } else {
            reader.readExcelStreamed(filepath_, options_, sink, batchSize_, data_);
        }
        if (!reader.getLastError().empty()) {
            SetError(reader.getLastError());
        }
    }

    void markBatchDelivered(bool stopRequested) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            batchDelivered_ = true;
            if (stopRequested) {
                stopRequested_ = true;
            }
        }
        cv_.notify_one();
    }

    void OnOK() override {
        Napi::Env env = Env();
        tsf_.Release();
        deferred_.Resolve(makeStreamResult(env, rowCount_, data_.warnings));
    }

    void OnError(const Napi::Error& e) override {
        Napi::Env env = Env();
        tsf_.Release();
        deferred_.Reject(makeCodedError(env, e.Message()).Value());
    }

    ExcelData data_; // images must stay alive while batches are converted

private:
    Napi::Promise::Deferred deferred_;
    Napi::ThreadSafeFunction tsf_;
    std::string filepath_;
    std::vector<uint8_t> bytes_;
    bool fromMemory_;
    ReadOptions options_;
    size_t batchSize_;
    size_t rowCount_ = 0;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool batchDelivered_ = false;
    bool stopRequested_ = false;
};

Value ReadExcelBatchedAsync(const CallbackInfo& info) {
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
    size_t batchSize = kDefaultBatchSize;
    Function callback;
    if (!readBatchArgument(info, batchSize, callback)) {
        return env.Null();
    }

    auto* worker = new ReadExcelBatchedWorker(env, std::move(filepath), std::move(bytes),
                                              fromMemory, options, batchSize, callback);
    auto promise = worker->Promise();
    worker->Queue();
    return promise;
}

// ---------------------------------------------------------------------------
// Writing: full sheet write. Cell values are pulled straight out of the JS
// array while the worksheet XML is generated, so the data is never copied into
// an intermediate representation.
// ---------------------------------------------------------------------------

struct ColumnSource {
    bool byIndex = false;
    std::string key;
    uint32_t index = 0;
};

// The workbook a write starts from: a path or an in-memory package.
struct TemplateArgument {
    bool present = false;
    std::string path;
    std::vector<uint8_t> bytes;

    TemplateSource source() const {
        TemplateSource out;
        if (!path.empty()) {
            out.path = &path;
        } else if (present) {
            out.bytes = &bytes;
        }
        return out;
    }
};

bool readTemplateArgument(const Object& options, TemplateArgument& out) {
    if (!options.Has("template")) return true;
    const Value value = options.Get("template");
    if (value.IsUndefined() || value.IsNull()) return true;
    if (value.IsString()) {
        out.present = true;
        out.path = value.As<String>().Utf8Value();
        return true;
    }
    if (value.IsBuffer()) {
        out.present = true;
        Buffer<uint8_t> buffer = value.As<Buffer<uint8_t>>();
        out.bytes.assign(buffer.Data(), buffer.Data() + buffer.Length());
        return true;
    }
    return false; // caller reports INVALID_OPTIONS
}

zipio::ZipWriter::Compression readCompression(const Object& options) {
    zipio::ZipWriter::Compression compression = zipio::ZipWriter::Compression::Default;
    if (options.Has("compression")) {
        const Value value = options.Get("compression");
        if (value.IsNumber()) {
            const int level = value.As<Number>().Int32Value();
            compression = level <= 0 ? zipio::ZipWriter::Compression::Store
                                     : (level <= 3 ? zipio::ZipWriter::Compression::Fast
                                                   : zipio::ZipWriter::Compression::Default);
        }
    }
    return compression;
}

std::string readOutputPath(const Object& options) {
    if (!options.Has("output")) return std::string();
    const Value value = options.Get("output");
    return value.IsString() ? value.As<String>().Utf8Value() : std::string();
}

// Writes a finished package to disk. No JS involved, so the async workers can
// run this off the main thread too.
bool writeBytesToFile(const std::string& path, const std::vector<uint8_t>& bytes,
                      std::string& error) {
    std::FILE* file = pathutil::openForWrite(path);
    if (!file) {
        error = "FILE_WRITE_FAILED|Cannot create " + path;
        return false;
    }
    const size_t written = bytes.empty() ? 0 : std::fwrite(bytes.data(), 1, bytes.size(), file);
    std::fclose(file);
    if (written != bytes.size()) {
        error = "FILE_WRITE_FAILED|Failed to write " + path;
        return false;
    }
    return true;
}

// Writes the assembled package to `path`, or hands it back as a Buffer.
Value finishWrite(Env env, const std::vector<uint8_t>& out, const std::string& outputPath,
                  Object summary) {
    if (outputPath.empty()) {
        return Buffer<uint8_t>::Copy(env, out.data(), out.size());
    }

    std::string error;
    if (!writeBytesToFile(outputPath, out, error)) {
        return failWith(env, error);
    }

    summary.Set("bytes", Number::New(env, static_cast<double>(out.size())));
    return summary;
}

WriteCell toWriteCell(const Value& value) {
    WriteCell cell;
    if (value.IsEmpty() || value.IsUndefined() || value.IsNull()) {
        return cell; // empty cell
    }
    if (value.IsNumber()) {
        cell.kind = WriteCell::Kind::Number;
        cell.number = value.As<Number>().DoubleValue();
        return cell;
    }
    if (value.IsBoolean()) {
        cell.kind = WriteCell::Kind::Boolean;
        cell.boolean = value.As<Boolean>().Value();
        return cell;
    }
    if (value.IsDate()) {
        // Stored as an Excel serial in local time, with a date number format
        // assigned by the writer: that is what makes it a real date in Excel
        // instead of an uncomputable string.
        const double millis = value.As<Date>().ValueOf();
        const std::time_t seconds = static_cast<std::time_t>(std::floor(millis / 1000.0));
        std::tm local{};
#if defined(_WIN32)
        localtime_s(&local, &seconds);
#else
        localtime_r(&seconds, &local);
#endif
        int millisPart = static_cast<int>(
            std::llround(millis - static_cast<double>(seconds) * 1000.0));
        if (millisPart < 0) millisPart = 0;
        cell.kind = WriteCell::Kind::Number;
        cell.isDate = true;
        cell.number = toExcelSerial(local.tm_year + 1900, local.tm_mon + 1, local.tm_mday,
                                    local.tm_hour, local.tm_min, local.tm_sec, millisPart);
        return cell;
    }
    if (value.IsString()) {
        cell.kind = WriteCell::Kind::Text;
        cell.text = value.As<String>().Utf8Value();
        return cell;
    }
    // Objects, arrays, functions: written as their string form rather than
    // dropped silently.
    cell.kind = WriteCell::Kind::Text;
    cell.text = value.ToString().Utf8Value();
    return cell;
}

class JsRowSource : public RowSource {
public:
    JsRowSource(Napi::Env env, Napi::Array rows, std::vector<ColumnSource> columns)
        : env_(env), rows_(rows), columns_(std::move(columns)) {}

    size_t rowCount() const override { return rows_.Length(); }

    bool nextRow(std::vector<WriteCell>& row) override {
        if (index_ >= rows_.Length()) {
            return false;
        }
        const Value item = rows_.Get(index_++);
        row.clear();
        for (const ColumnSource& column : columns_) {
            Value value = env_.Null();
            if (item.IsObject()) {
                Object object = item.As<Object>();
                value = column.byIndex ? object.Get(column.index) : object.Get(column.key);
            }
            row.push_back(toWriteCell(value));
        }
        return true;
    }

private:
    Napi::Env env_;
    Napi::Array rows_;
    std::vector<ColumnSource> columns_;
    uint32_t index_ = 0;
};

// Parses (rows, options) for a sheet write: the plan, how each column reads its
// value, and the workbook to start from. Shared by the sync and async entry
// points so both accept exactly the same options.
bool readSheetWriteArguments(const CallbackInfo& info, WritePlan& plan,
                             std::vector<ColumnSource>& columnSources,
                             TemplateArgument& templateArg, Napi::Array& rows, Object& options) {
    Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsArray()) {
        TypeError::New(env, "First argument must be an array of rows")
            .ThrowAsJavaScriptException();
        return false;
    }
    if (info.Length() < 2 || !info[1].IsObject()) {
        TypeError::New(env, "Second argument must be an options object")
            .ThrowAsJavaScriptException();
        return false;
    }

    rows = info[0].As<Napi::Array>();
    options = info[1].As<Object>();

    if (options.Has("sheetName")) {
        Value value = options.Get("sheetName");
        if (value.IsString()) {
            plan.sheetName = value.As<String>().Utf8Value();
        }
    }
    if (options.Has("includeHeader")) {
        Value value = options.Get("includeHeader");
        if (value.IsBoolean()) {
            plan.includeHeader = value.As<Boolean>().Value();
        }
    }
    if (options.Has("freezeHeader")) {
        Value value = options.Get("freezeHeader");
        if (value.IsBoolean()) {
            plan.freezeHeader = value.As<Boolean>().Value();
        }
    }
    if (options.Has("columns")) {
        Value value = options.Get("columns");
        if (value.IsArray()) {
            Napi::Array columns = value.As<Napi::Array>();
            const uint32_t count = columns.Length();
            for (uint32_t i = 0; i < count; ++i) {
                Value item = columns.Get(i);
                if (!item.IsObject()) continue;
                Object column = item.As<Object>();

                WriteColumn definition;
                if (column.Has("header")) {
                    Value header = column.Get("header");
                    if (header.IsString()) definition.header = header.As<String>().Utf8Value();
                }
                if (column.Has("numberFormat")) {
                    Value format = column.Get("numberFormat");
                    if (format.IsString()) definition.numberFormat = format.As<String>().Utf8Value();
                }
                if (column.Has("align")) {
                    Value align = column.Get("align");
                    if (align.IsString()) definition.align = align.As<String>().Utf8Value();
                }
                if (column.Has("width")) {
                    Value width = column.Get("width");
                    if (width.IsNumber()) definition.width = width.As<Number>().DoubleValue();
                }
                plan.columns.push_back(std::move(definition));

                ColumnSource source;
                if (column.Has("index")) {
                    Value index = column.Get("index");
                    if (index.IsNumber()) {
                        source.byIndex = true;
                        source.index = index.As<Number>().Uint32Value();
                    }
                } else if (column.Has("key")) {
                    Value key = column.Get("key");
                    if (key.IsString()) {
                        source.key = key.As<String>().Utf8Value();
                    } else if (key.IsNumber()) {
                        source.byIndex = true;
                        source.index = key.As<Number>().Uint32Value();
                    }
                }
                columnSources.push_back(std::move(source));
            }
        }
    }

    if (!readTemplateArgument(options, templateArg)) {
        failWith(env, "INVALID_OPTIONS|options.template must be a file path or a Buffer");
        return false;
    }
    if (!options.Has("sheetName")) {
        // A new workbook gets the default name; a template keeps its first sheet
        // unless the caller names one.
        plan.sheetName = templateArg.present ? std::string() : std::string("Sheet1");
    }
    return true;
}

// Copies the JS rows into the compact snapshot. This is the only part of an
// async write that runs on the main thread (it has to: the data lives in JS),
// and it is what makes the worker able to generate the workbook alone.
void snapshotRows(Env env, const Napi::Array& rows, const std::vector<ColumnSource>& columns,
                  WriteSnapshot& out) {
    const uint32_t count = rows.Length();
    out.reserve(count, static_cast<size_t>(count) * columns.size());

    std::vector<WriteCell> row;
    row.reserve(columns.size());
    for (uint32_t i = 0; i < count; ++i) {
        const Value item = rows.Get(i);
        row.clear();
        for (const ColumnSource& column : columns) {
            Value value = env.Null();
            if (item.IsObject()) {
                Object object = item.As<Object>();
                value = column.byIndex ? object.Get(column.index) : object.Get(column.key);
            }
            row.push_back(toWriteCell(value));
        }
        out.addRow(row);
    }
}

Value WriteExcel(const CallbackInfo& info) {
    Env env = info.Env();

    WritePlan plan;
    std::vector<ColumnSource> columnSources;
    TemplateArgument templateArg;
    Napi::Array rows;
    Object options;
    if (!readSheetWriteArguments(info, plan, columnSources, templateArg, rows, options)) {
        return env.Null();
    }

    JsRowSource source(env, rows, std::move(columnSources));
    std::vector<uint8_t> out;
    std::string error;
    const bool ok = templateArg.present
        ? replaceSheetData(templateArg.source(), plan, source, readCompression(options), out, error)
        : writeNewWorkbook(plan, source, readCompression(options), out, error);
    if (!ok) {
        return failWith(env, error.empty() ? "WRITE_FAILED|Failed to build the workbook" : error);
    }
    if (hasPendingException(env)) {
        return env.Null();
    }

    Object summary = Object::New(env);
    summary.Set("rowCount", Number::New(env, static_cast<double>(rows.Length())));
    summary.Set("sheetName", String::New(env, plan.sheetName));
    return finishWrite(env, out, readOutputPath(options), summary);
}

// ---------------------------------------------------------------------------
// Writing: patch individual cells of an existing workbook (mode 2).
// ---------------------------------------------------------------------------

// Parses `options.updates`. Shared by the sync and async entry points.
bool readUpdateList(Env env, const Object& spec, std::vector<CellUpdate>& updates) {
    if (!spec.Has("updates") || !spec.Get("updates").IsArray()) {
        failWith(env, "INVALID_OPTIONS|options.updates must be an array");
        return false;
    }

    const Array items = spec.Get("updates").As<Array>();
    const uint32_t count = items.Length();
    updates.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        const Value item = items.Get(i);
        if (!item.IsObject()) {
            failWith(env, "INVALID_OPTIONS|options.updates[" + std::to_string(i) +
                              "] must be an object");
            return false;
        }
        Object entry = item.As<Object>();

        CellUpdate update;
        if (entry.Has("sheet")) {
            const Value value = entry.Get("sheet");
            if (value.IsString()) update.sheet = value.As<String>().Utf8Value();
        }
        if (entry.Has("cell")) {
            const Value value = entry.Get("cell");
            if (value.IsString()) update.cell = value.As<String>().Utf8Value();
        }
        if (update.cell.empty()) {
            failWith(env, "INVALID_OPTIONS|options.updates[" + std::to_string(i) +
                              "].cell is required");
            return false;
        }
        if (entry.Has("numberFormat")) {
            const Value value = entry.Get("numberFormat");
            if (value.IsString()) update.numberFormat = value.As<String>().Utf8Value();
        }
        update.value = toWriteCell(entry.Has("value") ? entry.Get("value") : env.Null());
        updates.push_back(std::move(update));
    }
    return true;
}

Value WriteCells(const CallbackInfo& info) {
    Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsObject()) {
        TypeError::New(env, "An options object is required").ThrowAsJavaScriptException();
        return env.Null();
    }
    Object spec = info[0].As<Object>();

    TemplateArgument templateArg;
    if (!readTemplateArgument(spec, templateArg) || !templateArg.present) {
        return failWith(env,
                        "INVALID_OPTIONS|options.template is required (file path or Buffer)");
    }

    std::vector<CellUpdate> updates;
    if (!readUpdateList(env, spec, updates)) {
        return env.Null();
    }

    std::vector<uint8_t> out;
    std::string error;
    if (!updateCells(templateArg.source(), updates, readCompression(spec), out, error)) {
        return failWith(env, error.empty() ? "WRITE_FAILED|Failed to patch the workbook" : error);
    }
    if (hasPendingException(env)) {
        return env.Null();
    }

    Object summary = Object::New(env);
    summary.Set("cells", Number::New(env, static_cast<double>(updates.size())));
    return finishWrite(env, out, readOutputPath(spec), summary);
}

// ---------------------------------------------------------------------------
// Writing: template rendering (mode 3). Values arrive flattened (path ->
// primitive), so the renderer never has to call back into JS.
// ---------------------------------------------------------------------------

// The template workbook may be passed directly (a path or a Buffer).
bool readTemplateValue(const Value& value, TemplateArgument& out) {
    if (value.IsString()) {
        out.present = true;
        out.path = value.As<String>().Utf8Value();
        return true;
    }
    if (value.IsBuffer()) {
        out.present = true;
        Buffer<uint8_t> buffer = value.As<Buffer<uint8_t>>();
        out.bytes.assign(buffer.Data(), buffer.Data() + buffer.Length());
        return true;
    }
    return false;
}

TemplatePlan readTemplatePlan(const Object& options) {
    TemplatePlan plan;
    if (options.Has("sheetName")) {
        const Value value = options.Get("sheetName");
        if (value.IsString()) plan.sheetName = value.As<String>().Utf8Value();
    }
    if (options.Has("strict")) {
        const Value value = options.Get("strict");
        if (value.IsBoolean()) plan.strict = value.As<Boolean>().Value();
    }
    return plan;
}

void readTemplateValues(const Object& source, TemplateValues& out) {
    Napi::Array keys = source.GetPropertyNames();
    const uint32_t count = keys.Length();
    out.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        const Value key = keys.Get(i);
        if (!key.IsString()) continue;

        const WriteCell cell = toWriteCell(source.Get(key));
        TemplateValue value;
        switch (cell.kind) {
            case WriteCell::Kind::Number:
                value.kind = TemplateValue::Kind::Number;
                value.number = cell.number;
                break;
            case WriteCell::Kind::Boolean:
                value.kind = TemplateValue::Kind::Boolean;
                value.boolean = cell.boolean;
                break;
            case WriteCell::Kind::Text:
            case WriteCell::Kind::Empty:
                value.kind = TemplateValue::Kind::Text;
                value.text = cell.text;
                break;
        }
        out.emplace(key.As<String>().Utf8Value(), std::move(value));
    }
}

Value RenderTemplate(const CallbackInfo& info) {
    Env env = info.Env();

    if (info.Length() < 2 || !info[1].IsObject()) {
        return failWith(env,
                        "INVALID_OPTIONS|renderTemplate expects a template and a values object");
    }

    TemplateArgument templateArg;
    if (!readTemplateValue(info[0], templateArg)) {
        return failWith(env, "INVALID_OPTIONS|options.template is required (file path or Buffer)");
    }

    TemplateValues values;
    readTemplateValues(info[1].As<Object>(), values);

    Object options = (info.Length() > 2 && info[2].IsObject()) ? info[2].As<Object>()
                                                               : Object::New(env);
    const TemplatePlan plan = readTemplatePlan(options);

    std::vector<uint8_t> out;
    std::vector<std::string> renderedSheets;
    std::string error;
    if (!renderTemplate(templateArg.source(), values, plan, readCompression(options), out,
                        renderedSheets, error)) {
        return failWith(env, error.empty() ? "WRITE_FAILED|Failed to render the template" : error);
    }
    if (hasPendingException(env)) {
        return env.Null();
    }

    Object summary = Object::New(env);
    summary.Set("sheets", stringArray(env, renderedSheets));
    return finishWrite(env, out, readOutputPath(options), summary);
}

// ---------------------------------------------------------------------------
// Writing: asynchronous twins. The JS inputs are copied into plain C++
// structures on the main thread -- unavoidable, since the data lives in JS --
// and the expensive work (XML generation, deflate, package assembly, the file
// write) then runs on the libuv thread pool. The compact snapshot is what makes
// it legal: no worker ever touches a JS value.
// ---------------------------------------------------------------------------

// Guards every worker body: a failure must surface as a rejected promise, never
// as an exception escaping a thread.
#define BAJA_WORKER_GUARD(body)                             \
    try {                                                   \
        body                                                \
    } catch (const std::exception& e) {                     \
        SetError(std::string("WRITE_FAILED|") + e.what());  \
    } catch (...) {                                         \
        SetError("WRITE_FAILED|Unknown non-std exception"); \
    }

class WriteExcelWorker : public Napi::AsyncWorker {
public:
    WriteExcelWorker(Napi::Env env, WritePlan plan, WriteSnapshot snapshot,
                     TemplateArgument templateArg, zipio::ZipWriter::Compression compression,
                     std::string outputPath)
        : Napi::AsyncWorker(env),
          deferred_(Napi::Promise::Deferred::New(env)),
          plan_(std::move(plan)),
          snapshot_(std::move(snapshot)),
          templateArg_(std::move(templateArg)),
          compression_(compression),
          outputPath_(std::move(outputPath)) {}

    Napi::Promise Promise() { return deferred_.Promise(); }

    void Execute() override {
        BAJA_WORKER_GUARD(
            std::string error;
            const bool ok = templateArg_.present
                ? replaceSheetData(templateArg_.source(), plan_, snapshot_, compression_, out_,
                                   error)
                : writeNewWorkbook(plan_, snapshot_, compression_, out_, error);
            if (!ok) {
                SetError(error.empty() ? "WRITE_FAILED|Failed to build the workbook" : error);
                return;
            }
            if (!outputPath_.empty() && !writeBytesToFile(outputPath_, out_, error)) {
                SetError(error);
            }
        )
    }

    void OnOK() override {
        Napi::Env env = Env();
        if (outputPath_.empty()) {
            deferred_.Resolve(Buffer<uint8_t>::Copy(env, out_.data(), out_.size()));
            return;
        }
        Object summary = Object::New(env);
        summary.Set("bytes", Number::New(env, static_cast<double>(out_.size())));
        summary.Set("rowCount", Number::New(env, static_cast<double>(snapshot_.rowCount())));
        summary.Set("sheetName", String::New(env, plan_.sheetName));
        deferred_.Resolve(summary);
    }

    void OnError(const Napi::Error& e) override {
        deferred_.Reject(makeCodedError(Env(), e.Message()).Value());
    }

private:
    Napi::Promise::Deferred deferred_;
    WritePlan plan_;
    WriteSnapshot snapshot_;
    TemplateArgument templateArg_;
    zipio::ZipWriter::Compression compression_;
    std::string outputPath_;
    std::vector<uint8_t> out_;
};

Value WriteExcelAsync(const CallbackInfo& info) {
    Env env = info.Env();

    WritePlan plan;
    std::vector<ColumnSource> columnSources;
    TemplateArgument templateArg;
    Napi::Array rows;
    Object options;
    if (!readSheetWriteArguments(info, plan, columnSources, templateArg, rows, options)) {
        return env.Null();
    }

    WriteSnapshot snapshot;
    snapshotRows(env, rows, columnSources, snapshot);
    if (hasPendingException(env)) {
        return env.Null();
    }

    auto* worker = new WriteExcelWorker(env, std::move(plan), std::move(snapshot),
                                        std::move(templateArg), readCompression(options),
                                        readOutputPath(options));
    const Napi::Promise promise = worker->Promise();
    worker->Queue();
    return promise;
}

class WriteCellsWorker : public Napi::AsyncWorker {
public:
    WriteCellsWorker(Napi::Env env, TemplateArgument templateArg, std::vector<CellUpdate> updates,
                     zipio::ZipWriter::Compression compression, std::string outputPath)
        : Napi::AsyncWorker(env),
          deferred_(Napi::Promise::Deferred::New(env)),
          templateArg_(std::move(templateArg)),
          updates_(std::move(updates)),
          compression_(compression),
          outputPath_(std::move(outputPath)) {}

    Napi::Promise Promise() { return deferred_.Promise(); }

    void Execute() override {
        BAJA_WORKER_GUARD(
            std::string error;
            if (!updateCells(templateArg_.source(), updates_, compression_, out_, error)) {
                SetError(error.empty() ? "WRITE_FAILED|Failed to patch the workbook" : error);
                return;
            }
            if (!outputPath_.empty() && !writeBytesToFile(outputPath_, out_, error)) {
                SetError(error);
            }
        )
    }

    void OnOK() override {
        Napi::Env env = Env();
        if (outputPath_.empty()) {
            deferred_.Resolve(Buffer<uint8_t>::Copy(env, out_.data(), out_.size()));
            return;
        }
        Object summary = Object::New(env);
        summary.Set("bytes", Number::New(env, static_cast<double>(out_.size())));
        summary.Set("cells", Number::New(env, static_cast<double>(updates_.size())));
        deferred_.Resolve(summary);
    }

    void OnError(const Napi::Error& e) override {
        deferred_.Reject(makeCodedError(Env(), e.Message()).Value());
    }

private:
    Napi::Promise::Deferred deferred_;
    TemplateArgument templateArg_;
    std::vector<CellUpdate> updates_;
    zipio::ZipWriter::Compression compression_;
    std::string outputPath_;
    std::vector<uint8_t> out_;
};

Value WriteCellsAsync(const CallbackInfo& info) {
    Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsObject()) {
        TypeError::New(env, "An options object is required").ThrowAsJavaScriptException();
        return env.Null();
    }
    Object spec = info[0].As<Object>();

    TemplateArgument templateArg;
    if (!readTemplateArgument(spec, templateArg) || !templateArg.present) {
        return failWith(env,
                        "INVALID_OPTIONS|options.template is required (file path or Buffer)");
    }

    std::vector<CellUpdate> updates;
    if (!readUpdateList(env, spec, updates)) {
        return env.Null();
    }

    auto* worker = new WriteCellsWorker(env, std::move(templateArg), std::move(updates),
                                        readCompression(spec), readOutputPath(spec));
    const Napi::Promise promise = worker->Promise();
    worker->Queue();
    return promise;
}

class RenderTemplateWorker : public Napi::AsyncWorker {
public:
    RenderTemplateWorker(Napi::Env env, TemplateArgument templateArg, TemplateValues values,
                         TemplatePlan plan, zipio::ZipWriter::Compression compression,
                         std::string outputPath)
        : Napi::AsyncWorker(env),
          deferred_(Napi::Promise::Deferred::New(env)),
          templateArg_(std::move(templateArg)),
          values_(std::move(values)),
          plan_(plan),
          compression_(compression),
          outputPath_(std::move(outputPath)) {}

    Napi::Promise Promise() { return deferred_.Promise(); }

    void Execute() override {
        BAJA_WORKER_GUARD(
            std::string error;
            if (!renderTemplate(templateArg_.source(), values_, plan_, compression_, out_,
                                sheets_, error)) {
                SetError(error.empty() ? "WRITE_FAILED|Failed to render the template" : error);
                return;
            }
            if (!outputPath_.empty() && !writeBytesToFile(outputPath_, out_, error)) {
                SetError(error);
            }
        )
    }

    void OnOK() override {
        Napi::Env env = Env();
        if (outputPath_.empty()) {
            deferred_.Resolve(Buffer<uint8_t>::Copy(env, out_.data(), out_.size()));
            return;
        }
        Object summary = Object::New(env);
        summary.Set("bytes", Number::New(env, static_cast<double>(out_.size())));
        summary.Set("sheets", stringArray(env, sheets_));
        deferred_.Resolve(summary);
    }

    void OnError(const Napi::Error& e) override {
        deferred_.Reject(makeCodedError(Env(), e.Message()).Value());
    }

private:
    Napi::Promise::Deferred deferred_;
    TemplateArgument templateArg_;
    TemplateValues values_;
    TemplatePlan plan_;
    zipio::ZipWriter::Compression compression_;
    std::string outputPath_;
    std::vector<uint8_t> out_;
    std::vector<std::string> sheets_;
};

Value RenderTemplateAsync(const CallbackInfo& info) {
    Env env = info.Env();

    if (info.Length() < 2 || !info[1].IsObject()) {
        return failWith(env,
                        "INVALID_OPTIONS|renderTemplate expects a template and a values object");
    }

    TemplateArgument templateArg;
    if (!readTemplateValue(info[0], templateArg)) {
        return failWith(env, "INVALID_OPTIONS|options.template is required (file path or Buffer)");
    }

    TemplateValues values;
    readTemplateValues(info[1].As<Object>(), values);

    Object options = (info.Length() > 2 && info[2].IsObject()) ? info[2].As<Object>()
                                                               : Object::New(env);
    auto* worker = new RenderTemplateWorker(env, std::move(templateArg), std::move(values),
                                            readTemplatePlan(options), readCompression(options),
                                            readOutputPath(options));
    const Napi::Promise promise = worker->Promise();
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
    exports.Set("readExcelBatched", Function::New(env, ReadExcelBatched));
    exports.Set("readExcelBatchedAsync", Function::New(env, ReadExcelBatchedAsync));
    exports.Set("writeExcel", Function::New(env, WriteExcel));
    exports.Set("writeCells", Function::New(env, WriteCells));
    exports.Set("renderTemplate", Function::New(env, RenderTemplate));
    exports.Set("writeExcelAsync", Function::New(env, WriteExcelAsync));
    exports.Set("writeCellsAsync", Function::New(env, WriteCellsAsync));
    exports.Set("renderTemplateAsync", Function::New(env, RenderTemplateAsync));
    return exports;
}

NODE_API_MODULE(baja_xlsx, Init)
