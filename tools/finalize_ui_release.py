#!/usr/bin/env python3
"""Bind successful SDK verification to the exact source archive and manifest."""
import argparse
import hashlib
import json
from pathlib import Path
import tarfile

from package_ui import files
from ui_version import read_version

ROOT = Path(__file__).resolve().parents[1]
REQUIRED_STAGES = {'architecture', 'tests-debug', 'build-release', 'sanitizers',
                   'sdk-source', 'sdk-package', 'benchmark', 'benchmark-editor'} | {
    f'sdl-{mode}-{scenario}' for mode in ('deferred', 'immediate')
    for scenario in ('basic', 'navigation', 'editing', 'layout', 'color')}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def native_evidence(path, relationship):
    data = path.read_bytes()
    report = json.loads(data)
    require(set(report.get('backends', {})) == {'sdl2', 'vulkan'}, 'Native report must cover SDL2 and Vulkan')
    required = {'mouse_clipboard_undo_save_picker', 'document_undo_redo_across_save',
                'resize', 'native_close_cancel', 'save_before_native_close', 'exit_zero',
                'reopen_edit_save', 'reopen_exit_zero', 'runtime_log_clean'}
    for backend, checks in report['backends'].items():
        require(required <= set(checks) and all(value is True for value in checks.values()),
                f'Native report is incomplete or failed: {backend}')
    require(report['backends']['vulkan'].get('validation_enabled') is True, 'Vulkan validation was not enabled')
    return {'status': 'PASS', 'report_sha256': digest(data), 'relationship': relationship,
            'scope': report.get('scope'), 'backends': report['backends'],
            'not_verified': report.get('not_verified', [])}


def finalize(root, validation_dir, previous_native_report=None):
    version = read_version(root)
    prefix = f'ui-framework-{version}'
    report_bytes = (validation_dir/'validation.json').read_bytes()
    report = json.loads(report_bytes)
    stages = report.get('stages', [])
    require(report.get('version') == version, 'Validation version differs from current headers')
    require(report.get('status') == 'PASS', 'Project validation did not pass')
    names = [stage['name'] for stage in stages]
    require(len(names) == len(set(names)) and REQUIRED_STAGES <= set(names), 'Required validation stages are missing or duplicated')
    require(all(stage.get('status') == 'PASS' and stage.get('exit_code') == 0 for stage in stages), 'A validation stage did not pass')
    require(not report.get('native_requested') or 'native' in names, 'Native validation stage missing')
    sdk_bytes = (validation_dir/'sdk/package-verification.json').read_bytes()
    sdk = json.loads(sdk_bytes)
    require(sdk.get('result') == 'PASS' and sdk.get('version') == version, 'SDK verification did not pass for this version')
    for flag in ('reproducible', 'header_runtime_cmake_agree', 'different_versions_rejected'):
        require(sdk.get(flag) is True, f'SDK verification missing: {flag}')
    archive_dir = Path(report.get('ui_archive_dir', root/'build'))
    archive_path = archive_dir/f'{prefix}-source.tar.gz'
    archive_data = archive_path.read_bytes()
    require(digest(archive_data) == sdk.get('archive_sha256'), 'Archive differs from the archive verified by the SDK checks')
    with tarfile.open(archive_path) as archive:
        stream = archive.extractfile(f'{prefix}/manifest-sha256.json')
        require(stream is not None, 'Source manifest missing')
        manifest_bytes = stream.read()
    manifest = json.loads(manifest_bytes)
    require(digest(manifest_bytes) == sdk.get('manifest_sha256'), 'Manifest differs from SDK verification')
    require(set(manifest) == set(files) and sdk.get('files') == len(manifest), 'Source distribution list changed since verification')
    for name, expected in manifest.items():
        require(digest((root/name).read_bytes()) == expected, f'Source changed since verification: {name}')
    require((archive_dir/f'{prefix}-manifest-sha256.json').read_bytes() == manifest_bytes, 'Sidecar manifest differs')
    require(archive_path.with_suffix('.gz.sha256').read_text() == f'{digest(archive_data)}  {archive_path.name}\n', 'Archive checksum sidecar differs')
    result = {'version': version, 'status': 'PASS', 'scope': 'Source SDK with current automatic verification',
              'archive': {'file': archive_path.name, 'sha256': digest(archive_data),
                          'source_date_epoch': int.from_bytes(archive_data[4:8], 'little')},
              'manifest': {'file': f'{prefix}-manifest-sha256.json', 'sha256': digest(manifest_bytes), 'files': len(manifest)},
              'automatic_verification': {'report_sha256': digest(report_bytes), 'sdk_report_sha256': digest(sdk_bytes),
                                         'started_utc': report.get('started_utc'), 'finished_utc': report.get('finished_utc'),
                                         'host': report.get('host'), 'stages': [{'name': s['name'], 'status': s['status'], 'exit_code': s['exit_code']} for s in stages],
                                         'reproducible': True, 'header_runtime_cmake_agree': True, 'different_versions_rejected': True},
              'not_verified': report.get('not_verified', [])}
    if report.get('native_requested'):
        result['native_current_run'] = native_evidence(validation_dir/'native/report.json', 'Executed in the current validation run')
    if previous_native_report is not None:
        result['previous_native_evidence'] = native_evidence(previous_native_report, 'Previous run; not replayed for this archive')
    output = archive_dir/f'{prefix}-release.json'
    output.write_text(json.dumps(result, indent=2, sort_keys=True)+'\n')
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--validation-dir', type=Path, required=True)
    parser.add_argument('--previous-native-report', type=Path)
    args = parser.parse_args()
    try:
        print(finalize(ROOT, args.validation_dir.resolve(), args.previous_native_report))
    except (ValueError, OSError, KeyError, tarfile.TarError) as error:
        parser.exit(1, f'Release refused: {error}\n')


if __name__ == '__main__':
    main()
