#include "kzg.h"

/**
 * Verify a KZG poof claiming that `p(z) == y`.
 *
 * @param[in] {Bytes48} commitmentBytes - The serialized commitment
 * corresponding to polynomial p(x)
 * @param[in] {Bytes32} zBytes - The serialized evaluation point
 * @param[in] {Bytes32} yBytes - The serialized claimed evaluation result
 * @param[in] {Bytes48} proofBytes - The serialized KZG proof
 *
 * @return {boolean} - true/false depending on proof validity
 *
 * @throws {TypeError} - for invalid arguments or failure of the native library
 */
Napi::Value VerifyKzgProof(const Napi::CallbackInfo &info) {
    Napi::Env env = info.Env();
    Bytes48 *commitment_bytes = get_bytes48(env, info[0], "commitmentBytes");
    if (commitment_bytes == nullptr) {
        return env.Null();
    }
    Bytes32 *z_bytes = get_bytes32(env, info[1], "zBytes");
    if (z_bytes == nullptr) {
        return env.Null();
    }
    Bytes32 *y_bytes = get_bytes32(env, info[2], "yBytes");
    if (y_bytes == nullptr) {
        return env.Null();
    }
    Bytes48 *proof_bytes = get_bytes48(env, info[3], "proofBytes");
    if (proof_bytes == nullptr) {
        return env.Null();
    }
    KZGSettings *kzg_settings = get_kzg_settings(env, info);
    if (kzg_settings == nullptr) {
        return env.Null();
    }

    bool out;
    C_KZG_RET ret = verify_kzg_proof(
        &out, commitment_bytes, z_bytes, y_bytes, proof_bytes, kzg_settings
    );

    if (ret != C_KZG_OK) {
        std::ostringstream msg;
        msg << "Failed to verify KZG proof: " << from_c_kzg_ret(ret);
        Napi::Error::New(env, msg.str()).ThrowAsJavaScriptException();
        return env.Undefined();
    }

    return Napi::Boolean::New(env, out);
}

/**
 * Given a blob and its proof, verify that it corresponds to the provided
 * commitment.
 *
 * @param[in] {Blob}    blob - The serialized blob to verify
 * @param[in] {Bytes48} commitmentBytes - The serialized commitment to verify
 * @param[in] {Bytes48} proofBytes - The serialized KZG proof for verification
 *
 * @return {boolean} - true/false depending on proof validity
 *
 * @throws {TypeError} - for invalid arguments or failure of the native library
 */
Napi::Value VerifyBlobKzgProof(const Napi::CallbackInfo &info) {
    Napi::Env env = info.Env();
    Blob *blob_bytes = get_blob(env, info[0]);
    if (blob_bytes == nullptr) {
        return env.Null();
    }
    Bytes48 *commitment_bytes = get_bytes48(env, info[1], "commitmentBytes");
    if (commitment_bytes == nullptr) {
        return env.Null();
    }
    Bytes48 *proof_bytes = get_bytes48(env, info[2], "proofBytes");
    if (proof_bytes == nullptr) {
        return env.Null();
    }
    KZGSettings *kzg_settings = get_kzg_settings(env, info);
    if (kzg_settings == nullptr) {
        return env.Null();
    }

    bool out;
    C_KZG_RET ret = verify_blob_kzg_proof(
        &out, blob_bytes, commitment_bytes, proof_bytes, kzg_settings
    );

    if (ret != C_KZG_OK) {
        std::ostringstream msg;
        msg << "Error in verifyBlobKzgProof: " << from_c_kzg_ret(ret);
        Napi::Error::New(env, msg.str()).ThrowAsJavaScriptException();
        return env.Undefined();
    }

    return Napi::Boolean::New(env, out);
}

/**
 * Given an array of blobs and their proofs, verify that they corresponds to
 * their provided commitment.
 *
 * @remark blobs[0] relates to commitmentBytes[0] and proofBytes[0]
 *
 * @param[in] {Blob}    blobs - An array of serialized blobs to verify
 * @param[in] {Bytes48} commitmentBytes - An array of serialized commitments to
 *                                        verify
 * @param[in] {Bytes48} proofBytes - An array of serialized KZG proofs for
 *                                   verification
 *
 * @return {boolean} - true/false depending on batch validity
 *
 * @throws {TypeError} - for invalid arguments or failure of the native library
 */
