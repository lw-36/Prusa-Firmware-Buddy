import os
import re
from pathlib import Path
from collections import namedtuple

project_root = Path(__file__).resolve().parent.parent.parent
component_def_re = re.compile(r'LOG_COMPONENT_DEF\(([^,]+),\s*(\S+)\)')
ComponentDefintion = namedtuple(
    'ComponentDefintion',
    ['file_path', 'line_number', 'component_name', 'lowest_severity'])


def scan_file(file_path: Path):
    # A handful of vendored files aren't valid UTF-8 (e.g. a Latin-1 copyright symbol in an ST
    # HAL template); replace what can't be decoded rather than failing the whole scan, since
    # they're not going to contain a LOG_COMPONENT_DEF anyway.
    with open(file_path, 'r', encoding='utf-8', errors='replace') as f:
        for line_idx, line in enumerate(f.readlines()):
            match = component_def_re.match(line)
            if not match:
                continue
            yield ComponentDefintion(file_path=file_path,
                                     line_number=line_idx + 1,
                                     component_name=match.group(1),
                                     lowest_severity=match.group(2))


def scan_directory(directory: Path):
    for root, dirs, files in os.walk(directory):
        for file_name in files:
            file_path = Path(os.path.join(root, file_name))
            if file_path.suffix not in ['.c', '.cpp']:
                continue
            yield from scan_file(file_path)


def scan_project():
    yield from scan_directory(project_root / 'src')
    yield from scan_directory(project_root / 'lib')


if __name__ == "__main__":
    components = list(scan_project())
    doc = [
        '# Logging - Defined Components',
        'This file is generated automatically so don\'t edit it directly',
        '',
    ]
    for component in sorted(components,
                            key=lambda c: c.component_name + str(c.file_path)):
        component_def_path = component.file_path.relative_to(
            project_root).as_posix()
        doc += [
            f'- {component.component_name}: {component.lowest_severity}, {component_def_path}'
        ]
    doc_file = project_root / 'doc' / 'logging_components.md'
    with open(doc_file, 'w', encoding='utf-8') as f:
        f.writelines(l + '\n' for l in doc)
