"""Read the SDK's authoritative version header without a C toolchain."""
from pathlib import Path
import re


def read_version(root):
    header = (Path(root) / 'include/ui/version.h').read_text()
    components = []
    for name in ('MAJOR', 'MINOR', 'PATCH'):
        values = re.findall(r'^#define UI_VERSION_' + name + r' (0|[1-9][0-9]*)$', header, re.MULTILINE)
        if len(values) != 1:
            raise ValueError(f'Expected one decimal UI_VERSION_{name} in include/ui/version.h')
        components.append(values[0])
    return '.'.join(components)


if __name__ == '__main__':
    print(read_version(Path(__file__).resolve().parents[1]))
