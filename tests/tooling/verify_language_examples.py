#!/usr/bin/env python3
"""Check every Kyna example and verify deterministic examples end to end."""

from __future__ import annotations

import os
import pathlib
import re
import subprocess
import sys
import tempfile


ANSI = re.compile(r"\x1b\[[0-9;]*m")

EXPECTED_OUTPUT = {
    "examples/hello.kyna": "Hello Kyna\nadult\n",
    "examples/learning/01_basics/bindings.kyna": "bindings 10 15\n",
    "examples/learning/01_basics/operators_and_text.kyna": (
        "operators 42 true 2\ntext Hello Kyna 8 世界\n"
    ),
    "examples/learning/01_basics/variables_and_types.kyna": (
        "values Kyna 1 true true\ntypes str int bool null\n"
    ),
    "examples/learning/02_control_flow/conditionals.kyna": "conditional great\n",
    "examples/learning/02_control_flow/loops.kyna": "loops 10 24\n",
    "examples/learning/02_control_flow/matching.kyna": "match created\n",
    "examples/learning/03_functions/parameters_and_returns.kyna": (
        "functions Ada Lovelace 36\n"
    ),
    "examples/learning/03_functions/recursion_and_values.kyna": (
        "function values 120 42\n"
    ),
    "examples/learning/04_data/arrays_objects_json.kyna": (
        "array [4,1,2,5] 5 3\nobject Kyna backend [name, tags, version]\n"
    ),
    "examples/learning/04_data/collection_operations.kyna": (
        "collections [1,2,3,3] [3,1,2]\n"
    ),
    "examples/learning/04_data/toml_and_xml.kyna": "formats Kyna 8080 project Kyna\n",
    "examples/learning/05_errors_and_network/safe_fetch.kyna": (
        "request error KNET2001 true\n"
    ),
    "examples/learning/05_errors_and_network/typed_errors.kyna": (
        "caught KRT2300 lesson failure\ncleanup true\n"
    ),
    "examples/learning/06_system/environment.kyna": "environment lesson-value true\n",
    "examples/learning/06_system/files.kyna": "file published 2 true\n",
    "examples/learning/06_system/document_files.kyna": "document files Kyna 2 Kyna\n",
    "examples/classes.kyna": "woof Rex\n",
    "examples/match.kyna": "one\n",
    "examples/modules/main.kyna": "42\n",
    "examples/language/advanced_control_flow.kyna": "visits 12\nshortCircuit false true\n",
    "examples/language/await_and_network.kyna": "awaited int 42\nawaited fetch 3 Ada\n",
    "examples/language/bindings_and_nullability.kyna": (
        "bindings 42 2\nnullable true true\nequality true true\n"
    ),
    "examples/language/closures.kyna": (
        "addForty 42\nfirstCounter 1 2\nsecondCounter 11\nfactorial 120\nnestedAdder 42\n"
    ),
    "examples/language/collection_algorithms.kyna": (
        "mapped [2,4,4,6]\nreduced 8 first 2\nfiltered [2,2]\n"
        "distinct [1,2,3]\nordered [1,2,3]\npredicates true true\n"
    ),
    "examples/language/exceptions.kyna": (
        "recovered returned KVM2301:boom:boom:cleanup-first:cleanup-second:return-cleanup\n"
    ),
    "examples/language/first_class_functions.kyna": "sum 42\nproduct 42\n",
    "examples/language/interfaces_and_inheritance.kyna": (
        "interface Rex\noverride animal Rex says woof\n"
    ),
    "examples/language/json_and_objects.kyna": (
        "decoded 22 true\nproject Kyna 42\nheader application/json\n"
        "keys [\"name\",\"version\"]\nencoded {\"items\":[20,22],\"ready\":true}\n"
    ),
    "examples/language/recursive_functions.kyna": "factorial(6) 720\nfibonacci(10) 55\n",
    "examples/language/syntax_overview.kyna": (
        "bindings Kyna 42\nelse-if B\nswitch missing\nswitch other\n"
        "loop-broken 2\nawait 42\nlegacy let set 15\n"
    ),
    "examples/language/unicode_text.kyna": (
        "length 12\nslice Héllo\nfind 8\ncontains true\nreplace Héllo Kyna\n"
        "case äbc KYNA\nsplit 3 two\n"
    ),
    "examples/network/caught_transport_error.kyna": "caught network KNET2001 true\n",
    "examples/network/checkpoints.kyna": "checkpoints 200 3 Ada\n",
    "examples/network/response_text.kyna": "text response 200 114 Grace\n",
    "examples/network/fetch_result.kyna": (
        "safe fetch 200 true\nsafe failure KNET2001 true\n"
    ),
    "examples/standard_library/api_store.kyna": "store first updated true 1\n",
    "examples/standard_library/core_values.kyna": (
        "print int 4\nlog 3\ncall 42\ncaught KRT2300 expected\n"
    ),
    "examples/standard_library/environment_and_file_edit.kyna": (
        "environment checkpoint-value true\nedited published by Kyna 2 2\n"
    ),
    "examples/standard_library/filesystem.kyna": "files Hello filesystem Kyna true 2\n",
    "examples/standard_library/memory_and_color.kyna": "colored output\nmemory str\n",
    "examples/standard_library/mutable_collections.kyna": (
        "mutation [3,1,3] 2\nkeys [\"name\",\"version\"]\n"
        "unique [3,1]\nsort [1,3,3] [1,3,3]\n"
    ),
    "examples/standard_library/process_and_time.kyna": "process str\n",
}

