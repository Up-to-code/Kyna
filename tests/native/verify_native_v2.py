#!/usr/bin/env python3
"""Exercise retained closures, GC, wrong-thread calls, tree/VM execution and stale resources."""
import pathlib,subprocess,sys,tempfile
ky,library=map(lambda p:str(pathlib.Path(p).resolve()),sys.argv[1:])
with tempfile.TemporaryDirectory() as directory:
    root=pathlib.Path(directory)
    code='fn closure(seed: int) { var n=seed; fn next(): int { n=n+1; return n; } return next; } var callback=closure(40); retainTestCallback(callback); collectGarbage(); console.log(fireTestCallback()); console.log(wrongThreadTest()); var r=createTestResource(); collectGarbage(); console.log(readTestResource(r));'
    for tree in (False,True):
        (root/'dep.ky').write_text('export var marker=1;')
        (root/'main.ky').write_text(('import { marker } from "./dep.ky";' if tree else '')+code)
        r=subprocess.run([ky,'--native-library',library,'run',str(root/'main.ky')],capture_output=True,text=True,timeout=15)
        assert r.returncode==0 and r.stdout=='41\ntrue\n42\n',(r.stdout,r.stderr)
    (root/'main.ky').write_text('var r=createTestResource(); disposeTestResource(r); readTestResource(r);')
    r=subprocess.run([ky,'--native-library',library,'run',str(root/'main.ky')],capture_output=True,text=True)
    assert r.returncode!=0 and 'KNATIVE1006' in r.stderr,r.stderr
print('ABI 2 callback/resource tests passed')
