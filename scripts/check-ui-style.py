"""Guard shared typography and icon vocabulary; no Qt or extra dependencies.

Rendering, interaction and accessibility belong to the existing UI probes.
Run with python (Windows) or python3 (macOS).
"""
from pathlib import Path
import re
import sys

root = Path(__file__).resolve().parents[1]
paths = (root / 'src/ui/style/IconPaths.qml').read_text(encoding='utf-8')
symbols = set(re.findall(r'^\s*(\w+):"M', paths, re.MULTILINE))
aliases = set(re.findall(r'(\w+):"\w+"', paths.split('const aliases=')[1]))
errors = []
files = list((root / 'src/ui').rglob('*.qml'))
for path in files:
    source = path.read_text(encoding='utf-8')
    for number, line in enumerate(source.splitlines(), 1):
        # Font roles may select between tokens, but must not invent local sizes.
        for expression in re.findall(r'font\.pixelSize\s*:\s*([^;\r\n}]+)', line):
            values = expression.split('?')[-1]  # Numeric layout conditions are fine.
            if re.search(r'\b\d+\b', values) or re.search(r'(?:Size|Tokens\.\w+)\s*[+-]', values):
                errors.append(f'{path.relative_to(root)}:{number}: local font size {expression}')
        if re.search(r'font\.bold\s*:', line):
            errors.append(f'{path.relative_to(root)}:{number}: use explicit semantic weight')
    used = re.findall(r'\bsymbol\s*:\s*"(\w+)"', source)
    used += re.findall(r'(?:AppIcon|Glyph)\s*\{[^{}]*?\bname\s*:\s*"(\w+)"', source)
    for symbol in used:
        if symbol not in symbols | aliases:
            errors.append(f'{path.relative_to(root)}: undefined symbol {symbol}')

if errors:
    print('\n'.join(errors))
    sys.exit(1)
print(f'PASS: {len(files)} QML files share font roles and {len(symbols)} original vector symbols ({len(aliases)} aliases).')
