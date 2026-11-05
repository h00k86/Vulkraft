"""GraphicsApp source distribution format: explicit inputs, canonical metadata."""
import gzip
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import re
import tarfile

LIST = 'tools/graphics_app_sources.json'
MAX_ARCHIVE_BYTES = 128 * 1024 * 1024


def digest(data):
    return hashlib.sha256(data).hexdigest()


def read_version(root):
    values = re.findall(r'^set\(GRAPHICS_APP_VERSION ([0-9]+\.[0-9]+\.[0-9]+)\)$',
                        (Path(root) / 'cmake/GraphicsApp.cmake').read_text(), re.MULTILINE)
    if len(values) != 1:
        raise ValueError('Expected exactly one GRAPHICS_APP_VERSION')
    return values[0]


def source_path(name):
    if not isinstance(name, str) or not name or '\\' in name:
        raise ValueError('Invalid source path')
    path = PurePosixPath(name)
    if not path.parts or path.is_absolute() or '..' in path.parts or path.as_posix() != name:
        raise ValueError('Noncanonical source path: ' + name)
    if any(part.startswith('.') for part in path.parts):
        raise ValueError('Hidden source path: ' + name)
    allowed_roots = {'src', 'include', 'examples', 'tests', 'tools', 'docs', 'cmake', 'shaders', 'assets', 'backends'}
    if path.parts[0] not in allowed_roots and name not in {'CMakeLists.txt', 'Makefile', 'README.md', 'CHANGELOG.md'}:
        raise ValueError('Path outside distribution scope: ' + name)
    return path


def files(root):
    names = json.loads((Path(root) / LIST).read_text())
    if not isinstance(names, list) or not names or any(not isinstance(n, str) for n in names):
        raise ValueError('Source list must be a nonempty string array')
    if len(names) != len(set(names)) or LIST not in names:
        raise ValueError('Duplicate source or missing source-list file')
    for name in names:
        source_path(name)
    return sorted(names)


def package(root, output_dir, epoch):
    root = Path(root).resolve()
    if not isinstance(epoch, int) or not 0 <= epoch <= 0xffffffff:
        raise ValueError('SOURCE_DATE_EPOCH must fit an unsigned 32-bit timestamp')
    prefix = 'graphics-app-' + read_version(root)
    sources = {}
    for name in files(root):
        path = root / name
        if path.is_symlink() or not path.resolve().is_relative_to(root) or not path.is_file():
            raise ValueError('Source must be a regular file inside the checkout: ' + name)
        sources[name] = path.read_bytes()
    manifest = (json.dumps({n: digest(d) for n, d in sources.items()}, indent=2, sort_keys=True) + '\n').encode()
    members = dict(sources, **{'manifest-sha256.json': manifest})
    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    archive = output_dir / (prefix + '-source.tar.gz')
    with archive.open('wb') as raw:
        with gzip.GzipFile(filename='', fileobj=raw, mode='wb', compresslevel=9, mtime=epoch) as zipped:
            with tarfile.open(fileobj=zipped, mode='w', format=tarfile.USTAR_FORMAT) as tar:
                for name, data in sorted(members.items()):
                    info = tarfile.TarInfo(prefix + '/' + name)
                    info.size, info.mode, info.mtime = len(data), 0o644, epoch
                    info.uid = info.gid = 0
                    info.uname = info.gname = ''
                    tar.addfile(info, io.BytesIO(data))
    (output_dir / (prefix + '-manifest-sha256.json')).write_bytes(manifest)
    archive.with_suffix('.gz.sha256').write_text(digest(archive.read_bytes()) + '  ' + archive.name + '\n')
    return archive


def verify(archive):
    """Validate before extraction; no directories, links or unchecked paths."""
    if Path(archive).stat().st_size > MAX_ARCHIVE_BYTES:
        raise ValueError('Oversized source archive')
    raw = Path(archive).read_bytes()
    if len(raw) > MAX_ARCHIVE_BYTES or raw[:4] != b'\x1f\x8b\x08\x00':
        raise ValueError('Invalid or oversized canonical gzip archive')
    epoch = int.from_bytes(raw[4:8], 'little')
    sources, names, manifest = {}, [], None
    prefix = None
    total = 0
    with tarfile.open(fileobj=io.BytesIO(raw), mode='r:gz') as tar:
        for member in tar:
            total += member.size
            if len(names) >= 2048 or total > MAX_ARCHIVE_BYTES:
                raise ValueError('Archive exceeds source distribution bounds')
            if member.size < 0 or member.pax_headers or not member.isfile() or member.mode != 0o644 or member.uid or member.gid or member.uname or member.gname or member.mtime != epoch:
                raise ValueError('Noncanonical tar member metadata')
            top, separator, relative = member.name.partition('/')
            if not separator or not re.fullmatch(r'graphics-app-[0-9]+\.[0-9]+\.[0-9]+', top):
                raise ValueError('Invalid archive prefix')
            if prefix is not None and prefix != top:
                raise ValueError('Multiple archive prefixes')
            prefix = top
            if member.name in names:
                raise ValueError('Duplicate tar member')
            names.append(member.name)
            if relative != 'manifest-sha256.json':
                source_path(relative)
            data = tar.extractfile(member).read()
            if relative == 'manifest-sha256.json':
                manifest = data
            else:
                sources[relative] = data
    if names != sorted(names) or manifest is None:
        raise ValueError('Unsorted or missing archive manifest')
    expected = json.loads(manifest)
    actual = {name: digest(data) for name, data in sources.items()}
    if expected != actual:
        raise ValueError('Source content or membership differs from manifest')
    if manifest != (json.dumps(actual, indent=2, sort_keys=True) + '\n').encode():
        raise ValueError('Noncanonical manifest')
    listed = json.loads(sources.get(LIST, b'null'))
    if not isinstance(listed, list) or any(not isinstance(n, str) for n in listed) or sorted(listed) != sorted(sources):
        raise ValueError('Manifest differs from explicit source list')
    version = prefix.removeprefix('graphics-app-')
    return {'version': version, 'epoch': epoch, 'sha256': digest(raw),
            'manifest': manifest, 'sources': sources}


def extract(verified, destination):
    root = Path(destination) / ('graphics-app-' + verified['version'])
    root.mkdir(parents=True, exist_ok=False)
    for name, data in verified['sources'].items():
        target = root / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
    (root / 'manifest-sha256.json').write_bytes(verified['manifest'])
    return root
