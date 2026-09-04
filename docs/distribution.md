# Distribution

Kyna publishes native `ky` archives, the `kyna` 1.x compatibility alias, per-user installers, a VS Code extension, and a Linux container from protected release tags.

## Per-user installation

Stable releases use:

```sh
curl -fsSL https://github.com/Up-to-code/Kyna/releases/latest/download/install.sh | sh
```

```powershell
irm https://github.com/Up-to-code/Kyna/releases/latest/download/install.ps1 | iex
```

Tagged releases also publish the matching verified npm installer under the official organization and its short alias:

```sh
npm i -g kyna
npm i -g @kyna-language/cli
```

The npm package version is locked to the GitHub release tag. Its postinstall script downloads the same native archive and `SHA256SUMS`, restricts redirects to GitHub release hosts, verifies the archive, and exposes both `ky` and `kyna`. It does not compile Kyna on the user's machine or require a repository clone.

```sh
npm i -g kyna@latest       # latest stable release
npm i -g kyna@preview      # latest prerelease
npm i -g kyna@1.2.3        # exact release
npm update -g kyna         # update the installed stable package
npm outdated -g kyna       # inspect available updates
```

The release action derives the npm version from the Git tag, downloads and executes the newly published GitHub binary as a smoke test, and then publishes that immutable npm version. A stable tag advances npm's `latest` distribution tag; a prerelease tag advances `preview`. Re-running a completed version repairs its distribution tag instead of attempting to overwrite the published package.

The Git tag is the release trigger and version source. Push a tag matching the version declared by the root CMake project, or manually run the release workflow while selecting that existing tag:

```sh
git tag v1.0.0
git push origin v1.0.0
```

No npm manifest version edit is required. The action rejects a tag whose base semantic version differs from the compiler project version.

Unix defaults to `~/.local`; Windows defaults to `%LOCALAPPDATA%\Kyna`. Both installers detect the OS and architecture, download the exact native archive over HTTPS, verify `SHA256SUMS` before extraction, install atomically, and retain the previous executable for rollback. Use `--prefix`, `--version`, `--channel stable|preview`, and non-interactive mode as needed. A preview requires an explicit version.

`ky self update` invokes the same verified installer contract. `ky self uninstall` removes only the Kyna executables under the resolved per-user prefix.

This is the current zero-clone installation path. Homebrew, Scoop, WinGet, and Linux repository manifests are distribution adapters to add after hosted-runner release validation is complete. They must install the same versioned archives and preserve the checksum/signature policy rather than introducing separately built binaries. A rustup-style toolchain manager is deferred until Kyna supports multiple compiler channels or versions concurrently; the present installer and `ky self` interface are the smaller, deeper solution.

## Release assets

- `kyna-darwin-arm64.tar.gz`
- `kyna-darwin-x86_64.tar.gz`
- `kyna-linux-arm64.tar.gz`
- `kyna-linux-x86_64.tar.gz`
- `kyna-windows-x86_64.zip`
- `kyna-language-support-<version>.vsix`
- `install.sh`, `install.ps1`, `SHA256SUMS`, the canonical `@kyna-language/cli` npm package, and its `kyna` short alias
- GitHub build-provenance attestations

Each CLI archive contains `bin/ky`, the `bin/kyna` alias, templates, the license, documentation, and runnable examples. Stable macOS binaries are Developer ID signed and notarized; stable Windows binaries are Authenticode signed. Missing signing credentials block a stable release. Unsigned tagged builds must be marked prerelease and installed with an explicit version.

## Container and Dev Container

Tagged releases publish `ghcr.io/up-to-code/kyna:<version>` for Linux amd64 and arm64. The image runs as a non-root user and exposes port 3000; the application must explicitly choose `0.0.0.0` when container ingress is intended. Native binaries remain the primary experience.

The repository’s `.devcontainer` definition installs `ky`, recommends the Kyna VS Code extension, enables format-on-save, and forwards port 3000.

## VS Code

`tools/package-vscode.sh` uses official `@vscode/vsce`. Source README artwork remains repository-relative for GitHub, while release packaging rewrites it to a release-tag-specific HTTPS base. Validation rejects relative packaged images, missing grammar/snippet/example/icon assets, and—during a tagged release—any image URL that does not return HTTP 200.

Install a downloaded package with:

```sh
code --install-extension kyna-language-support-<version>.vsix --force
```

Marketplace publication is enabled only when the repository has a publisher-scoped `VSCE_PAT` secret.

## Release and documentation cadence

A weekly maintenance review may refresh research records, implementation status, compatibility notes, examples, and benchmark baselines. It does not promise a weekly binary release. Documentation must describe the checked-in implementation and release policy; forward-looking work remains labeled as planned or deferred.
