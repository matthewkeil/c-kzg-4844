#include "kzg.h"

/**
 * Convert a blob to a KZG commitment.
 *
 * @param[in] {Blob} blob - The blob representing the polynomial to be
 *                          committed to
 *
 * @return {KZGCommitment} - The resulting commitment
 *
 * @throws {TypeError} - For invalid arguments or failure of the native library
 */
Napi::Value BlobToKzgCommitment(const Napi::CallbackInfo &info) {
    Napi::Env env = info.Env();
    Blob *blob = get_blob(env, info[0]);
    if (blob == nullptr) {
        return env.Null();
    }
    KZGSettings *kzg_settings = get_kzg_settings(env, info);
    if (kzg_settings == nullptr) {
        return env.Null();
    }

    KZGCommitment commitment;
    C_KZG_RET ret = blob_to_kzg_commitment(&commitment, blob, kzg_settings);
    if (ret != C_KZG_OK) {
        std::ostringstream msg;
        msg << "Failed to convert blob to commitment: " << from_c_kzg_ret(ret);
        Napi::Error::New(env, msg.str()).ThrowAsJavaScriptException();
        return env.Undefined();
    }

    return Napi::Buffer<uint8_t>::Copy(
        env, reinterpret_cast<uint8_t *>(&commitment), BYTES_PER_COMMITMENT
    );
}

/**
 * Get the cells for a given blob.
 *
 * @param[in] {Blob}    blob - The blob to get cells for
 *
 * @return {Cell[]} - An array of cells
 *
 * @throws {Error} - Failure to allocate or compute cells
 */
Napi::Value ComputeCells(const Napi::CallbackInfo &info) {
    Napi::Env env = info.Env();
    Napi::Value result = env.Null();
    Blob *blob = get_blob(env, info[0]);
    if (blob == nullptr) {
        return env.Null();
    }
    KZGSettings *kzg_settings = get_kzg_settings(env, info);
    if (kzg_settings == nullptr) {
        return env.Null();
    }

    C_KZG_RET ret;
    Cell *cells = NULL;
    Napi::Array cellArray;

    cells = (Cell *)calloc(CELLS_PER_EXT_BLOB, BYTES_PER_CELL);
    if (cells == nullptr) {
        std::ostringstream msg;
        msg << "Failed to allocate cells in computeCells";
        Napi::Error::New(env, msg.str()).ThrowAsJavaScriptException();
        goto out;
    }

    ret = compute_cells_and_kzg_proofs(cells, NULL, blob, kzg_settings);
    if (ret != C_KZG_OK) {
        std::ostringstream msg;
        msg << "Error in computeCells: " << from_c_kzg_ret(ret);
        Napi::Error::New(env, msg.str()).ThrowAsJavaScriptException();
        goto out;
    }

    cellArray = Napi::Array::New(env, CELLS_PER_EXT_BLOB);
    for (size_t i = 0; i < CELLS_PER_EXT_BLOB; i++) {
        cellArray.Set(
            i,
            Napi::Buffer<uint8_t>::Copy(
                env, reinterpret_cast<uint8_t *>(&cells[i]), BYTES_PER_CELL
            )
        );
    }

    result = cellArray;

out:
    free(cells);
    return result;
}

/**
 * Convert an array of cells to a blob.
 *
 * @param[in] {Cell[]}  cells - The cells to convert to a blob
 *
 * @return {Blob} - The blob for the given cells
 *
 * @throws {Error} - Invalid input, failure to allocate, or invalid conversion
 */
Napi::Value CellsToBlob(const Napi::CallbackInfo &info) {
    C_KZG_RET ret;
    Cell *cells = NULL;
    Napi::Env env = info.Env();
    Napi::Value result = env.Null();
    if (!info[0].IsArray()) {
        Napi::Error::New(env, "Cells must be an array")
            .ThrowAsJavaScriptException();
        return result;
    }
    Napi::Array cells_param = info[0].As<Napi::Array>();
    if (cells_param.Length() != CELLS_PER_EXT_BLOB) {
        Napi::Error::New(env, "Cells must have CELLS_PER_EXT_BLOB cells")
            .ThrowAsJavaScriptException();
        goto out;
    }

    cells = (Cell *)calloc(CELLS_PER_EXT_BLOB, BYTES_PER_CELL);
    if (cells == nullptr) {
        Napi::Error::New(env, "Error while allocating memory for cells")
            .ThrowAsJavaScriptException();
        goto out;
    }

    for (size_t i = 0; i < CELLS_PER_EXT_BLOB; i++) {
        // add HandleScope here to release reference to temp values
        // after each iteration since data is being memcpy
        Napi::HandleScope scope{env};
        Cell *cell = get_cell(env, cells_param[i]);
        if (cell == nullptr) {
            goto out;
        }
        memcpy(&cells[i], cell, BYTES_PER_CELL);
    }

    Blob blob;
    ret = cells_to_blob(&blob, cells);

    if (ret != C_KZG_OK) {
        std::ostringstream msg;
        msg << "Error in cellsToBlob: " << from_c_kzg_ret(ret);
        Napi::Error::New(env, msg.str()).ThrowAsJavaScriptException();
        goto out;
    }

    result = Napi::Buffer<uint8_t>::Copy(
        env, reinterpret_cast<uint8_t *>(&blob), BYTES_PER_BLOB
    );

out:
    free(cells);
    return result;
}

