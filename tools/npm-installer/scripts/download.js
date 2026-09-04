// Downloads Kyna release metadata and archives over restricted HTTPS redirects.

"use strict";

const fs = require("node:fs");
const https = require("node:https");

const ALLOWED_HOSTS = new Set([
  "github.com",
  "objects.githubusercontent.com",
  "release-assets.githubusercontent.com",
]);

function validateUrl(value) {
  const url = new URL(value);
  if (url.protocol !== "https:" || !ALLOWED_HOSTS.has(url.hostname)) {
    throw new Error(`Refusing download from untrusted origin ${url.origin}.`);
  }
  return url;
}

function request(url, redirects, onResponse, reject) {
  const target = validateUrl(url);
  https
    .get(target, { headers: { "user-agent": "kyna npm installer" } }, (response) => {
      if (response.statusCode >= 300 && response.statusCode < 400 && response.headers.location) {
        response.resume();
        if (redirects === 0) {
          reject(new Error("Too many redirects while downloading Kyna."));
          return;
        }
        const redirect = new URL(response.headers.location, target).toString();
        request(redirect, redirects - 1, onResponse, reject);
        return;
      }
      if (response.statusCode !== 200) {
        response.resume();
        reject(new Error(`Kyna download failed with HTTP ${response.statusCode}.`));
        return;
      }
      onResponse(response);
    })
    .on("error", reject);
}

function downloadText(url) {
  return new Promise((resolve, reject) => {
    request(
      url,
      5,
      (response) => {
        const chunks = [];
        response.on("data", (chunk) => chunks.push(chunk));
        response.on("end", () => resolve(Buffer.concat(chunks).toString("utf8")));
        response.on("error", reject);
      },
      reject,
    );
  });
}

function downloadFile(url, destination) {
  return new Promise((resolve, reject) => {
    request(
      url,
      5,
      (response) => {
        const output = fs.createWriteStream(destination, { flags: "wx" });
        response.pipe(output);
        response.on("error", reject);
        output.on("error", reject);
        output.on("finish", () => output.close(resolve));
      },
      reject,
    );
  });
}

module.exports = { downloadFile, downloadText, validateUrl };
