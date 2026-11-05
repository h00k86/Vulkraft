#!/usr/bin/env python3
"""Check Vulkraft module boundaries and linked UI test isolation."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
issues = []
# Domain-store access is explicit; only the context owner sees its layout.
ui_state_access = {'ui_buttons.c': {'frame', 'navigation', 'buttons', 'tooltip'},
 'ui_capture.c': {'buttons',
                  'debug',
                  'dropdowns',
                  'frame',
                  'menu',
                  'modals',
                  'navigation',
                  'numeric',
                  'panels',
                  'picker',
                  'sliders',
                  'text_fields'},
 'ui_collections.c': {'buttons', 'panels', 'frame'},
 'ui_color_controls.c': {'colors', 'sliders', 'frame'},
 'ui_dropdown.c': {'frame', 'navigation', 'dropdowns'},
 'ui_file_picker.c': {'render', 'picker'},
 'ui_menu.c': {'frame', 'menu'},
 'ui_modals.c': {'frame', 'navigation', 'modals'},
 'ui_navigation.c': {'dropdowns',
                     'frame',
                     'menu',
                     'modals',
                     'navigation',
                     'numeric',
                     'panels',
                     'text_fields'},
 'ui_numeric_controls.c': {'frame', 'sliders', 'navigation', 'numeric'},
 'ui_panels.c': {'frame', 'navigation', 'ids', 'panels'},
 'ui_text_field.c': {'frame', 'text_fields', 'navigation', 'clipboard'}}
for subtree in ('src', 'include', 'examples', 'backends'):
    for path in (root / subtree).rglob('*'):
        if path.suffix not in ('.c', '.h') or path.name == 'stb_image.h':
            continue
        rel = path.relative_to(root).as_posix()
        text = path.read_text()
        code = re.sub(r'/\*[\s\S]*?\*/|//[^\n]*', '', text)
        if not rel.startswith(('src/platform/backends/glfw', 'src/render/backends/glfw')):
            if re.search(r'\bGLFW\w*|\bglfw\w*', code):
                issues.append(f'{rel}: GLFW dependency outside selected backend')
        if (re.search(r'#\s*include\s*[<"][^>"\n]*SDL|\bSDL_\w+', code)
                and not rel.startswith(('src/platform/backends/sdl2', 'src/render/backends/sdl2',
                                        'backends/sdl2/', 'include/backends/'))
                and rel not in ('examples/ui_sdl2.c', 'examples/config_editor/sdl_main.c')):
            issues.append(f'{rel}: SDL dependency outside selected backend')
        if not rel.startswith(('src/render/', 'include/render/')):
            if re.search(r'#\s*include\s*[<"][^>"\n]*(?:vulkan|gfx_vulkan)', code):
                issues.append(f'{rel}: backend header above rendering boundary')
            if re.search(r'\bVk[A-Z]\w*\b|\bvk[A-Z]\w*\s*\(', code):
                issues.append(f'{rel}: Vulkan API above rendering boundary')
        if (rel.startswith(('src/ui/', 'include/ui/')) and
                rel not in ('src/ui/ui_app_adapter.c', 'include/ui/app_adapter.h')):
            if re.search(r'\bAPP\b', code):
                issues.append(f'{rel}: application type outside UI app adapter')
            if re.search(r'#\s*include\s*[<"]render/', code):
                issues.append(f'{rel}: concrete render header in portable UI')
        if rel.startswith(('backends/sdl2/', 'include/backends/')) or rel in ('examples/ui_sdl2.c', 'examples/ui_reference_scene.c', 'examples/config_editor/view.c', 'examples/config_editor/view.h', 'examples/config_editor/document.c', 'examples/config_editor/document.h', 'examples/config_editor/editor.c', 'examples/config_editor/editor.h', 'examples/config_editor/sdl_main.c'):
            if re.search(r'\bAPP\b|#\s*include\s*[<"](?:app/|render/|[^">]*_internal\.h)', code):
                issues.append(f'{rel}: standalone backend/scene depends on app or private implementation')
        if rel.startswith(('src/ui/', 'include/ui/')) and re.search(r'#\s*include\s*[<"](?:SDL|backends/)', code):
            issues.append(f'{rel}: concrete SDL backend in UI core')
        if rel == 'examples/ui_standalone.c':
            includes = re.findall(r'#\s*include\s*[<"]([^>"]+)', code)
            if any(name.endswith('.c') or '..' in name or
                   name.endswith(('_internal.h', '_private.h')) or
                   name.startswith(('app/', 'input/keyboard', 'platform/window',
                                    'render/'))
                   for name in includes):
                issues.append(f'{rel}: standalone consumer must use only public UI APIs')
            if re.search(r'\bAPP\b|\bui_ctx\s*\(', code):
                issues.append(f'{rel}: application or private context in standalone consumer')
        if rel.startswith('src/ui/') and not path.name.startswith(('ui_file_picker', 'ui_app_adapter')):
            if re.search(r'#\s*include\s*"platform/(?:paths|platform_fs)\.h"|\b(?:paths_\w+|platform_(?:dir_\w+|file_\w+|real_path|resolve_within)|fopen|fclose|fgets)\s*\(', code):
                issues.append(f'{rel}: filesystem dependency in UI core')
        if rel.startswith('src/ui/') and re.search(r'#\s*include\s*[<"][^>"\n]+\.c[>"]', code):
            issues.append(f'{rel}: UI implementation must be compiled separately')
        if rel.startswith('src/ui/'):
            if rel != 'src/ui/ui.c' and re.search(r'#\s*include\s*"ui_context_internal\.h"', code):
                issues.append(f'{rel}: physical context layout outside its owner')
            if rel != 'src/ui/ui_context_internal.h' and re.search(r'\bstruct\s+UIContext\s*{', code):
                issues.append(f'{rel}: duplicate physical context layout')
            if path.suffix == '.c' and rel != 'src/ui/ui.c':
                accessed = set(re.findall(r'\bui_state_(\w+?)\s*\(', code))
                imported = set(re.findall(r'#\s*include\s*"ui_state_(\w+)_internal\.h"', code)) - {'types'}
                accessed = {name.removesuffix('_read') for name in accessed}
                extra = (accessed | imported) - ui_state_access.get(path.name, set())
                if extra:
                    issues.append(f'{rel}: undeclared state domains: {sorted(extra)}')
        if rel.startswith('src/ui/') and rel != 'src/ui/ui_capture.c':
            if re.search(r'->g_click_claimed_by_(?:menu|dropdown_list)\s*=(?!=)|->g_escape_consumed\s*=\s*true', code):
                issues.append(f'{rel}: overlay claim/escape consumption outside capture policy')
        if rel == 'src/ui/ui_capture.c':
            if re.search(r'ui_paint_internal\.h|\bui_paint_\w+\s*\(', code):
                issues.append(f'{rel}: rendering dependency in input capture policy')
        if rel in ('src/ui/ui_navigation.c', 'src/ui/ui_text_edit.c'):
            if re.search(r'\b(?:ui_ctx|ui_context_get_current|ui_paint_\w+|ui_get_theme)\s*\(', code):
                issues.append(f'{rel}: input module depends on implicit context or rendering')
        if rel in ('src/ui/ui_text_edit.c', 'src/ui/ui_text_edit_internal.h', 'src/ui/ui_theme.c', 'src/ui/ui_color_picker.c', 'src/ui/ui_progress.c', 'src/ui/ui_layout.c', 'src/ui/ui_properties.c'):
            if re.search(r'ui_context_internal\.h|\bui_ctx\s*\(|\bUIContext\b', code):
                issues.append(f'{rel}: UI module accesses context internals')
        if rel in ('src/ui/ui.c', 'src/ui/ui_buttons.c', 'src/ui/ui_numeric_controls.c', 'src/ui/ui_text_field.c', 'src/ui/ui_capture.c', 'src/ui/ui_menu.c', 'src/ui/ui_dropdown.c', 'src/ui/ui_modals.c', 'src/ui/ui_panels.c', 'src/ui/ui_navigation.c', 'src/ui/ui_text_edit.c', 'src/ui/ui_theme.c', 'src/ui/ui_color_picker.c', 'src/ui/ui_progress.c', 'src/ui/ui_layout.c', 'src/ui/ui_properties.c'):
            if re.search(r'#\s*include\s*"render/(?:text_renderer|texture_manager)\.h"|(?<![.\w])(?:draw_quad(?:_alpha)?|draw_text_colored|draw_textured_quad|draw_hud_paper_panel|draw_hud_trash_swipe|measure_text_width|text_clip_\w+|texture_manager_\w+)\s*\(', code):
                issues.append(f'{rel}: concrete rendering dependency in UI core')
            if re.search(r'#\s*include\s*"(?:app/application|platform/window|input/keyboard)\.h"|\binput_keyboard_\w+\s*\(|\bplatform_key_state\s*\(|\bapp\s*->|\bg_virtual(?:Width|Height)\b|\bVIRTUAL_[WH]\b', code):
                issues.append(f'{rel}: window/application input dependency in UI core')
        if re.search(r'#\s*include\s*"(?:game|editor|audio|particles|mesh|vfx)/', code):
            issues.append(f'{rel}: dependency outside the application framework')
        if re.search(r'\bGAME\b|\bGAME_STATE_\w+\b', code):
            issues.append(f'{rel}: domain-specific state in generic application')

# Test fixtures may include other fixtures, but linked suites must not pull in ui.c.
def test_includes_core(path, seen=None):
    seen = set() if seen is None else seen
    path = path.resolve()
    if path in seen or not path.is_file():
        return False
    seen.add(path)
    if path == root / 'src/ui/ui.c':
        return True
    for name in re.findall(r'#\s*include\s*"([^"\n]+)"', path.read_text()):
        child = path.parent / name
        if child.is_file() and test_includes_core(child, seen):
            return True
    return False

for name in ('ui_navigation_test', 'ui_interaction_test', 'ui_tabs_splitter_test',
             'ui_basic_widgets_test', 'ui_text_edit_test', 'ui_text_edit_app_test',
             'ui_renderer_test', 'ui_frame_input_test', 'ui_draw_list_test',
             'config_editor_ui_test', 'config_editor_session_test', 'ui_color_picker_test', 'ui_state_contract_test',
             'ui_dropdown_storage_test', 'ui_navigation_batch_test', 'ui_menu_storage_test', 'ui_error_test', 'ui_session_test'):
    if test_includes_core(root / 'tests' / (name + '.c')):
        issues.append(f'tests/{name}.c: linked test includes core implementation')

for issue in issues:
    print('FAIL:', issue)
if issues:
    raise SystemExit(1)
print('Architecture boundary: PASS')
