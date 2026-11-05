#!/usr/bin/env python3
"""Build, relocate and consume the complete SDK outside the source checkout."""
import argparse
from contextlib import nullcontext
import json
import os
import re
from pathlib import Path
import shutil
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]


def validate_editor_workflow(run, case, sdk, editor_source, base, version, work):
    """Reuse the behavioral UI suite against installed libraries, outside ROOT.

    The deterministic renderer/input exercise the full view/controller and real
    filesystem, without desktop input or GPU claims. No framework C is compiled.
    """
    tests = base / 'editor-workflow-consumer'
    tests.mkdir()
    (tests / 'ui_navigation_test.c').write_bytes((ROOT / 'tests/ui_navigation_test.c').read_bytes())
    source = (ROOT / 'tests/config_editor_ui_test.c').read_text()
    source = source.replace('#include "../examples/config_editor/view.h"', '#include "view.h"')
    (tests / 'workflow.c').write_text(source)
    (tests / 'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.20)\n'
        'project(InstalledEditorWorkflow C)\n'
        f'find_package(GraphicsApp {version} EXACT REQUIRED COMPONENTS application)\n'
        'add_executable(editor_workflow workflow.c "${EDITOR_SOURCE_DIR}/document.c" '
        '"${EDITOR_SOURCE_DIR}/editor.c" "${EDITOR_SOURCE_DIR}/view.c")\n'
        'target_include_directories(editor_workflow PRIVATE "${EDITOR_SOURCE_DIR}")\n'
        'target_link_libraries(editor_workflow PRIVATE GraphicsApp::application)\n')
    run(case + '-workflow-configure', ['cmake', '-S', tests, '-B', tests / 'build',
        '-DCMAKE_PREFIX_PATH=' + str(sdk), '-DEDITOR_SOURCE_DIR=' + str(editor_source)], cwd=work)
    run(case + '-workflow-build', ['cmake', '--build', tests / 'build', '-j4'], cwd=work)
    log = run(case + '-workflow', [tests / 'build/editor_workflow'], cwd=work)
    if 'Config editor UI: PASS' not in log:
        raise RuntimeError('Installed SDK editor workflow did not complete')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', action='store_true', help='Open the owned Vulkan sample windows')
    parser.add_argument('--native', action='store_true', help='Also validate the external editor using owned X11/i3 windows (implies --runtime)')
    parser.add_argument('--keep-workdir', action='store_true', help='Keep temporary SDKs/consumers for inspection or a focused native retry')
    parser.add_argument('--output-dir', type=Path, default=ROOT / 'evidence/app-sdk')
    args = parser.parse_args()
    version = re.search(r'^set\(GRAPHICS_APP_VERSION ([0-9]+\.[0-9]+\.[0-9]+)\)$',
        (ROOT / 'cmake/GraphicsApp.cmake').read_text(), re.MULTILINE).group(1)
    if args.native:
        args.runtime = True
    out = args.output_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    report = {'status': 'RUNNING', 'runtime_requested': args.runtime, 'native_requested': args.native,
              'graphics_app_version': version,
              'stages': [], 'consumers': {},
              'not_verified': ['manual visual review', 'native HiDPI/fullscreen', 'Wayland',
                               'other OS/GPU', 'LeakSanitizer', 'Release native input (Debug only)']}
    if not args.native:
        report['not_verified'].append('native editor workflows (use --native)')
    if not args.runtime:
        report['not_verified'].append('Vulkan runtime')

    def save():
        (out / 'report.json').write_text(json.dumps(report, indent=2) + '\n')

    def run(name, command, cwd=None, env=None, expected=0, timeout=240):
        entry = {'name': name, 'command': list(map(str, command)), 'cwd': str(cwd or ROOT),
                 'status': 'RUNNING', 'log': name + '.log'}
        report['stages'].append(entry)
        save()
        print(name + ': running', flush=True)
        start = time.monotonic()
        with (out / entry['log']).open('w') as log:
            try:
                result = subprocess.run(entry['command'], cwd=cwd or ROOT, env=env,
                                        stdout=log, stderr=subprocess.STDOUT, timeout=timeout)
                entry.update(exit_code=result.returncode,
                             status='PASS' if result.returncode == expected else 'FAIL')
            except (OSError, subprocess.TimeoutExpired) as error:
                entry.update(status='FAIL', error=str(error))
        entry['elapsed_seconds'] = round(time.monotonic() - start, 3)
        save()
        print(name + ': ' + entry['status'], flush=True)
        if entry['status'] != 'PASS':
            raise RuntimeError(name + ' failed; see ' + entry['log'])
        return (out / entry['log']).read_text()

    try:
        run('architecture', ['python3', 'tools/check_boundaries.py'])
        run('resource-tests', ['make', '-j4', 'test-app-resources', 'test-app-resources-sanitize'])
        # Both install prefixes and consumer sources/builds are outside the repo.
        workspace = (nullcontext(tempfile.mkdtemp(prefix='graphics-app-sdk-')) if args.keep_workdir
                     else tempfile.TemporaryDirectory(prefix='graphics-app-sdk-'))
        with workspace as temporary:
            work = Path(temporary)
            report['work_directory'] = str(work)
            report['work_directory_retained'] = args.keep_workdir
            for host in ('glfw', 'sdl2'):
                for config in ('Debug', 'Release'):
                    case = host + '-' + config.lower()
                    base = work / case
                    build = base / 'sdk-build'
                    original = base / 'original-sdk'
                    sdk = base / 'relocated-sdk'
                    run(case + '-configure', ['cmake', '-S', ROOT, '-B', build,
                        '-DGRAPHICS_APP_WITH_VULKAN=ON', '-DGRAPHICS_APP_PLATFORM=' + host,
                        '-DCMAKE_BUILD_TYPE=' + config, '-DUI_BUILD_EXAMPLES=OFF'])
                    run(case + '-build', ['cmake', '--build', build, '-j4'])
                    run(case + '-tests', ['ctest', '--test-dir', build, '--output-on-failure'])
                    run(case + '-install', ['cmake', '--install', build, '--prefix', original])
                    original.rename(sdk)
                    for path in sdk.rglob('*.cmake'):
                        contents = path.read_text()
                        if str(ROOT) in contents or str(original) in contents:
                            raise RuntimeError('Non-relocatable installed CMake file: ' + str(path))
                    consumer = base / 'consumer-source'
                    shutil.copytree(sdk / 'share/graphics-app/examples/external_app', consumer)
                    # Multiple app subdirectories may request deployment; ensure
                    # repeating the helper for one target remains harmless.
                    with (consumer / 'CMakeLists.txt').open('a') as cmake:
                        cmake.write('\ngraphics_app_deploy_resources(external_app)\n')
                    external_build = base / 'consumer-build'
                    run(case + '-consumer-configure', ['cmake', '-S', consumer, '-B', external_build,
                        '-DCMAKE_PREFIX_PATH=' + str(sdk), '-DCMAKE_BUILD_TYPE=' + config], cwd=work)
                    run(case + '-consumer-build', ['cmake', '--build', external_build, '-j4'], cwd=work)
                    # A resource-only change must be deployed without relinking.
                    shader = external_build / 'resources/shaders/vert.spv'
                    original_shader = shader.read_bytes()
                    binary_stamp = (external_build / 'external_app').stat().st_mtime_ns
                    shader.write_bytes(b'stale resource fixture')
                    run(case + '-resource-sync', ['cmake', '--build', external_build, '-j4'], cwd=work)
                    if shader.read_bytes() != original_shader or (external_build / 'external_app').stat().st_mtime_ns != binary_stamp:
                        raise RuntimeError('Resource deployment requires a relink or did not refresh assets')

                    editor_source = base / 'editor-source'
                    shutil.copytree(sdk / 'share/graphics-app/examples/config_editor', editor_source)
                    editor_build = base / 'editor-build'
                    run(case + '-editor-configure', ['cmake', '-S', editor_source, '-B', editor_build,
                        '-DCMAKE_PREFIX_PATH=' + str(sdk), '-DCMAKE_BUILD_TYPE=' + config], cwd=work)
                    run(case + '-editor-build', ['cmake', '--build', editor_build, '-j4'], cwd=work)
                    validate_editor_workflow(run, case, sdk, editor_source, base, version, work)
                    editor_bundle = base / 'editor-moved-bundle'
                    editor_bundle.mkdir()
                    shutil.move(editor_build / 'config_editor', editor_bundle / 'config_editor')
                    shutil.move(editor_build / 'resources', editor_bundle / 'resources')

                    # A callback-only executable must not pull the legacy hook,
                    # even when the linker does not discard unused sections.
                    for executable in (external_build / 'external_app', editor_bundle / 'config_editor'):
                        symbols = run(case + '-' + executable.name + '-symbols', ['nm', executable], cwd=work)
                        if re.search(r'\bapp_draw$', symbols, re.MULTILINE):
                            raise RuntimeError('Callback consumer pulls the legacy global app_draw')
                    legacy = base / 'legacy-consumer'
                    legacy.mkdir()
                    (legacy / 'main.c').write_text('#include "app/application.h"\n'
                        'void app_draw(APP *app) { app_request_close(app); }\n'
                        'int main(void) { APP app={0}; app.resources.root_directory="resources";\n'
                        'bool ok=app_init(&app); if(ok) app_loop(&app); app_shutdown(&app);\n'
                        'return !ok || app.error.code; }\n')
                    (legacy / 'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.20)\n'
                        'project(LegacyConsumer C)\n'
                        f'find_package(GraphicsApp {version} EXACT REQUIRED COMPONENTS application)\n'
                        'add_executable(legacy main.c)\n'
                        'target_link_libraries(legacy PRIVATE GraphicsApp::application)\n'
                        'graphics_app_deploy_resources(legacy)\n')
                    run(case + '-legacy-configure', ['cmake', '-S', legacy, '-B', legacy / 'build',
                        '-DCMAKE_PREFIX_PATH=' + str(sdk)], cwd=work)
                    run(case + '-legacy-build', ['cmake', '--build', legacy / 'build', '-j4'], cwd=work)

                    # Include every installed header on its own, with no private
                    # headers, source-tree includes or previous header ordering.
                    headers = base / 'header-consumer'
                    headers.mkdir()
                    header_files = sorted((sdk / 'include').rglob('*.h'))
                    for index, header in enumerate(header_files):
                        (headers / f'header_{index}.c').write_text('#include "' +
                            header.relative_to(sdk / 'include').as_posix() + '"\n')
                    (headers / 'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.20)\n'
                        'project(PublicHeaders C)\n'
                        'find_package(GraphicsApp 0.2.0 EXACT QUIET)\n'
                        'if(GraphicsApp_FOUND)\nmessage(FATAL_ERROR "Previous SDK version accepted")\nendif()\n'
                        f'find_package(GraphicsApp {version} EXACT REQUIRED COMPONENTS application)\n'
                        'file(GLOB sources "${CMAKE_CURRENT_SOURCE_DIR}/header_*.c")\n'
                        'add_library(public_headers OBJECT ${sources})\n'
                        'target_link_libraries(public_headers PRIVATE GraphicsApp::application)\n'
                        'if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")\n'
                        'target_compile_options(public_headers PRIVATE -Wall -Wextra -Werror)\nendif()\n')
                    run(case + '-headers-configure', ['cmake', '-S', headers, '-B', headers / 'build',
                        '-DCMAKE_PREFIX_PATH=' + str(sdk)], cwd=work)
                    run(case + '-headers-build', ['cmake', '--build', headers / 'build', '-j4'], cwd=work)
                    bundle = base / 'moved-bundle'
                    bundle.mkdir()
                    shutil.move(external_build / 'external_app', bundle / 'external_app')
                    shutil.move(external_build / 'resources', bundle / 'resources')
                    # The application must not use the SDK's files at runtime.
                    sdk.rename(base / 'sdk-unavailable-at-runtime')
                    dependencies = run(case + '-links', ['ldd', bundle / 'external_app'], cwd=work)
                    editor_dependencies = run(case + '-editor-links', ['ldd', editor_bundle / 'config_editor'], cwd=work)
                    foreign = 'libSDL' if host == 'glfw' else 'libglfw'
                    selected = 'libglfw' if host == 'glfw' else 'libSDL'
                    if foreign in dependencies or selected not in dependencies or 'not found' in dependencies:
                        raise RuntimeError('Incorrect host dependencies for ' + case)
                    if foreign in editor_dependencies or selected not in editor_dependencies or 'not found' in editor_dependencies:
                        raise RuntimeError('Incorrect editor host dependencies for ' + case)
                    checks = {'callback_consumers_without_global_hook': True, 'legacy_consumer_built': True,
                              'installed_sdk_relocated': True, 'external_consumer_built': True,
                              'consumer_bundle_moved': True, 'host_dependencies_correct': True,
                              'resource_sync_without_relink': True, 'external_editor_built': True,
                              'installed_sdk_editor_workflow': True,
                              'editor_host_dependencies_correct': True,
                              'public_headers_compile_independently': True}
                    report.setdefault('public_headers', [h.relative_to(sdk / 'include').as_posix() for h in header_files])
                    report['consumers'][case] = checks

                    if args.runtime:
                        env = dict(os.environ, VK_LAYER_VALIDATE_SYNC='1')
                        for name in ('NO_VALIDATION', 'APP_AUTOSTART_SECONDS', 'APP_FRAME_DUMP_DIR',
                                     'VK_LAYER_DISABLES', 'VK_LAYER_ENABLES'):
                            env.pop(name, None)
                        run(case + '-legacy-runtime', [legacy / 'build/legacy'], cwd=work, env=env, timeout=40)
                        checks['legacy_runtime'] = True
                        log = run(case + '-runtime', [bundle / 'external_app', '--smoke'],
                                  cwd=work, env=env, timeout=40)
                        checks['runtime_frames'] = 'External application completed: 16 frames' in log
                        checks['runtime_log_clean'] = not any(token in log for token in
                            ('VUID-', 'Validation Error', 'SYNC-HAZARD', '[ERROR]'))
                        if config == 'Debug':
                            checks['validation_enabled'] = 'Vulkan validation attiva' in log
                        if not all(checks.values()):
                            raise RuntimeError('Runtime checks failed for ' + case)

                        # Exercise each resource override at its actual load site.
                        files = [('assets/fonts/font_utf8.json', 'font metadata'),
                                 ('assets/fonts/font_utf8.png', 'font atlas'),
                                 ('shaders/vert.spv', 'pipeline'), ('shaders/frag.spv', 'pipeline')]
                        for index, (relative, stage) in enumerate(files):
                            resource = bundle / 'resources' / relative
                            hidden = resource.with_name(resource.name + '.missing')
                            resource.rename(hidden)
                            try:
                                failure = run(case + '-missing-' + str(index),
                                    [bundle / 'external_app', '--smoke'], cwd=work, env=env,
                                    expected=1, timeout=40)
                                if stage not in failure or any(token in failure for token in
                                    ('VUID-', 'Validation Error', 'SYNC-HAZARD')):
                                    raise RuntimeError('Missing-resource diagnostic/cleanup failed')
                            finally:
                                hidden.rename(resource)
                        checks['missing_resources_rejected_and_cleaned_up'] = True
                        # Exercise the absolute-root configuration independently.
                        custom = base / 'custom-resources'
                        custom.mkdir()
                        shutil.move(bundle / 'resources', custom / 'payload')
                        log = run(case + '-runtime-absolute-root',
                            [bundle / 'external_app', '--smoke', '--resources', custom / 'payload'],
                            cwd=work, env=env, timeout=40)
                        checks['absolute_resource_root'] = ('External application completed: 16 frames' in log
                            and not any(token in log for token in
                                        ('VUID-', 'Validation Error', 'SYNC-HAZARD', '[ERROR]')))
                        if not checks['absolute_resource_root']:
                            raise RuntimeError('Absolute resource root failed')
                        overrides = []
                        flags = ['--font-metadata', '--font-atlas', '--vertex-shader', '--fragment-shader']
                        names = ['custom metadata.json', 'custom atlas.png', 'custom vertex.spv', 'custom fragment.spv']
                        for (relative, _), flag, name in zip(files, flags, names):
                            destination = custom / 'payload' / name
                            (custom / 'payload' / relative).rename(destination)
                            # Include both absolute filenames and filenames rooted at --resources.
                            overrides.extend([flag, str(destination) if flag == '--font-metadata' else name])
                        log = run(case + '-runtime-custom-files',
                            [bundle / 'external_app', '--smoke', '--resources', custom / 'payload', *overrides],
                            cwd=work, env=env, timeout=40)
                        checks['all_resource_overrides'] = ('External application completed: 16 frames' in log
                            and not any(token in log for token in
                                        ('VUID-', 'Validation Error', 'SYNC-HAZARD', '[ERROR]')))
                        if not checks['all_resource_overrides']:
                            raise RuntimeError('Custom resource filenames failed')
                        documents = base / 'editor-documents'
                        documents.mkdir()
                        log = run(case + '-editor-smoke', [editor_bundle / 'config_editor',
                            '--smoke', '--root', documents], cwd=work, env=env, timeout=40)
                        checks['external_editor_smoke'] = ('Config editor Vulkan: PASS' in log
                            and not any(token in log for token in
                                        ('VUID-', 'Validation Error', 'SYNC-HAZARD', '[ERROR]')))
                        if config == 'Debug':
                            checks['editor_validation_enabled'] = 'Vulkan validation attiva' in log
                        if not checks['external_editor_smoke'] or not checks.get('editor_validation_enabled', True):
                            raise RuntimeError('External editor smoke failed')
                        if args.native and config == 'Debug':
                            native_out = out / (host + '-editor-native')
                            run(case + '-editor-native', ['python3', ROOT / 'tools/config_editor_native_validation.py',
                                '--backend', 'vulkan', '--vulkan-platform', host,
                                '--vulkan-binary', editor_bundle / 'config_editor', '--working-directory', work,
                                '--quick-clicks', '--document-history', '--focus-cycle', '--open-picker', '--output-dir', native_out],
                                cwd=work, timeout=240)
                            native_report = json.loads((native_out / 'report-vulkan.json').read_text())
                            native_checks = native_report['backends']['vulkan']
                            checks['external_editor_native'] = ('error' not in native_checks and all(native_checks.values()))
                            if not checks['external_editor_native']:
                                raise RuntimeError('External editor native workflow failed')
                    save()

                    # A UI-only consumer of this installation must need no host/GPU discovery.
                    ui = base / 'ui-consumer'
                    ui.mkdir()
                    (ui / 'main.c').write_text('#include "ui/version.h"\nint main(void) { return !ui_version_string(); }\n')
                    (ui / 'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.20)\n'
                        'project(UICoreOnly C)\nfind_package(UIFramework REQUIRED COMPONENTS core)\n'
                        'add_executable(ui_probe main.c)\ntarget_link_libraries(ui_probe PRIVATE UIFramework::core)\n')
                    moved_sdk = base / 'sdk-unavailable-at-runtime'
                    run(case + '-ui-isolation', ['cmake', '-S', ui, '-B', ui / 'build',
                        '-DCMAKE_PREFIX_PATH=' + str(moved_sdk), '-DCMAKE_DISABLE_FIND_PACKAGE_Vulkan=TRUE',
                        '-DCMAKE_DISABLE_FIND_PACKAGE_glfw3=TRUE', '-DCMAKE_DISABLE_FIND_PACKAGE_PkgConfig=TRUE'])
                    run(case + '-ui-build', ['cmake', '--build', ui / 'build', '-j4'])
                    run(case + '-ui-run', [ui / 'build/ui_probe'], cwd=work)
            report['status'] = 'PASS'
    except (RuntimeError, OSError) as error:
        report.update(status='FAIL', error=str(error))
    save()
    print('GraphicsApp SDK ' + report['status'] + ': ' + str(out / 'report.json'), flush=True)
    return 0 if report['status'] == 'PASS' else 1


if __name__ == '__main__':
    raise SystemExit(main())
