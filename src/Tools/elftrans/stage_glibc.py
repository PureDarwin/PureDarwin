# stage_glibc.py - stage a glibc ELF and its libraries in a test image
# Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
import argparse
import json
import re
import shutil
import subprocess
from pathlib import Path


def command(*args):
    return subprocess.run(args, check=True, capture_output=True, text=True).stdout


def find_patchelf():
    found = shutil.which('patchelf')
    if found:
        return found
    matches = list(Path('/nix/store').glob('*-patchelf-*/bin/patchelf'))
    if matches:
        def version(path):
            match = re.search(r'-patchelf-([0-9.]+)', str(path))
            return tuple(map(int, match.group(1).split('.'))) if match else ()
        return str(max(matches, key=version))
    raise FileNotFoundError('patchelf is required, pass --patchelf')


def interpreter(path):
    match = re.search(r'Requesting program interpreter: ([^]]+)',
                      command('readelf', '-l', str(path)))
    if not match:
        raise ValueError(f'{path}: no ELF interpreter')
    loader = Path(match.group(1))
    if not loader.is_file():
        raise FileNotFoundError(loader)
    return loader


def dependencies(path):
    result = {}
    for line in command('ldd', str(path)).splitlines():
        match = re.match(r'\s*(\S+) => (/\S+) \(', line)
        if match:
            name, source = Path(match.group(1)).name, Path(match.group(2))
        else:
            match = re.match(r'\s*(/\S+) \(', line)
            if not match:
                continue
            source = Path(match.group(1))
            name = source.name
        if name.startswith('ld-linux-'):
            continue
        if name in result and result[name].resolve() != source.resolve():
            raise ValueError(f'conflicting {name}: {result[name]} and {source}')
        result[name] = source
    if not result or any(not source.is_file() for source in result.values()):
        raise ValueError(f'{path}: unresolved shared libraries')
    return result


def absolute_needed(path):
    return re.findall(r'Shared library: \[(/[^]]+)\]',
                      command('readelf', '-d', str(path)))


def prepare_raw(source, name, raw_dir, names, patchelf, guest_loader=None,
                copy_all=False):
    absolute = absolute_needed(source)
    if not absolute and not guest_loader and not copy_all:
        return source, False
    target = raw_dir / name
    shutil.copy2(source, target)
    target.chmod(target.stat().st_mode | 0o200)
    for needed in absolute:
        basename = Path(needed).name
        if basename not in names:
            raise ValueError(f'{source}: missing {needed}')
        command(patchelf, '--replace-needed', needed, basename, str(target))
    if guest_loader:
        command(patchelf, '--set-interpreter', guest_loader, str(target))
    if absolute_needed(target):
        raise ValueError(f'{target}: absolute DT_NEEDED remains')
    return target, bool(absolute or guest_loader)


def debugfs(root, action):
    return subprocess.run(['debugfs', '-w', '-R', action, str(root)],
                          check=True, capture_output=True, text=True).stdout


def data_batch(out, trees, extra_files):
    batch = out / 'data-debugfs-commands'
    paths = list(extra_files)
    directories = set()
    for source, guest_dir in trees:
        for file in source.rglob('*'):
            if not file.is_file():
                continue
            destination = guest_dir / file.relative_to(source)
            paths.append((file, destination))
            current = destination.parent
            while current != Path('/'):
                directories.add(current)
                current = current.parent
    with batch.open('w') as commands:
        for directory in sorted(directories, key=lambda p: (len(p.parts), str(p))):
            commands.write(f'mkdir {directory}\n')
        for source, destination in paths:
            commands.write(f'rm {destination}\n')
            commands.write(f'write {source} {destination}\n')
    return batch, paths


