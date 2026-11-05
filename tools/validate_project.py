#!/usr/bin/env python3
"""Repeatable project validation; desktop input is opt-in via --native."""
import argparse
import datetime
import json
import os
from pathlib import Path
import platform
import subprocess
import sys
import time
from ui_version import read_version
from app_package import read_version as read_app_version

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--native', action='store_true', help='Also run owned-window X11/i3 desktop interaction')
    parser.add_argument('--runtime', action='store_true', help='Also run Vulkan SDK smoke windows (no desktop input)')
    parser.add_argument('--output-dir', type=Path, default=ROOT / 'evidence/consolidation')
    parser.add_argument('--jobs', type=int, default=4)
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error('--jobs must be positive')
    out = args.output_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    app_version = read_app_version(ROOT)
    report = {'version': read_version(ROOT), 'started_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
              'graphics_app_version': app_version,
              'host': platform.platform(), 'python': platform.python_version(),
              'native_requested': args.native, 'stages': [],
              'runtime_requested': args.runtime or args.native,
              'ui_archive_dir': str(out / 'ui-source'),
              'not_verified': ['manual visual review', 'Wayland', 'other OS/GPU', 'LeakSanitizer',
                               'GPU performance and real font rendering performance']}
    if not args.native:
        report['not_verified'].append('native desktop interaction (use --native)')

    def save():
        (out / 'validation.json').write_text(json.dumps(report, indent=2) + '\n')

    def run(name, command, env=None):
        entry = {'name': name, 'command': command, 'status': 'RUNNING', 'log': name + '.log'}
        report['stages'].append(entry)
        save()
        print(name + ': running', flush=True)
        start = time.monotonic()
        try:
            with (out / entry['log']).open('w') as log:
                result = subprocess.run(command, cwd=ROOT, env=env, stdout=log, stderr=subprocess.STDOUT)
            entry.update(status='PASS' if result.returncode == 0 else 'FAIL', exit_code=result.returncode)
        except OSError as error:
            entry.update(status='FAIL', error=str(error))
        entry['elapsed_seconds'] = round(time.monotonic() - start, 3)
        save()
        print(name + ': ' + entry['status'], flush=True)
        return entry['status'] == 'PASS'

    make = ['make', f'-j{args.jobs}']
    stages = [
        ('architecture', [sys.executable, 'tools/check_boundaries.py']),
        ('tests-debug', make + ['BUILD=debug', 'test', 'test-platform-sdl2', 'test-ui-sdl2', 'ui-reference-sdl2',
                              'ui-reference-vulkan', 'config-editor-sdl2', 'config-editor-vulkan']),
        ('build-release', make + ['BUILD=release', 'ui-reference-sdl2', 'ui-reference-vulkan',
                                 'config-editor-sdl2', 'config-editor-vulkan', 'build/ui-benchmark.out', 'build/ui-editor-benchmark.out']),
        ('sanitizers', make + ['test-sanitize', 'test-platform-sdl2-sanitize', 'test-ui-sdl2-sanitize', 'test-benchmark-sanitize']),
        ('sdk-source', [sys.executable, 'tools/package_ui.py', '--output-dir', str(out / 'ui-source')]),
        ('sdk-package', [sys.executable, 'tools/check_ui_package.py', '--archive-dir', str(out / 'ui-source'), '--output-dir', str(out / 'sdk')]),
        ('app-source', [sys.executable, 'tools/package_app.py', '--output-dir', str(out / 'app-source')]),
        ('app-sdk', [sys.executable, 'tools/check_app_package.py', '--archive',
                     str(out / 'app-source' / f'graphics-app-{app_version}-source.tar.gz'),
                     '--output-dir', str(out / 'app-sdk')] + (['--native'] if args.native else ['--runtime'] if args.runtime else [])),
    ]
    for name, command in stages:
        if not run(name, command):
            break
    else:
        env = dict(os.environ, SDL_VIDEODRIVER='dummy')
        smoke_ok = True
        for mode in ('deferred', 'immediate'):
            for scenario in ('basic', 'navigation', 'editing', 'layout', 'color'):
                command = ['./build/ui-reference-sdl2-debug.out', f'--{scenario}-smoke']
                if mode == 'immediate': command.append('--immediate')
                if not run(f'sdl-{mode}-{scenario}', command, env):
                    smoke_ok = False
                    break
            if not smoke_ok: break
        if smoke_ok:
            for name, binary in [('benchmark', './build/ui-benchmark.out'),
                                 ('benchmark-editor', './build/ui-editor-benchmark.out')]:
                if not run(name, [binary]): break
                try:
                    measurement = json.loads((out / f'{name}.log').read_text())
                    (out / f'{name}.json').write_text(json.dumps(measurement, indent=2) + '\n')
                except (ValueError, OSError) as error:
                    report['stages'][-1].update(status='FAIL', error=str(error))
                    break
            if args.native and report['stages'][-1]['status'] == 'PASS':
                run('native', [sys.executable, 'tools/config_editor_native_validation.py', '--backend', 'all',
                               '--quick-clicks', '--document-history', '--focus-cycle',
                               '--output-dir', str(out / 'native')])
    report['status'] = 'PASS' if all(stage['status'] == 'PASS' for stage in report['stages']) else 'FAIL'
    report['finished_utc'] = datetime.datetime.now(datetime.timezone.utc).isoformat()
    save()
    print(f"Validation {report['status']}: {out / 'validation.json'}", flush=True)
    return 0 if report['status'] == 'PASS' else 1


if __name__ == '__main__':
    raise SystemExit(main())
