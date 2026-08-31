#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ext="$root/editors/vscode-kyna"
version=$(sed -n 's/.*"version": "\([^"]*\)".*/\1/p' "$ext/package.json" | head -1)
test -n "$version"
out="$ext/kyna-language-support-$version.vsix"
rm -f "$ext"/kyna-language-support-*.vsix
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
mkdir -p "$tmp/extension"
cp -R "$ext"/* "$tmp/extension/"
rm -f "$tmp/extension/"*.vsix
mkdir -p "$tmp/extension/examples" "$tmp/extension/docs"
find "$root/examples" -type f -name '*.kyna' -print | while IFS= read -r file; do
  relative=${file#"$root/examples/"}
  destination="$tmp/extension/examples/$relative"
  mkdir -p "$(dirname "$destination")"
  cp "$file" "$destination"
done
cp "$root/docs/language-spec.md" "$root/docs/diagnostics.md" "$root/docs/stdlib.md" \
  "$root/docs/database.md" \
  "$tmp/extension/docs/"
cat > "$tmp/[Content_Types].xml" <<'XML'
<?xml version="1.0" encoding="utf-8"?><Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types"><Default Extension="json" ContentType="application/json"/><Default Extension="js" ContentType="application/javascript"/><Default Extension="xml" ContentType="application/xml"/><Default Extension="png" ContentType="image/png"/><Default Extension="svg" ContentType="image/svg+xml"/><Override PartName="/extension.vsixmanifest" ContentType="application/vsix-manifest"/></Types>
XML
cat > "$tmp/extension.vsixmanifest" <<XML
<?xml version="1.0" encoding="utf-8"?><PackageManifest xmlns="http://schemas.microsoft.com/developer/vsx-schema/2011" Version="2.0.0"><Metadata><Identity Id="kyna-language-support" Version="$version" Language="en-US" Publisher="kyna-lang" /><DisplayName>Kyna Language Support</DisplayName><Description xml:space="preserve">Kyna syntax, completions, live diagnostics, imports, comments, and run/check tools.</Description></Metadata><Installation AllUsers="false" Scope="CurrentUser" /><Dependencies /><Assets><Asset Type="Microsoft.VisualStudio.Code.Manifest" Path="extension/package.json" /><Asset Type="Microsoft.VisualStudio.Services.Content.Details" Path="extension/README.md" /></Assets></PackageManifest>
XML
rm -f "$out"
(cd "$tmp" && zip -qr "$out" .)
echo "$out"
