#!/usr/bin/env python3
"""Run real native adapters, checked wrappers, rejected conversions, and install/build smoke tests."""
import hashlib, os, pathlib, subprocess, sys, tempfile
ky, root, gui, geometry, ml = map(pathlib.Path, sys.argv[1:])
ky, root, gui, geometry, ml = (p.resolve() for p in (ky, root, gui, geometry, ml))
env = dict(os.environ, QT_QPA_PLATFORM="offscreen")
def run(args, success=True, cwd=root):
    r = subprocess.run([str(ky), *map(str,args)], cwd=cwd, env=env, capture_output=True, text=True, timeout=30)
    assert (r.returncode == 0) == success, (args, r.stdout, r.stderr)
    return r
assert "[9, 11]" in run(["--native-library",ml,"run",root/"examples/native/apps/regression.ky"]).stdout
assert "1" in run(["--native-library",geometry,"run",root/"examples/native/apps/hull.ky"]).stdout
mesh_output = run(["--native-library",geometry,"run",root/"examples/native/apps/mesh.ky"]).stdout
assert abs(float(mesh_output.splitlines()[-1]) - 1/6) < 1e-6
assert "GUI callback delivered" in run(["--native-library",gui,"run",root/"examples/native/apps/gui_smoke.ky"]).stdout
with tempfile.TemporaryDirectory(prefix="kyna-native-packages-") as folder:
    project=pathlib.Path(folder)
    def source(code):
        p=project/"main.ky";p.write_text(code);return p
    for library,code in [(ml,'mlTrain([[1.0, 2.0]], [1.0]);'),(ml,'var m = mlTrain([[1.0, 2.0]], [3.0, 5.0]); mlDispose(m); mlPredict(m, [[3.0]]);'),(geometry,'geometryOrientation([0.0], [1.0, 1.0], [2.0, 2.0]);')]:
        run(["--native-library",library,"run",source(code)],False)
    source('fn failure(): void { var bad: any = 1; bad(); } var timer=guiAfter(1,failure); guiRun();')
    run(["--native-library",gui,"run",project/"main.ky"],False)
    # Callback contracts must also hold when static typing is explicitly bypassed.
    for code, expected in [
        ('fn wrong(value: int): void {} var callback: any = wrong; guiAfter(1,callback);', 'KNATIVE1003'),
        ('fn wrong(): int { return 1; } var callback: any = wrong; guiAfter(1,callback); guiRun();', 'KNATIVE1004'),
    ]:
        for imported in (False, True):
            (project/'dep.ky').write_text('export var marker=1;')
            source(('import { marker } from "./dep.ky";' if imported else '') + code)
            failure=run(["--native-library",gui,"run",project/"main.ky"],False)
            assert expected in failure.stderr, failure.stderr
    # A retained child keeps its native parent alive after language roots disappear.
    source('fn make() { var window=guiWindow("Ownership",320,240); var layout=guiColumn(window); return guiInput(layout,"retained child"); } var child=make(); collectGarbage(); console.log(guiRead(child));')
    assert 'retained child' in run(["--native-library",gui,"run",project/"main.ky"]).stdout
    source('geometryOrientation([0.0,0.0], [1.0,1.0], [2.0,2.0]); console.log(geometryIntersection([0.0,0.0],[1.0,0.0],[2.0,0.0],[3.0,0.0]));')
    assert "[]" in run(["--native-library",geometry,"run",project/"main.ky"]).stdout
    # Wrappers retain checked contracts across ordinary imports.
    (project/"ml.ky").write_text((root/"examples/native/packages/ml.ky").read_text())
    run(["--native-library",ml,"check",source('import { train } from "./ml.ky"; train(false, []);')],False)
    source('import { train, predict } from "./ml.ky"; var m = train([[1.0, 2.0, 3.0]], [3.0, 5.0, 7.0]); console.log(predict(m, [[4.0]]));')
    run(["--native-library",ml,"build","main.ky","--output",project/"explicit-app"],cwd=project)
    launcher=project/"explicit-app"/("run.cmd" if sys.platform=="win32" else "run")
    explicit=subprocess.run([str(launcher)],env=env,capture_output=True,text=True,timeout=30)
    assert explicit.returncode==0 and "[9]" in explicit.stdout,(explicit.stdout,explicit.stderr)
    host = "darwin" if sys.platform == "darwin" else "windows" if sys.platform == "win32" else "linux"
    import platform
    target=host+("-arm64" if platform.machine().lower() in ("arm64","aarch64") else "-x86_64")
    checksum=hashlib.sha256(ml.read_bytes()).hexdigest()
    (project/"kyna.toml").write_text(f'[project]\nentry = "main.ky"\n[dependencies.ml.native]\nversion = "1.0.16"\ntarget = "{target}"\nabi = 2\npath = "{ml.as_posix()}"\nsha256 = "{checksum}"\n')
    run(["install"],cwd=project)
    run(["install","--locked"],cwd=project)
    assert "[9]" in run(["run","main.ky"],cwd=project).stdout
    run(["build","main.ky","--output",project/"dist"],cwd=project)
    launcher=project/"dist"/("run.cmd" if sys.platform=="win32" else "run")
    app=subprocess.run([str(launcher)],env=env,capture_output=True,text=True,timeout=30)
    assert app.returncode==0 and "[9]" in app.stdout,(app.stdout,app.stderr)
    failure=run(["--diagnostic-format","json","build","main.ky","--output",project/"dist"],False,cwd=project)
    import json
    diagnostic=json.loads(failure.stderr)
    assert diagnostic["schema"] == "kyna.diagnostic/v1", diagnostic
    assert "KBUILD1001" in failure.stderr and "Choose a fresh" in failure.stderr
    artifact=next((project/".kyna/native").glob("*.native"));artifact.write_bytes(b"tampered")
    run(["run","main.ky"],False,cwd=project)
    # Native archives retain source wrappers and verify every installed file.
    import tarfile, io
    archive=project/"ml.tar.gz"
    with tarfile.open(archive,"w:gz") as tar:
        tar.add(ml,arcname="libkyna_ml.so")
        data=(root/"examples/native/packages/ml.ky").read_bytes(); info=tarfile.TarInfo("ml.ky"); info.size=len(data);tar.addfile(info,io.BytesIO(data))
    sha=hashlib.sha256(archive.read_bytes()).hexdigest()
    (project/"kyna.toml").write_text(f'[project]\nentry = "main.ky"\n[dependencies.ml.native]\nversion = "1.0.16"\ntarget = "{target}"\nabi = 2\npath = "ml.tar.gz"\nsha256 = "{sha}"\narchive = true\nlibrary = "libkyna_ml.so"\n')
    (project/"ml.ky").unlink();source('import { train, predict } from "ml"; var model=train([[1.0,2.0,3.0]],[3.0,5.0,7.0]); console.log(predict(model,[[4.0]]));')
    run(["install"],cwd=project);run(["install","--locked"],cwd=project)
    assert "[9]" in run(["run","main.ky"],cwd=project).stdout
    run(["build","main.ky","--output",project/"archive-app"],cwd=project)
    launcher=project/"archive-app"/("run.cmd" if sys.platform=="win32" else "run")
    app=subprocess.run([str(launcher)],env=env,capture_output=True,text=True,timeout=30)
    assert app.returncode==0 and "[9]" in app.stdout,(app.stdout,app.stderr)
    wrapper=next((project/".kyna/native").glob("*/ml.ky"));wrapper.write_text("tampered")
    run(["run","main.ky"],False,cwd=project)
    # A verified archive is still rejected if it contains a link.
    with tarfile.open(archive,"w:gz") as tar:
        info=tarfile.TarInfo("unsafe");info.type=tarfile.SYMTYPE;info.linkname="/tmp";tar.addfile(info)
    text=(project/"kyna.toml").read_text().replace(sha,hashlib.sha256(archive.read_bytes()).hexdigest());(project/"kyna.toml").write_text(text)
    run(["install"],False,cwd=project)
    # Reject expanded size before writing an oversized member to staging.
    class Zeroes:
        def read(self, count): return bytes(count)
    with tarfile.open(archive,"w:gz",compresslevel=1) as tar:
        info=tarfile.TarInfo("oversized");info.size=513*1024*1024;tar.addfile(info,Zeroes())
    text=(project/"kyna.toml").read_text()
    import re
    text=re.sub(r'sha256 = "[a-f0-9]+"', 'sha256 = "'+hashlib.sha256(archive.read_bytes()).hexdigest()+'"', text)
    (project/"kyna.toml").write_text(text)
    rejected=run(["install"],False,cwd=project)
    assert "512 MiB" in rejected.stderr, rejected.stderr

print("Optional native package and application smoke tests passed")
