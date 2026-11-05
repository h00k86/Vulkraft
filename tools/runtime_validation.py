#!/usr/bin/env python3
"""Opt-in X11 validation of this process's window; preserves old audit evidence."""
import json
import os
from pathlib import Path
import subprocess
import time
import tempfile
import signal
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'evidence/runtime-validation'
OUT.mkdir(exist_ok=True)
temporary_frames = tempfile.TemporaryDirectory(prefix='graphics-runtime-')
FRAMES = Path(temporary_frames.name)
report = {'checks': {}, 'events': [], 'cwd': '/tmp'}
env = dict(os.environ, APP_AUTOSTART_SECONDS='45', APP_FRAME_DUMP_DIR=str(FRAMES),
           APP_FRAME_DUMP_FPS='3', APP_FRAME_DUMP_MAX='100',
           VK_LAYER_VALIDATE_SYNC='1')
env.pop('VK_LAYER_ENABLES', None)
env.pop('VK_LAYER_DISABLES', None)
env.pop('NO_VALIDATION', None)
env.pop('APP_NO_VSYNC', None)

def command(*args):
    return subprocess.run(args, check=True, text=True, capture_output=True, timeout=8).stdout

def ensure_focus():
    if command('xdotool','getwindowfocus').strip() != window:
        raise RuntimeError('test window lost focus; aborting desktop input')

def pause(seconds=0.15):
    time.sleep(seconds)
    if process.poll() is not None:
        raise RuntimeError(f'application exited early: {process.returncode}')

def key(k):
    ensure_focus()
    command('xdotool','keydown',k); pause(0.18)
    command('xdotool','keyup',k); pause(0.18)

def click(x,y):
    ensure_focus()
    command('xdotool','mousemove','--window',window,str(x),str(y),'mousedown','1')
    pause(0.10); command('xdotool','mouseup','1'); pause(0.2)

def replace(text):
    ensure_focus()
    command('xdotool','keydown','ctrl','keydown','a'); pause(0.10)
    command('xdotool','keyup','a','keyup','ctrl'); pause(0.10)
    for char in text:
        key({'-':'minus','.':'period',' ':'space'}.get(char,char))
    key('Return')

def capture(name):
    pause(0.6)
    frames=sorted(FRAMES.glob('frame_*.ppm'))
    if not frames: raise RuntimeError('no renderer captures')
    # The newest file may still be written; previous capture is complete.
    image=Image.open(frames[-2] if len(frames)>1 else frames[-1])
    image.save(OUT / f'{name}.png')
    report['events'].append(f'capture {name}: {image.size}')

with (OUT/'runtime.log').open('w') as log:
    process=subprocess.Popen([str(ROOT/'build/runtime-validation.out')],cwd='/tmp',env=env,stdout=log,stderr=subprocess.STDOUT)
    try:
        window=None
        for _ in range(60):
            found=subprocess.run(['xdotool','search','--pid',str(process.pid),'--name','Graphics App Base'],capture_output=True,text=True)
            if found.returncode==0 and found.stdout.strip():
                window=found.stdout.splitlines()[0]; break
            pause(0.1)
        if not window: raise RuntimeError('window not found')
        report['window_id']=window
        command('i3-msg',f'[id="{window}"] floating enable')
        command('xdotool','windowactivate','--sync',window)
        command('xdotool','windowsize','--sync',window,'1000','800'); pause(0.6)
        capture('initial')
        click(150,95); replace('base pronta')
        click(150,95); replace('secondo')
        # A deterministic frame stall: both X11 events arrive before the next poll.
        click(150,95)
        process.send_signal(signal.SIGSTOP)
        try:
            time.sleep(0.2)
            command('xdotool','keydown','z'); time.sleep(0.05)
            command('xdotool','keyup','z'); time.sleep(0.05)
        finally:
            process.send_signal(signal.SIGCONT)
        pause(0.7); key('Return')
        capture('short-key-after-stall')
        click(450,152); replace('0.75')
        click(450,212); replace('16777217')
        capture('edited')
        click(130,335); pause(0.5)
        click(130,335); pause(0.5)
        for width,height in [(700,820),(1100,700),(1000,800)]:
            command('xdotool','windowsize','--sync',window,str(width),str(height)); pause(0.8)
            geometry=command('xdotool','getwindowgeometry','--shell',window)
            report['events'].append(geometry)
            report['checks'][f'resize_{width}x{height}']=f'WIDTH={width}\n' in geometry and f'HEIGHT={height}\n' in geometry
        capture('resized')
        command('xdotool','windowminimize',window); time.sleep(0.7)
        report['events'].append(command('xprop','-id',window,'WM_STATE'))
        command('xdotool','windowmap',window)
        command('xdotool','windowactivate','--sync',window); pause(0.7)
        command('i3-msg',f'[id="{window}"] move scratchpad'); time.sleep(0.7)
        visible=subprocess.run(['xdotool','search','--onlyvisible','--pid',str(process.pid),'--name','Graphics App Base'],capture_output=True,text=True)
        hidden=window not in visible.stdout.splitlines()
        command('i3-msg',f'[id="{window}"] scratchpad show')
        command('xdotool','windowactivate','--sync',window); pause(0.7)
        visible=subprocess.run(['xdotool','search','--onlyvisible','--pid',str(process.pid),'--name','Graphics App Base'],capture_output=True,text=True)
        report['checks']['hide_restore']=hidden and window in visible.stdout.splitlines()
        command('xdotool','windowsize','--sync',window,'1000','800'); pause(1.0)
        capture('restored')
        click(130,275); capture('picker')
        click(680,675); capture('picker-confirmed')
        report['exit_code']=process.wait(timeout=40)
    except Exception as error:
        report['error']=str(error)
        process.terminate()
        report['exit_code']=process.wait(timeout=5)
    finally:
        # Prevent a held modifier on exceptional exits from affecting later input.
        if window:
            subprocess.run(['xdotool','keyup','ctrl','a','Return'],capture_output=True)
