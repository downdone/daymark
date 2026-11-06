"""Verify the vendored update SDK source inventory without network access."""
import hashlib
import json
from pathlib import Path

root = Path(__file__).resolve().parents[1] / 'third_party/update'
inventory = json.loads((root / 'SOURCES.json').read_text(encoding='utf-8'))
assert inventory['format'] == 1
count = 0
for package in inventory['packages']:
    assert len(package['commit']) == 40
    assert package['origin'].startswith('https://github.com/')
    for entry in package['files']:
        path = (root / entry['path']).resolve()
        assert root.resolve() in path.parents
        assert path.stat().st_size == entry['bytes'], entry['path']
        assert hashlib.sha256(path.read_bytes()).hexdigest() == entry['sha256'], entry['path']
        count += 1
print(f'PASS: {count} pinned update dependency files, licenses and upstream revisions.')
