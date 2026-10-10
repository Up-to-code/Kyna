#!/usr/bin/env python3
"""Verify native module and usage failures through the stable public diagnostic schema."""
import json
import subprocess
import sys
binary, library, bad_library = sys.argv[1:]
def run(source, *args):
    return subprocess.run([binary, 'run', '-', '--no-color', '--diagnostic-format', 'json', *args],
                          input=source, text=True, capture_output=True, timeout=20)
def failure(result, exit_status, code):
    assert result.returncode == exit_status, result
    payload = json.loads(result.stderr)
    assert payload['schema'] == 'kyna.diagnostic/v1', payload
    assert code in {entry['code'] for entry in payload['diagnostics']}, payload
result = run('log(nativeTwice(21));', '--native-library', library)
assert result.returncode == 0 and result.stdout == '42\n', result
failure(run('nativeTwice(false);', '--native-library', library), 1, 'KSEM1202')
failure(run('var f: any = nativeTwice; f(false);', '--native-library', library), 1, 'KNATIVE1003')
failure(run('', '--native-library', bad_library), 2, 'KNATIVE1001')
failure(run('', '--no-such-option'), 2, 'KCLI1001')
print('native CLI diagnostic verification passed')
