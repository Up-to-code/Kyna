# Contributing to Kyna

## People

Kyna is maintained by [Ahmed Mansour](https://github.com/Up-to-code). [Cursor](https://cursor.com) is used as an AI pair programmer on this repository.

## Before you start

For language changes, read the [language specification](docs/language-spec.md) and [architecture guide](docs/architecture.md). For security issues, follow [SECURITY.md](SECURITY.md) instead of opening an issue.

## Local workflow

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DKYNA_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
git diff --check
```

Use `make format` when changing C++ and keep public interfaces in the narrowest domain layer that owns them. Add a focused characterization or regression test for behavior changes. Keep `.kyna` examples small and runnable with the checked-in CLI.

## Pull requests

Describe the user-visible behavior, the tests you ran, and any compatibility or diagnostic-code changes. Keep commits focused; do not commit build directories, generated archives, VSIX files, credentials, or API keys. Pull requests must pass the Linux, macOS, and Windows CI matrix before merge.

## Code review expectations

Reviewers look for stable source spans, deterministic diagnostics, capability boundaries, correct ownership/GC roots, and no reverse dependency from a lower architecture layer to a higher one. New warnings should include a stable code and a test for both the diagnostic and exit status.
