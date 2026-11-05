#!/usr/bin/env python3
"""Opt-in native X11/i3 validation. Input is gated on the child window's focus.
Uses only temporary documents; replaces clipboard with synthetic test text.
No keyboard/clipboard contents from other applications are read.
"""
import argparse
import json
import os
import re
import signal
from pathlib import Path
import subprocess
import tempfile
import time
from PIL import ImageGrab

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--backend', choices=('all', 'sdl2', 'vulkan'), default='all')
parser.add_argument('--vulkan-platform', choices=('glfw', 'sdl2'), default='glfw', help='Window backend for the Vulkan host')
parser.add_argument('--vulkan-binary', type=Path, help='Use an external installed-SDK consumer (requires --backend vulkan)')
parser.add_argument('--working-directory', type=Path, help='Launch owned editor processes from this directory')
parser.add_argument('--quick-clicks', action='store_true',
                    help='Queue a complete native click while the child is paused')
parser.add_argument('--document-history', action='store_true', help='Verify document undo/redo across saves')
parser.add_argument('--focus-cycle', action='store_true', help='Verify focus loss using a second owned editor window')
parser.add_argument('--open-picker', action='store_true', help='Reopen the saved document through the actual UI picker')
parser.add_argument('--output-dir', type=Path, help='Directory for logs, screenshots and report')
args_cli = parser.parse_args()
if args_cli.vulkan_binary and args_cli.backend != 'vulkan':
    parser.error('--vulkan-binary requires --backend vulkan')
if args_cli.vulkan_binary:
    args_cli.vulkan_binary = args_cli.vulkan_binary.resolve()
    if not args_cli.vulkan_binary.is_file() or not os.access(args_cli.vulkan_binary, os.X_OK):
        parser.error('--vulkan-binary must be an executable file')
launch_directory = args_cli.working_directory.resolve() if args_cli.working_directory else ROOT
if not launch_directory.is_dir():
    parser.error('--working-directory must exist')
OUT = args_cli.output_dir or ROOT / ('evidence/config-editor-document-history' if args_cli.document_history else 'evidence/config-editor-native-quick-clicks' if args_cli.quick_clicks else 'evidence/config-editor-native')
if args_cli.output_dir is None and args_cli.vulkan_platform == 'sdl2':
    OUT = OUT.with_name(OUT.name + '-vulkan-sdl2')
OUT = OUT.resolve()
OUT.mkdir(parents=True, exist_ok=True)
backends = ('sdl2', 'vulkan') if args_cli.backend == 'all' else (args_cli.backend,)
report_path = OUT / ('report.json' if args_cli.backend == 'all' else f'report-{args_cli.backend}.json')
report = {'scope': 'Automated native X11 input, not manual testing',
          'external_vulkan_binary': str(args_cli.vulkan_binary) if args_cli.vulkan_binary else None,
          'working_directory': str(launch_directory),
          'queued_complete_clicks': args_cli.quick_clicks,
          'open_picker_requested': args_cli.open_picker,
          'owned_processes': [],
          'focus_cycle_requested': args_cli.focus_cycle, 'vulkan_platform': args_cli.vulkan_platform, 'backends': {}}

def cmd(*args):
    result = subprocess.run(args, capture_output=True, text=True, timeout=8)
    if result.returncode: raise RuntimeError(f'{args[0]} failed: {result.stderr.strip() or result.stdout.strip()}')
    return result.stdout.strip()

def wait_for(test, message, timeout=8):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if test(): return
        time.sleep(.1)
    raise RuntimeError(message)

