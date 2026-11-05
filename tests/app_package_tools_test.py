"""Distribution guards: tampering, unsafe members and reproducible source bytes."""
import gzip
import io
import json
from pathlib import Path
import sys
import tarfile
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from app_package import LIST, extract, package, source_path, verify


class AppPackageTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix='app-package-test-')
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.names = ['cmake/GraphicsApp.cmake', 'src/sample.c', LIST]
        for name, text in [('cmake/GraphicsApp.cmake', 'set(GRAPHICS_APP_VERSION 0.2.0)\n'),
                           ('src/sample.c', 'int sample(void) { return 42; }\n'),
                           (LIST, json.dumps(self.names))]:
            path = self.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(text)

    def archive(self):
        return package(self.root, self.root / 'output', 123)

    def rewrite(self, archive, change):
        with tarfile.open(archive) as reader:
            members = [(m, reader.extractfile(m).read()) for m in reader]
        members = change(members)
        with archive.open('wb') as raw:
            with gzip.GzipFile(fileobj=raw, filename='', mode='wb', mtime=123) as zipped:
                with tarfile.open(fileobj=zipped, mode='w', format=tarfile.USTAR_FORMAT) as writer:
                    for member, data in members:
                        member.size = len(data)
                        writer.addfile(member, io.BytesIO(data))

    def test_repack_ignores_permissions_and_mtimes(self):
        archive = self.archive()
        verified = verify(archive)
        source = extract(verified, self.root / 'extracted')
        for name in self.names:
            (source / name).chmod(0o700)
        rebuilt = package(source, self.root / 'rebuilt', 123)
        self.assertEqual(archive.read_bytes(), rebuilt.read_bytes())

    def test_new_unlisted_files_are_not_distributed(self):
        (self.root / 'src/private-token.txt').write_text('not-for-distribution')
        self.assertNotIn('src/private-token.txt', verify(self.archive())['sources'])

    def test_missing_and_symlink_sources_rejected(self):
        path = self.root / 'src/sample.c'
        path.unlink()
        with self.assertRaises(ValueError): self.archive()
        path.symlink_to(self.root / 'cmake/GraphicsApp.cmake')
        with self.assertRaises(ValueError): self.archive()

    def test_duplicate_source_list_rejected(self):
        (self.root / LIST).write_text(json.dumps(self.names + [self.names[0]]))
        with self.assertRaises(ValueError): self.archive()

    def test_unsafe_paths_rejected(self):
        for path in ('../outside', '/etc/passwd', '.git/config', 'build/key', '.', 'src/../key', 'src//key', 'src\\key'):
            with self.subTest(path=path), self.assertRaises(ValueError): source_path(path)

    def test_content_tampering_rejected(self):
        archive = self.archive()
        self.rewrite(archive, lambda members: [(m, b'changed' if m.name.endswith('sample.c') else data) for m, data in members])
        with self.assertRaisesRegex(ValueError, 'manifest'): verify(archive)

    def test_duplicate_tar_member_rejected(self):
        archive = self.archive()
        self.rewrite(archive, lambda members: members + [members[-1]])
        with self.assertRaisesRegex(ValueError, 'Duplicate'): verify(archive)

    def test_tar_path_escape_rejected_before_extraction(self):
        archive = self.archive()
        def change(members):
            members[-1][0].name = 'graphics-app-0.2.0/../../outside'
            return members
        self.rewrite(archive, change)
        with self.assertRaises(ValueError): verify(archive)
        self.assertFalse((self.root / 'outside').exists())

    def test_symlink_member_rejected(self):
        archive = self.archive()
        def change(members):
            members[-1][0].type = tarfile.SYMTYPE
            members[-1][0].linkname = '/etc/passwd'
            return members
        self.rewrite(archive, change)
        with self.assertRaisesRegex(ValueError, 'metadata'): verify(archive)

    def test_invalid_epoch_rejected(self):
        for epoch in (-1, 0x100000000):
            with self.assertRaises(ValueError): package(self.root, self.root / 'output', epoch)


if __name__ == '__main__':
    unittest.main()
