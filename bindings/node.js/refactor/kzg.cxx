#include "kzg.h"

std::string from_c_kzg_ret(C_KZG_RET ret) {
    switch (ret) {
    case C_KZG_RET::C_KZG_OK:
        return "C_KZG_OK";
    case C_KZG_RET::C_KZG_BADARGS:
        return "C_KZG_BADARGS";
    case C_KZG_RET::C_KZG_ERROR:
        return "C_KZG_ERROR";
    case C_KZG_RET::C_KZG_MALLOC:
        return "C_KZG_MALLOC";
    default:
        std::ostringstream msg;
        msg << "UNKNOWN (" << ret << ")";
        return msg.str();
    }
}

inline uint8_t *get_bytes(
    const Napi::Env &env,
    const Napi::Value &val,
    size_t length,
    std::string_view name
) {
    if (!val.IsTypedArray() ||
        val.As<Napi::TypedArray>().TypedArrayType() != napi_uint8_array) {
        std::ostringstream msg;
        msg << "Expected " << name << " to be a Uint8Array";
        Napi::TypeError::New(env, msg.str()).ThrowAsJavaScriptException();
        return nullptr;
    }
    Napi::Uint8Array array = val.As<Napi::Uint8Array>();
    if (array.ByteLength() != length) {
        std::ostringstream msg;
        msg << "Expected " << name << " to be " << length << " bytes";
        Napi::TypeError::New(env, msg.str()).ThrowAsJavaScriptException();
        return nullptr;
    }
    return array.Data();
}
C_KSG_TS_RET get_blob(
    Blob *blob, const Napi::Env &env, const Napi::Value &val
) {
    return reinterpret_cast<Blob *>(get_bytes(env, val, BYTES_PER_BLOB, "blob")
    );
}
C_KSG_TS_RET get_bytes32(
    Bytes32 *bytes,
    const Napi::Env &env,
    const Napi::Value &val,
    std::string_view name
) {
    return reinterpret_cast<Bytes32 *>(
        get_bytes(env, val, BYTES_PER_FIELD_ELEMENT, name)
    );
}
C_KSG_TS_RET get_bytes48(
    Bytes48 *bytes,
    const Napi::Env &env,
    const Napi::Value &val,
    std::string_view name
) {
    return reinterpret_cast<Bytes48 *>(
        get_bytes(env, val, BYTES_PER_COMMITMENT, name)
    );
}
C_KSG_TS_RET get_cell(
    Cell *cell, const Napi::Env &env, const Napi::Value &val
) {
    return reinterpret_cast<Cell *>(get_bytes(env, val, BYTES_PER_CELL, "cell")
    );
}
C_KSG_TS_RET get_cell_id(
    uint64_t &id, const Napi::Env &env, const Napi::Value &val
) {
    if (!val.IsNumber()) {
        Napi::TypeError::New(env, "cell id should be a number")
            .ThrowAsJavaScriptException();
        /* TODO: how will the caller know there was an error? */
        return 0;
    }
    id = static_cast<uint64_t>(val.As<Napi::Number>().DoubleValue());
    return C_KSG_TS_RET::C_KZG_TS_OK;
}

/**
 * This cleanup function follows the `napi_finalize` interface and will be
 * called by the runtime when the exports object is garbage collected. Is
 * passed with napi_set_instance_data call when data is set.
 *
 * @remark This function should not be called, only the runtime should do
 *         the cleanup.
 *
 * @param[in] env  (unused)
 * @param[in] data Pointer KzgAddonData stored by the runtime
 * @param[in] hint (unused)
 */
void delete_kzg_addon_data(napi_env /*env*/, void *data, void * /*hint*/) {
    if (((KzgAddonData *)data)->is_setup) {
        free_trusted_setup(&((KzgAddonData *)data)->settings);
    }
    free(data);
}

/**
 * Get kzg_settings from bindings instance data
 *
 * Checks for:
 * - loadTrustedSetup has been run
 *
 * Designed to raise the correct javascript exception and return a
 * valid pointer to the calling context to avoid native stack-frame
 * unwinds.  Calling context can check for `nullptr` to see if an
 * exception was raised or a valid KZGSettings was returned.
 *
 * @param[in] env    Passed from calling context
 * @param[in] val    Napi::Value to validate and get pointer from
 *
 * @return - Pointer to the KZGSettings
 */
KZGSettings *get_kzg_settings(Napi::Env &env, const Napi::CallbackInfo &info) {
    KzgAddonData *data = env.GetInstanceData<KzgAddonData>();
    if (!data->is_setup) {
        Napi::Error::New(
            env,
            "Must run loadTrustedSetup before running any other c-kzg functions"
        )
            .ThrowAsJavaScriptException();
        return nullptr;
    }
    return &(data->settings);
}

Napi::Value LoadTrustedSetup(const Napi::CallbackInfo &info) {
    Napi::Env env = info.Env();

    // Check if the trusted setup is already loaded
    KzgAddonData *data = env.GetInstanceData<KzgAddonData>();
    if (data->is_setup) {
        Napi::Error::New(env, "Error trusted setup is already loaded")
            .ThrowAsJavaScriptException();
        return env.Undefined();
    }

    // Parse the precompute value
    size_t precompute = static_cast<size_t>(
        info[0].As<Napi::Number>().Int64Value()
    );

    // Open the trusted setup file
    std::string file_path = info[1].As<Napi::String>().Utf8Value();
    FILE *file_handle = fopen(file_path.c_str(), "r");
    if (file_handle == nullptr) {
        Napi::Error::New(env, "Error opening trusted setup file: " + file_path)
            .ThrowAsJavaScriptException();
        return env.Undefined();
    }

    // Load the trusted setup from that file
    C_KZG_RET ret = load_trusted_setup_file(
        &(data->settings), file_handle, precompute
    );
    // Close the trusted setup file
    fclose(file_handle);

    // Check that loading the trusted setup was successful
    if (ret != C_KZG_OK) {
        std::ostringstream msg;
        msg << "Error loading trusted setup file: " << from_c_kzg_ret(ret);
        Napi::Error::New(env, msg.str()).ThrowAsJavaScriptException();
        return env.Undefined();
    }

    data->is_setup = true;
    return env.Undefined();
}

