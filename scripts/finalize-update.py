"""Bind a release draft to actual GitHub upload metadata and sign it. No upload."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def finalize(draft: Path, asset_file: Path, release_file: Path, signer: Path, key: Path) -> Path:
    manifest = json.loads(draft.read_text(encoding='utf-8'))
    asset = json.loads(asset_file.read_text(encoding='utf-8'))
    release = json.loads(release_file.read_text(encoding='utf-8'))
    package = manifest['package']
    archive = draft.parent / package['name']
    with archive.open('rb') as stream:
        checksum = hashlib.file_digest(stream, 'sha256').hexdigest()
    if checksum != package['sha256'] or archive.stat().st_size != package['size']:
        raise ValueError('local archive no longer matches draft')
    if release.get('tag_name') != manifest['tag'] or release.get('draft') is not False or release.get('prerelease') is not False:
        raise ValueError('not the expected public stable release')
    if asset.get('name') != package['name'] or asset.get('state') != 'uploaded' or asset.get('size') != package['size']:
        raise ValueError('GitHub asset does not match this package')
    if asset.get('digest') != 'sha256:' + checksum:
        raise ValueError('GitHub digest does not match local signed candidate')
    for entry in (release, asset):
        if isinstance(entry.get('id'), bool) or not isinstance(entry.get('id'), int) or entry['id'] <= 0:
            raise ValueError('invalid GitHub identity')
    expected_url = 'https://api.github.com/repos/' + manifest['repository'] + '/releases/assets/' + str(asset['id'])
    if asset.get('url') != expected_url:
        raise ValueError('asset repository identity mismatch')
    expected_download = 'https://github.com/' + manifest['repository'] + '/releases/download/' + manifest['tag'] + '/' + package['name']
    if asset.get('browser_download_url') != expected_download or release.get('url') != 'https://api.github.com/repos/' + manifest['repository'] + '/releases/' + str(release['id']):
        raise ValueError('asset release identity mismatch')
    manifest['release_id'] = str(release['id'])
    package['asset_id'] = str(asset['id'])
    destination = draft.with_name('update-windows-x64.json')
    if destination.exists() or destination.with_suffix('.json.sig').exists():
        raise ValueError('refusing to replace a finalized manifest')
    destination.write_text(json.dumps(manifest, ensure_ascii=False, sort_keys=True, separators=(',', ':')), encoding='utf-8')
    try:
        subprocess.run([str(signer.resolve()), 'sign', str(key.resolve()), str(destination)], check=True, timeout=30)
    except BaseException:
        destination.unlink(missing_ok=True)
        destination.with_suffix('.json.sig').unlink(missing_ok=True)
        raise
    return destination


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('draft', 'asset', 'release', 'signer', 'key'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    print(finalize(args.draft, args.asset, args.release, args.signer, args.key))
