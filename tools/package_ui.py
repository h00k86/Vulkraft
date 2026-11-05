#!/usr/bin/env python3
"""Create an explicit source distribution, never sweep the workspace or secrets."""
from pathlib import Path
import argparse
import gzip
import hashlib
import os
import io
import json
import tarfile
from ui_version import read_version
root = Path(__file__).resolve().parents[1]
files = ['tools/finalize_ui_release.py', 'tools/check_ui_package.py', 'tests/ui_release_tools_test.py', 'tools/package_ui.py', 'tools/ui_version.py', 'CHANGELOG.md', 'docs/UI_COMPATIBILITY.md', 'docs/UI_RELEASE.md', 'include/ui/version.h', 'CMakeLists.txt', 'cmake/UIFrameworkConfig.cmake.in',
         'src/ui/ui.c', 'src/ui/ui_buttons.c', 'src/ui/ui_numeric_controls.c', 'src/ui/ui_text_field.c', 'src/ui/ui_state_types_internal.h', 'src/ui/ui_state_navigation_internal.h', 'src/ui/ui_state_render_internal.h', 'src/ui/ui_state_clipboard_internal.h', 'src/ui/ui_state_tooltip_internal.h', 'src/ui/ui_state_frame_internal.h', 'src/ui/ui_state_ids_internal.h', 'src/ui/ui_state_panels_internal.h', 'src/ui/ui_state_buttons_internal.h', 'src/ui/ui_state_modals_internal.h', 'src/ui/ui_state_style_internal.h', 'src/ui/ui_state_menu_internal.h', 'src/ui/ui_state_numeric_internal.h', 'src/ui/ui_state_sliders_internal.h', 'src/ui/ui_state_text_fields_internal.h', 'src/ui/ui_state_dropdowns_internal.h', 'src/ui/ui_state_picker_internal.h', 'src/ui/ui_state_debug_internal.h', 'src/ui/ui_capture.c', 'src/ui/ui_menu.c', 'src/ui/ui_dropdown.c', 'src/ui/ui_modals.c', 'src/ui/ui_capture_internal.h', 'src/ui/ui_overlay_internal.h', 'src/ui/ui_layout.c', 'src/ui/ui_properties.c', 'src/ui/ui_panels.c', 'src/ui/ui_widget_internal.h', 'src/ui/ui_draw_list.c', 'src/ui/ui_context_internal.h',
         'src/ui/ui_renderer_internal.h', 'src/ui/ui_text_edit_internal.h', 'src/ui/ui_text_edit.c', 'src/ui/ui_navigation_internal.h', 'src/ui/ui_navigation.c', 'src/ui/ui_interaction_internal.h', 'src/ui/ui_theme.c', 'src/ui/ui_color_picker.c', 'src/ui/ui_progress.c', 'src/ui/ui_paint_internal.h', 'src/ui/ui_geometry_internal.h', 'src/ui/ui_file_picker.c',
         'src/ui/ui_file_picker_internal.h', 'src/platform/paths.c', 'src/platform/platform_fs.c',
         'include/ui/ui.h', 'include/ui/error.h', 'src/ui/ui_state_errors_internal.h', 'src/ui/ui_error_internal.h', 'include/ui/renderer.h', 'include/ui/draw_list.h', 'include/ui/file_picker.h',
         'include/text/font_types.h', 'include/text/anchor.h', 'include/text/utf8.h',
         'include/input/events.h', 'include/platform/key_codes.h', 'include/platform/paths.h',
         'include/platform/platform_fs.h', 'include/platform/platform_str.h',
         'include/math/types.h', 'include/log/log.h', 'include/backends/ui_sdl2.h',
         'backends/sdl2/ui_sdl2.c', 'backends/sdl2/ui_sdl2_input.c',
         'examples/ui_standalone.c', 'examples/ui_sdl2.c', 'examples/ui_reference_scene.c',
         'examples/ui_reference_scene.h', 'tests/ui_sdl2_test.c', 'tests/ui_state_contract_test.c', 'tests/ui_dropdown_storage_test.c', 'tests/ui_navigation_batch_test.c', 'tests/ui_menu_storage_test.c', 'tests/ui_error_test.c', 'tests/ui_session_test.c', 'tools/ui_benchmark_metrics.h', 'tests/ui_storage_alloc_probe.h', 'tests/ui_layout_modules_test.c', 'tests/ui_overlay_modules_test.c', 'tests/ui_navigation_test.c', 'tests/ui_navigation_module_test.c', 'tests/ui_tabs_splitter_test.c', 'tests/ui_basic_widgets_test.c', 'tests/ui_interaction_test.c', 'tests/ui_color_picker_test.c', 'tests/ui_text_edit_test.c', 'tests/ui_text_edit_module_test.c',
         'assets/fonts/OpenSans-Italic.ttf', 'assets/fonts/OpenSans-OFL.txt',
         'examples/config_editor/example.cfgapp', 'examples/config_editor/document.c', 'examples/config_editor/document.h',
         'examples/config_editor/editor.c', 'examples/config_editor/editor.h',
         'examples/config_editor/view.c', 'examples/config_editor/view.h', 'examples/config_editor/sdl_main.c',
         'tests/config_editor_test.c', 'tests/config_editor_session_test.c', 'tests/config_editor_ui_test.c', 'docs/TABS_SPLITTER.md', 'docs/CONFIG_EDITOR.md', 'docs/CONFIG_EDITOR_VALIDATION.md', 'docs/PROJECT_VALIDATION.md',
         'docs/UI_KEYBOARD_NAVIGATION.md', 'docs/UI_TEXT_EDITING.md', 'docs/UI_PACKAGE.md', 'docs/UI_SDL2.md', 'docs/UI_API.md', 'docs/UI_ERRORS.md', 'docs/UI_INPUT_CAPTURE.md', 'docs/UI_INTERNAL_ARCHITECTURE.md']

