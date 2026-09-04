// Extracts a verified native release archive with platform-owned tooling.

"use strict";

const { spawnSync } = require("node:child_process");

function extractArchive(archive, destination, platform) {
  let command;
  let args;
  if (platform === "win32") {
    command = "powershell.exe";
    args = [
      "-NoProfile",
      "-NonInteractive",
      "-Command",
      "Expand-Archive -LiteralPath $args[0] -DestinationPath $args[1] -Force",
      archive,
      destination,
    ];
  } else {
    command = "tar";
    args = ["-xzf", archive, "-C", destination];
  }

  const result = spawnSync(command, args, { encoding: "utf8" });
  if (result.error || result.status !== 0) {
    const detail = result.error?.message || result.stderr.trim() || `exit ${result.status}`;
    throw new Error(`Could not extract the Kyna archive: ${detail}`);
  }
}

module.exports = { extractArchive };
