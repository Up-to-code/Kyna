# Standard library

The Kyna runtime exposes standard-library functions through the global environment rather than special-casing them in the parser:

- `print(value, ...)`, `log(value, ...)`, `console.log(value, ...)`, `logColor(color, message)`, and `typeOf(value)`
- `len`, `push`, `pop`, and `keys` for arrays, strings, and closed objects
- `textContains`, `textFind`, `textSlice`, `textReplace`, `textSplit`, `textTrim`, `textLower`, and `textUpper` for UTF-8 text; indices and `len(string)` count Unicode code points rather than bytes
- `readFile(path)`, `writeFile(path, content)`, `readJsonFile(path)`, and `writeJsonFile(path, value)`
- `createDirectory(path)`, `fileExists(path)`, `removePath(path)`, and `listDirectory(path)`
- `fs.read`, `fs.write`, `fs.readJson`, `fs.writeJson`, `fs.createDirectory`, `fs.exists`, `fs.remove`, and `fs.list`
- `processRun(command)`, `build(command)`, `processEnv(name)`, `sleep(milliseconds)`, and `wait(milliseconds)`
- `httpGet(url)` for a raw response body and `fetch(url)` for a response object with `ok`, `status`, `url`, `text()`, and `json()`
- `jsonParse`, `jsonStringify`, `process.json`, `process.stringify`, `process.run`, and `process.env`
- `filter`, `map`, `reduce`, `find`, `any`, `all`, `unique`, `bubbleSort`, `sort`, and `call`
- `createApiStore(records)` for an in-memory CRUD store with `list`, `get`, `create`, `update`, and `remove`
- `db.query(connection, sql, parameters?)` and `db.execute(...)` for parameterized PostgreSQL operations
- `error(message)` for language errors caught by `try`/`catch`
- `collectGarbage()` and `gcStats()` for heap diagnostics

Filesystem, process, networking, and sleeping operations call injected `RuntimeCapabilities`; deterministic embedders can replace every host adapter. The production network adapter uses linked libcurl for HTTP and HTTPS with certificate verification, HTTP/1.1, redirects, a Kyna user agent, a 10-second connect timeout, a 30-second overall timeout, and two bounded retries for transient transport failures. Errors identify DNS, connection, TLS, send, receive, timeout, or HTTP-response phases without exposing query parameters. Standard proxy environment variables are honored by libcurl. Streaming, async I/O, and cancellation remain future work. Process execution uses the host shell and is an explicitly trusted capability.

`fetch` and response `json()`/`text()` now execute through the bytecode native adapter when the response is used directly or stored in a local binding. Request options support `method`, `body`, positive millisecond `timeout`, and string-valued `headers`; malformed options and response JSON produce typed, catchable `KNET`/`K5100` errors.

Database operations cross the injected `DatabasePort` seam. Official dependency builds use libpq, PostgreSQL `$1` parameter binding, SQL null mapping, and phase-specific `KDB2001` diagnostics. See [database.md](database.md) for the result model and repository-module pattern.

`examples/fake_api_store.kyna` uses the real `https://fakestoreapi.com/products` endpoints and writes the retrieved collection to `fake-store-output/products.json`. Fake Store API mutations are intentionally simulated by that service: responses demonstrate CRUD request/JSON behavior but do not permanently modify its database.

`examples/weather_api.kyna` uses the keyless Open-Meteo forecast endpoint to smoke-test HTTPS and JSON parsing, then writes the response to `weather-output/current.json`.
