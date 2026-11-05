#!/usr/bin/env python3
"""Verify source distribution, install relocation and a separate consumer."""
from pathlib import Path
import argparse
import os
import sys
from ui_version import read_version
import tarfile, tempfile, subprocess, json, hashlib
root=Path(__file__).resolve().parents[1]
version=read_version(root)
prefix=f'ui-framework-{version}'
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output-dir',type=Path,default=root/'evidence/ui-sdl2')
parser.add_argument('--archive-dir',type=Path,default=root/'build')
args=parser.parse_args()
output=args.output_dir.resolve()
archive_dir=args.archive_dir.resolve()
archive_path=archive_dir/f'{prefix}-source.tar.gz'
archive_bytes=archive_path.read_bytes()
archive_hash=hashlib.sha256(archive_bytes).hexdigest()
epoch=int.from_bytes(archive_bytes[4:8], 'little')
output.mkdir(parents=True,exist_ok=True)
work=Path(tempfile.mkdtemp(prefix='ui-package-check-'))
log=(output/'package-verification.txt').open('w')
def run(*args,cwd=None,env=None):
    log.write('COMMAND: '+' '.join(map(str,args))+'\n'); log.flush()
    subprocess.run(list(map(str,args)),cwd=cwd,env=env,stdout=log,stderr=subprocess.STDOUT,check=True)
with tarfile.open(archive_path) as archive:
    members=archive.getmembers()
    assert [m.name for m in members]==sorted(m.name for m in members)
    assert all(m.isfile() and m.mode==0o644 and m.uid==m.gid==0 and not m.uname and not m.gname and m.mtime==epoch for m in members)
    archive.extractall(work,filter='data')
source=work/prefix
manifest_bytes=(source/'manifest-sha256.json').read_bytes()
manifest=json.loads(manifest_bytes)
assert {m.name for m in members}=={f'{prefix}/{name}' for name in manifest}|{f'{prefix}/manifest-sha256.json'}
assert (archive_dir/f'{prefix}-manifest-sha256.json').read_bytes()==manifest_bytes
assert archive_path.with_suffix('.gz.sha256').read_text()==f'{archive_hash}  {archive_path.name}\n'
assert read_version(source)==version
for name,digest in manifest.items():
    assert hashlib.sha256((source/name).read_bytes()).hexdigest()==digest
# Rebuild from extracted sources; then perturb only their metadata and repeat.
repack_env=dict(os.environ, SOURCE_DATE_EPOCH=str(epoch))
for attempt in range(2):
    destination=work/f'repacked-{attempt}'
    run(sys.executable,source/'tools/package_ui.py','--output-dir',destination,env=repack_env)
    assert (destination/archive_path.name).read_bytes()==archive_bytes
    if attempt==0:
        for i,name in enumerate(sorted(manifest)):
            os.utime(source/name,(123456789+i,123456789+i))
            (source/name).chmod(0o755 if i%2 else 0o600)
run('cmake','-S',source,'-B',work/'core','-DCMAKE_BUILD_TYPE=Release')
run('cmake','--build',work/'core','-j4')
run('ctest','--test-dir',work/'core','--output-on-failure')
run('cmake','-S',source,'-B',work/'full','-DCMAKE_BUILD_TYPE=Release','-DUI_WITH_SDL2=ON','-DUI_WITH_PICKER=ON')
run('cmake','--build',work/'full','-j4')
run('ctest','--test-dir',work/'full','--output-on-failure')
run('cmake','--install',work/'full','--prefix',work/'install-original')
(work/'install-original').rename(work/'relocated-sdk')
sdk=work/'relocated-sdk'; consumer=work/'consumer'; consumer.mkdir()
# The full SDK must remain usable by core/picker consumers without SDL discovery.
# Build actual external programs, not just successful configure-only probes.
def probe(name, body, build=False, sources=None):
    directory=work/name; directory.mkdir()
    for filename,contents in (sources or {}).items(): (directory/filename).write_text(contents)
    (directory/'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.20)\nproject(Probe C)\n'+body.replace('@VERSION@',version))
    run('cmake','-S',directory,'-B',directory/'build',f'-DCMAKE_PREFIX_PATH={sdk}',
        '-DCMAKE_DISABLE_FIND_PACKAGE_PkgConfig=TRUE',
        f'-DSDK_EXAMPLES={sdk}/share/ui-framework/examples')
    if build:
        run('cmake','--build',directory/'build','-j4')
        run(directory/'build/probe',cwd=work)


