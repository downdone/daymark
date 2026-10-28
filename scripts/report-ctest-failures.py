#!/usr/bin/env python3
"""Print one GitHub Actions error annotation per failing CTest JUnit case.

The Actions job log is not readable without admin rights on some accounts,
but workflow annotations are public. Emitting failing test names as
`::error title=...::` lines keeps CI failures diagnosable from the API.

Usage: report-ctest-failures.py <path-to-ctest-report.xml>
"""
import sys
import xml.etree.ElementTree as ET


def main(argv):
    if len(argv) != 2:
        return 0
    try:
        root = ET.parse(argv[1]).getroot()
    except (OSError, ET.ParseError):
        return 0
    for case in root.iter('testcase'):
        if case.find('failure') is None and case.find('error') is None:
            continue
        detail = ''
        node = case.find('failure')
        if node is None:
            node = case.find('error')
        message = (node.text or '').strip().splitlines()
        if message:
            detail = ' - ' + message[0][:200]
        print('::error title=CTest failure::%s%s' % (case.get('name', '?'), detail))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
