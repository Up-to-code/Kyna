# Database programming

Kyna exposes parameterized SQL through the `db` namespace. Database behavior crosses the injected `DatabasePort` seam: official builds provide the libpq PostgreSQL adapter, while tests can provide deterministic in-memory adapters without changing language code.

```kyna
const connection = process.env("KYNA_DATABASE_URL");
if (connection == null) {
    error("KYNA_DATABASE_URL is required");
}

try {
    const result = db.query(connection,
        "SELECT id, email, active FROM users WHERE email = $1",
        ["ada@example.test"]);
    console.log(result.rows, result.affectedRows, result.command);
} catch (message) {
    console.log("database operation failed", message);
}
```

`db.query(connection, sql, parameters?)` and `db.execute(...)` return an object containing `rows`, `affectedRows`, and `command`. Every row is a closed Kyna object. PostgreSQL null, boolean, integer, floating-point, and text values become their corresponding Kyna values. Decimal/numeric, JSON, UUID, date, time, and unknown PostgreSQL types remain strings so precision or representation is never changed silently; use `json.parse` for JSON columns.

Parameters accept `null`, booleans, numbers, strings, and characters. Values are sent through libpq parameter binding. Never construct SQL by concatenating untrusted input. The `K2610` lint warns when SQL text is dynamic, and database calls outside `try` produce `K2601`.

Database failures use `KDB2001` and include the operation phase, SQLSTATE/native code, retry classification, source span, and runtime call frame. Connection strings are not included in Kyna's diagnostic message. Applications should obtain credentials from environment or a secret provider rather than committing them to source.

The repository pattern in [`examples/backend/user_repository.kyna`](../examples/backend/user_repository.kyna) is the current ORM integration seam: keep constant SQL and row mapping inside a module, and expose domain functions to callers. Schema generation, migrations, connection pooling, transactions, and a first-party typed ORM remain release work.
