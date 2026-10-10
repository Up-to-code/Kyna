#!/usr/bin/env python3
"""Exercise type contracts through the public CLI, imports, and warm caches."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    binary = str(Path(sys.argv[1]).resolve())
    def check(source, code=None, name=None):
        arguments = [binary, 'check', '-', '--no-color', '--diagnostic-format', 'json']
        if name: arguments += ['--source-name', str(name)]
        result = subprocess.run(arguments, input=source, text=True, capture_output=True, timeout=20)
        diagnostics = json.loads(result.stderr)['diagnostics'] if result.stderr else []
        if code:
            assert result.returncode == 1, (source, result.stderr)
            assert code in {d['code'] for d in diagnostics}, (source, diagnostics)
        else:
            assert result.returncode == 0, (source, result.stderr)
        return diagnostics

    cases = [
        ('fn f(x: int): int { return x; } var g = f; g("bad");', 'KSEM1202'),
        ('fn f(x: int) { return x; } var g = f; g();', 'KSEM1201'),
        ('fn f(x: int) { return x; } var result: str = f(1);', 'KSEM1505'),
        ('fn f(x: int) { return f(x); }', 'KSEM1503'),
        ('type A = B; type B = A;', 'KSEM1501'),
        ('type A = int; print(A);', 'KSEM1504'),
        ('loop { fn f(): void { break; } break; }', 'KSEM1301'),
        ('loop { fn f(): void { continue; } break; }', 'KSEM1301'),
        ('switch (1) { case 1: {} case 1: {} }', 'KSEM1306'),
        ('switch (1) { case 1: {} case 1.0: {} }', 'KSEM1306'),
        ('var x = 1; switch (1) { case x: {} }', 'KSEM1305'),
        ('var wrong: int = 1 / 2;', 'KSEM1505'),
        ('var wrong = 1 && true;', 'KSEM1601'),
        ('var wrong = !1;', 'KSEM1601'),
        ('var wrong = "a" < "b";', 'KSEM1602'),
        ('var wrong = 1.5 % 2;', 'KSEM1603'),
        ('console.missing("hello");', 'KSEM2404'),
    ]
    for source, code in cases: check(source, code)
    for source in [
        'type Count = int; fn f(x: Count) { return x + 1; } var g = f; var result: Count = g(1);',
        'fn f(x: int): int { return x; } var g: fn(int): int = f; var result: int = g(1);',
        'var result: float = 1 / 2;',
        'switch (1.0) { case 1.000001: {} case 1.000002: {} }',
    ]: check(source)

    with tempfile.TemporaryDirectory(prefix='kyna-contracts-') as directory:
        root = Path(directory)
        types = root / 't.ky'
        types.write_text('export type Count = int; export intf Person { name: str; } export intf Box<T> { value: T; } print("must not run");')
        library = root / 'library.ky'
        library.write_text('export fn add(value: int) { return value + 1; } export var count = 42;')
        entry = root / 'main.ky'
        source = 'import { add as plus, count } from "./library.ky"; var x: int = plus(count);'
        for _ in range(2): check(source, name=entry)
        for _ in range(2):
            check('import { add as plus } from "./library.ky"; plus("bad");', 'KSEM1202', entry)
            check('import * as lib from "./library.ky"; lib.add();', 'KSEM1201', entry)
            check('import { count } from "./library.ky"; var bad: str = count;', 'KSEM1505', entry)
            check('import { hidden } from "./library.ky";', 'K4004', entry)
        check('import type { Count } from "./t.ky"; var count: Count = 42;', name=entry)
        check('import type { Count as Total } from "./t.ky"; var count: Total = 42;', name=entry)
        check('import type { Person } from "./t.ky"; var person: Person = {name:"A"};', name=entry)
        check('import type { Box } from "./t.ky"; var box: Box<int> = {value:1};', name=entry)
        check('import type { Box } from "./t.ky"; var box: Box<int> = {value:"bad"};', 'KSEM1506', entry)
        check('import type { Box } from "./t.ky"; var box: Box = {value:1};', 'KSEM1502', entry)
        check('import * as types from "./t.ky"; print(types.Person);', 'KSEM1504', entry)
        check('var count = 1; export type { count };', 'KSEM1504', entry)
        (root / 'reexport.ky').write_text('import type { Person } from "./t.ky"; export type { Person };')
        check('import type { Person } from "./reexport.ky"; var person: Person = {name:"A"};', name=entry)
        check('import type * as types from "./t.ky"; var count: types.Count = 42;', name=entry)
        check('import type { Count } from "./t.ky"; print(Count);', 'KSEM1504', entry)
        entry.write_text('import type { Count } from "./t.ky"; var count: Count = 42; print(count);')
        result = subprocess.run([binary, 'run', str(entry), '--no-color'], capture_output=True, text=True, timeout=20)
        assert result.returncode == 0 and result.stdout == '42\n', result
        # Exact bytes, not file metadata alone, determine cache validity.
        original = library.stat()
        library.write_text('export fn add(value: str) { return value; } export var count = 42;')
        os.utime(library, ns=(original.st_atime_ns, original.st_mtime_ns))
        check(source, 'KSEM1202', entry)
    print('type contract verification passed')

if __name__ == '__main__': main()
