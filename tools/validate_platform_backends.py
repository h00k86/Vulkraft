#!/usr/bin/env python3
"""Compare GLFW/SDL2 application hosts; --runtime opens Vulkan windows."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile
import time

ROOT=Path(__file__).resolve().parents[1]
SCENARIOS={'basic':'APP_REFERENCE_BASIC_SMOKE', 'navigation':'APP_REFERENCE_NAV_SMOKE',
           'editing':'APP_REFERENCE_EDIT_SMOKE', 'layout':'APP_REFERENCE_LAYOUT_SMOKE',
           'color':'APP_REFERENCE_COLOR_SMOKE'}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime',action='store_true',help='Also run app-local synthetic events on the actual Vulkan surface')
    parser.add_argument('--output-dir',type=Path,default=ROOT/'evidence/platform-sdl2')
    args=parser.parse_args()
    out=args.output_dir.resolve(); out.mkdir(parents=True,exist_ok=True)
    report={'scope':'GLFW/SDL2 host comparison; one active application/window', 'runtime_requested':args.runtime,
            'stages':[], 'link_dependencies':{}, 'runtime':{},
            'not_verified':['manual visual session','native HiDPI monitor','native fullscreen modes',
                            'Wayland/other OS/GPU','LeakSanitizer','desktop input/clipboard workflow']}
    if not args.runtime: report['not_verified'].append('Vulkan runtime')
    def save(): (out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    def run(name,command,env=None,timeout=240):
        entry={'name':name,'command':command,'status':'RUNNING','log':name+'.log'}
        report['stages'].append(entry); save(); print(name+': running',flush=True)
        start=time.monotonic()
        with (out/entry['log']).open('w') as log:
            try:
                result=subprocess.run(command,cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=timeout)
                entry.update(exit_code=result.returncode,status='PASS' if result.returncode==0 else 'FAIL')
            except (OSError,subprocess.TimeoutExpired) as error: entry.update(status='FAIL',error=str(error))
        entry['elapsed_seconds']=round(time.monotonic()-start,3); save(); print(name+': '+entry['status'],flush=True)
        if entry['status']!='PASS': raise RuntimeError(name+' failed')
        return (out/entry['log']).read_text()
    try:
        run('architecture',['python3','tools/check_boundaries.py'])
        run('platform-tests',['make','-j4','test-platform','test-platform-glfw','test-platform-sdl2','test-platform-sdl2-sanitize'])
        for platform in ('glfw','sdl2'):
            suffix='' if platform=='glfw' else '-sdl2'
            for build in ('debug','release'):
                run(f'build-{platform}-{build}',['make','-j4',f'PLATFORM={platform}',f'BUILD={build}',f'build/app-{build}{suffix}.out','ui-reference-vulkan','config-editor-vulkan'])
                for app in ('app','config-editor-vulkan','ui-reference-vulkan'):
                    binary=f'build/{app}-{build}{suffix}.out'
                    dependencies=run(f'links-{platform}-{build}-{app}',['ldd',binary])
                    valid='libglfw' not in dependencies if platform=='sdl2' else 'libSDL' not in dependencies
                    report['link_dependencies'][binary]={'status':'PASS' if valid else 'FAIL', 'selected_platform':platform}
                    if not valid: raise RuntimeError('Unexpected window library dependency: '+binary)
        # Compile-only check of the original no-window backend.
        run('build-headless',['make','-j4','PLATFORM=headless','BUILD=debug','build/app-debug-headless.out'])
        if args.runtime:
            env=dict(os.environ,VK_LAYER_VALIDATE_SYNC='1',SDL_VIDEODRIVER='x11')
            for name in ('NO_VALIDATION','VK_LAYER_DISABLES','VK_LAYER_ENABLES','APP_AUTOSTART_SECONDS','APP_FRAME_DUMP_DIR','UI_INPUT_DEBUG',*SCENARIOS.values()): env.pop(name,None)
            with tempfile.TemporaryDirectory(prefix='platform-vulkan-runtime-') as directory:
                for platform in ('glfw','sdl2'):
                    suffix='' if platform=='glfw' else '-sdl2'
                    for scenario in ('editor',*SCENARIOS):
                        case_env=dict(env)
                        if scenario=='editor': command=[f'./build/config-editor-vulkan-debug{suffix}.out','--smoke','--root',directory]
                        else:
                            command=[f'./build/ui-reference-vulkan-debug{suffix}.out']; case_env[SCENARIOS[scenario]]='1'
                        name=f'runtime-{platform}-{scenario}'
                        log=run(name,command,case_env,timeout=40)
                        marker='Config editor Vulkan: PASS' if scenario=='editor' else f'Vulkan {scenario}: PASS'
                        checks={'scenario_pass':marker in log,'validation_enabled':'Vulkan validation attiva' in log,
                                'runtime_log_clean':not any(token in log for token in ('VUID-','Validation Error','SYNC-HAZARD','[ERROR]'))}
                        checks['status']='PASS' if all(checks.values()) else 'FAIL'
                        report['runtime'][f'{platform}-{scenario}']=checks; save()
                        if checks['status']!='PASS': raise RuntimeError(name+' validation failed')
        report['status']='PASS'
    except (RuntimeError,OSError) as error: report.update(status='FAIL',error=str(error))
    save(); print('Platform validation '+report['status']+': '+str(out/'report.json'),flush=True)
    return 0 if report['status']=='PASS' else 1


if __name__=='__main__':
    raise SystemExit(main())
