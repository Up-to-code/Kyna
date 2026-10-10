#!/usr/bin/env python3
"""Require checker import contracts and actual source spans to use unsaved module content."""
import json,pathlib,subprocess,sys,tempfile
ky=str(pathlib.Path(sys.argv[1]).resolve())
with tempfile.TemporaryDirectory() as directory:
    root=pathlib.Path(directory);main=root/'main.ky';dep=root/'dep.ky';overlay=root/'overlay.toml'
    main.write_text('import { value } from "./dep.ky"; var result: int = value;')
    dep.write_text('export var value = 42;')
    overlay.write_text('[[source]]\npath = '+json.dumps(str(dep))+'\ncontent = '+json.dumps('export var value = "unsaved";')+'\n')
    def run(extra):return subprocess.run([ky,'check',str(main),'--diagnostic-format','json',*extra],capture_output=True,text=True)
    assert run([]).returncode==0
    r=run(['--source-overlay',str(overlay)]);assert r.returncode==1,(r.stdout,r.stderr)
    diagnostics=json.loads(r.stderr)['diagnostics'];assert any('int' in d['message'] for d in diagnostics)
    assert run([]).returncode==0,'overlay contracts must not poison disk-source cache'
    dep.write_text('export var value = 42;')
    overlay.write_text('[[source]]\npath = '+json.dumps(str(dep))+'\ncontent = '+json.dumps('export var value: int = "wrong";')+'\n')
    r=run(['--source-overlay',str(overlay)]);assert r.returncode!=0
    assert any(pathlib.Path(d['file']).resolve()==dep.resolve() for d in json.loads(r.stderr)['diagnostics'])
print('Unsaved dependency overlay checks passed')
