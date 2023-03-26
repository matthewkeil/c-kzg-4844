const path = require("path");
const { readFileSync } = require("fs");
const { globSync } = require("glob");
const yaml = require("js-yaml");
const {
  loadTrustedSetup,
  blobToKzgCommitment,
  computeKzgProof,
  computeBlobKzgProof,
  verifyKzgProof,
  verifyBlobKzgProof,
  verifyBlobKzgProofBatch,
} = require("../lib/kzg");

function bytesFromHex(hexString) {
  return Buffer.from(hexString.slice(2), "hex");
}

(async function () {
  const TEST_DIR = "../../tests";
  const GLOB_PATTERN = "/*/*/data.yaml";
  const BLOB_TO_KZG_COMMITMENT_TESTS =
    TEST_DIR + "/blob_to_kzg_commitment" + GLOB_PATTERN;
  const COMPUTE_KZG_PROOF_TESTS =
    TEST_DIR + "/compute_kzg_proof" + GLOB_PATTERN;
  const COMPUTE_BLOB_KZG_PROOF_TESTS =
    TEST_DIR + "/compute_blob_kzg_proof" + GLOB_PATTERN;
  const VERIFY_KZG_PROOF_TESTS = TEST_DIR + "/verify_kzg_proof" + GLOB_PATTERN;
  const VERIFY_BLOB_KZG_PROOF_TESTS =
    TEST_DIR + "/verify_blob_kzg_proof" + GLOB_PATTERN;
  const VERIFY_BLOB_KZG_PROOF_BATCH_TESTS =
    TEST_DIR + "/verify_blob_kzg_proof_batch" + GLOB_PATTERN;

  const SETUP_FILE_PATH = path.resolve(
    __dirname,
    "__fixtures__",
    "testing_trusted_setups.json",
  );
  loadTrustedSetup(SETUP_FILE_PATH);

  globSync(BLOB_TO_KZG_COMMITMENT_TESTS).forEach((testFile) => {
    const test = yaml.load(readFileSync(testFile, "ascii"));
    const blob = bytesFromHex(test.input.blob);
    try {
      blobToKzgCommitment(blob);
    } catch (err) {
      console.log(err);
    }
  });

  globSync(COMPUTE_KZG_PROOF_TESTS).forEach((testFile) => {
    const test = yaml.load(readFileSync(testFile, "ascii"));
    const blob = bytesFromHex(test.input.blob);
    const z = bytesFromHex(test.input.z);
    try {
      computeKzgProof(blob, z);
    } catch (err) {
      console.log(err);
    }
  });

  globSync(COMPUTE_BLOB_KZG_PROOF_TESTS).forEach((testFile) => {
    const test = yaml.load(readFileSync(testFile, "ascii"));
    const blob = bytesFromHex(test.input.blob);
    const commitment = bytesFromHex(test.input.commitment);
    try {
      computeBlobKzgProof(blob, commitment);
    } catch (err) {
      console.log(err);
    }
  });

  globSync(VERIFY_KZG_PROOF_TESTS).forEach((testFile) => {
    const test = yaml.load(readFileSync(testFile, "ascii"));
    const commitment = bytesFromHex(test.input.commitment);
    const z = bytesFromHex(test.input.z);
    const y = bytesFromHex(test.input.y);
    const proof = bytesFromHex(test.input.proof);
    try {
      verifyKzgProof(commitment, z, y, proof);
    } catch (err) {
      console.log(err);
    }
  });

  globSync(VERIFY_BLOB_KZG_PROOF_TESTS).forEach((testFile) => {
    const test = yaml.load(readFileSync(testFile, "ascii"));
    const blob = bytesFromHex(test.input.blob);
    const commitment = bytesFromHex(test.input.commitment);
    const proof = bytesFromHex(test.input.proof);
    try {
      verifyBlobKzgProof(blob, commitment, proof);
    } catch (err) {
      console.log(err);
    }
  });

  globSync(VERIFY_BLOB_KZG_PROOF_BATCH_TESTS).forEach((testFile) => {
    const test = yaml.load(readFileSync(testFile, "ascii"));
    const blobs = test.input.blobs.map(bytesFromHex);
    const commitments = test.input.commitments.map(bytesFromHex);
    const proofs = test.input.proofs.map(bytesFromHex);
    try {
      verifyBlobKzgProofBatch(blobs, commitments, proofs);
    } catch (err) {
      console.log(err);
    }
  });
})();
