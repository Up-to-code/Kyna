# Native package archives

Create a local adapter archive after building and testing the optional package:

```bash
python3 tools/native-packager/package.py --package ml --version 1.0.16 \
  --target darwin-arm64 --library build-native/native/libkyna_ml.so \
  --output build-native/packages
```

The archive contains the adapter, checked `.ky` wrapper, ABI/target/version
metadata, dependency inventory, and license notices. `SHA256SUMS` covers the
compressed archives. Packaging is deterministic for identical inputs. Use the
archive's SHA-256 in `[dependencies.ml.native]`, together with `archive = true`
and `library = "libkyna_ml.so"`; `ky install` verifies the archive and every
installed file before loading it.

By default, runtime dependencies remain external. `--runtime-files DIR` includes
regular files from an explicitly prepared staging directory. Relocate dynamic
library paths, include exact dependency notices, and verify on a clean target
before publishing. This command does not deploy Qt plugins, rewrite load paths,
sign applications, or establish platform support. Native package versioning is
independent of the core CLI release; npm's CLI adapter must continue to match its
GitHub release tag and consume the same core archives as the shell installers.
