# @kyna-language/cli

This package installs the official native Kyna programming-language CLI from the matching GitHub release.

```sh
npm i -g kyna
ky --version
```

`kyna` is the short alias for the canonical organization package `@kyna-language/cli`. Both names are generated from and install the same release:

```sh
npm i -g @kyna-language/cli
```

Version and update commands:

```sh
npm i -g kyna@latest       # latest stable GitHub release
npm i -g kyna@preview      # latest prerelease
npm i -g kyna@1.2.3        # one exact release
npm update -g kyna         # update a global installation
npm outdated -g kyna       # check without updating
```

The postinstall script selects the exact supported platform archive, downloads `SHA256SUMS` and the archive over restricted HTTPS origins, verifies SHA-256, and installs the native files inside the npm package. Both `ky` and `kyna` command shims dispatch to the same native executable.

The npm package version and GitHub release tag must match. For example, `kyna@1.0.0` and `@kyna-language/cli@1.0.0` both install assets from `v1.0.0`. Supported targets are macOS arm64/x64, Linux arm64/x64, and Windows x64. The canonical package is owned by the official `@kyna-language` npm organization.

The GitHub release workflow derives the npm version from the pushed tag, installs and executes the matching GitHub artifact as a smoke test, and only then publishes. Stable tags update npm's `latest` channel; tags containing a prerelease suffix update `preview`. Re-running the same workflow synchronizes the channel without attempting to overwrite npm's immutable package version.

Source: [Up-to-code/Kyna](https://github.com/Up-to-code/Kyna). Report installation problems through the [issue tracker](https://github.com/Up-to-code/Kyna/issues).