def package(output_dir, epoch):
    if not 0 <= epoch <= 0xffffffff:
        raise ValueError('SOURCE_DATE_EPOCH must fit an unsigned 32-bit gzip timestamp')
    if len(files) != len(set(files)):
        raise ValueError('Duplicate source distribution path')
    version = read_version(root)
    prefix = f'ui-framework-{version}'
    # Read each file once: hashes and archive bytes describe the same snapshot.
    sources = {name: (root/name).read_bytes() for name in sorted(files)}
    manifest = {name: hashlib.sha256(data).hexdigest() for name, data in sources.items()}
    manifest_bytes = (json.dumps(manifest, indent=2, sort_keys=True)+'\n').encode()
    members = dict(sources, **{'manifest-sha256.json': manifest_bytes})
    output_dir.mkdir(parents=True, exist_ok=True)
    output = output_dir / f'{prefix}-source.tar.gz'
    with output.open('wb') as raw:
        with gzip.GzipFile(filename='', fileobj=raw, mode='wb', compresslevel=9, mtime=epoch) as zipped:
            with tarfile.open(fileobj=zipped, mode='w', format=tarfile.USTAR_FORMAT) as archive:
                for name, data in sorted(members.items()):
                    info = tarfile.TarInfo(f'{prefix}/{name}')
                    info.size = len(data)
                    info.mode = 0o644
                    info.mtime = epoch
                    info.uid = info.gid = 0
                    info.uname = info.gname = ''
                    archive.addfile(info, io.BytesIO(data))
    (output_dir/f'{prefix}-manifest-sha256.json').write_bytes(manifest_bytes)
    digest = hashlib.sha256(output.read_bytes()).hexdigest()
    output.with_suffix(output.suffix+'.sha256').write_text(f'{digest}  {output.name}\n')
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir', type=Path, default=root/'build')
    args = parser.parse_args()
    try:
        epoch = int(os.environ.get('SOURCE_DATE_EPOCH', '0'))
        print(package(args.output_dir.resolve(), epoch))
    except (ValueError, OSError) as error:
        parser.exit(1, f'Package failed: {error}\n')


if __name__ == '__main__':
    main()