Napi::Value VerifyBlobKzgProofBatch(const Napi::CallbackInfo &info) {
    Napi::Env env = info.Env();
    C_KZG_RET ret;
    Blob *blobs = NULL;
    Bytes48 *commitments = NULL;
    Bytes48 *proofs = NULL;
    Napi::Value result = env.Null();
    if (!(info[0].IsArray() && info[1].IsArray() && info[2].IsArray())) {
        Napi::Error::New(
            env, "Blobs, commitments, and proofs must all be arrays"
        )
            .ThrowAsJavaScriptException();
        return result;
    }
    Napi::Array blobs_param = info[0].As<Napi::Array>();
    Napi::Array commitments_param = info[1].As<Napi::Array>();
    Napi::Array proofs_param = info[2].As<Napi::Array>();
    KZGSettings *kzg_settings = get_kzg_settings(env, info);
    if (kzg_settings == nullptr) {
        return env.Null();
    }
    uint32_t count = blobs_param.Length();
    if (count != commitments_param.Length() || count != proofs_param.Length()) {
        Napi::Error::New(
            env, "Requires equal number of blobs/commitments/proofs"
        )
            .ThrowAsJavaScriptException();
        return result;
    }
    blobs = (Blob *)calloc(count, sizeof(Blob));
    if (blobs == nullptr) {
        Napi::Error::New(env, "Error while allocating memory for blobs")
            .ThrowAsJavaScriptException();
        goto out;
    }
    commitments = (Bytes48 *)calloc(count, sizeof(Bytes48));
    if (commitments == nullptr) {
        Napi::Error::New(env, "Error while allocating memory for commitments")
            .ThrowAsJavaScriptException();
        goto out;
    }
    proofs = (Bytes48 *)calloc(count, sizeof(Bytes48));
    if (proofs == nullptr) {
        Napi::Error::New(env, "Error while allocating memory for proofs")
            .ThrowAsJavaScriptException();
        goto out;
    }

    for (uint32_t index = 0; index < count; index++) {
        // add HandleScope here to release reference to temp values
        // after each iteration since data is being memcpy
        Napi::HandleScope scope{env};
        Blob *blob = get_blob(env, blobs_param[index]);
        if (blob == nullptr) {
            goto out;
        }
        memcpy(&blobs[index], blob, BYTES_PER_BLOB);
        Bytes48 *commitment = get_bytes48(
            env, commitments_param[index], "commitmentBytes"
        );
        if (commitment == nullptr) {
            goto out;
        }
        memcpy(&commitments[index], commitment, BYTES_PER_COMMITMENT);
        Bytes48 *proof = get_bytes48(env, proofs_param[index], "proofBytes");
        if (proof == nullptr) {
            goto out;
        }
        memcpy(&proofs[index], proof, BYTES_PER_PROOF);
    }

    bool out;
    ret = verify_blob_kzg_proof_batch(
        &out, blobs, commitments, proofs, count, kzg_settings
    );

    if (ret != C_KZG_OK) {
        std::ostringstream msg;
        msg << "Error in verifyBlobKzgProofBatch: " << from_c_kzg_ret(ret);
        Napi::Error::New(env, msg.str()).ThrowAsJavaScriptException();
        goto out;
    }

    result = Napi::Boolean::New(env, out);

out:
    free(blobs);
    free(commitments);
    free(proofs);
    return result;
}

/**
 * Verify that a cell's proof is valid.
 *
 * @param[in] {Bytes48}   commitmentBytes - Commitment bytes
 * @param[in] {number}    cellId - The cell identifier
 * @param[in] {Cell}      cell - The cell to verify
 * @param[in] {Bytes48}   proofBytes - The proof for the cell
 *
 * @return {boolean} - True if the cell is valid with respect to this commitment
 *
 * @throws {Error} - Errors validating cell's proof
 */
Napi::Value VerifyCellKzgProof(const Napi::CallbackInfo &info) {
    Napi::Env env = info.Env();
    Bytes48 *commitment_bytes = get_bytes48(env, info[0], "commitmentBytes");
    if (commitment_bytes == nullptr) {
        return env.Null();
    }
    uint64_t cell_id = get_cell_id(env, info[1]);
    Cell *cell = get_cell(env, info[2]);
    if (cell == nullptr) {
        return env.Null();
    }
    Bytes48 *proof_bytes = get_bytes48(env, info[3], "proofBytes");
    if (proof_bytes == nullptr) {
        return env.Null();
    }
    KZGSettings *kzg_settings = get_kzg_settings(env, info);
    if (kzg_settings == nullptr) {
        return env.Null();
    }

    bool out;
    C_KZG_RET ret = verify_cell_kzg_proof(
        &out, commitment_bytes, cell_id, cell, proof_bytes, kzg_settings
    );

    if (ret != C_KZG_OK) {
        std::ostringstream msg;
        msg << "Error in verifyCellKzgProof: " << from_c_kzg_ret(ret);
        Napi::Error::New(env, msg.str()).ThrowAsJavaScriptException();
        return env.Undefined();
    }

    return Napi::Boolean::New(env, out);
}

/**
 * Verify that multiple cells' proofs are valid.
 *
 * @param[in] {Bytes48[]} commitmentsBytes - The commitments for all blobs
 * @param[in] {number[]}  rowIndices - The row index for each cell
 * @param[in] {number[]}  columnIndices - The column index for each cell
 * @param[in] {Cell[]}    cells - The cells to verify
 * @param[in] {Bytes48[]} proofsBytes - The proof for each cell
 *
 * @return {boolean} - True if the cells are valid with respect to the given
 * commitments
 *
 * @throws {Error} - Invalid input, failure to allocate memory, or errors
 * verifying batch
 */
