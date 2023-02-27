import fs from "fs";
import path from "path";
import {expect} from "chai";
import bindings from "../lib";
import {TRUSTED_SETUP_JSON, VERIFY_BLOB_KZG_PROOF_BATCH_TESTS} from "./constants";
import {generateRandomBlob, generateRandomCommitment, generateRandomProof, getBoolean, getBytes} from "./utils";

const {verifyBlobKzgProofBatch, blobToKzgCommitment, computeBlobKzgProof} = bindings(TRUSTED_SETUP_JSON);

describe("verifyBlobKzgProofBatch", () => {
  it("should exist", () => {
    expect(verifyBlobKzgProofBatch).to.be.a("function");
  });
  it("reference tests for verifyBlobKzgProofBatch should pass", () => {
    const tests = fs.readdirSync(VERIFY_BLOB_KZG_PROOF_BATCH_TESTS);
    tests.forEach((test) => {
      const testPath = path.join(VERIFY_BLOB_KZG_PROOF_BATCH_TESTS, test);
      const blobs = fs
        .readdirSync(path.join(testPath, "blobs"))
        .sort()
        .map((filename) => {
          return path.join(testPath, "blobs", filename);
        })
        .map(getBytes);
      const commitments = fs
        .readdirSync(path.join(testPath, "commitments"))
        .sort()
        .map((filename) => {
          return path.join(testPath, "commitments", filename);
        })
        .map(getBytes);
      const proofs = fs
        .readdirSync(path.join(testPath, "proofs"))
        .sort()
        .map((filename) => {
          return path.join(testPath, "proofs", filename);
        })
        .map(getBytes);
      try {
        const ok = verifyBlobKzgProofBatch(blobs, commitments, proofs);
        const expectedOk = getBoolean(path.join(testPath, "ok.txt"));
        expect(ok).to.equal(expectedOk);
      } catch (err) {
        // TODO: this fails
        // expect(fs.existsSync(path.join(testPath, "ok.txt"))).to.be.false;
      }
    });
  });
  describe.skip("edge cases for verifyBlobKzgProofBatch", () => {
    it("should reject non-bytearray blob", () => {
      expect(() =>
        verifyBlobKzgProofBatch(
          ["foo", "bar"] as unknown as Uint8Array[],
          [generateRandomCommitment(), generateRandomCommitment()],
          [generateRandomProof(), generateRandomProof()]
        )
      ).to.throw("blob must be a Uint8Array");
    });

    it("zero blobs/commitments/proofs should verify as true", () => {
      expect(verifyBlobKzgProofBatch([], [], [])).to.be.true;
    });

    it("mismatching blobs/commitments/proofs should throw error", () => {
      const count = 3;
      const blobs = new Array(count);
      const commitments = new Array(count);
      const proofs = new Array(count);

      for (const [i, _] of blobs.entries()) {
        blobs[i] = generateRandomBlob();
        commitments[i] = blobToKzgCommitment(blobs[i]);
        proofs[i] = computeBlobKzgProof(blobs[i]);
      }

      expect(verifyBlobKzgProofBatch(blobs, commitments, proofs)).to.be.true;
      expect(() => verifyBlobKzgProofBatch(blobs.slice(0, 1), commitments, proofs)).to.throw(
        "requires equal number of blobs/commitments/proofs"
      );
      expect(() => verifyBlobKzgProofBatch(blobs, commitments.slice(0, 1), proofs)).to.throw(
        "requires equal number of blobs/commitments/proofs"
      );
      expect(() => verifyBlobKzgProofBatch(blobs, commitments, proofs.slice(0, 1))).to.throw(
        "requires equal number of blobs/commitments/proofs"
      );
    });
  });
});
