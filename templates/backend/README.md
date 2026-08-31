# Kyna backend project

Start the API with `ky serve` or use `ky dev` while editing.

The layout follows the familiar Express separation of concerns:

```text
src/
├── main.kyna                 # process entry
├── app.kyna                  # application composition
├── middleware/request_logger.kyna
└── routes/
    ├── index.kyna            # central route registry
    └── health.kyna           # GET /health
```

The `[server]` section in `kyna.toml` is the single source of truth for the host and port used by `ky run`, `ky serve`, and `ky dev`.

Generate and register another GET route:

```sh
ky generate route users
ky generate route users --method post --path /api/users
```
