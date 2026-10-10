#!/usr/bin/env python3
"""Package built adapters and checked source wrappers with reproducible checksums.

Runtime dependencies must be supplied as already relocated regular files in a
staging directory. This tool does not claim platform support or resolve licenses.
"""
import argparse, gzip, hashlib, io, json, pathlib, re, tarfile

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package', choices=('gui', 'geometry', 'ml'), required=True)
    parser.add_argument('--version', required=True)
    parser.add_argument('--target', required=True)
    parser.add_argument('--library', type=pathlib.Path, required=True)
    parser.add_argument('--runtime-files', type=pathlib.Path)
    parser.add_argument('--output', type=pathlib.Path, required=True)
    args = parser.parse_args()
    for text in (args.version, args.target):
        if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9._-]*', text):
            parser.error('version and target must be safe release identifiers')
    root = pathlib.Path(__file__).resolve().parents[2]
    files = {args.library.name: args.library.read_bytes(),
             args.package + '.ky': (root / 'examples/native/packages' / (args.package + '.ky')).read_bytes()}
    files['licenses/Kyna-LICENSE.txt'] = (root / 'LICENSE').read_bytes()
    prefixes = {'gui': ('Qt-',), 'geometry': ('CGAL-', 'GMP-'),
                'ml': ('mlpack-', 'Armadillo-', 'cereal-', 'ensmallen-')}[args.package]
    for file in (root / 'docs/licenses/native').iterdir():
        if file.is_file() and file.name.startswith(prefixes):
            files['licenses/' + file.name] = file.read_bytes()
    if args.runtime_files:
        for file in sorted(args.runtime_files.rglob('*')):
            if file.is_symlink():
                parser.error('runtime staging must contain regular files, not links')
            if file.is_file():
                relative = file.relative_to(args.runtime_files).as_posix()
                if relative in files:
                    parser.error('runtime file collides with adapter package: ' + relative)
                files[relative] = file.read_bytes()
    metadata = dict(package=args.package, version=args.version, target=args.target,
                    abi=2, library=args.library.name,
                    runtime_dependencies='staged' if args.runtime_files else 'external; see dependency inventory')
    files['native-package.json'] = (json.dumps(metadata, indent=2) + '\n').encode()
    files['DEPENDENCIES.md'] = (root / 'docs/native-dependencies.md').read_bytes()
    args.output.mkdir(parents=True, exist_ok=True)
    archive = args.output / f'kyna-{args.package}-{args.version}-{args.target}.tar.gz'
    with archive.open('wb') as stream, gzip.GzipFile(filename='', fileobj=stream, mode='wb', mtime=0) as compressed:
        with tarfile.open(fileobj=compressed, mode='w', format=tarfile.USTAR_FORMAT) as tar:
            for name, data in sorted(files.items()):
                entry = tarfile.TarInfo(name)
                entry.size, entry.mode, entry.mtime = len(data), 0o644, 0
                tar.addfile(entry, io.BytesIO(data))
    checksum = hashlib.sha256(archive.read_bytes()).hexdigest()
    checksums = args.output / 'SHA256SUMS'
    entries = {}
    if checksums.exists():
        for line in checksums.read_text().splitlines():
            digest, name = line.split('  ', 1)
            entries[name] = digest
    entries[archive.name] = checksum
    checksums.write_text(''.join(f'{digest}  {name}\n' for name, digest in sorted(entries.items())))
    print(archive)

if __name__ == '__main__':
    main()
