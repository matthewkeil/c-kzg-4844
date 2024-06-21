#pragma once
#include "blst.h"
#include "c_kzg_4844.h"
#include <iostream>
#include <napi.h>
#include <sstream> // std::ostringstream
#include <stdio.h>
#include <string_view>

enum C_KSG_TS_RET {
    C_KZG_TS_OK = 0,      /**< Success! */
    C_KZG_BADARGS,        /**< The supplied data is invalid in some way. */
    C_KZG_ERROR,          /**< Internal error - this should never occur. */
    C_KZG_MALLOC,         /**< Could not allocate memory. */
    C_KZG_TS_ERROR_THROWN /**< JS Error thrown */
}

/**
 * Convert C_KZG_RET to a string representation for error messages.
 */
std::string
from_c_kzg_ret(C_KZG_RET ret);

/**
 * Structure containing information needed for the lifetime of the bindings
 * instance. It is not safe to use global static data with worker instances.
 * Native node addons are loaded as a dll's once no matter how many node
 * instances are using the library.  Each node instance will initialize an
 * instance of the bindings and workers share memory space.  In addition
 * the worker JS thread will be independent of the main JS thread. Global
 * statics are not thread safe and have the potential for initialization and
 * clean-up overwrites which results in segfault or undefined behavior.
 *
 * An instance of this struct will get created during initialization and it
 * will be available from the runtime. It can be retrieved via
 * `napi_get_instance_data` or `Napi::Env::GetInstanceData`.
 */
typedef struct {
    bool is_setup;
    KZGSettings settings;
} KzgAddonData;

/**
 * Checks for:
 * - arg is Uint8Array or Buffer (inherits from Uint8Array)
 * - underlying ArrayBuffer length is correct
 *
 * Internal function for argument validation. Prefer to use
 * the helpers below that already have the reinterpreted casts:
 * - get_blob
 * - get_bytes32
 * - get_bytes48
 *
 * Built to pass in a raw Napi::Value so it can be used like
 * `get_bytes(env, info[0])` or can also be used to pull props from
 * arrays like `get_bytes(env, passed_napi_array[2])`.
 *
 * Designed to raise the correct javascript exception and return a
 * valid pointer to the calling context to avoid native stack-frame
 * unwinds.  Calling context can check for `nullptr` to see if an
 * exception was raised or a valid pointer was returned from V8.
 *
 * @param[in] env    Passed from calling context
 * @param[in] val    Napi::Value to validate and get pointer from
 * @param[in] length Byte length to validate Uint8Array data against
 * @param[in] name   Name of prop being validated for error reporting
 *
 * @return - native pointer to first byte in ArrayBuffer
 */
C_KSG_TS_RET get_bytes(
    uint8_t *bytes,
    const Napi::Env &env,
    const Napi::Value &val,
    size_t length,
    std::string_view name
);
/**
 * Unwraps a Blob from a Napi::Value
 *
 * @param[in] env    Passed from calling context
 * @param[in] val    Napi::Value to validate and get pointer from
 */
C_KSG_TS_RET get_blob(Blob *blob, const Napi::Env &env, const Napi::Value &val);
/**
 * Unwraps Bytes32 from a Napi::Value. Returns proper error message with correct
 * name of the data type.
 *
 * @param[in] env    Passed from calling context
 * @param[in] val    Napi::Value to validate and get pointer from
 * @param[in] name   Name of prop being validated for error reporting
 */
C_KSG_TS_RET get_bytes32(
    Bytes32 *bytes,
    const Napi::Env &env,
    const Napi::Value &val,
    std::string_view name
);
/**
 * Unwraps Bytes48 from a Napi::Value. Returns proper error message with correct
 * name of the data type.
 *
 * @param[in] env    Passed from calling context
 * @param[in] val    Napi::Value to validate and get pointer from
 * @param[in] name   Name of prop being validated for error reporting
 */
C_KSG_TS_RET get_bytes48(
    Bytes48 *bytes,
    const Napi::Env &env,
    const Napi::Value &val,
    std::string_view name
);
/**
 * Unwraps a Cell from a Napi::Value
 *
 * @param[in] env    Passed from calling context
 * @param[in] val    Napi::Value to validate and get pointer from
 */
C_KSG_TS_RET get_cell(Cell *cell, const Napi::Env &env, const Napi::Value &val);
/**
 * Unwraps a CellId from a Napi::Value
 *
 * @param[in] env    Passed from calling context
 * @param[in] val    Napi::Value to validate and get pointer from
 */
C_KSG_TS_RET get_cell_id(
    uint64_t &id, const Napi::Env &env, const Napi::Value &val
);
