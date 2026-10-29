#!/usr/bin/env python3
"""Print one GitHub Actions error annotation per failing CTest case.

The Actions job log is not readable without admin rights on some accounts,
but workflow annotations are public. Emitting failing test names as
`::error title=...::` lines keeps CI failures diagnosable from the API.

Usage: report-ctest-failures.py <path-to-ctest-report.xml> [<console-log>]

The JUnit file is preferred; the console log is a fallback so a run that
died before CTest wrote the report still names the failing tests.
"""
import re
import sys
import xml.etree.ElementTree as ET


def from_junit(path):
    try:
        root = ET.parse(path).getroot()
    except (OSError, ET.ParseError):
        return []
    out = []
    for case in root.iter('testcase'):
        node = case.find('failure')
        if node is None:
            node = case.find('error')
        if node is None:
            continue
        first = (node.text or '').strip().splitlines()
        detail = ' - ' + first[0][:200] if first else ''
        out.append('%s%s' % (case.get('name', '?'), detail))
    return out


def from_console(path):
    try:
        text = open(path, encoding='utf-8', errors='replace').read()
    except OSError:
        return []
    names = re.findall(r'^\s*\d+/\d+\s+Test\s+#\d+:\s+(\S+)\s+\.+\*\*\*', text, re.M)
    if not names:
        names = re.findall(r'^\s*(\d+)\s+-\s+(\S+)\s+\(Failed\)', text, re.M)
        names = [n[1] for n in names]
    return names


def failure_context(path):
    """Pull the probe's own failure lines (stage name) out of the console log."""
    try:
        text = open(path, encoding='utf-8', errors='replace').read()
    except OSError:
        return []
    found = []
    for pattern in (
        r'Basic UI smoke failed at .{0,200}',
        r'UX\d+ stage \d+:[^\n]{0,300}',
        r'Verification exited before all scenarios completed.{0,80}',
        r'Compact insets.{0,300}',
        r'Stale completion state.{0,300}',
        r'List title axes.{0,300}',
        r'Wheel geometry.{0,300}',
        r'Desktop layout.{0,300}',
        r'Menu state.{0,300}',
        r'Mini menu state.{0,300}',
        r'Registration state.{0,300}',
        r'Drag reorder index.{0,200}',
        r'Completion check at phase.{0,120}',
        r'Incubation still pending.{0,120}',
        r'Drawer drop.{0,300}',
    ):
        found.extend(m.strip() for m in re.findall(pattern, text))
    return found[:20]


def main(argv):
    if len(argv) < 2:
        return 0
    failures = from_junit(argv[1])
    if not failures and len(argv) > 2:
        failures = from_console(argv[2])
    for name in failures:
        print('::error title=CTest failure::%s' % name)
    if len(argv) > 2:
        for line in failure_context(argv[2]):
            print('::error title=CTest detail::%s' % line)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
