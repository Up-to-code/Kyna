# Kyna projects

Kyna commands search upward from the working directory for `kyna.toml`, so `ky run`, `ky check`, `ky fmt`, `ky dev`, and dependency commands work from project subdirectories.

```toml
[project]
name = "hello-api"
version = "0.1.0"
entry = "src/main.kyna"
template = "backend"

[server]
host = "127.0.0.1"
port = 3000

[scripts]
check = "ky check"
test = "ky check tests"

[dependencies]
shared = { git = "https://github.com/example/shared-kyna.git", rev = "<commit>" }
local_tools = { path = "../local-tools" }
```

`ky new` creates a new directory. `ky init` initializes an existing empty directory and refuses unrelated files; `--force` is limited to known project outputs rather than granting arbitrary overwrite permission. Git is initialized only when available and can be disabled with `--no-git`.

The `minimal` template contains a manifest, `src/main.kyna`, `.gitignore`, and README. The `backend` template adds a loopback HTTP service, test location, `.env.example`, VS Code recommendations/settings, and an Express-style application layout:

```text
src/
├── main.kyna
├── app.kyna
├── middleware/request_logger.kyna
└── routes/
    ├── index.kyna
    └── health.kyna
```

`main.kyna` imports the application factory, `app.kyna` composes the official injected `http` runtime with middleware and routes, and `routes/index.kyna` registers individual route modules. The `[server]` section of `kyna.toml` is authoritative for `ky run`, `ky serve`, and `ky dev`; backend source does not duplicate the port. Generate another route and wire it into the registry automatically:

Use `ky dev` for the save-and-restart workflow. `ky run dev` is accepted as a compatibility spelling. The watcher checks changed source before restarting and keeps the last good server running when a check fails.

```sh
ky generate route users
# `ky g route users` is the short form.
ky generate route users --method post --path /api/users
ky generate route user-detail --path /teams/:team/users/:user
```

Named segments are available in `request.params`; query-string values are available separately in `request.query`. Generated route modules carry a `# ky:route` identity header so editor tooling can identify the method, path, and exported handler without guessing from the filename.

`ky add` supports Git URLs pinned by `--rev` and local paths. `ky install` resolves exact Git commits into `kyna.lock`; `--locked` fails if the current manifest would produce different lock data. Git checkouts use the platform cache. Kyna never executes dependency-provided installation scripts.
