#!/usr/bin/env python3
"""Release provenance guards: fail closed on partial, stale or mismatched proof."""
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'tools'))
import package_ui
from finalize_ui_release import REQUIRED_STAGES, finalize
from ui_version import read_version


class ReleaseProofTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='ui-release-test-')
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        # Synthetic fixture: no test claims to build/validate these dummy files.
        for name in package_ui.files:
            target = self.root/name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes((ROOT/name).read_bytes() if name == 'include/ui/version.h' else name.encode())
        with patch.object(package_ui, 'root', self.root):
            self.archive = package_ui.package(self.root/'build', 0)
        self.version = read_version(self.root)
        self.validation = self.root/'validation'
        (self.validation/'sdk').mkdir(parents=True)
        self.report = {'version': self.version, 'status': 'PASS', 'native_requested': False,
                       'stages': [{'name': name, 'status': 'PASS', 'exit_code': 0} for name in sorted(REQUIRED_STAGES)]}
        manifest = self.root/f'build/ui-framework-{self.version}-manifest-sha256.json'
        self.sdk = {'version': self.version, 'result': 'PASS', 'reproducible': True,
                    'header_runtime_cmake_agree': True, 'different_versions_rejected': True,
                    'archive_sha256': hashlib.sha256(self.archive.read_bytes()).hexdigest(),
                    'manifest_sha256': hashlib.sha256(manifest.read_bytes()).hexdigest(),
                    'files': len(package_ui.files)}
        self.persist()

    def persist(self):
        (self.validation/'validation.json').write_text(json.dumps(self.report))
        (self.validation/'sdk/package-verification.json').write_text(json.dumps(self.sdk))

    def test_success_binds_archive_and_manifest(self):
        output = json.loads(finalize(self.root, self.validation).read_text())
        self.assertEqual(output['archive']['sha256'], self.sdk['archive_sha256'])
        self.assertEqual(output['manifest']['sha256'], self.sdk['manifest_sha256'])
        self.assertEqual(output['version'], self.version)
        self.assertNotIn('native_current_run', output)

    def test_validation_local_archive_directory(self):
        destination = self.validation/'ui-source'
        destination.mkdir()
        for artifact in (self.root/'build').iterdir():
            artifact.rename(destination/artifact.name)
        self.report['ui_archive_dir'] = str(destination)
        self.persist()
        previous = self.root/f'build/ui-framework-{self.version}-release.json'
        previous.write_text('previous release preserved')
        output = finalize(self.root, self.validation)
        self.assertEqual(output.parent, destination)
        self.assertEqual(previous.read_text(), 'previous release preserved')
        result = json.loads(output.read_text())
        self.assertEqual(result['archive']['sha256'], self.sdk['archive_sha256'])

    def test_failed_or_incomplete_project_report(self):
        for status in ('RUNNING', 'FAIL'):
            self.report['status'] = status
            self.persist()
            with self.assertRaisesRegex(ValueError, 'did not pass'): finalize(self.root, self.validation)
        self.report['status'] = 'PASS'
        self.report['stages'].pop()
        self.persist()
        with self.assertRaisesRegex(ValueError, 'stages'): finalize(self.root, self.validation)

    def test_failed_stage_cannot_be_hidden_by_top_level_pass(self):
        self.report['stages'][0]['exit_code'] = 1
        self.persist()
        with self.assertRaisesRegex(ValueError, 'stage did not pass'): finalize(self.root, self.validation)

    def test_source_changed_after_validation(self):
        (self.root/'CHANGELOG.md').write_text('changed after validation')
        with self.assertRaisesRegex(ValueError, 'Source changed'): finalize(self.root, self.validation)

    def test_archive_changed_after_validation(self):
        with self.archive.open('ab') as output: output.write(b'changed')
        with self.assertRaisesRegex(ValueError, 'Archive differs'): finalize(self.root, self.validation)

    def test_missing_reproducibility_or_version_proof(self):
        for flag in ('reproducible', 'header_runtime_cmake_agree', 'different_versions_rejected'):
            self.sdk[flag] = False
            self.persist()
            with self.assertRaisesRegex(ValueError, flag): finalize(self.root, self.validation)
            self.sdk[flag] = True

    def test_mismatched_version(self):
        self.report['version'] = '99.99.99'
        self.persist()
        with self.assertRaisesRegex(ValueError, 'version differs'): finalize(self.root, self.validation)

    def test_checksum_sidecar_changed(self):
        self.archive.with_suffix('.gz.sha256').write_text('incorrect checksum')
        with self.assertRaisesRegex(ValueError, 'checksum sidecar'): finalize(self.root, self.validation)

    def test_previous_native_report_cannot_become_current_proof(self):
        flags = {name: True for name in ('mouse_clipboard_undo_save_picker', 'document_undo_redo_across_save',
                 'resize', 'native_close_cancel', 'save_before_native_close', 'exit_zero',
                 'reopen_edit_save', 'reopen_exit_zero', 'runtime_log_clean')}
        native = self.root/'previous-native.json'
        report = {'backends': {'sdl2': dict(flags), 'vulkan': dict(flags, validation_enabled=True)}}
        native.write_text(json.dumps(report))
        output = json.loads(finalize(self.root, self.validation, native).read_text())
        self.assertNotIn('native_current_run', output)
        self.assertEqual(output['previous_native_evidence']['relationship'], 'Previous run; not replayed for this archive')
        report['backends']['sdl2']['reopen_edit_save'] = False
        native.write_text(json.dumps(report))
        with self.assertRaisesRegex(ValueError, 'incomplete or failed'): finalize(self.root, self.validation, native)

    def test_requested_native_run_requires_stage(self):
        self.report['native_requested'] = True
        self.persist()
        with self.assertRaisesRegex(ValueError, 'Native validation stage missing'): finalize(self.root, self.validation)

    def test_version_bump_changes_archive_name(self):
        header = self.root/'include/ui/version.h'
        major, minor, current = self.version.split('.')
        header.write_text(header.read_text().replace(f'#define UI_VERSION_PATCH {current}\n', f'#define UI_VERSION_PATCH {int(current)+1}\n'))
        with patch.object(package_ui, 'root', self.root):
            output = package_ui.package(self.root/'bumped', 0)
        self.assertEqual(output.name, f'ui-framework-{major}.{minor}.{int(current)+1}-source.tar.gz')

    def test_invalid_version_and_epoch_rejected(self):
        with patch.object(package_ui, 'root', self.root):
            for epoch in (-1, 0x100000000):
                with self.assertRaisesRegex(ValueError, 'SOURCE_DATE_EPOCH'): package_ui.package(self.root/'invalid', epoch)
        header = self.root/'include/ui/version.h'
        header.write_text(header.read_text().replace('#define UI_VERSION_MAJOR ', '#define UI_VERSION_MAJOR 0x'))
        with self.assertRaisesRegex(ValueError, 'UI_VERSION_MAJOR'): read_version(self.root)


if __name__ == '__main__':
    unittest.main()
