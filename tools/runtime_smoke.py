#!/usr/bin/env python3
"""Linux/X11 smoke run of this repository's window only."""
from pathlib import Path
import subprocess
import time
import os
import json

root = Path(__file__).resolve().parents[1]
evidence = root / 'evidence'
frames = evidence / 'frames'
frames.mkdir(parents=True, exist_ok=True)
env = dict(os.environ, APP_AUTOSTART_SECONDS='12', APP_FRAME_DUMP_DIR=str(frames), APP_FRAME_DUMP_FPS='3', APP_FRAME_DUMP_MAX='36')
results = {'cwd': '/tmp', 'interactions': [], 'window_sizes': []}
with (evidence / 'runtime.log').open('w') as log:
    process = subprocess.Popen([str(root / 'build/app-debug.out')], cwd='/tmp', env=env, stdout=log, stderr=subprocess.STDOUT)
    window = None
    for _ in range(45):
        found = subprocess.run(['xdotool', 'search', '--pid', str(process.pid), '--name', 'Graphics App Base'], capture_output=True, text=True)
        if found.returncode == 0 and found.stdout.strip():
            window = found.stdout.splitlines()[0]
            break
        if process.poll() is not None:
            break
        time.sleep(0.1)
    if window:
        results['window_id'] = window
        # Float only the window owned by this child process, so the WM allows resize.
        wm = subprocess.run(['i3-msg', f'[id="{window}"] floating enable'], capture_output=True, text=True)
        results['own_window_floating'] = wm.returncode == 0
        subprocess.run(['xdotool', 'windowactivate', '--sync', window], check=True)
        subprocess.run(['xdotool', 'windowsize', '--sync', window, '1000', '680'], check=True)
        time.sleep(0.6)
        geometry = subprocess.run(['xdotool', 'getwindowgeometry', '--shell', window], capture_output=True, text=True)
        results['window_sizes'].append(geometry.stdout)
        # Test the existing button at its logical-pixel location.
        for _ in range(2):
            subprocess.run(['xdotool', 'mousemove', '--window', window, '110', '190', 'mousedown', '1'], check=True)
            time.sleep(0.10)
            subprocess.run(['xdotool', 'mouseup', '1'], check=True)
            time.sleep(0.15)
        results['interactions'].append('button: two click events sent')
        # Exercise single-line input without relying on between-frame key taps.
        subprocess.run(['xdotool', 'mousemove', '--window', window, '150', '143', 'mousedown', '1'], check=True)
        time.sleep(0.10)
        subprocess.run(['xdotool', 'mouseup', '1', 'keydown', 'ctrl', 'keydown', 'a'], check=True)
        time.sleep(0.10)
        subprocess.run(['xdotool', 'keyup', 'a', 'keyup', 'ctrl'], check=True)
        for key in ('b','a','s','e','space','p','r','o','n','t','a'):
            subprocess.run(['xdotool', 'keydown', key], check=True)
            time.sleep(0.06)
            subprocess.run(['xdotool', 'keyup', key], check=True)
            time.sleep(0.06)
        results['interactions'].append('text: base pronta key sequence sent')
        time.sleep(0.8)
        subprocess.run(['xdotool', 'mousemove', '--window', window, '110', '253', 'mousedown', '1'], check=True)
        time.sleep(0.10)
        subprocess.run(['xdotool', 'mouseup', '1'], check=True)
        results['interactions'].append('theme toggle click event sent')
        time.sleep(0.6)
        subprocess.run(['xdotool', 'windowsize', '--sync', window, '700', '820'], check=True)
        time.sleep(0.8)
        geometry = subprocess.run(['xdotool', 'getwindowgeometry', '--shell', window], capture_output=True, text=True)
        results['window_sizes'].append(geometry.stdout)
    else:
        results['window_found'] = False
    try:
        results['exit_code'] = process.wait(timeout=25)
    except subprocess.TimeoutExpired:
        process.terminate()
        results['exit_code'] = process.wait(timeout=5)
        results['timeout'] = True
results['captured_frames'] = len(list(frames.glob('frame_*.ppm')))
(evidence / 'runtime-smoke.json').write_text(json.dumps(results, indent=2) + '\n')
print(json.dumps(results, indent=2))
if results['exit_code'] != 0 or not results['captured_frames']:
    raise SystemExit(1)
