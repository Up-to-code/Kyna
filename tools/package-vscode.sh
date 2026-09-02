#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
extension="$root/editors/vscode-kyna"
version=$(sed -n 's/.*"version": "\([^"]*\)".*/\1/p' "$extension/package.json" | head -1)
test -n "$version"

release_ref=${KYNA_RELEASE_REF:-main}
image_base="https://raw.githubusercontent.com/Up-to-code/Kyna/$release_ref/editors/vscode-kyna"
content_base="https://github.com/Up-to-code/Kyna/blob/$release_ref/editors/vscode-kyna"
output="$extension/kyna-language-support-$version.vsix"
staging=$(mktemp -d)
trap 'rm -rf "$staging"' EXIT HUP INT TERM

mkdir -p "$staging/extension" "$staging/extension/examples" "$staging/extension/docs"
(cd "$extension" && tar --exclude='*.vsix' --exclude='node_modules' -cf - .) |
  (cd "$staging/extension" && tar -xf -)
(cd "$root/examples" && tar -cf - .) | (cd "$staging/extension/examples" && tar -xf -)
cp "$root/docs/language-spec.md" "$root/docs/diagnostics.md" "$root/docs/stdlib.md" \
  "$root/docs/database.md" "$root/docs/networking.md" "$staging/extension/docs/"

(cd "$staging/extension" && npx --yes @vscode/vsce@3.9.2 package \
  --baseImagesUrl "$image_base" \
  --baseContentUrl "$content_base" \
  --out "$output")

KYNA_VERIFY_REMOTE_IMAGES=${KYNA_VERIFY_REMOTE_IMAGES:-0} \
  python3 "$root/build_tools/verify_vscode_extension.py" "$output"
printf '%s\n' "$output"
