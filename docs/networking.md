# HTTP and API programming

Kyna provides `httpGet(url)` for a raw response body and `fetch(url, options?)` for structured HTTP and HTTPS requests. All network operations are fallible and should be protected with `try`/`catch`.

```kyna
try {
    const response = fetch("https://api.example.com/v1/items", {
        method: "POST",
        body: jsonStringify({ name: "Kyna" }),
        timeout: 10000,
        headers: {
            "Content-Type": "application/json",
            Accept: "application/json"
        }
    });
    const result = response.json();
    print(response.status, result);
} catch (failure) {
    print(failure.code, failure.message);
}
```

Request options are:

- `method`: HTTP method string; defaults to `GET` and is normalized to uppercase;
- `body`: request body string, commonly produced by `jsonStringify`;
- `timeout`: positive timeout in milliseconds;
- `headers`: object whose values must be strings. Quoted object keys support names such as `Content-Type`, `X-API-Key`, and `X-CSRF-Token`.

Responses expose `ok`, `status`, `url`, `method`, `headers`, `text()`, and `json()`. `ok` is true for status codes from 200 through 299. HTTP 4xx and 5xx statuses remain inspectable responses; transport failures throw a catchable `Error`.

## Authentication and cookies

Load secrets from an injected environment or another secure host capability. Never commit production credentials in a `.kyna` file.

```kyna
const token = processEnv("KYNA_API_TOKEN");
const response = fetch("https://api.example.com/v1/profile", {
    timeout: 10000,
    headers: { Authorization: "Bearer " + token }
});
```

API keys and cookies are ordinary headers:

```kyna
const response = fetch("https://api.example.com/v1/session", {
    headers: {
        "X-API-Key": processEnv("KYNA_API_KEY"),
        Cookie: "session=" + processEnv("KYNA_SESSION"),
        "X-CSRF-Token": processEnv("KYNA_CSRF_TOKEN")
    }
});
```

Authorization and cookie values must be redacted from diagnostics, logs, crash bundles, and test snapshots. Query values are removed from network failure endpoints.

## Failure model

- `KNET1001`: invalid `fetch` arguments;
- `KNET1002`: invalid method/body/timeout option;
- `KNET1003`: invalid header container or non-string header value;
- `KNET2001`: DNS, connection, TLS, send, receive, timeout, HTTP-transfer, or other transport failure;
- `KNET2100`: response-only operation used with a non-response value;
- `K5100`: response or input JSON is malformed.

Caught errors expose `code`, `message`, and `cause`. Uncaught errors additionally produce a source excerpt and runtime call frames.

## Examples and deterministic tests

The [`examples/network/`](../examples/network/) directory contains standalone programs for:

- deterministic request/response checkpoints;
- raw response text and JSON decoding;
- POST bodies and content-type headers;
- Bearer authorization and API-key headers;
- cookies and CSRF headers;
- query parameters;
- HTTP status handling;
- caught transport failures;
- public no-key API smoke tests.

Ordinary CI never requires a public service. The runtime test suite injects a recording network adapter and verifies method, body, timeout, authorization, API-key, cookie, and content-type transport. It also checks 401 responses, invalid response JSON, TLS failures, and timeouts. Public API examples are compiled in CI and can be run separately as opt-in smoke tests.

## Non-throwing requests

Use `http.tryFetch(url, options)` or its global alias `fetchResult(url, options)` when request failure is expected application state and an outer `try`/`catch` would add noise. It always returns an object with:

- `ok`: whether the request completed with a 2xx HTTP status;
- `response`: the normal response object when transport completed, otherwise `null`;
- `error`: a typed Kyna `Error` for validation, DNS, TLS, connection, or timeout failure, otherwise `null`.

HTTP responses such as 401 and 500 remain available in `response`; they do not become transport errors. Use ordinary `fetch` when the caller should catch or propagate failures. See `examples/network/fetch_result.kyna` for both paths.

## Awaiting a network result

`await expr` is a unary prefix operator that binds the result of a call before the program continues. It reads naturally around network processing such as `fetch(...).json()`, where the response must arrive and be decoded before the value is bound:

```kyna
try {
    const users = await fetch("https://api.example.com/v1/users").json();
    print(len(users), users[0].name);
} catch (failure) {
    print(failure.code, failure.message);
}
```

`await` currently passes synchronous results through, so `await fetch(...)` code compiles on both the tree-walk and bytecode runtimes without a separate async runtime. An event loop is tracked separately in `docs/implementation-status.md`. See `examples/language/await_and_network.kyna` for a deterministic `await` computation and `await fetch(...).json()` pair.