/**
 * Declarations for functions in cells_and_blobs.cxx, proofs.cxx and verify.cxx
 */
Napi::Value BlobToKzgCommitment(const Napi::CallbackInfo &info);
Napi::Value ComputeCells(const Napi::CallbackInfo &info);
Napi::Value CellsToBlob(const Napi::CallbackInfo &info);
Napi::Value RecoverAllCells(const Napi::CallbackInfo &info);
Napi::Value ComputeCells(const Napi::CallbackInfo &info);
Napi::Value CellsToBlob(const Napi::CallbackInfo &info);
Napi::Value RecoverAllCells(const Napi::CallbackInfo &info);
Napi::Value VerifyKzgProof(const Napi::CallbackInfo &info);
Napi::Value VerifyBlobKzgProof(const Napi::CallbackInfo &info);
Napi::Value VerifyBlobKzgProofBatch(const Napi::CallbackInfo &info);
Napi::Value VerifyCellKzgProof(const Napi::CallbackInfo &info);
Napi::Value VerifyCellKzgProofBatch(const Napi::CallbackInfo &info);

Napi::Object Init(Napi::Env env, Napi::Object exports) {
    KzgAddonData *data = (KzgAddonData *)malloc(sizeof(KzgAddonData));
    if (data == nullptr) {
        Napi::Error::New(env, "Error allocating memory for kzg setup handle")
            .ThrowAsJavaScriptException();
        return exports;
    }
    data->is_setup = false;
    napi_status status = napi_set_instance_data(
        env, data, delete_kzg_addon_data, NULL
    );
    if (status != napi_ok) {
        Napi::Error::New(env, "Error setting kzg bindings instance data")
            .ThrowAsJavaScriptException();
        return exports;
    }

    // Functions
    exports["loadTrustedSetup"] = Napi::Function::New(
        env, LoadTrustedSetup, "setup"
    );
    exports["blobToKzgCommitment"] = Napi::Function::New(
        env, BlobToKzgCommitment, "blobToKzgCommitment"
    );
    exports["computeKzgProof"] = Napi::Function::New(
        env, ComputeKzgProof, "computeKzgProof"
    );
    exports["computeBlobKzgProof"] = Napi::Function::New(
        env, ComputeBlobKzgProof, "computeBlobKzgProof"
    );
    exports["verifyKzgProof"] = Napi::Function::New(
        env, VerifyKzgProof, "verifyKzgProof"
    );
    exports["verifyBlobKzgProof"] = Napi::Function::New(
        env, VerifyBlobKzgProof, "verifyBlobKzgProof"
    );
    exports["verifyBlobKzgProofBatch"] = Napi::Function::New(
        env, VerifyBlobKzgProofBatch, "verifyBlobKzgProofBatch"
    );
    exports["computeCells"] = Napi::Function::New(
        env, ComputeCells, "computeCells"
    );
    exports["computeCellsAndKzgProofs"] = Napi::Function::New(
        env, ComputeCellsAndKzgProofs, "computeCellsAndKzgProofs"
    );
    exports["cellsToBlob"] = Napi::Function::New(
        env, CellsToBlob, "cellsToBlob"
    );
    exports["recoverAllCells"] = Napi::Function::New(
        env, RecoverAllCells, "recoverAllCells"
    );
    exports["verifyCellKzgProof"] = Napi::Function::New(
        env, VerifyCellKzgProof, "verifyCellKzgProof"
    );
    exports["verifyCellKzgProofBatch"] = Napi::Function::New(
        env, VerifyCellKzgProofBatch, "verifyCellKzgProofBatch"
    );

    // Constants
    exports["BYTES_PER_BLOB"] = Napi::Number::New(env, BYTES_PER_BLOB);
    exports["BYTES_PER_COMMITMENT"] = Napi::Number::New(
        env, BYTES_PER_COMMITMENT
    );
    exports["BYTES_PER_FIELD_ELEMENT"] = Napi::Number::New(
        env, BYTES_PER_FIELD_ELEMENT
    );
    exports["BYTES_PER_PROOF"] = Napi::Number::New(env, BYTES_PER_PROOF);
    exports["FIELD_ELEMENTS_PER_BLOB"] = Napi::Number::New(
        env, FIELD_ELEMENTS_PER_BLOB
    );
    exports["FIELD_ELEMENTS_PER_EXT_BLOB"] = Napi::Number::New(
        env, FIELD_ELEMENTS_PER_EXT_BLOB
    );
    exports["FIELD_ELEMENTS_PER_CELL"] = Napi::Number::New(
        env, FIELD_ELEMENTS_PER_CELL
    );
    exports["CELLS_PER_EXT_BLOB"] = Napi::Number::New(env, CELLS_PER_EXT_BLOB);
    exports["BYTES_PER_CELL"] = Napi::Number::New(env, BYTES_PER_CELL);

    return exports;
}

NODE_API_MODULE(addon, Init)
