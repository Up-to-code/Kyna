#!/bin/sh
set -eu

repository="Up-to-code/Kyna"
prefix="${KYNA_INSTALL_PREFIX:-$HOME/.local}"
version=""
channel="stable"

while [ "$#" -gt 0 ]; do
  case "$1" in
    --prefix) prefix=$2; shift 2 ;;
    --version) version=$2; shift 2 ;;
    --channel) channel=$2; shift 2 ;;
    --no-interactive) shift ;;
    -h|--help)
      printf '%s\n' "Usage: install.sh [--prefix PATH] [--version VERSION] [--channel stable|preview] [--no-interactive]"
      exit 0 ;;
    *) printf 'install.sh: unknown option: %s\n' "$1" >&2; exit 2 ;;
  esac
done

case "$channel" in stable|preview) ;; *) printf 'install.sh: invalid channel: %s\n' "$channel" >&2; exit 2 ;; esac
if [ "$channel" = preview ] && [ -z "$version" ]; then
  printf '%s\n' "install.sh: preview installs require --version and never use the stable latest URL" >&2
  exit 2
fi

case "$(uname -s)" in Darwin) platform=darwin ;; Linux) platform=linux ;; *) printf '%s\n' "install.sh: unsupported operating system" >&2; exit 2 ;; esac
case "$(uname -m)" in x86_64|amd64) architecture=x86_64 ;; arm64|aarch64) architecture=arm64 ;; *) printf '%s\n' "install.sh: unsupported architecture" >&2; exit 2 ;; esac

if [ -n "${KYNA_RELEASE_BASE_URL:-}" ]; then
  # Integration tests serve release assets from a disposable loopback server.
  # Production installs leave this unset and always use GitHub over HTTPS.
  base=${KYNA_RELEASE_BASE_URL%/}
elif [ -n "$version" ]; then
  tag=$version
  case "$tag" in v*) ;; *) tag="v$tag" ;; esac
  base="https://github.com/$repository/releases/download/$tag"
else
  base="https://github.com/$repository/releases/latest/download"
fi
asset="kyna-${platform}-${architecture}.tar.gz"
temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT HUP INT TERM

printf 'Downloading Kyna %s for %s-%s...\n' "${version:-stable}" "$platform" "$architecture" >&2
if [ -n "${KYNA_RELEASE_BASE_URL:-}" ]; then
  curl -fL --retry 3 "$base/$asset" -o "$temporary/$asset"
  curl -fL --retry 3 "$base/SHA256SUMS" -o "$temporary/SHA256SUMS"
else
  curl -fL --retry 3 --proto '=https' --tlsv1.2 "$base/$asset" -o "$temporary/$asset"
  curl -fL --retry 3 --proto '=https' --tlsv1.2 "$base/SHA256SUMS" -o "$temporary/SHA256SUMS"
fi
expected=$(awk -v name="$asset" '$2 == name || $2 == "*" name { print $1; exit }' "$temporary/SHA256SUMS")
test -n "$expected" || { printf '%s\n' "install.sh: archive is missing from SHA256SUMS" >&2; exit 2; }
if command -v sha256sum >/dev/null 2>&1; then actual=$(sha256sum "$temporary/$asset" | awk '{print $1}')
elif command -v shasum >/dev/null 2>&1; then actual=$(shasum -a 256 "$temporary/$asset" | awk '{print $1}')
else printf '%s\n' "install.sh: sha256sum or shasum is required" >&2; exit 2
fi
[ "$expected" = "$actual" ] || { printf '%s\n' "install.sh: checksum verification failed" >&2; exit 2; }

mkdir -p "$temporary/unpacked" "$prefix/bin" "$prefix/share/kyna"
tar -xzf "$temporary/$asset" -C "$temporary/unpacked"
ky_source=$(find "$temporary/unpacked" -type f -path '*/bin/ky' -print | head -1)
test -n "$ky_source" || { printf '%s\n' "install.sh: archive does not contain bin/ky" >&2; exit 2; }

if [ -f "$prefix/bin/ky" ]; then cp "$prefix/bin/ky" "$prefix/bin/ky.previous"; fi
if [ -f "$prefix/bin/kyna" ]; then cp "$prefix/bin/kyna" "$prefix/bin/kyna.previous"; fi
cp "$ky_source" "$prefix/bin/ky.new"
chmod 755 "$prefix/bin/ky.new"
mv "$prefix/bin/ky.new" "$prefix/bin/ky"
cp "$prefix/bin/ky" "$prefix/bin/kyna.new"
mv "$prefix/bin/kyna.new" "$prefix/bin/kyna"

share_source=$(dirname "$(dirname "$ky_source")")/share/kyna
if [ -d "$share_source" ]; then cp -R "$share_source"/. "$prefix/share/kyna/"; fi
{
  printf '%s\n' "bin/ky" "bin/kyna"
  [ -f "$prefix/bin/ky.previous" ] && printf '%s\n' "bin/ky.previous"
  [ -f "$prefix/bin/kyna.previous" ] && printf '%s\n' "bin/kyna.previous"
} > "$prefix/share/kyna/install-manifest.txt"

case ":$PATH:" in *":$prefix/bin:"*) ;; *)
  printf '\nKyna was installed, but %s/bin is not on PATH.\n' "$prefix" >&2
  printf 'Add this line to your shell profile: export PATH="%s/bin:$PATH"\n' "$prefix" >&2 ;;
esac
printf 'Installed ky and the kyna compatibility alias in %s/bin\n' "$prefix" >&2
