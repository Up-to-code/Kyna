# Standard library

The Kyna runtime exposes standard-library functions through the global environment rather than special-casing them in the parser:

- `print(value, ...)`, `log(value, ...)`, `console.log(value, ...)`, `logColor(color, message)`, and `typeOf(value)`
- Structured backend logging: `slogInfo(message, fields?)`, `slogWarn`, `slogError`, and the `slog.info` / `slog.warn` / `slog.error` object (JSON lines). Prefer these over `print` in services.
- `len`, `push`, `pop`, and `keys` for arrays, strings, and closed objects
- Queue helpers: `createQueue()`, `enqueue(queue, value)`, `dequeue(queue)`, `peekQueue(queue)`, `queueIsEmpty(queue)`
- `textContains`, `textFind`, `textSlice`, `textReplace`, `textSplit`, `textTrim`, `textLower`, and `textUpper` for UTF-8 text; indices and `len(string)` count Unicode code points rather than bytes
- `readFile(path)`, `writeFile(path, content)`, `readJsonFile(path)`, and `writeJsonFile(path, value)`
- `copyFile(source, destination)` streams bytes in 64KiB chunks (does not load the whole file)
- `createDirectory(path)`, `fileExists(path)`, `removePath(path)`, and `listDirectory(path)`
- `fs.read`, `fs.write`, `fs.readJson`, `fs.writeJson`, `fs.createDirectory`, `fs.exists`, `fs.remove`, and `fs.list`
- `processRun(command)`, `build(command)`, `processEnv(name)`, `sleep(milliseconds)`, `wait(milliseconds)`, and `timeSleep(milliseconds)`
- `clockMs()` and `timeNow()` for monotonic millisecond and nanosecond timestamps, plus `measure(fn)` and `profileLog(label, value)` for timing code
- `os.name()`, `os.architecture()`, `os.cwd()`, `terminal.interactive()`, and `terminal.supportsColor()` through the injected host-information capability
- `httpGet(url)` for a raw response body and `fetch(url)` for a response object with `ok`, `status`, `url`, `text()`, and `json()`
- `parseIP(text)` returns a canonical IPv4 string or `null` when the text is not a valid address
- `jsonParse`, `jsonStringify`, `process.json`, `process.stringify`, `process.run`, and `process.env`
- `toml.parse`, `toml.stringify`, `xml.parse`, and `xml.stringify`, with canonical
  `tomlParse`, `tomlStringify`, `xmlParse`, and `xmlStringify` aliases
- `filter`, `map`, `reduce`, `find`, `any`, `all`, `unique`, `bubbleSort`, `sort`, and `call`
- `cryptoSha256(data)` returning the lowercase hex SHA-256 digest of a string
- `createApiStore(records)` for an in-memory CRUD store with `list`, `get`, `create`, `update`, and `remove`
- `db.query(connection, sql, parameters?)` and `db.execute(...)` for parameterized PostgreSQL operations
- `error(message)` for language errors caught by `try`/`catch`
- `collectGarbage()` and `gcStats()` for heap diagnostics

`sort` returns a new sorted copy and runs in O(n log n) using introsort with
median-of-three pivoting, an insertion-sort cutoff, and a heapsort fallback, so
worst-case inputs stay fast. `cryptoSha256` returns the standard lowercase-hex
SHA-256 digest and is deterministic. `timeNow()` returns monotonic nanoseconds;
`timeSleep(ms)` suspends the current thread like `sleep`. These builtins run in
both the tree-walk interpreter and the bytecode VM.

Filesystem, process, host information, networking, and sleeping operations call injected `RuntimeCapabilities`; deterministic embedders can replace every host adapter. The production network adapter uses linked libcurl for HTTP and HTTPS with certificate verification, HTTP/1.1, redirects, a Kyna user agent, a 10-second connect timeout, a 30-second overall timeout, and two bounded retries for transient transport failures. Errors identify DNS, connection, TLS, send, receive, timeout, or HTTP-response phases without exposing query parameters. Standard proxy environment variables are honored by libcurl. Streaming, async I/O, and cancellation remain future work. Process execution uses the host shell and is an explicitly trusted capability.

`processEnv(name)` and `process.env(name)` return `str?`: the value is a string when the variable exists and `null` when it does not. Text and JSON file edits use an explicit read-modify-write cycle; `writeFile`/`fs.write` and `writeJsonFile`/`fs.writeJson` replace the complete contents exposed by the adapter. See `examples/standard_library/environment_and_file_edit.kyna` for a cleanup-safe checkpoint.

TOML uses pinned toml++ and converts tables/arrays/scalars to ordinary Kyna values. XML uses pinned pugixml and represents each element as `{ name, attributes, text, children }`. Both adapters have stable `KFORMAT` errors and execute through the same value-conversion seam in the tree interpreter and bytecode VM. See `examples/learning/04_data/toml_and_xml.kyna` for in-memory round trips and `examples/learning/06_system/document_files.kyna` for cleanup-safe create/read/edit/delete work.

`fetch` and response `json()`/`text()` now execute through the bytecode native adapter when the response is used directly or stored in a local binding. Request options support `method`, `body`, positive millisecond `timeout`, and string-valued `headers`; malformed options and response JSON produce typed, catchable `KNET`/`K5100` errors.

See [networking.md](networking.md) for POST bodies, query parameters, Bearer authorization, API keys, cookies, HTTP status handling, security guidance, and the deterministic network-test strategy.

Database operations cross the injected `DatabasePort` seam. Official dependency builds use libpq, PostgreSQL `$1` parameter binding, SQL null mapping, and phase-specific `KDB2001` diagnostics. See [database.md](database.md) for the result model and repository-module pattern.

`examples/fake_api_store.kyna` uses the real `https://fakestoreapi.com/products` endpoints and writes the retrieved collection to `fake-store-output/products.json`. Fake Store API mutations are intentionally simulated by that service: responses demonstrate CRUD request/JSON behavior but do not permanently modify its database.

`examples/weather_api.kyna` uses the keyless Open-Meteo forecast endpoint to smoke-test HTTPS and JSON parsing, then writes the response to `weather-output/current.json`.
