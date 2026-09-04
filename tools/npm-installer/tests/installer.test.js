// Verifies npm installer platform, checksum, and download-origin contracts.

"use strict";

const assert = require("node:assert/strict");
const crypto = require("node:crypto");
const fs = require("node:fs");
const os = require("node:os");
const path = require("node:path");
const test = require("node:test");

const { expectedChecksum, fileChecksum, verifyChecksum } = require("../scripts/checksum");
const { validateUrl } = require("../scripts/download");
const { releaseAsset } = require("../scripts/platform");

test("maps supported Node platforms to exact release assets", () => {
  assert.equal(releaseAsset("darwin", "arm64"), "kyna-darwin-arm64.tar.gz");
  assert.equal(releaseAsset("darwin", "x64"), "kyna-darwin-x86_64.tar.gz");
  assert.equal(releaseAsset("linux", "arm64"), "kyna-linux-arm64.tar.gz");
  assert.equal(releaseAsset("linux", "x64"), "kyna-linux-x86_64.tar.gz");
  assert.equal(releaseAsset("win32", "x64"), "kyna-windows-x86_64.zip");
  assert.throws(() => releaseAsset("win32", "arm64"), /does not publish/u);
});

test("selects only an exact checksum entry", () => {
  const digest = "a".repeat(64);
  const checksums = `${"b".repeat(64)}  other.tar.gz\n${digest} *kyna-linux-x86_64.tar.gz\n`;
  assert.equal(expectedChecksum(checksums, "kyna-linux-x86_64.tar.gz"), digest);
  assert.throws(() => expectedChecksum(checksums, "missing.tar.gz"), /missing/u);
});

test("hashes and rejects changed archive contents", () => {
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), "kyna-npm-test-"));
  const file = path.join(directory, "archive");
  try {
    fs.writeFileSync(file, "trusted archive");
    const digest = crypto.createHash("sha256").update("trusted archive").digest("hex");
    assert.equal(fileChecksum(file), digest);
    assert.doesNotThrow(() => verifyChecksum(file, digest));
    fs.writeFileSync(file, "changed archive");
    assert.throws(() => verifyChecksum(file, digest), /Checksum verification failed/u);
  } finally {
    fs.rmSync(directory, { force: true, recursive: true });
  }
});

test("accepts only the GitHub release download origins", () => {
  assert.equal(validateUrl("https://github.com/Up-to-code/Kyna").hostname, "github.com");
  assert.equal(
    validateUrl("https://release-assets.githubusercontent.com/file").hostname,
    "release-assets.githubusercontent.com",
  );
  assert.throws(() => validateUrl("http://github.com/Up-to-code/Kyna"), /untrusted/u);
  assert.throws(() => validateUrl("https://example.com/archive"), /untrusted/u);
});