probe('version-api', '''
find_package(UIFramework @VERSION@ EXACT REQUIRED COMPONENTS core)
configure_file(expected.h.in expected.h @ONLY)
add_executable(probe main.c)
target_include_directories(probe PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(probe PRIVATE UIFramework::core)
''', build=True, sources={
    'expected.h.in': '#define EXPECTED_VERSION "@UIFramework_VERSION@"\n',
    'main.c': '''#include "ui/ui.h"
#include "expected.h"
#include <stdio.h>
#include <string.h>
int main(void) {
  if(strcmp(UI_VERSION_STRING,EXPECTED_VERSION)||strcmp(ui_version_string(),UI_VERSION_STRING)) return 1;
  UIContext *ctx=ui_context_create();
  if(!ctx || !ui_context_set_current(ctx)) return 2;
  UIColorPickerOptions options={.integer_channels=true};
  UIColor color={0}; bool started=true;
  if(ui_property_color_ex(NULL,"color",NULL,"Color",&color,&options,&started)||started) return 8;
  options.mode=UI_COLOR_HSV;
  UIColorRGBA rgba={.a=1};
  if(ui_property_color_rgba(NULL,"rgba",NULL,"Color",&rgba,&options,&started)||started) return 9;
  UIIcon icon={.kind=UI_ICON_LOCK}; int selected=-1;
  if(ui_icon_button("icon",NULL,(UIRect){0},&icon,NULL,NULL)) return 10;
  if(ui_list("list",NULL,(UIRect){0},NULL,0,&selected).activated_index!=-1) return 11;
  if(ui_tree("tree",NULL,(UIRect){0},NULL,0,&selected).activated_index!=-1) return 12;
  UITextInputOptions input={.placeholder="Search"};
  if(ui_text_input_ex("search",NULL,ANCHOR_TOP_LEFT,0,14,0,0,100,NULL,0,&input)) return 13;
  UIColorHSV hsv=ui_color_to_hsv((UIColor){1,0,0});
  if(hsv.h!=0||hsv.s!=1||hsv.v!=1||ui_color_from_hsv(hsv).r!=1) return 14;
  char label[32];
  ui_humanize_label("FX",label,sizeof(label));
  if(strcmp(label,"FX")) return 3;
  ui_set_sentence_case(true);
  static const char *const acronyms[]={"FX","VFX"};
  if(!ui_set_sentence_case_exceptions(acronyms,2)) return 6;
  ui_humanize_label("MOSTRA FX/VFX",label,sizeof(label));
  if(strcmp(label,"Mostra FX/VFX")) return 7;
  ui_humanize_label("NUOVO LAYER",label,sizeof(label));
  if(strcmp(label,"Nuovo layer") || !ui_context_reset(ctx)) return 4;
  ui_humanize_label("FX",label,sizeof(label));
  if(strcmp(label,"FX") || !ui_context_destroy(ctx)) return 5;
  printf("SDK header/runtime/CMake version and label policy: %s\\n",ui_version_string());
  return 0;
}
'''
})
components=[int(part) for part in version.split('.')]
for label,requested in [('patch',f'{components[0]}.{components[1]}.{components[2]+1}'),
                        ('minor',f'{components[0]}.{components[1]+1}.0')]:
    probe('reject-version-'+label, f'''
find_package(UIFramework {requested} EXACT QUIET COMPONENTS core)
if(UIFramework_FOUND)
  message(FATAL_ERROR "Different SDK version accepted")
endif()
find_package(UIFramework @VERSION@ EXACT REQUIRED COMPONENTS core)
''')