Napi::Value VerifyCellKzgProofBatch(const Napi::CallbackInfo &info) {
    Napi::Env env = info.Env();
    Napi::Value result = env.Null();
    if (!(info[0].IsArray() && info[1].IsArray() && info[2].IsArray() &&
          info[3].IsArray() && info[4].IsArray())) {
        Napi::Error::New(
            env,
            "commitments, row_indices, column_indices, cells, and proofs must "
            "be arrays"
        )
            .ThrowAsJavaScriptException();
        return result;
    }
    Napi::Array commitments_param = info[0].As<Napi::Array>();
    Napi::Array row_indices_param = info[1].As<Napi::Array>();
    Napi::Array column_indices_param = info[2].As<Napi::Array>();
    Napi::Array cells_param = info[3].As<Napi::Array>();
    Napi::Array proofs_param = info[4].As<Napi::Array>();
    KZGSettings *kzg_settings = get_kzg_settings(env, info);
    if (kzg_settings == nullptr) {
        return env.Null();
    }

    C_KZG_RET ret;
    bool out;
    Bytes48 *commitments = NULL;
    uint64_t *row_indices = NULL;
    uint64_t *column_indices = NULL;
    Cell *cells = NULL;
    Bytes48 *proofs = NULL;

    size_t num_cells = cells_param.Length();
    size_t num_commitments = commitments_param.Length();

    if (row_indices_param.Length() != num_cells ||
        column_indices_param.Length() != num_cells ||
        proofs_param.Length() != num_cells) {
        Napi::Error::New(
            env,
            "Must have equal lengths for row_indices, column_indices, cells, "
            "and proofs"
        )
            .ThrowAsJavaScriptException();
        goto out;
    }

    commitments = (Bytes48 *)calloc(
        commitments_param.Length(), sizeof(Bytes48)
    );
    if (commitments == nullptr) {
        Napi::Error::New(env, "Error while allocating memory for commitments")
            .ThrowAsJavaScriptException();
        goto out;
    }
    row_indices = (uint64_t *)calloc(
        row_indices_param.Length(), sizeof(uint64_t)
    );
    if (row_indices == nullptr) {
        Napi::Error::New(env, "Error while allocating memory for row_indices")
            .ThrowAsJavaScriptException();
        goto out;
    }
    column_indices = (uint64_t *)calloc(
        column_indices_param.Length(), sizeof(uint64_t)
    );
    if (column_indices == nullptr) {
        Napi::Error::New(
            env, "Error while allocating memory for column_indices"
        )
            .ThrowAsJavaScriptException();
        goto out;
    }
    cells = (Cell *)calloc(cells_param.Length(), sizeof(Cell));
    if (cells == nullptr) {
        Napi::Error::New(env, "Error while allocating memory for cells")
            .ThrowAsJavaScriptException();
        goto out;
    }
    proofs = (Bytes48 *)calloc(proofs_param.Length(), sizeof(Bytes48));
    if (proofs == nullptr) {
        Napi::Error::New(env, "Error while allocating memory for proofs")
            .ThrowAsJavaScriptException();
        goto out;
    }

    for (size_t i = 0; i < num_commitments; i++) {
        // add HandleScope here to release reference to temp values
        // after each iteration since data is being memcpy
        Napi::HandleScope scope{env};
        Bytes48 *commitment = get_bytes48(
            env, commitments_param[i], "commitmentBytes"
        );
        if (commitment == nullptr) {
            goto out;
        }
        memcpy(&commitments[i], commitment, BYTES_PER_COMMITMENT);
    }

    for (size_t i = 0; i < num_cells; i++) {
        // add HandleScope here to release reference to temp values
        // after each iteration since data is being memcpy
        Napi::HandleScope scope{env};
        row_indices[i] = get_cell_id(env, row_indices_param[i]);
        column_indices[i] = get_cell_id(env, column_indices_param[i]);
        Cell *cell = get_cell(env, cells_param[i]);
        if (cell == nullptr) {
            goto out;
        }
        memcpy(&cells[i], cell, BYTES_PER_CELL);
        Bytes48 *proof = get_bytes48(env, proofs_param[i], "proofBytes");
        if (proof == nullptr) {
            goto out;
        }
        memcpy(&proofs[i], proof, BYTES_PER_PROOF);
    }

    ret = verify_cell_kzg_proof_batch(
        &out,
        commitments,
        num_commitments,
        row_indices,
        column_indices,
        cells,
        proofs,
        num_cells,
        kzg_settings
    );
    if (ret != C_KZG_OK) {
        std::ostringstream msg;
        msg << "Error in verifyCellKzgProofBatch: " << from_c_kzg_ret(ret);
        Napi::Error::New(env, msg.str()).ThrowAsJavaScriptException();
        goto out;
    }

    result = Napi::Boolean::New(env, out);

out:
    free(commitments);
    free(row_indices);
    free(column_indices);
    free(cells);
    free(proofs);

    return result;
}