def install(root, disk, offset_mib, batch, translator, schemas, records,
            data_trees, data_files, out):
    directories = set()
    for record in records:
        current = Path(record['destination']).parent
        while current != Path('/'):
            directories.add(current)
            current = current.parent
    for directory in sorted(directories, key=lambda p: (len(p.parts), str(p))):
        debugfs(root, f'mkdir {directory}')
    subprocess.run(['debugfs', '-w', '-f', str(batch), str(root)], check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if translator:
        for action in ('rm /bin/e', f'write {translator} /bin/e',
                       'sif /bin/e mode 0100755'):
            debugfs(root, action)
    for source, data_dir in schemas:
        destination = data_dir / 'glib-2.0/schemas/gschemas.compiled'
        current = Path('/')
        for part in destination.parent.parts[1:]:
            current /= part
            debugfs(root, f'mkdir {current}')
        debugfs(root, f'rm {destination}')
        debugfs(root, f'write {source} {destination}')
    if data_trees or data_files:
        extra_batch, files = data_batch(out, data_trees, data_files)
        subprocess.run(['debugfs', '-w', '-f', str(extra_batch), str(root)],
                       check=True, stdout=subprocess.DEVNULL,
                       stderr=subprocess.DEVNULL)
    else:
        files = []
    for destination in (['/bin/e'] if translator else []) + [
                        *(r['destination'] for r in records),
                        *(str(d / 'glib-2.0/schemas/gschemas.compiled')
                          for _, d in schemas),
                        *(str(destination) for _, destination in files)]:
        if 'Inode:' not in debugfs(root, f'stat {destination}'):
            raise RuntimeError(f'{destination}: missing from staged image')
    with root.open('rb') as source, disk.open('r+b') as destination:
        destination.seek(offset_mib * 1024 * 1024)
        shutil.copyfileobj(source, destination, 1024 * 1024)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('app', type=Path)
    parser.add_argument('--out', required=True, type=Path)
    parser.add_argument('--elfaot', default='elfaot')
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--raw', dest='raw', action='store_true')
    mode.add_argument('--preconvert', dest='raw', action='store_false')
    parser.set_defaults(raw=True)
    parser.add_argument('--guest-app', default='/h')
    parser.add_argument('--guest-loader', default='/lib/ld-linux-x86-64.so.2')
    parser.add_argument('--patchelf')
    parser.add_argument('--root', type=Path)
    parser.add_argument('--disk', type=Path)
    parser.add_argument('--translator', type=Path)
    parser.add_argument('--offset-mib', type=int, default=61)
    parser.add_argument('--schema', action='append', default=[],
                        metavar='COMPILED:GUEST_DATA_DIR')
    parser.add_argument('--data-tree', action='append', default=[],
                        metavar='HOST_DIR:GUEST_DIR')
    parser.add_argument('--data-file', action='append', default=[],
                        metavar='HOST_FILE:GUEST_FILE')
    parser.add_argument('--module', action='append', default=[],
                        metavar='HOST_DSO:GUEST_DSO')
    args = parser.parse_args()
    if not args.patchelf:
        args.patchelf = find_patchelf()
    if bool(args.root) != bool(args.disk) or (args.translator and not args.root):
        parser.error('--root and --disk must be given together')

    app = args.app.resolve()
    if not args.guest_app.startswith('/') or not args.guest_loader.startswith('/'):
        parser.error('guest app and loader paths must be absolute')
    loader = interpreter(app)
    libraries = dependencies(app)
    libc = loader.parent / 'libc.so.6'
    if libraries.get('libc.so.6', libc).resolve() != libc.resolve():
        raise ValueError('libc does not match the ELF interpreter')
    libraries['libc.so.6'] = libc
    schemas = []
    for entry in args.schema:
        source, separator, guest_dir = entry.partition(':')
        if not separator or not Path(source).is_file() or not guest_dir.startswith('/'):
            parser.error(f'invalid --schema {entry}')
        schemas.append((Path(source), Path(guest_dir)))
    data_trees = []
    for entry in args.data_tree:
        source, separator, guest_dir = entry.partition(':')
        if not separator or not Path(source).is_dir() or not guest_dir.startswith('/'):
            parser.error(f'invalid --data-tree {entry}')
        data_trees.append((Path(source), Path(guest_dir)))
    data_files = []
    for entry in args.data_file:
        source, separator, guest_file = entry.partition(':')
        if not separator or not Path(source).is_file() or not guest_file.startswith('/'):
            parser.error(f'invalid --data-file {entry}')
        data_files.append((Path(source), Path(guest_file)))
    modules = []
    for entry in args.module:
        source, separator, guest_file = entry.partition(':')
        if not separator or not Path(source).is_file() or not guest_file.startswith('/'):
            parser.error(f'invalid --module {entry}')
        source = Path(source)
        modules.append((source, Path(guest_file)))
        for name, dependency in dependencies(source).items():
            if name in libraries and libraries[name].resolve() != dependency.resolve():
                raise ValueError(f'conflicting {name}: {libraries[name]} and {dependency}')
            libraries[name] = dependency

    args.out.mkdir(parents=True, exist_ok=True)
    raw = args.out / 'raw'
    converted = args.out / 'aot'
    raw.mkdir(exist_ok=True)
    converted.mkdir(exist_ok=True)
    entries = [('h', app, args.guest_app),
               ('l', loader, args.guest_loader if args.raw else '/l')]
    entries += [(name, source, '/lib/' + name)
                for name, source in sorted(libraries.items())]
    entries += [('module-' + str(index), source, str(destination))
                for index, (source, destination) in enumerate(modules)]
    names = set(libraries)
    records = []
    batch = args.out / 'debugfs-commands'
    with batch.open('w') as commands:
        for name, source, destination in entries:
            input_path, patched = prepare_raw(
                source, name, raw, names, args.patchelf,
                args.guest_loader if args.raw and name == 'h' else None,
                args.raw)
            if args.raw:
                target = input_path
                print(f'stage raw {name}: {source}', flush=True)
            else:
                target = converted / name
                target.unlink(missing_ok=True)
                print(f'convert {name}: {source}', flush=True)
                subprocess.run([args.elfaot, str(input_path), str(target)],
                               check=True)
            commands.write(f'rm {destination}\n')
            commands.write(f'write {target} {destination}\n')
            records.append({'name': name, 'source': str(source),
                            'staged': str(target), 'destination': destination,
                            'patched': patched})

    (args.out / 'manifest.json').write_text(
        json.dumps({'app': str(app), 'loader': str(loader),
                    'objects': records, 'raw': args.raw,
                    'guest_app': args.guest_app,
                    'guest_loader': args.guest_loader if args.raw else '/l',
                    'schemas': [{'source': str(s), 'guest_data_dir': str(d)}
                                for s, d in schemas],
                    'data_trees': [{'source': str(s), 'guest_dir': str(d)}
                                   for s, d in data_trees],
                    'data_files': [{'source': str(s), 'destination': str(d)}
                                   for s, d in data_files]}, indent=2) + '\n')
    if args.root:
        install(args.root, args.disk, args.offset_mib, batch,
                args.translator, schemas, records, data_trees, data_files,
                args.out)
    print(f'{len(entries)} {"raw" if args.raw else "AOT"} objects staged in {args.out}')
    if schemas:
        print('XDG_DATA_DIRS=' + ':'.join(
            str(data_dir) for _, data_dir in schemas))


if __name__ == '__main__':
    main()
