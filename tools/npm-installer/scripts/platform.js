// Maps Node platform identifiers to Kyna release artifact names.

"use strict";

function releaseAsset(platform, architecture) {
  const targets = {
    "darwin-arm64": "kyna-darwin-arm64.tar.gz",
    "darwin-x64": "kyna-darwin-x86_64.tar.gz",
    "linux-arm64": "kyna-linux-arm64.tar.gz",
    "linux-x64": "kyna-linux-x86_64.tar.gz",
    "win32-x64": "kyna-windows-x86_64.zip",
  };
  const key = `${platform}-${architecture}`;
  const asset = targets[key];
  if (!asset) {
    throw new Error(`Kyna does not publish a native archive for ${key}.`);
  }
  return asset;
}

module.exports = { releaseAsset };
