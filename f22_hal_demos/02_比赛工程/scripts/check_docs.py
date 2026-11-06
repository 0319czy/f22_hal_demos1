#!/usr/bin/env python3
"""Check local Markdown links within this independently portable project."""
from pathlib import Path
import re
from urllib.parse import unquote
root = Path(__file__).resolve().parent.parent
errors = []
count = 0
for source in root.rglob('*.md'):
    if '.pio' in source.parts or 'archive' in source.relative_to(root).parts:
        continue
    for match in re.finditer(r'\[[^\]]*\]\(([^)]+)\)', source.read_text()):
        target = match.group(1).strip().split(' "')[0].strip('<>')
        if target.startswith(('http:', 'https:', 'mailto:', '#')):
            continue
        target = unquote(target.split('#', 1)[0])
        if not target:
            continue
        count += 1
        if not (source.parent / target).exists():
            errors.append(f'{source.relative_to(root)}: {target}')
if errors:
    raise SystemExit('\n'.join(errors))
print(f'Competition Markdown: {count} local links exist')
