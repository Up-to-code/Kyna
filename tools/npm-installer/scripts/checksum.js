// Parses release checksums and verifies downloaded archive contents.

"use strict";

const crypto = require("node:crypto");
const fs = require("node:fs");

function expectedChecksum(checksums, asset) {
  for (const line of checksums.split(/\r?\n/u)) {
    const match = line.match(/^([a-fA-F0-9]{64})\s+\*?(.+)$/u);
    if (match && match[2] === asset) {
      return match[1].toLowerCase();
    }
  }
  throw new Error(`${asset} is missing from SHA256SUMS.`);
}

function fileChecksum(file) {
  const hash = crypto.createHash("sha256");
  hash.update(fs.readFileSync(file));
  return hash.digest("hex");
}

function verifyChecksum(file, expected) {
  const actual = fileChecksum(file);
  if (actual !== expected) {
    throw new Error(`Checksum verification failed for ${file}.`);
  }
}

module.exports = { expectedChecksum, fileChecksum, verifyChecksum };
