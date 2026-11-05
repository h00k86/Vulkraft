#!/usr/bin/env python3
"""Verify GraphicsApp archive, exact repacks and SDKs built only from its sources."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import tarfile
import time
import zlib
from app_package import digest, extract, read_version, verify

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--archive', type=Path)
    parser.add_argument('--output-dir', type=Path, default=ROOT / 'evidence/app-package')
    parser.add_argument('--runtime', action='store_true', help='Include Vulkan windows from extracted sources')
    parser.add_argument('--native', action='store_true', help='Include owned-window native editor workflow (implies runtime)')
    args = parser.parse_args()
    archive = (args.archive or ROOT / f'build/graphics-app-{read_version(ROOT)}-source.tar.gz').resolve()
    out = args.output_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    report = {'status': 'RUNNING', 'archive': str(archive), 'stages': [],
              'runtime_requested': args.runtime or args.native, 'native_requested': args.native}

    def save():
        (out / 'package-verification.json').write_text(json.dumps(report, indent=2) + '\n')

    def run(name, command, env=None, timeout=900):
        entry = {'name': name, 'command': list(map(str, command)), 'status': 'RUNNING', 'log': name + '.log'}
        report['stages'].append(entry)
        save()
        start = time.monotonic()
        with (out / entry['log']).open('w') as log:
            try:
                result = subprocess.run(entry['command'], cwd=work, env=env, timeout=timeout,
                                        stdout=log, stderr=subprocess.STDOUT)
                entry.update(exit_code=result.returncode, status='PASS' if result.returncode == 0 else 'FAIL')
            except (OSError, subprocess.TimeoutExpired) as error:
                entry.update(status='FAIL', error=str(error))
        entry['elapsed_seconds'] = round(time.monotonic() - start, 3)
        save()
        if entry['status'] != 'PASS':
            raise ValueError(name + ' failed; see ' + entry['log'])

    try:
        verified = verify(archive)
        prefix = 'graphics-app-' + verified['version']
        if archive.with_suffix('.gz.sha256').read_text() != verified['sha256'] + '  ' + archive.name + '\n':
            raise ValueError('Checksum sidecar mismatch')
        if (archive.parent / (prefix + '-manifest-sha256.json')).read_bytes() != verified['manifest']:
            raise ValueError('Manifest sidecar mismatch')
        report.update(version=verified['version'], archive_sha256=verified['sha256'],
                      manifest_sha256=digest(verified['manifest']), files=len(verified['sources']))
        save()
        with tempfile.TemporaryDirectory(prefix='graphics-app-package-') as temporary:
            work = Path(temporary)
            source = extract(verified, work)
            if read_version(source) != verified['version']:
                raise ValueError('CMake version differs from archive version')
            env = dict(os.environ, SOURCE_DATE_EPOCH=str(verified['epoch']))
            for attempt in range(2):
                destination = work / f'repack-{attempt}'
                run(f'repack-{attempt}', [sys.executable, source / 'tools/package_app.py', '--output-dir', destination], env)
                if (destination / archive.name).read_bytes() != archive.read_bytes():
                    raise ValueError('Archive is not byte reproducible')
                if attempt == 0:
                    for index, name in enumerate(sorted(verified['sources'])):
                        path = source / name
                        path.chmod(0o755 if index % 2 else 0o600)
                        os.utime(path, (123456789 + index, 123456789 + index))
            report['reproducible_after_metadata_changes'] = True
            command = [sys.executable, source / 'tools/validate_app_sdk.py', '--output-dir', out / 'sdk']
            if args.native: command.append('--native')
            elif args.runtime: command.append('--runtime')
            run('sdk-from-archive', command)
            sdk = json.loads((out / 'sdk/report.json').read_text())
            if sdk['status'] != 'PASS' or sdk['graphics_app_version'] != verified['version']:
                raise ValueError('Extracted SDK validation incomplete or version mismatch')
            cases = {host + '-' + config for host in ('glfw', 'sdl2') for config in ('debug', 'release')}
            if set(sdk.get('consumers', {})) != cases or not sdk.get('stages') or any(s.get('status') != 'PASS' for s in sdk['stages']):
                raise ValueError('Extracted SDK matrix incomplete or a stage failed')
            for case, checks in sdk['consumers'].items():
                if not checks or any(value is not True for value in checks.values()) or checks.get('installed_sdk_editor_workflow') is not True:
                    raise ValueError('Incomplete consumer proof: ' + case)
                if (args.runtime or args.native) and checks.get('external_editor_smoke') is not True:
                    raise ValueError('Vulkan editor proof missing: ' + case)
                if args.native and case.endswith('-debug') and checks.get('external_editor_native') is not True:
                    raise ValueError('Native editor proof missing: ' + case)
            report.update(status='PASS', source_only_sdk_verified=True, sdk_report_sha256=digest((out / 'sdk/report.json').read_bytes()),
                          not_verified=sdk['not_verified'])
    except (ValueError, OSError, KeyError, tarfile.TarError, EOFError, zlib.error) as error:
        report.update(status='FAIL', error=str(error))
    save()
    print('GraphicsApp source package ' + report['status'] + ': ' + str(out / 'package-verification.json'), flush=True)
    return 0 if report['status'] == 'PASS' else 1


if __name__ == '__main__':
    raise SystemExit(main())