RUN_IN_TEMPORARY_DIRECTORY = {
    "examples/learning/06_system/files.kyna",
    "examples/learning/06_system/document_files.kyna",
    "examples/standard_library/environment_and_file_edit.kyna",
    "examples/standard_library/filesystem.kyna",
}

EXAMPLE_ENVIRONMENTS = {
    "examples/learning/06_system/environment.kyna": {
        "KYNA_LEARNING_VALUE": "lesson-value",
        "KYNA_LEARNING_MISSING_91F4": None,
    },
    "examples/standard_library/environment_and_file_edit.kyna": {
        "KYNA_TEST_ENV": "checkpoint-value",
        "KYNA_TEST_MISSING_7D3A91": None,
    },
}

BUILTIN_COVERAGE = {
    "print": "examples/standard_library/core_values.kyna",
    "log": "examples/standard_library/core_values.kyna",
    "typeOf": "examples/standard_library/core_values.kyna",
    "len": "examples/standard_library/core_values.kyna",
    "error": "examples/standard_library/core_values.kyna",
    "call": "examples/standard_library/core_values.kyna",
    "push": "examples/standard_library/mutable_collections.kyna",
    "pop": "examples/standard_library/mutable_collections.kyna",
    "keys": "examples/standard_library/mutable_collections.kyna",
    "unique": "examples/standard_library/mutable_collections.kyna",
    "sort": "examples/standard_library/mutable_collections.kyna",
    "bubbleSort": "examples/standard_library/mutable_collections.kyna",
    "filter": "examples/language/collection_algorithms.kyna",
    "map": "examples/language/collection_algorithms.kyna",
    "reduce": "examples/language/collection_algorithms.kyna",
    "find": "examples/language/collection_algorithms.kyna",
    "any": "examples/language/collection_algorithms.kyna",
    "all": "examples/language/collection_algorithms.kyna",
    "readFile": "examples/standard_library/filesystem.kyna",
    "writeFile": "examples/standard_library/filesystem.kyna",
    "readJsonFile": "examples/standard_library/filesystem.kyna",
    "writeJsonFile": "examples/standard_library/filesystem.kyna",
    "createDirectory": "examples/standard_library/filesystem.kyna",
    "fileExists": "examples/standard_library/filesystem.kyna",
    "removePath": "examples/standard_library/filesystem.kyna",
    "listDirectory": "examples/standard_library/filesystem.kyna",
    "processRun": "examples/standard_library/process_and_time.kyna",
    "build": "examples/standard_library/process_and_time.kyna",
    "processEnv": "examples/standard_library/process_and_time.kyna",
    "osName": "examples/learning/06_system/os_and_terminal.kyna",
    "osArchitecture": "examples/learning/06_system/os_and_terminal.kyna",
    "osWorkingDirectory": "examples/learning/06_system/os_and_terminal.kyna",
    "terminalIsInteractive": "examples/learning/06_system/os_and_terminal.kyna",
    "terminalSupportsColor": "examples/learning/06_system/os_and_terminal.kyna",
    "sleep": "examples/standard_library/process_and_time.kyna",
    "wait": "examples/standard_library/process_and_time.kyna",
    "httpGet": "examples/network/http_get.kyna",
    "fetch": "examples/weather_api.kyna",
    "fetchResult": "examples/network/fetch_result.kyna",
    "jsonParse": "examples/language/json_and_objects.kyna",
    "jsonStringify": "examples/language/json_and_objects.kyna",
    "tomlParse": "examples/learning/04_data/toml_and_xml.kyna",
    "tomlStringify": "examples/learning/04_data/toml_and_xml.kyna",
    "xmlParse": "examples/learning/04_data/toml_and_xml.kyna",
    "xmlStringify": "examples/learning/04_data/toml_and_xml.kyna",
    "textContains": "examples/language/unicode_text.kyna",
    "textFind": "examples/language/unicode_text.kyna",
    "textSlice": "examples/language/unicode_text.kyna",
    "textReplace": "examples/language/unicode_text.kyna",
    "textSplit": "examples/language/unicode_text.kyna",
    "textTrim": "examples/language/unicode_text.kyna",
    "textLower": "examples/language/unicode_text.kyna",
    "textUpper": "examples/language/unicode_text.kyna",
    "collectGarbage": "examples/standard_library/memory_and_color.kyna",
    "gcStats": "examples/standard_library/memory_and_color.kyna",
    "logColor": "examples/standard_library/memory_and_color.kyna",
    "createApiStore": "examples/standard_library/api_store.kyna",
    "clockMs": "examples/standard_library/timing.kyna",
    "timeNow": "examples/standard_library/timing.kyna",
    "timeSleep": "examples/standard_library/timing.kyna",
    "cryptoSha256": "examples/standard_library/crypto.kyna",
    "profileLog": "examples/standard_library/timing.kyna",
    "measure": "examples/standard_library/timing.kyna",
}