probe('core-without-sdl', '''
find_package(UIFramework @VERSION@ EXACT REQUIRED COMPONENTS core)
if(TARGET UIFramework::sdl2 OR TARGET UIFramework::picker)
  message(FATAL_ERROR "Unrequested optional target imported")
endif()
find_package(UIFramework REQUIRED COMPONENTS picker)
if(NOT TARGET UIFramework::picker)
  message(FATAL_ERROR "Repeated discovery did not load picker")
endif()
add_executable(probe "${SDK_EXAMPLES}/ui_standalone.c")
target_link_libraries(probe PRIVATE UIFramework::core UIFramework::picker)
''', build=True)
probe('optional-sdl-unavailable', '''
find_package(UIFramework REQUIRED COMPONENTS core OPTIONAL_COMPONENTS sdl2)
if(UIFramework_sdl2_FOUND OR TARGET UIFramework::sdl2)
  message(FATAL_ERROR "Unavailable optional SDL component accepted")
endif()
''')
probe('required-sdl-unavailable', '''
find_package(UIFramework QUIET COMPONENTS sdl2)
if(UIFramework_FOUND OR TARGET UIFramework::sdl2)
  message(FATAL_ERROR "Missing required SDL dependency accepted")
endif()
find_package(UIFramework REQUIRED COMPONENTS core)
if(NOT UIFramework_FOUND)
  message(FATAL_ERROR "Failed lookup poisoned later core discovery")
endif()
''')
probe('unknown-component', '''
find_package(UIFramework QUIET COMPONENTS unknown)
if(UIFramework_FOUND)
  message(FATAL_ERROR "Unknown required component accepted")
endif()
find_package(UIFramework REQUIRED COMPONENTS core OPTIONAL_COMPONENTS unknown)
''')
probe('default-components-unavailable', '''
find_package(UIFramework QUIET)
if(UIFramework_FOUND)
  message(FATAL_ERROR "Default lookup silently omitted installed SDL backend")
endif()
''')
(consumer/'CMakeLists.txt').write_text('''cmake_minimum_required(VERSION 3.20)
project(Consumer C)
find_package(UIFramework @VERSION@ EXACT REQUIRED COMPONENTS core picker sdl2)
add_executable(consumer "${SDK_EXAMPLES}/ui_sdl2.c" "${SDK_EXAMPLES}/ui_reference_scene.c")
target_link_libraries(consumer PRIVATE UIFramework::picker UIFramework::sdl2)
add_executable(config_editor "${SDK_EXAMPLES}/config_editor/sdl_main.c" "${SDK_EXAMPLES}/config_editor/document.c" "${SDK_EXAMPLES}/config_editor/editor.c" "${SDK_EXAMPLES}/config_editor/view.c")
target_link_libraries(config_editor PRIVATE UIFramework::picker UIFramework::sdl2)
'''.replace('@VERSION@',version))
run('cmake','-S',consumer,'-B',work/'consumer-build',f'-DCMAKE_PREFIX_PATH={sdk}',f'-DSDK_EXAMPLES={sdk}/share/ui-framework/examples')
run('cmake','--build',work/'consumer-build','-j4')
import os
env=dict(os.environ,SDL_VIDEODRIVER='dummy')
run(work/'consumer-build/consumer','--smoke','--font',sdk/'share/ui-framework/fonts/OpenSans-Italic.ttf','--root',work,cwd=work,env=env)
run(work/'consumer-build/config_editor','--smoke','--font',sdk/'share/ui-framework/fonts/OpenSans-Italic.ttf','--root',work,cwd=work,env=env)
log.write('PASS: reproducible source bytes after metadata changes, CMake/header/runtime version, incompatible version rejection, source hashes, isolated core/full builds, tests, install relocation, component isolation/optional/missing/repeated discovery and external consumers\n'); log.close()
(output/'package-verification.json').write_text(json.dumps({'work_directory':str(work),'version':version,'archive_sha256':archive_hash,'manifest_sha256':hashlib.sha256(manifest_bytes).hexdigest(),'reproducible':True,'header_runtime_cmake_agree':True,'different_versions_rejected':True,'files':len(manifest),'result':'PASS'},indent=2)+'\n')
print('Package verification PASS:',work)
