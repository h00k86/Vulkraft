#!/usr/bin/env python3
"""Opt-in X11/i3 runtime check; no pointer moves, keystrokes or focus requests."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile
import time
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--output', default='evidence/runtime-final')
parser.add_argument('--unicode', action='store_true')
parser.add_argument('--draw-list', action='store_true', help='Validate deferred UI replay')
args = parser.parse_args()
OUT = ROOT / args.output
OUT.mkdir(parents=True, exist_ok=True)
report = {'draw_list': args.draw_list, 'checks': {}, 'scope': 'Vulkan runtime; input in separate hidden GLFW probe'}
checks = report['checks']

def command(*args):
    return subprocess.run(args, check=True, capture_output=True, text=True, timeout=8).stdout

def clean(text):
    return not any(s in text for s in ('Validation Error', 'Validation Warning', 'VUID-', 'SYNC-HAZARD', '[ERROR]'))

with tempfile.TemporaryDirectory(prefix='graphics-final-') as directory:
    env = dict(os.environ, APP_AUTOSTART_SECONDS='10', APP_RUNTIME_PASSIVE='1',
               APP_FRAME_DUMP_DIR=directory, APP_FRAME_DUMP_FPS='3', APP_FRAME_DUMP_MAX='40',
               VK_LAYER_VALIDATE_SYNC='1')
    if args.unicode:
        env['APP_RUNTIME_UNICODE_DEMO'] = '1'
    for name in ('NO_VALIDATION', 'APP_NO_VSYNC', 'VK_LAYER_DISABLES', 'VK_LAYER_ENABLES'):
        env.pop(name, None)
    with (OUT / 'runtime.log').open('w') as log:
        process = subprocess.Popen([str(ROOT / ('build/draw-list-runtime.out' if args.draw_list else 'build/runtime-validation.out'))], cwd='/tmp',
                                   env=env, stdout=log, stderr=subprocess.STDOUT)
        try:
            window = None
            for _ in range(80):
                found = subprocess.run(['xdotool', 'search', '--pid', str(process.pid), '--name',
                                        'Graphics App Base'], capture_output=True, text=True)
                if found.returncode == 0 and found.stdout.strip():
                    window = found.stdout.splitlines()[0]
                    break
                if process.poll() is not None:
                    raise RuntimeError('application exited before window discovery')
                time.sleep(0.1)
            if not window:
                raise RuntimeError('window not found')
            report['window_id'] = window
            command('i3-msg', f'[id="{window}"] floating enable')
            for width, height in ((700, 820), (1100, 700), (1000, 800)):
                command('xdotool', 'windowsize', '--sync', window, str(width), str(height))
                time.sleep(1)
                geometry = command('xdotool', 'getwindowgeometry', '--shell', window)
                checks[f'resize_{width}x{height}'] = f'WIDTH={width}\n' in geometry and f'HEIGHT={height}\n' in geometry
            report['exit_code'] = process.wait(timeout=20)
            checks['exit_zero'] = report['exit_code'] == 0
        except Exception as error:
            report['error'] = str(error)
            checks['completed'] = False
        finally:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=5)
    text = (OUT / 'runtime.log').read_text()
    checks['validation_active'] = 'Vulkan validation attiva' in text
    checks['validation_clean'] = clean(text)
    checks['vsync_both'] = 'RUNTIME vsync=0' in text and 'RUNTIME vsync=1' in text
    frames = sorted(Path(directory).glob('frame_*.ppm'))
    report['captured_frames'] = len(frames)
    checks['captures'] = len(frames) >= 6
    for label, index in (('initial', 0), ('final', -1)):
        if frames:
            with Image.open(frames[index]) as picture:
                picture.save(OUT / f'{label}.png')
    # Independent starter run, using the production example composition.
    starter_env = dict(env, APP_AUTOSTART_SECONDS='2', APP_FRAME_DUMP_MAX='0')
    starter_env.pop('APP_RUNTIME_PASSIVE', None)
    starter_env.pop('APP_FRAME_DUMP_DIR', None)
    with (OUT / 'starter.log').open('w') as log:
        starter = subprocess.run([str(ROOT / 'build/app-debug.out')], cwd='/tmp', env=starter_env,
                                 stdout=log, stderr=subprocess.STDOUT, timeout=20)
    starter_text = (OUT / 'starter.log').read_text()
    checks['starter_exit_zero'] = starter.returncode == 0
    checks['starter_validation_active'] = 'Vulkan validation attiva' in starter_text
    checks['starter_validation_clean'] = clean(starter_text)
    probe = subprocess.run([str(ROOT / 'build/glfw-key-event-probe.out')], cwd=ROOT,
                           capture_output=True, text=True, timeout=15)
    (OUT / 'input-probe.txt').write_text(probe.stdout + probe.stderr)
    checks['hidden_input_probe'] = probe.returncode == 0
report['not_verified'] = ['real device loss', 'iconic minimization', 'other GPUs/OS', 'LeakSanitizer',
                          'interactive picker and full editing on Vulkan scene']
report['validation_settings'] = {'VK_LAYER_VALIDATE_SYNC': env['VK_LAYER_VALIDATE_SYNC'], 'VK_LAYER_ENABLES': 'unset'}
(OUT / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report, indent=2))
raise SystemExit(0 if all(checks.values()) else 1)