text=(OUT/'runtime.log').read_text()
checks=report['checks']
checks['exit_zero']=report.get('exit_code')==0 and 'error' not in report
checks['validation_active']='Vulkan validation attiva' in text
checks['validation_clean']=not any(x in text for x in ('Validation Error','VUID-','SYNC-HAZARD','Validation Warning'))
checks['text_first_commit']='RUNTIME text=base pronta focus=0' in text
checks['text_second_commit']='RUNTIME text=secondo focus=0' in text
checks['short_press_delivered']='RUNTIME key=Z action=1' in text and 'RUNTIME key=Z action=0' in text
# Compare against the actual pre-stall value, independently of earlier typing.
lines=text.splitlines()
press=next((i for i,line in enumerate(lines) if line=='RUNTIME key=Z action=1'),None)
before=[line for line in lines[:press] if line.startswith('RUNTIME text=')] if press is not None else []
after=[line for line in lines[press:] if line.startswith('RUNTIME text=')] if press is not None else []
pre_text=before[-1].split('RUNTIME text=',1)[1].rsplit(' focus=',1)[0] if before else None
post_text=after[0].split('RUNTIME text=',1)[1].rsplit(' focus=',1)[0] if after else None
report['short_press_probe']={'before':pre_text,'after':post_text}
checks['short_press_preserved']=pre_text is not None and post_text==pre_text+'z'
checks['float_commit']='RUNTIME float=0.75' in text
checks['integer_exact']='RUNTIME integer=16777217' in text
checks['vsync_both']='RUNTIME vsync=0' in text and 'RUNTIME vsync=1' in text
report['minimization']='PASS' if 'RUNTIME iconified=1' in text and 'RUNTIME iconified=0' in text else 'NOT VERIFIED: i3 kept WM_STATE Normal; hide/restore tested separately'
checks['picker_confirm']=f'RUNTIME picker_selected={ROOT}/assets/fonts' in text
report['captured_frames']=len(list(FRAMES.glob('frame_*.ppm')))
# Smoke the unmodified starter composition too, from outside the project root.
with (OUT/'starter.log').open('w') as log:
    starter_env=dict(env,APP_AUTOSTART_SECONDS='2',APP_FRAME_DUMP_MAX='2')
    starter=subprocess.run([str(ROOT/'build/app-debug.out')],cwd='/tmp',env=starter_env,stdout=log,stderr=subprocess.STDOUT,timeout=15)
report['starter_exit_code']=starter.returncode
starter_text=(OUT/'starter.log').read_text()
checks['starter_exit_zero']=starter.returncode==0
checks['starter_validation_clean']=not any(x in starter_text for x in ('Validation Error','VUID-','SYNC-HAZARD','Validation Warning'))
Image.open(FRAMES/'frame_00001.ppm').save(OUT/'starter.png')
report['validation_settings']={'VK_LAYER_VALIDATE_SYNC':'1','NO_VALIDATION':'unset'}
temporary_frames.cleanup()
(OUT/'report.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
raise SystemExit(0 if all(checks.values()) else 1)
