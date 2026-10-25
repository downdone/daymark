"""Audit semantic color pairs in DesignTokens.qml (Python 3, no dependencies)."""
from pathlib import Path
import json
import re
import sys

source = (Path(__file__).resolve().parents[1] / 'src/ui/style/DesignTokens.qml').read_text(encoding='utf-8')
expressions = dict(re.findall(r'readonly property color (\w+): ([^\r\n]+)', source))

def luminance(color):
    components = [int(color[index:index + 2], 16) / 255 for index in (1, 3, 5)]
    linear = [value / 12.92 if value <= .04045 else ((value + .055) / 1.055) ** 2.4 for value in components]
    return sum(value * weight for value, weight in zip(linear, (.2126, .7152, .0722)))

def contrast(first, second):
    low, high = sorted((luminance(first), luminance(second)))
    return (high + .05) / (low + .05)

report = {}
for increased in (False, True):
    for dark in (False, True):
        values = {}
        for name, expression in expressions.items():
            colors = re.findall(r'#[0-9a-fA-F]{6}', expression)
            expected = 4 if expression.startswith('highContrast ?') else 2
            if len(colors) != expected or 'dark ?' not in expression:
                raise ValueError(f'Unsupported token expression: {name}; update the audit parser.')
            index = (0 if dark else 1) + (0 if increased or expected == 2 else 2)
            values[name] = colors[index]
        checks = []
        backgrounds = ('surface', 'sidebar', 'grouped', 'hover', 'pressed', 'selected', 'fieldFill')
        for foreground in ('text', 'secondary', 'accent', 'danger', 'success', 'categoryOrange', 'categoryPurple'):
            checks.extend((f'{foreground}/{background}', contrast(values[foreground], values[background]), 4.5) for background in backgrounds)
        checks.extend((f'{foreground}/{background}', contrast(values[foreground], values[background]), 4.5) for foreground, background in (('accentText', 'accent'), ('calendarTodayText', 'calendarToday'), ('primaryInk', 'primaryFill'), ('scheduleInk', 'scheduleFill'), ('danger', 'primaryFill'), ('accentText', 'danger'), ('accentText', 'secondary'), ('accentText', 'categoryOrange')))
        checks.extend((f'toggleOn/{background}', contrast(values['toggleOn'], values[background]), 3) for background in backgrounds)
        for foreground in ('outline', 'fieldLine'):
            checks.extend((f'{foreground}/{background}', contrast(values[foreground], values[background]), 3) for background in backgrounds)
        report[f'{"dark" if dark else "light"}-contrast-{increased}'] = {
            'pairs': len(checks),
            'minimumText': round(min(value for _, value, target in checks if target == 4.5), 3),
            'minimumBoundary': round(min(value for _, value, target in checks if target == 3), 3),
            'failures': [{'pair': name, 'ratio': round(value, 3), 'target': target} for name, value, target in checks if value < target],
        }
print(json.dumps(report, indent=2))
sys.exit(1 if any(item['failures'] for item in report.values()) else 0)
