# @kyna-language/cli

The official npm installer for the native [Kyna programming language](https://github.com/Up-to-code/Kyna) CLI.

## Install

Kyna is currently available on the preview channel:

```sh
npm install --global @kyna-language/cli@preview
ky --version
```

The installation provides two equivalent commands:

```sh
ky --help
kyna --help
```

## Update or select a version

```sh
npm install --global @kyna-language/cli@preview         # install or update preview
npm install --global @kyna-language/cli@1.0.0-preview.3 # install an exact version
npm outdated --global @kyna-language/cli                # check for an update
```

The `latest` channel will become the recommended default after the first signed
stable release.

## Supported systems

| Platform | Architectures |
| --- | --- |
| macOS | x64, ARM64 |
| Linux | x64, ARM64 |
| Windows | x64 |

Node.js 18 or newer is required only for installation and command dispatch.
The compiler itself is a native executable.

## How installation works

The package version maps to the same GitHub release tag. During installation it:

1. detects the operating system and CPU architecture;
2. downloads the matching native archive and `SHA256SUMS` from the official release;
3. verifies the archive before extraction;
4. installs the native files inside the npm package;
5. exposes the `ky` and `kyna` command shims.

Packages are published from GitHub Actions through npm trusted publishing and
include build provenance. The release workflow tests every native archive before
publishing its immutable npm version.

## Links

- [Source and main README](https://github.com/Up-to-code/Kyna)
- [Releases](https://github.com/Up-to-code/Kyna/releases)
- [Installation documentation](https://github.com/Up-to-code/Kyna/blob/main/docs/distribution.md)
- [Report an issue](https://github.com/Up-to-code/Kyna/issues)

MIT © Ahmed Mansour
