# Installation and security

Kyna release archives are built by GitHub Actions on clean Linux, Windows, and macOS runners. They contain the CLI and documentation; they are not signed installers or notarized macOS applications.

## Verify a download

Download the `SHA256SUMS` file and the archive for your platform from the [release page](https://github.com/Up-to-code/Kyna/releases). From the directory containing both files:

```sh
sha256sum --check SHA256SUMS --ignore-missing       # Linux
shasum -a 256 -c SHA256SUMS                         # macOS
```

On Windows PowerShell:

```powershell
Get-FileHash .\kyna-1.0.0-Windows-AMD64.zip -Algorithm SHA256
```

Compare the printed digest with the published `SHA256SUMS` entry before running the executable.

## macOS “Move to Bin” warning

The current macOS archive contains a command-line executable, not an iOS app. macOS Gatekeeper may warn that it cannot verify an app or developer when software was downloaded from the internet and is not signed and notarized. The warning is a security control, not evidence that Kyna is malware.

First verify the checksum above and make sure the archive came from the Kyna release page. Then extract it and run the CLI from Terminal:

```sh
unzip kyna-1.0.0-Darwin-arm64.zip
cd kyna-1.0.0-Darwin-arm64
./bin/ky --version
```

If Finder still blocks a verified download, Apple documents the temporary **System Settings → Privacy & Security → Open Anyway** approval flow. Do not bypass Gatekeeper for an archive whose checksum or source you cannot verify. Kyna will add signed and notarized macOS artifacts when an Apple Developer ID release process is available.

See Apple’s guidance on [opening apps from an unknown developer](https://support.apple.com/guide/mac-help/open-a-mac-app-from-an-unknown-developer-mh40616/mac) and [Gatekeeper runtime protection](https://support.apple.com/guide/security/gatekeeper-and-runtime-protection-sec5599b66df/web).

## Runtime capabilities

The CLI can access the filesystem, process environment, clock, and network only through injected runtime capabilities. Network examples use public keyless APIs and do not contain credentials. Treat Kyna scripts as executable programs: review source before running third-party scripts, limit filesystem/module paths, and use a test adapter for deterministic integration tests.
