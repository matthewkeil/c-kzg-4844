#include "blst.h"
#include "c_kzg_4844.h"
#include <iostream>
#include <napi.h>
#include <sstream> // std::ostringstream
#include <stdio.h>
#include <string_view>

using namespace std::string_literals;

enum class C_KZG_TS_RET {
    OK = 0,         /**< Success! */
    BADARGS,        /**< The supplied data is invalid in some way. */
    ERROR,          /**< Internal error - this should never occur. */
    MALLOC,         /**< Could not allocate memory. */
    JS_ERROR_THROWN /**< JS Error thrown */
};
C_KZG_TS_RET get_bytes_new(
    uint8_t *&bytes,
    const Napi::Env &env,
    const Napi::Value &val,
    size_t length,
    std::string_view name
) {
    if (!val.IsTypedArray() ||
        val.As<Napi::TypedArray>().TypedArrayType() != napi_uint8_array) {
        Napi::TypeError::New(
            env, "Expected "s + std::string(name) + " to be a Uint8Array"s
        )
            .ThrowAsJavaScriptException();
        bytes = nullptr;
        return C_KZG_TS_RET::JS_ERROR_THROWN;
    }
    Napi::Uint8Array array = val.As<Napi::Uint8Array>();
    if (array.ByteLength() != length) {
        Napi::TypeError::New(
            env,
            "Expected "s + std::string(name) + " to be " +
                std::to_string(length) + " bytes"s
        )
            .ThrowAsJavaScriptException();
        bytes = nullptr;
        return C_KZG_TS_RET::OK;
    }
    bytes = array.Data();
    return C_KZG_TS_RET::OK;
}

C_KZG_TS_RET get_blob_new(
    Blob *&blob, const Napi::Env &env, const Napi::Value &val
) {
    return get_bytes_new(
        reinterpret_cast<uint8_t *&>(blob), env, val, BYTES_PER_BLOB, "blob"
    );
}

C_KZG_TS_RET get_bytes32(
    Bytes32 *&bytes,
    const Napi::Env &env,
    const Napi::Value &val,
    std::string_view name
) {
    return get_bytes_new(
        reinterpret_cast<uint8_t *&>(bytes),
        env,
        val,
        BYTES_PER_FIELD_ELEMENT,
        name
    );
}

C_KZG_TS_RET get_bytes48(
    Bytes48 *&bytes,
    const Napi::Env &env,
    const Napi::Value &val,
    std::string_view name
) {
    return get_bytes_new(
        reinterpret_cast<uint8_t *&>(bytes),
        env,
        val,
        BYTES_PER_FIELD_ELEMENT,
        name
    );
}

C_KZG_TS_RET get_cell(
    Cell *&cell, const Napi::Env &env, const Napi::Value &val
) {
    return get_bytes_new(
        reinterpret_cast<uint8_t *&>(cell), env, val, BYTES_PER_CELL, "cell"
    );
}

C_KZG_TS_RET get_cell_id(
    uint64_t &cell_id, const Napi::Env &env, const Napi::Value &val
) {
    if (!val.IsNumber()) {
        Napi::TypeError::New(env, "cell id should be a number")
            .ThrowAsJavaScriptException();
        return C_KZG_TS_RET::JS_ERROR_THROWN;
    }
    cell_id = static_cast<uint64_t>(val.As<Napi::Number>().Uint32Value());
    return C_KZG_TS_RET::OK;
}
