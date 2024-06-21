#include "kzg.h"

C_KSG_TS_RET prepare_compute_kzg_proof() {
    Napi::Env env = info.Env();
    Blob *blob = get_blob(env, info[0]);
    if (blob == nullptr) {
        return env.Null();
    }
    Bytes32 *z_bytes = get_bytes32(env, info[1], "zBytes");
    if (z_bytes == nullptr) {
        return env.Null();
    }
    KZGSettings *kzg_settings = get_kzg_settings(env, info);
    if (kzg_settings == nullptr) {
        return env.Null();
    }
}

C_KSG_RET compute_kzg_proof(
    KZGProof &proof,
    Bytes32 &y_out,
    const Blob *blob,
    const Bytes32 *z_bytes,
    const KZGSettings *kzg_settings
) {
    return compute_kzg_proof(&proof, &y_out, blob, z_bytes, kzg_settings);
}

/**
 * Compute KZG proof for polynomial in Lagrange form at position z.
 *
 * @param[in] {Blob}    blob - The blob (polynomial) to generate a proof for
 * @param[in] {Bytes32} zBytes - The generator z-value for the evaluation points
 *
 * @return {ProofResult} - Tuple containing the resulting proof and evaluation
 *                         of the polynomial at the evaluation point z
 *
 * @throws {TypeError} - for invalid arguments or failure of the native library
 */
Napi::Value ComputeKzgProof(const Napi::CallbackInfo &info) {
    C_KZG_RET ret = prepare_compute_kzg_proof();

    KZGProof proof;
    Bytes32 y_out;
    ret = compute_kzg_proof();
    if (ret != C_KZG_OK) {
        std::ostringstream msg;
        msg << "Failed to compute proof: " << from_c_kzg_ret(ret);
        Napi::Error::New(env, msg.str()).ThrowAsJavaScriptException();
        return env.Undefined();
    }

    Napi::Array tuple = Napi::Array::New(env, 2);
    tuple[(uint32_t)0] = Napi::Buffer<uint8_t>::Copy(
        env, reinterpret_cast<uint8_t *>(&proof), BYTES_PER_PROOF
    );
    tuple[(uint32_t)1] = Napi::Buffer<uint8_t>::Copy(
        env, reinterpret_cast<uint8_t *>(&y_out), BYTES_PER_FIELD_ELEMENT
    );
    return tuple;
}

/**
 * Given a blob, return the KZG proof that is used to verify it against the
 * commitment.
 *
 * @param[in] {Blob}    blob - The blob (polynomial) to generate a proof for
 * @param[in] {Bytes48} commitmentBytes - Commitment to verify
 *
 * @return {KZGProof} - The resulting proof
 *
 * @throws {TypeError} - for invalid arguments or failure of the native library
 */
Napi::Value ComputeBlobKzgProof(const Napi::CallbackInfo &info) {
    Napi::Env env = info.Env();
    Blob *blob = get_blob(env, info[0]);
    if (blob == nullptr) {
        return env.Null();
    }
    Bytes48 *commitment_bytes = get_bytes48(env, info[1], "commitmentBytes");
    if (commitment_bytes == nullptr) {
        return env.Null();
    }
    KZGSettings *kzg_settings = get_kzg_settings(env, info);
    if (kzg_settings == nullptr) {
        return env.Null();
    }

    KZGProof proof;
    C_KZG_RET ret = compute_blob_kzg_proof(
        &proof, blob, commitment_bytes, kzg_settings
    );

    if (ret != C_KZG_OK) {
        std::ostringstream msg;
        msg << "Error in computeBlobKzgProof: " << from_c_kzg_ret(ret);
        Napi::Error::New(env, msg.str()).ThrowAsJavaScriptException();
        return env.Undefined();
    }

    return Napi::Buffer<uint8_t>::Copy(
        env, reinterpret_cast<uint8_t *>(&proof), BYTES_PER_PROOF
    );
}

/**
 * Get the cells and proofs for a given blob.
 *
 * @param[in] {Blob}    blob - the blob to get cells/proofs for
 *
 * @return {[Cell[], KZGProof[]]} - A tuple of cells and proofs
 *
 * @throws {Error} - Failure to allocate or compute cells and proofs
 */
Napi::Value ComputeCellsAndKzgProofs(const Napi::CallbackInfo &info) {
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
    KZGProof *proofs = NULL;
    Napi::Array tuple;
    Napi::Array cellArray;
    Napi::Array proofArray;

    cells = (Cell *)calloc(CELLS_PER_EXT_BLOB, BYTES_PER_CELL);
    if (cells == nullptr) {
        std::ostringstream msg;
        msg << "Failed to allocate cells in computeCellsAndKzgProofs";
        Napi::Error::New(env, msg.str()).ThrowAsJavaScriptException();
        goto out;
    }

    proofs = (KZGProof *)calloc(CELLS_PER_EXT_BLOB, BYTES_PER_PROOF);
    if (proofs == nullptr) {
        std::ostringstream msg;
        msg << "Failed to allocate proofs in computeCellsAndKzgProofs";
        Napi::Error::New(env, msg.str()).ThrowAsJavaScriptException();
        goto out;
    }

    ret = compute_cells_and_kzg_proofs(cells, proofs, blob, kzg_settings);
    if (ret != C_KZG_OK) {
        std::ostringstream msg;
        msg << "Error in computeCellsAndKzgProofs: " << from_c_kzg_ret(ret);
        Napi::Error::New(env, msg.str()).ThrowAsJavaScriptException();
        goto out;
    }

    cellArray = Napi::Array::New(env, CELLS_PER_EXT_BLOB);
    proofArray = Napi::Array::New(env, CELLS_PER_EXT_BLOB);
    for (size_t i = 0; i < CELLS_PER_EXT_BLOB; i++) {
        cellArray.Set(
            i,
            Napi::Buffer<uint8_t>::Copy(
                env, reinterpret_cast<uint8_t *>(&cells[i]), BYTES_PER_CELL
            )
        );
        proofArray.Set(
            i,
            Napi::Buffer<uint8_t>::Copy(
                env, reinterpret_cast<uint8_t *>(&proofs[i]), BYTES_PER_PROOF
            )
        );
    }

    tuple = Napi::Array::New(env, 2);
    tuple[(uint32_t)0] = cellArray;
    tuple[(uint32_t)1] = proofArray;
    result = tuple;

out:
    free(cells);
    free(proofs);
    return result;
}
