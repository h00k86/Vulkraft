#!/usr/bin/env python3
"""Architectural check; --extraction also verifies the original function whitelist."""
from pathlib import Path
import json
import re
import sys

root = Path(__file__).resolve().parents[1]
issues = []
for subtree in ('src', 'include', 'examples'):
    for path in (root / subtree).rglob('*'):
        if path.suffix not in ('.c', '.h') or path.name == 'stb_image.h':
            continue
        rel = path.relative_to(root).as_posix()
        text = path.read_text()
        code = re.sub(r'/\*[\s\S]*?\*/|//[^\n]*', '', text)
        if not rel.startswith(('src/render/', 'include/render/')):
            if re.search(r'#\s*include\s*[<"][^>"\n]*(?:vulkan|gfx_vulkan)', code):
                issues.append(f'{rel}: backend header above rendering boundary')
            if re.search(r'\bVk[A-Z]\w*\b|\bvk[A-Z]\w*\s*\(', code):
                issues.append(f'{rel}: Vulkan API above rendering boundary')
        if re.search(r'#\s*include\s*"(?:game|editor|audio|particles|mesh|vfx)/', code):
            issues.append(f'{rel}: dependency on omitted engine module')
        if re.search(r'\bGAME\b|\bGAME_STATE_\w+\b', code):
            issues.append(f'{rel}: gameplay state in generic application')

if '--extraction' in sys.argv:
    manifest = json.loads((root / 'docs/extraction-manifest.json').read_text())
    allowed = {(row['file'], row['function']) for row in manifest['functions']}
    pattern = re.compile(r'(?m)^(?:(?:static|inline|extern|const|unsigned|signed)\s+)*[A-Za-z_]\w*(?:\s+|\s*\*+\s*)([A-Za-z_]\w*)\s*\([^;{}]*?\)\s*\{')
    count = 0
    for subtree in ('src', 'include', 'examples', 'tests', 'shaders'):
        for path in (root / subtree).rglob('*'):
            if path.suffix not in ('.c', '.h', '.vert', '.frag') or path.name == 'stb_image.h':
                continue
            text = path.read_text()
            code = re.sub(r'/\*[\s\S]*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'', lambda m: ''.join('\n' if c == '\n' else ' ' for c in m[0]), text)
            rel = path.relative_to(root).as_posix()
            for found in pattern.finditer(code):
                count += 1
                if (rel, found[1]) not in allowed:
                    issues.append(f'{rel}: {found[1]} has no original definition in extraction manifest')
    print(f'Extraction provenance: {count} definitions inspected')

for issue in issues:
    print('FAIL:', issue)
if issues:
    raise SystemExit(1)
print('Architecture boundary: PASS')
