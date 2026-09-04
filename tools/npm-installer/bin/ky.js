#!/usr/bin/env node
// Dispatches npm-installed commands to the verified native Kyna executable.

"use strict";

const path = require("node:path");
const { spawnSync } = require("node:child_process");

const executable = path.join(
  __dirname,
  "..",
  "vendor",
  "bin",
  process.platform === "win32" ? "ky.exe" : "ky",
);
const result = spawnSync(executable, process.argv.slice(2), { stdio: "inherit" });

if (result.error) {
  console.error(`Kyna could not start its native executable: ${result.error.message}`);
  console.error("Reinstall with: npm i -g kyna");
  process.exit(1);
}

if (result.signal) {
  console.error(`Kyna terminated after receiving ${result.signal}.`);
  process.exit(1);
}

process.exit(result.status ?? 1);
