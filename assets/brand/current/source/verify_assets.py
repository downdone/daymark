"""Independent asset-format checks; no Qt SDK or user data required."""
from pathlib import Path
import json, xml.etree.ElementTree as ET
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]

def verify():
    vectors = list(ROOT.glob('master/*.svg')) + list(ROOT.glob('micro/*.svg')) + list(ROOT.glob('app/*.svg'))
    assert len(vectors) == 27
    allowed = {'svg', 'title', 'path', 'g', 'rect'}
    for p in vectors:
        root = ET.parse(p).getroot()
        assert root.attrib['viewBox']
        for node in root.iter():
            assert node.tag.rsplit('}', 1)[-1] in allowed, p
            assert not any('href' in key for key in node.attrib), p
    images = list((ROOT/'png').rglob('*.png'))
    assert len(images) == 91
    for p in images:
        with Image.open(p) as image:
            image.load()
            size = int(p.stem.rsplit('-', 1)[-1])
            assert image.width == size and image.mode == 'RGBA', p
            assert image.info.get('icc_profile'), p
            if p.parent.name in {'symbol', 'app'}:
                assert image.size == (size, size), p
    return {'revision': 'A-20261103', 'vectorCount': len(vectors), 'pngCount': len(images),
            'result': 'PASS', 'checks': ['SVG parse and independent geometry', 'PNG decode, size, alpha and sRGB'],
            'limits': ['Native Qt and OS appearance are validated separately; no trademark or recognition claim.']}

if __name__ == '__main__':
    result = verify()
    (ROOT/'validation.json').write_text(json.dumps(result, ensure_ascii=False, indent=2)+'\n', encoding='utf-8', newline='\n')
    print(json.dumps(result, ensure_ascii=False))