for backend in backends:
    checks = report['backends'][backend] = {}
    with tempfile.TemporaryDirectory(prefix='config-native-') as directory:
        document = Path(directory) / 'configurazione.cfgapp'
        env = dict(os.environ, VK_LAYER_VALIDATE_SYNC='1', UI_INPUT_DEBUG='1')
        for key in ('SDL_VIDEODRIVER', 'APP_AUTOSTART_SECONDS', 'APP_FRAME_DUMP_DIR', 'NO_VALIDATION', 'VK_LAYER_DISABLES', 'VK_LAYER_ENABLES'):
            env.pop(key, None)
        env['SDL_VIDEODRIVER'] = 'x11'
        process = None
        try:
            for reopen in (False, True):
                tag = backend + ('-reopen' if reopen else '')
                suffix = '-sdl2' if backend == 'vulkan' and args_cli.vulkan_platform == 'sdl2' else ''
                binary = args_cli.vulkan_binary or ROOT / f'build/config-editor-{backend}-debug{suffix}.out'
                args = [str(binary), '--root', directory]
                if reopen: args += ['--open', str(document)]
                with (OUT / f'{tag}.log').open('w') as log:
                    process = subprocess.Popen(['stdbuf', '-oL', *args], cwd=launch_directory, env=env, stdout=log, stderr=subprocess.STDOUT)
                    report['owned_processes'].append({'phase': tag, 'pid': process.pid})
                    window = None
                    def discover():
                        global window
                        if process.poll() is not None: raise RuntimeError('editor exited during startup')
                        result = subprocess.run(['xdotool', 'search', '--all', '--onlyvisible', '--pid', str(process.pid), '--name', 'Editor di configurazione'], capture_output=True, text=True, timeout=8)
                        if result.returncode == 0 and result.stdout.strip(): window = result.stdout.splitlines()[0]
                        return window is not None
                    wait_for(discover, 'child window not found')
                    if cmd('xdotool', 'getwindowpid', window) != str(process.pid): raise RuntimeError('window does not belong to child')
                    # Vulkan maps its window before device/font startup finishes.
                    # Wait for an actual UI frame before sending native input.
                    wait_for(lambda: '[UI INPUT]' in (OUT / f'{tag}.log').read_text(), 'first UI frame not ready', timeout=20)
                    cmd('i3-msg', f'[id="{window}"] floating enable')
                    cmd('xdotool', 'windowsize', '--sync', window, '1000', '800')
                    cmd('xdotool', 'windowactivate', '--sync', window)
                    time.sleep(.5)
                    last_action = 'startup'
                    def focus():
                        if process.poll() is not None: raise RuntimeError('editor exited before completion')
                        actual = cmd('xdotool', 'getwindowfocus')
                        if actual != window:
                            # IDs/PIDs only: no titles or contents of unrelated
                            # windows. Preserve evidence of ownership on abort.
                            owner = subprocess.run(['xdotool', 'getwindowpid', actual], capture_output=True, text=True, timeout=8)
                            checks['focus_at_failure'] = {'phase': tag, 'expected_window': window,
                                                         'expected_pid': process.pid, 'actual_window': actual,
                                                         'actual_pid': owner.stdout.strip() if owner.returncode == 0 else None}
                            raise RuntimeError(f'child lost focus; input aborted ({last_action}, expected {window}, actual {actual})')
                    def key(name, may_close=False):
                        global last_action
                        last_action = 'key ' + name
                        focus(); cmd('xdotool', 'key', '--clearmodifiers', name); time.sleep(.12)
                        if not may_close: focus()
                    def type_text(value):
                        for char in value:
                            # Uppercase keysyms do not reliably synthesize Shift
                            # with --clearmodifiers on every X11 keyboard map.
                            name = 'shift+' + char.lower() if 'A' <= char <= 'Z' else {' ': 'space', '!': 'exclam'}.get(char, char)
                            key(name)
                    def click(x, y):
                        global last_action
                        last_action = f'click {x},{y}'
                        focus(); cmd('xdotool', 'mousemove', '--window', window, str(x), str(y))
                        focus()
                        if args_cli.quick_clicks:
                            # Both transitions arrive before the next native poll.
                            # Pause only our child, and always resume it on error.
                            process.send_signal(signal.SIGSTOP)
                            try:
                                _, status = os.waitpid(process.pid, os.WUNTRACED)
                                if not os.WIFSTOPPED(status):
                                    raise RuntimeError('child did not stop for queued click')
                                cmd('xdotool', 'click', '1')
                            finally:
                                process.send_signal(signal.SIGCONT)
                        else:
                            cmd('xdotool', 'mousedown', '1')
                            time.sleep(.12)
                            cmd('xdotool', 'mouseup', '1')
                        time.sleep(.25); focus()
                    def capture(name):
                        focus()
                        geometry = dict(line.split('=', 1) for line in cmd('xdotool', 'getwindowgeometry', '--shell', window).splitlines() if '=' in line)
                        x, y, w, h = (int(geometry[k]) for k in ('X', 'Y', 'WIDTH', 'HEIGHT'))
                        ImageGrab.grab(bbox=(x, y, x+w, y+h)).save(OUT / f'{backend}-{name}.png')
                    def persisted(value):
                        return document.exists() and f'\t{value}\t' in document.read_text()
                    def target_ui(suffix):
                        for _ in range(64):
                            matches = re.findall(r'\[EDITOR NAV\] focus=(\S+)', (OUT / f'{tag}.log').read_text())
                            if matches and matches[-1].endswith(suffix): return
                            key('Tab')
                        raise RuntimeError('UI focus target not reached: ' + suffix)
                    def close_request():
                        focus(); cmd('i3-msg', f'[id="{window}"] kill'); time.sleep(.4)
                    if args_cli.focus_cycle and not reopen:
                        click(270, 332)
                        wait_for(lambda: 'focus=config/1/1/name' in (OUT / f'{tag}.log').read_text(), 'field did not focus')
                        peer = None
                        with (OUT / f'{backend}-focus-peer.log').open('w') as peer_log:
                            try:
                                peer = subprocess.Popen(['stdbuf', '-oL', *args], cwd=launch_directory, env=env,
                                                        stdout=peer_log, stderr=subprocess.STDOUT)
                                report['owned_processes'].append({'phase': tag + '-focus-peer', 'pid': peer.pid})
                                peer_window = None
                                def discover_peer():
                                    global peer_window
                                    if peer.poll() is not None: raise RuntimeError('focus peer exited during startup')
                                    result = subprocess.run(['xdotool', 'search', '--all', '--onlyvisible', '--pid', str(peer.pid),
                                                             '--name', 'Editor di configurazione'], capture_output=True, text=True, timeout=8)
                                    if result.returncode == 0 and result.stdout.strip(): peer_window = result.stdout.splitlines()[0]
                                    return peer_window is not None
                                wait_for(discover_peer, 'focus peer not found', timeout=20)
                                if peer_window == window or cmd('xdotool', 'getwindowpid', peer_window) != str(peer.pid):
                                    raise RuntimeError('focus peer window ownership mismatch')
                                wait_for(lambda: '[UI INPUT]' in (OUT / f'{backend}-focus-peer.log').read_text(), 'peer frame not ready', timeout=20)
                                cmd('xdotool', 'windowactivate', '--sync', peer_window)
                                wait_for(lambda: cmd('xdotool', 'getwindowfocus') == peer_window, 'peer did not receive focus')
                                wait_for(lambda: ' focus=- ' in (OUT / f'{tag}.log').read_text().split('focus=config/1/1/name')[-1],
                                         'editor focus not cleared after native focus loss')
                                checks['native_focus_loss'] = True
                                cmd('i3-msg', f'[id="{peer_window}"] kill')
                                if peer.wait(timeout=8) != 0: raise RuntimeError('focus peer exit failed')
                                # i3 may warp the pointer while closing the peer.
                                # Reposition it within our surviving child during
                                # this explicit focus transition, before resuming
                                # the strict per-action focus guards. No clicks
                                # or keys are sent while focus is unverified.
                                if cmd('xdotool', 'getwindowpid', window) != str(process.pid):
                                    raise RuntimeError('return window ownership mismatch')
                                cmd('xdotool', 'mousemove', '--sync', '--window', window, '500', '400')
                                cmd('xdotool', 'windowactivate', '--sync', window)
                                # Let i3 finish focus events queued by peer teardown
                                # before resuming the input-gated interaction steps.
                                time.sleep(.5)
                                focus()
                                checks['native_focus_loss_and_return'] = True
                            finally:
                                if peer is not None and peer.poll() is None:
                                    peer.terminate()
                                    try: peer.wait(timeout=4)
                                    except subprocess.TimeoutExpired: peer.kill(); peer.wait(timeout=4)
                    if reopen:
                        click(270, 332); key('End'); type_text(' reopened'); key('Return')
                        click(470, 125)
                        wait_for(lambda: persisted('Pending reopened'), 'loaded document/edit/save mismatch')
                        checks['reopen_edit_save'] = True
                        capture('reopened')
                        close_request()
                    else:
                        click(270, 332); capture('field-focus'); key('ctrl+a'); type_text('Native clipboard'); capture('typed')
                        key('ctrl+a'); key('ctrl+c'); key('End'); type_text('!')
                        key('ctrl+a'); key('ctrl+v'); capture('pasted'); key('ctrl+z'); capture('undo'); key('ctrl+y'); capture('redo'); key('Return')
                        click(470, 125); key('shift+Tab'); key('Return')
                        wait_for(lambda: persisted('Native clipboard'), 'native clipboard or Save picker mismatch')
                        checks['mouse_clipboard_undo_save_picker'] = True
                        if args_cli.document_history:
                            click(270, 332)
                            for _ in range(6): key('Tab')
                            key('Return')  # Document undo, after Name/Note/check/levels/remove.
                            click(470, 125)
                            wait_for(lambda: persisted('Profilo 1'), 'document undo/save mismatch')
                            click(270, 332)
                            for _ in range(7): key('Tab')
                            key('Return')  # Redo survives saving the undone document.
                            click(470, 125)
                            wait_for(lambda: persisted('Native clipboard'), 'document redo/save mismatch')
                            checks['document_undo_redo_across_save'] = True
                            capture('document-redo')
                        saved_name = 'Native clipboard'
                        if args_cli.open_picker:
                            target_ui('config/actions/1'); key('Return')
                            target_ui('configurazione.cfgapp/entry'); key('Return')
                            capture('open-picker')
                            target_ui('actions/confirm'); key('Return')
                            click(270, 332); key('End'); type_text(' opened'); key('Return')
                            click(470, 125)
                            saved_name += ' opened'
                            wait_for(lambda: persisted(saved_name), 'Open picker did not reload the saved document')
                            checks['native_open_picker_reload_edit_save'] = True
                        for w, h in ((700, 650), (1000, 800)):
                            cmd('xdotool', 'windowsize', '--sync', window, str(w), str(h)); time.sleep(.5)
                            geometry = cmd('xdotool', 'getwindowgeometry', '--shell', window)
                            if f'WIDTH={w}\n' not in geometry+'\n' or f'HEIGHT={h}\n' not in geometry+'\n': raise RuntimeError('resize mismatch')
                        checks['resize'] = True
                        click(270, 332); key('ctrl+a'); type_text('Pending'); key('Return')
                        close_request(); capture('unsaved-dialog'); key('Escape')
                        if not persisted(saved_name): raise RuntimeError('cancel changed file')
                        checks['native_close_cancel'] = True
                        close_request(); key('Tab'); key('Return', may_close=True)
                        wait_for(lambda: persisted('Pending'), 'Save and close did not save document')
                        checks['save_before_native_close'] = True
                    code = process.wait(timeout=8)
                    if code != 0: raise RuntimeError(f'exit status {code}')
                    checks['reopen_exit_zero' if reopen else 'exit_zero'] = True
                    process = None
            logs = ''.join((OUT / f'{backend}{suffix}.log').read_text() for suffix in ('', '-reopen'))
            if args_cli.focus_cycle: logs += (OUT / f'{backend}-focus-peer.log').read_text()
            if any(token in logs for token in ('VUID-', 'Validation Error', 'SYNC-HAZARD', '[ERROR]')): raise RuntimeError('runtime validation errors')
            checks['runtime_log_clean'] = True
            if backend == 'vulkan': checks['validation_enabled'] = 'Vulkan validation attiva' in logs
            print(backend + ': PASS', flush=True)
        except Exception as error:
            checks['error'] = str(error)
            try:
                if process is not None and process.poll() is None: capture('failure')
                if document.exists(): checks['document_at_failure'] = document.read_text()
            except Exception: pass
            print(backend + ': FAIL: ' + str(error), flush=True)
        finally:
            if process is not None and process.poll() is None:
                process.terminate()
                try: process.wait(timeout=4)
                except subprocess.TimeoutExpired: process.kill(); process.wait(timeout=4)
            report_path.write_text(json.dumps(report, indent=2)+'\n')
        if 'error' in checks: break  # Do not continue desktop input after an unexpected event.
report['not_verified'] = ['manual user session', 'Wayland', 'other OS/GPU', 'LeakSanitizer']
report['clipboard'] = 'synthetic test string replaces previous clipboard; no previous contents read'
report_path.write_text(json.dumps(report, indent=2)+'\n')
raise SystemExit(0 if len(report['backends']) == len(backends) and all('error' not in v and all(v.values()) for v in report['backends'].values()) else 1)