/**
 * Given at least 50% of cells, reconstruct the missing ones.
 *
 * @param[in] {number[]}  cellIds - The identifiers for the cells you have
 * @param[in] {Cell[]}    cells - The cells you have
 *
 * @return {Cell[]} - All cells for that blob
 *
 * @throws {Error} - Invalid input, failure to allocate or error recovering
 * cells
 */
Napi::Value RecoverAllCells(const Napi::CallbackInfo &info) {
    C_KZG_RET ret;
    uint64_t *cell_ids = NULL;
    Cell *cells = NULL;
    Cell *recovered = NULL;
    Napi::Array cellArray;
    size_t num_cells;

    Napi::Env env = info.Env();
    Napi::Value result = env.Null();
    if (!info[0].IsArray()) {
        Napi::Error::New(env, "CellIds must be an array")
            .ThrowAsJavaScriptException();
        return result;
    }
    if (!info[1].IsArray()) {
        Napi::Error::New(env, "Cells must be an array")
            .ThrowAsJavaScriptException();
        return result;
    }
    KZGSettings *kzg_settings = get_kzg_settings(env, info);
    if (kzg_settings == nullptr) {
        return env.Null();
    }

    Napi::Array cell_ids_param = info[0].As<Napi::Array>();
    Napi::Array cells_param = info[1].As<Napi::Array>();

    if (cell_ids_param.Length() != cells_param.Length()) {
        Napi::Error::New(env, "There must equal lengths of cellIds and cells")
            .ThrowAsJavaScriptException();
        goto out;
    }

    num_cells = cells_param.Length();
    cell_ids = (uint64_t *)calloc(num_cells, sizeof(uint64_t));
    if (cell_ids == nullptr) {
        Napi::Error::New(env, "Error while allocating memory for cell_ids")
            .ThrowAsJavaScriptException();
        goto out;
    }
    cells = (Cell *)calloc(num_cells, BYTES_PER_CELL);
    if (cells == nullptr) {
        Napi::Error::New(env, "Error while allocating memory for cells")
            .ThrowAsJavaScriptException();
        goto out;
    }
    recovered = (Cell *)calloc(CELLS_PER_EXT_BLOB, BYTES_PER_CELL);
    if (recovered == nullptr) {
        Napi::Error::New(env, "Error while allocating memory for recovered")
            .ThrowAsJavaScriptException();
        goto out;
    }

    for (size_t i = 0; i < num_cells; i++) {
        // add HandleScope here to release reference to temp values
        // after each iteration since data is being memcpy
        Napi::HandleScope scope{env};
        cell_ids[i] = get_cell_id(env, cell_ids_param[i]);
        Cell *cell = get_cell(env, cells_param[i]);
        if (cell == nullptr) {
            goto out;
        }
        memcpy(&cells[i], cell, BYTES_PER_CELL);
    }

    ret = recover_all_cells(
        recovered, cell_ids, cells, num_cells, kzg_settings
    );
    if (ret != C_KZG_OK) {
        std::ostringstream msg;
        msg << "Error in recoverAllCells: " << from_c_kzg_ret(ret);
        Napi::Error::New(env, msg.str()).ThrowAsJavaScriptException();
        goto out;
    }

    cellArray = Napi::Array::New(env, CELLS_PER_EXT_BLOB);
    for (size_t i = 0; i < CELLS_PER_EXT_BLOB; i++) {
        cellArray.Set(
            i,
            Napi::Buffer<uint8_t>::Copy(
                env, reinterpret_cast<uint8_t *>(&recovered[i]), BYTES_PER_CELL
            )
        );
    }

    result = cellArray;

out:
    free(cells);
    return result;
}