# These examples must compile through every production compiler representation.
# Modules, database adapters, and callback-driven collection algorithms still have
# explicitly tracked bytecode migration work and continue through the compatibility engine.
BYTECODE_EXAMPLES = {
    relative
    for relative in EXPECTED_OUTPUT
    if relative
    not in {
        "examples/modules/main.kyna",
        "examples/language/collection_algorithms.kyna",
        "examples/standard_library/api_store.kyna",
        "examples/standard_library/core_values.kyna",
        "examples/standard_library/memory_and_color.kyna",
    }
}


def invoke(
    binary: pathlib.Path,
    root: pathlib.Path,
    command: str,
    source: pathlib.Path,
    working_directory: pathlib.Path | None = None,
    environment: dict[str, str | None] | None = None,
):
    process_environment = os.environ.copy()
    for name, value in (environment or {}).items():
        if value is None:
            process_environment.pop(name, None)
        else:
            process_environment[name] = value
    return subprocess.run(
        [str(binary), command, str(source), "--no-color"],
        cwd=working_directory or root,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        env=process_environment,
        check=False,
    )


def main() -> int:
    binary = pathlib.Path(sys.argv[1]).resolve()
    root = pathlib.Path(sys.argv[2]).resolve()
    failures: list[str] = []
    examples = sorted((root / "examples").rglob("*.kyna"))

    symbol_source = (
        root / "compiler/kyna_symbols/src/catalog/standard_library_symbols.cpp"
    ).read_text()
    callable_symbols = set(
        re.findall(r'StandardLibrarySymbol\{"([A-Za-z][A-Za-z0-9]*)", true,', symbol_source)
    )
    if callable_symbols != set(BUILTIN_COVERAGE):
        missing = sorted(callable_symbols - set(BUILTIN_COVERAGE))
        stale = sorted(set(BUILTIN_COVERAGE) - callable_symbols)
        failures.append(f"builtin coverage mismatch: missing={missing}, stale={stale}")
    for builtin, relative in BUILTIN_COVERAGE.items():
        source = root / relative
        if not source.is_file():
            failures.append(f"{builtin}: missing coverage example {relative}")
            continue
        if not re.search(rf"\b{re.escape(builtin)}\s*\(", source.read_text()):
            failures.append(f"{builtin}: {relative} does not call the builtin")

    for source in examples:
        relative = source.relative_to(root).as_posix()
        for command in ("check", "tokens", "ast"):
            result = invoke(binary, root, command, source)
            if result.returncode != 0:
                failures.append(
                    f"{relative}: {command} exited {result.returncode}: {result.stderr.strip()}"
                )

    for relative in sorted(BYTECODE_EXAMPLES):
        for command in ("hir", "mir", "bytecode"):
            result = invoke(binary, root, command, root / relative)
            if result.returncode != 0:
                failures.append(
                    f"{relative}: {command} exited {result.returncode}: {result.stderr.strip()}"
                )

    for relative, expected in EXPECTED_OUTPUT.items():
        if relative in RUN_IN_TEMPORARY_DIRECTORY:
            with tempfile.TemporaryDirectory(prefix="kyna-example-") as directory:
                executed = invoke(
                    binary,
                    root,
                    "run",
                    root / relative,
                    pathlib.Path(directory),
                    EXAMPLE_ENVIRONMENTS.get(relative),
                )
        else:
            executed = invoke(
                binary, root, "run", root / relative,
                environment=EXAMPLE_ENVIRONMENTS.get(relative)
            )
        actual = ANSI.sub("", executed.stdout)
        if executed.returncode != 0:
            failures.append(f"{relative}: run exited {executed.returncode}: {executed.stderr.strip()}")
        elif actual != expected:
            failures.append(f"{relative}: output mismatch\nexpected={expected!r}\nactual={actual!r}")

    missing = sorted(set(EXPECTED_OUTPUT) - {path.relative_to(root).as_posix() for path in examples})
    failures.extend(f"missing deterministic example: {relative}" for relative in missing)

    if failures:
        print("Kyna language example verification failed:", file=sys.stderr)
        for failure in failures:
            print(f"  {failure}", file=sys.stderr)
        return 1

    print(
        f"checked {len(examples)} examples and {len(BUILTIN_COVERAGE)} builtins; "
        f"compiled {len(BYTECODE_EXAMPLES)} through bytecode; "
        f"ran {len(EXPECTED_OUTPUT)} deterministic examples"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
