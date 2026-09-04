# Installation and security

Users install Kyna from a release without cloning or building the repository. The shell and PowerShell installers select the exact operating-system and CPU archive, download it over HTTPS, verify its SHA-256 entry, and replace the per-user executable only after verification succeeds.

## One-command installation

Unix and macOS:

```sh
curl -fsSL https://github.com/Up-to-code/Kyna/releases/latest/download/install.sh | sh
```

Windows PowerShell:

```powershell
irm https://github.com/Up-to-code/Kyna/releases/latest/download/install.ps1 | iex
```

npm, when Node.js is already installed:

```sh
npm i -g kyna
```

The equivalent organization-qualified command is `npm i -g @kyna-language/cli`. The release workflow generates both package names from the same source and version.

The npm installer is version-locked to the corresponding GitHub tag and verifies the same native archive against `SHA256SUMS`. It does not contain or compile a separate implementation of the language.

Use `npm i -g kyna@latest` for an explicit stable installation, `npm i -g kyna@preview` for the latest prerelease, `npm i -g kyna@<version>` to pin a release, and `npm update -g kyna` to update an existing global installation. npm distribution tags select a published package version; the package then downloads only the matching GitHub tag.

Piping a remote script executes code from the network. A more inspectable installation downloads the script first, reviews it, and then runs it:

```sh
curl -fLO https://github.com/Up-to-code/Kyna/releases/latest/download/install.sh
less install.sh
sh install.sh
```

Use `--prefix`, `--version`, `--channel stable|preview`, and non-interactive mode as documented by `install.sh --help` or `install.ps1 -Help`. Preview installation requires an explicit version and never silently follows the stable `latest` URL.

## Release trust chain

Stable release policy requires all of the following:

- clean Linux, Windows, and macOS hosted-runner builds;
- SHA-256 entries for every archive and installer;
- GitHub build-provenance attestations for native archives;
- Developer ID signing and Apple notarization for macOS binaries;
- Authenticode signing with a trusted timestamp for Windows binaries;
- a release workflow failure when stable signing credentials are absent.

Tags containing a prerelease suffix are published as prereleases. They may be unsigned and therefore require an explicit version instead of the stable installer route. A workflow definition expresses policy; users should still inspect the actual release assets and attestations for the version they install.

## Verify a downloaded archive

Download `SHA256SUMS` and the archive from the [Kyna release page](https://github.com/Up-to-code/Kyna/releases). From the directory containing both files:

```sh
sha256sum --check SHA256SUMS --ignore-missing       # Linux
shasum -a 256 -c SHA256SUMS                         # macOS
```

On Windows PowerShell:

```powershell
Get-FileHash .\kyna-windows-x86_64.zip -Algorithm SHA256
```

Compare the digest with the archive's exact `SHA256SUMS` entry before execution. SHA-256 detects corruption or substitution relative to that checksum file; provenance attestations and platform signatures provide the stronger link to the release workflow and publisher identity.

## Update, rollback, and removal

`ky self update` invokes the same verified installer contract. Before replacing an existing executable, the installers retain a `.previous` copy under the selected prefix. Installation is scoped to `~/.local` by default on Unix and `%LOCALAPPDATA%\Kyna` on Windows; a custom prefix stays explicit.

`ky self uninstall` removes only files recorded under the resolved Kyna prefix. It does not remove projects or arbitrary user files.

## Platform warnings

A correctly published stable macOS binary is signed and notarized. A Gatekeeper warning on a stable asset should be investigated: confirm the release source, checksum, signature, notarization result, and provenance instead of bypassing the warning. Explicit prerelease builds may lack platform signing and can trigger security warnings.

Apple documents [opening software safely](https://support.apple.com/guide/mac-help/open-a-mac-app-from-an-unknown-developer-mh40616/mac) and [Gatekeeper runtime protection](https://support.apple.com/guide/security/gatekeeper-and-runtime-protection-sec5599b66df/web).

## Runtime capabilities

The CLI accesses the filesystem, process environment, clock, network, and database through injected host capabilities. The compiler core does not own those effects. Network examples use public keyless services and contain no credentials.

Treat Kyna scripts as executable programs: review third-party source, constrain module and filesystem paths, avoid exposing secrets through process environments, and use deterministic host adapters in integration tests. Checksums and code signing protect distribution; they do not make an untrusted Kyna program safe.
