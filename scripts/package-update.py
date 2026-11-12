"""Build an immutable, signed managed Windows ZIP from an already validated payload.

No upload, no key generation, no user data discovery. Invoke the native signer using
an argument vector; private key bytes are never read or printed by this tool.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import stat
import subprocess
import tempfile
import zipfile
from update_release_gates import check_gates, verify_deployment_report, verify_executables


def digest(path: Path) -> str:
    with path.open('rb') as source:
        return hashlib.file_digest(source, 'sha256').hexdigest()


def write_json(path: Path, value: object) -> None:
    path.write_text(json.dumps(value, ensure_ascii=False, separators=(',', ':'), sort_keys=True), encoding='utf-8')


def private_payload_path(path: Path) -> bool:
    name = path.name.lower()
    return (path.suffix.lower() in {'.sqlite', '.sqlite3', '.db', '.db3', '.pem', '.pfx', '.p12', '.key', '.dpapi', '.dmb', '.log', '.jsonl'}
            or name.startswith('.env') or '.local.' in name
            or name in {'private.json', 'official-service.local.txt', 'sync-device.ini', 'ui.ini', 'update-fixture.json'}
            or any(part.lower() in {'profiles', 'backups', 'logs', 'update-state', '.git'} for part in path.parts))


def build(args: argparse.Namespace) -> dict:
    if not re.fullmatch(r'(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)', args.version):
        raise ValueError('invalid stable version')
    if not re.fullmatch('[0-9a-f]{40}', args.source):
        raise ValueError('a full source revision is required')
    if args.runtime_min != 'bundled':
        raise ValueError('v1 requires a self-contained bundled runtime; external runtime installation is manual')
    if args.local_fixture and (args.release_id <= 0 or args.asset_id <= 0):
        raise ValueError('positive asset identities are required (local fixtures use synthetic identities)')
    payload = args.payload.resolve(strict=True)
    if args.payload.is_symlink() or args.payload.is_junction():
        raise ValueError('payload root must not be a filesystem link')
    version_probe = subprocess.run([str(payload / 'daymark.exe'), '--version'], capture_output=True, text=True, timeout=15)
    if version_probe.returncode or version_probe.stdout.strip() != 'Daymark ' + args.version:
        raise ValueError('payload executable version does not match the requested package version')
    local_candidate = getattr(args, "local_candidate", False)
    if args.local_fixture and local_candidate: raise ValueError("conflicting package scopes")
    if not args.local_fixture:
        info = json.loads((payload / 'BUILD-INFO.json').read_text(encoding='utf-8-sig'))
        if info.get('version') != args.version or info.get('sourceRevision') != args.source or (info.get('localPreview') and not local_candidate):
            raise ValueError('release payload identity is not a matching non-preview build')
        pin = getattr(args, 'self_signed_cert_sha256', '')
        if local_candidate:
            import xml.etree.ElementTree as ET
            report = getattr(args, 'local_test_report', None)
            if not pin or not report or info.get('cleanMachine') is not False or info.get('fullTestGatePassed') is not True:
                raise ValueError('local candidate requires explicit local test and pinned signature evidence')
            suite=ET.parse(report).getroot(); cases=list(suite.iter('testcase'))
            if len(cases)<44 or any(c.find('failure') is not None or c.find('error') is not None or c.find('skipped') is not None for c in cases):
                raise ValueError('complete local CTest report required, no failed or skipped cases')
        else:
            check_gates(info, pin)
        if not local_candidate and (args.deployment_report is None or args.deployment_report.resolve() != payload.parent / 'deployment-report.json'):
            raise ValueError('matching independent candidate deployment report required')
        if not local_candidate: verify_deployment_report(payload.parent, info)
        for file, name in ((args.launcher, 'daymark-launcher.exe'), (args.cli, 'daymark-update-cli.exe'), (args.updater, 'daymark-updater.exe')):
            if file.resolve() != payload.parent / 'native' / name:
                raise ValueError('native host differs from the independently validated candidate layout')
        if getattr(args,'setup_helper',None) and args.setup_helper.resolve()!=payload.parent/'native/daymark-setup-helper.exe':
            raise ValueError('setup helper differs from candidate layout')
        if pin:
            verify_executables([payload / 'daymark.exe', args.launcher, args.cli, args.updater] + ([args.setup_helper] if getattr(args,'setup_helper',None) else []), pin)
        if args.release_id or args.asset_id:
            raise ValueError('upload the immutable ZIP first, then use finalize-update.py with actual GitHub asset metadata')
    for path in payload.rglob('*'):
        if path.is_symlink() or path.is_junction() or getattr(path.lstat(),'st_file_attributes',0) & getattr(stat,'FILE_ATTRIBUTE_REPARSE_POINT',0x400) or (path.is_file() and private_payload_path(path.relative_to(payload))):
            raise ValueError('payload contains a link or forbidden private file')
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    name = f'Daymark-{args.version}-windows-x64' + ('-local' if local_candidate else '')
    archive = out / (name + '.zip')
    if archive.exists() or (out / 'update-windows-x64.json').exists() or (out / 'update-windows-x64.draft.json').exists():
        raise ValueError('refusing to replace an existing release asset')
    version_key = args.version + '-' + args.source[:12]
    with tempfile.TemporaryDirectory(prefix='daymark-update-package-', dir=out) as temporary:
        root = Path(temporary) / name
        target = root / 'versions' / version_key
        shutil.copytree(payload, target)
        for source, filename in [(args.launcher, 'daymark.exe'), (args.updater, 'daymark-updater.exe'), (args.cli, 'daymark-update-cli.exe')]:
            shutil.copy2(source, root / filename)
        if getattr(args,'setup_helper',None): shutil.copy2(args.setup_helper, root / 'daymark-setup-helper.exe')
        license_root = Path(__file__).resolve().parents[1] / 'third_party' / 'update'
        for dependency in ('miniz', 'json'):
            destination = root / 'update-licenses' / dependency
            destination.mkdir(parents=True)
            shutil.copy2(license_root / dependency / 'LICENSE', destination / 'LICENSE')
        shutil.copy2(license_root / 'SOURCES.json', root / 'update-licenses' / 'SOURCES.json')
        policy = ('Local installation candidate; independent hosted clean-machine validation has not run.' if local_candidate else
                  'Local verification fixture; not a formal release.' if args.local_fixture else
                  'Project self-signed Authenticode; Windows public publisher trust is not established.'
                  if getattr(args, 'self_signed_cert_sha256', '') else 'Public Authenticode signing policy.')
        (root / 'README.txt').write_text(
            f'Daymark {args.version} - managed Windows portable package\n\n'
            'Start daymark.exe in this top-level directory. Keep the whole directory together; '
            'do not run or move only a version subdirectory.\n\n'
            'Startup checks GitHub and prepares a signed new version in the background. '
            'Restart after the notification; unchanged old program files are removed only '
            'after the new application confirms successful startup.\n\n'
            f'{policy}\n'
            'Update integrity and authenticity use the embedded ECDSA P-256 public key. '
            'No root certificate import or TLS verification bypass is required.\n\n'
            'Qt 6.10.2 and the release runtime are bundled. License texts, module SBOMs and '
            'source access information are inside versions/<version>/. '
            'User task data is not bundled and must not be stored under versions/.\n', encoding='utf-8')
        files = []
        for path in sorted(root.rglob('*')):
            if not path.is_file():
                continue
            relative = path.relative_to(root).as_posix()
            if len(relative) > 220 or not relative.isascii():
                raise ValueError('unsupported payload path')
            files.append({'path': relative, 'size': path.stat().st_size, 'sha256': digest(path)})
        write_json(root / 'FILES.json', {'format': 1, 'files': files})
        index_hash = digest(root / 'FILES.json')
        seed = {'format': 1, 'repository': 'downdone/daymark', 'version': args.version,
                'source_revision': args.source, 'runtime_family': args.runtime,
                'payload_dir': 'versions/' + version_key, 'entry': 'daymark.exe',
                'files_index_sha256': index_hash}
        write_json(root / 'bootstrap-release.json', seed)
        subprocess.run([str(args.cli.resolve()), 'sign', str(args.key.resolve()), str(root / 'bootstrap-release.json'), 'Daymark.BootstrapPayload.v1'], check=True, timeout=30)
        all_files = sorted(p for p in root.rglob('*') if p.is_file())
        unpacked = sum(p.stat().st_size for p in all_files)
        if unpacked > 2 * 1024**3 or len(all_files) > 10000:
            raise ValueError('package exceeds updater capabilities')
        with zipfile.ZipFile(archive, 'x', compression=zipfile.ZIP_DEFLATED) as zip_file:
            for path in all_files:
                zip_file.write(path, name + '/' + path.relative_to(root).as_posix())
        manifest = {'format': 1, 'repository': 'downdone/daymark', 'channel': 'stable',
                    'version': args.version, 'tag': 'v' + args.version, 'source_revision': args.source,
                    'release_id': str(args.release_id),
                    'target': {'os': 'windows', 'arch': 'x86_64', 'runtime_family': args.runtime,
                               'runtime_min': args.runtime_min, 'minimum_windows_build': args.min_windows_build},
                    'bootstrap_protocol': {'min': 1, 'max': 1},
                    'data': {'schema': 11, 'read_write_compatibility': 'task-store-v11-rw1'},
                    'package': {'name': archive.name, 'asset_id': str(args.asset_id), 'size': archive.stat().st_size,
                                'sha256': digest(archive), 'layout': 'portable-managed-v1', 'archive_root': name,
                                'payload_dir': 'versions/' + version_key, 'entry': 'daymark.exe',
                                'unpacked_size': unpacked, 'file_count': len(all_files),
                                'files_index': 'FILES.json', 'files_index_sha256': index_hash}}
        if args.local_fixture:
            write_json(out / 'update-windows-x64.json', manifest)
            subprocess.run([str(args.cli.resolve()), 'sign', str(args.key.resolve()), str(out / 'update-windows-x64.json')], check=True, timeout=30)
        elif not local_candidate:
            write_json(out / 'update-windows-x64.draft.json', manifest)
        (out / (archive.name + '.sha256')).write_text(digest(archive) + '  ' + archive.name + '\n', encoding='ascii')
        (out / 'SHA256SUMS.txt').write_text(digest(archive) + '  ' + archive.name + '\n', encoding='ascii')
        if args.extract:
            if args.extract.exists():
                raise ValueError('extract destination already exists')
            shutil.copytree(root, args.extract)
        return {'archive': str(archive), 'sha256': digest(archive), 'files': len(all_files), 'bytes': archive.stat().st_size, 'local_fixture': args.local_fixture}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('payload', 'launcher', 'updater', 'cli', 'key', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--version', required=True)
    parser.add_argument('--source', required=True)
    parser.add_argument('--runtime', choices=('mingw131', 'msvc143'), required=True)
    parser.add_argument('--runtime-min', required=True)
    parser.add_argument('--min-windows-build', type=int, required=True)
    parser.add_argument('--release-id', type=int, default=0)
    parser.add_argument('--asset-id', type=int, default=0)
    parser.add_argument('--setup-helper', type=Path)
    parser.add_argument('--local-candidate', action='store_true', help='Locally tested signed installer candidate; never generates publishable update metadata')
    parser.add_argument('--local-test-report', type=Path)
    parser.add_argument('--local-fixture', action='store_true', help='Synthetic local test package; not a formal release')
    parser.add_argument('--self-signed-cert-sha256', default='', help='Explicit pinned self-signed publisher policy; does not imply Windows public trust')
    parser.add_argument('--deployment-report', type=Path)
    parser.add_argument('--extract', type=Path)
    args = parser.parse_args()
    print(json.dumps(build(args), ensure_ascii=True))


if __name__ == '__main__':
    main()
