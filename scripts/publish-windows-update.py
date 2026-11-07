"""Publish prevalidated immutable update assets; only used by the protected job."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import tempfile
import urllib.request
from update_release_gates import check_gates, verify_deployment_report

REPOSITORY = 'downdone/daymark'


def api(path, token):
    request = urllib.request.Request('https://api.github.com/repos/' + REPOSITORY + path,
        headers={'Authorization': 'Bearer ' + token, 'Accept': 'application/vnd.github+json',
                 'X-GitHub-Api-Version': '2026-03-10', 'User-Agent': 'Daymark-update-publisher'})
    with urllib.request.urlopen(request, timeout=30) as response:
        return json.load(response)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--directory', type=Path, required=True)
    parser.add_argument('--tag', required=True)
    parser.add_argument('--source', required=True)
    parser.add_argument('--makensis', type=Path, required=True)
    parser.add_argument('--signtool', type=Path, required=True)
    parser.add_argument('--certificate-thumbprint', required=True)
    parser.add_argument('--installer-test', type=Path, required=True)
    args = parser.parse_args()
    source = Path(__file__).resolve().parents[1]
    token = os.environ['GH_TOKEN']
    key_value = os.environ['UPDATE_SIGNING_KEY']
    candidate = args.directory.resolve()
    info = json.loads((candidate / 'payload/BUILD-INFO.json').read_text(encoding='utf-8-sig'))
    version = args.tag.removeprefix('v')
    if info.get('version') != version or info.get('sourceRevision') != args.source or info.get('localPreview'):
        raise ValueError('candidate source identity mismatch')
    pin = os.environ.get('DAYMARK_AUTHENTICODE_CERT_SHA256', '')
    check_gates(info, pin)
    verify_deployment_report(candidate, info)
    release = api('/releases/tags/' + args.tag, token)
    expected_names = {f'Daymark-{version}-windows-x64.zip', f'Daymark-{version}-windows-x64.zip.sha256',
                      f'Daymark-{version}-windows-x64-setup.exe', f'Daymark-{version}-windows-x64-setup.exe.sha256',
                      'SHA256SUMS.txt', 'update-windows-x64.json', 'update-windows-x64.json.sig'}
    if release.get('draft') or release.get('prerelease') or release.get('tag_name') != args.tag:
        raise ValueError('a public stable source release must already exist')
    existing = api('/releases/' + str(release['id']) + '/assets?per_page=100', token)
    if len(existing) >= 100 or any(item.get('name') in expected_names for item in existing):
        raise ValueError('release assets already exist or inventory is incomplete; no overwrite is allowed')
    # Only a temporary file holds private bytes. The checked signer never prints it.
    with tempfile.TemporaryDirectory(prefix='daymark-sign-', dir=os.environ['RUNNER_TEMP']) as temporary:
        key = Path(temporary) / 'private.json'
        key.write_text(key_value, encoding='utf-8')
        output = Path(temporary) / 'release'
        cli = candidate / 'native/daymark-update-cli.exe'
        package_command = ['python', str(source / 'scripts/package-update.py'), '--payload', str(candidate / 'payload'),
            '--launcher', str(candidate / 'native/daymark-launcher.exe'), '--updater', str(candidate / 'native/daymark-updater.exe'),
            '--setup-helper', str(candidate / 'native/daymark-setup-helper.exe'),
            '--cli', str(cli), '--key', str(key), '--output', str(output), '--version', version,
            '--source', args.source, '--runtime', 'msvc143', '--runtime-min', 'bundled', '--min-windows-build', '19045',
            '--extract', str(Path(temporary) / 'probe'), '--deployment-report', str(candidate / 'deployment-report.json')]
        if pin:
            package_command += ['--self-signed-cert-sha256', pin]
        subprocess.run(package_command, check=True, timeout=180)
        # Embed the pristine signed bundle before initialize creates mutable state.
        installer = output / f'Daymark-{version}-windows-x64-setup.exe'
        subprocess.run(['python', str(source / 'scripts/package-setup.py'),
                        '--makensis', str(args.makensis), '--signtool', str(args.signtool),
                        '--certificate-thumbprint', args.certificate_thumbprint,
                        '--certificate-sha256', pin, '--bundle', str(Path(temporary) / 'probe'),
                        '--output', str(installer)], check=True, timeout=360)
        proof = source / 'build/installer-publication-proof'
        proof.mkdir(parents=True, exist_ok=True)
        environment = dict(os.environ, DAYMARK_INSTALLER_ARTIFACT=str(installer))
        test_log = proof / 'qt-test.log'
        acceptance = subprocess.run([str(args.installer_test.resolve()), '-o', str(test_log) + ',txt'], env=environment,
                                    capture_output=True, timeout=270)
        (proof / 'installer-test.log').write_bytes(acceptance.stdout + acceptance.stderr)
        acceptance.check_returncode()
        if not test_log.is_file() or b'PASS: real NSIS Unicode/space path' not in test_log.read_bytes():
            raise ValueError('installer acceptance did not produce its completion evidence')
        checksum = hashlib.sha256(installer.read_bytes()).hexdigest()
        (proof / 'report.json').write_text(json.dumps(dict(format=1, passed=True,
            source_revision=args.source, version=version, installer_sha256=checksum,
            signature_policy='self-signed-pinned', public_trust=False,
            acceptance='QTemporaryDir native install, Qt startup/module audit, busy refusal, repair, uninstall/data preservation'), indent=2), encoding='utf-8')
        # Compiled public trust must validate the freshly signed bootstrap before upload.
        subprocess.run([str(Path(temporary) / 'probe/daymark-update-cli.exe'), 'initialize', '--root', str(Path(temporary) / 'probe'), '--json'], check=True, timeout=30)
        archive = output / f'Daymark-{version}-windows-x64.zip'
        with (output / 'SHA256SUMS.txt').open('a', encoding='ascii') as index:
            index.write(checksum + '  ' + installer.name + '\n')
        upload = [archive, output / (archive.name + '.sha256'), installer,
                  installer.with_suffix('.exe.sha256'), output / 'SHA256SUMS.txt']
        subprocess.run(['gh', 'release', 'upload', args.tag, *map(str, upload), '--repo', REPOSITORY], check=True, timeout=180)
        assets = api('/releases/' + str(release['id']) + '/assets?per_page=100', token)
        matching = [asset for asset in assets if asset.get('name') == archive.name]
        if len(matching) != 1:
            raise ValueError('uploaded archive is missing or ambiguous; no ready files were published')
        asset = matching[0]
        asset_file = output / 'asset.json'; asset_file.write_text(json.dumps(asset), encoding='utf-8')
        release_file = output / 'release.json'; release_file.write_text(json.dumps(release), encoding='utf-8')
        spec = importlib.util.spec_from_file_location('finalize_update', source / 'scripts/finalize-update.py')
        module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
        manifest = module.finalize(output / 'update-windows-x64.draft.json', asset_file, release_file, cli, key)
        # Ready files are deliberately the final upload. An interruption leaves no
        # installable release; remediation uses a new version, never --clobber.
        subprocess.run(['gh', 'release', 'upload', args.tag, str(manifest), str(manifest.with_suffix('.json.sig')),
                        '--repo', REPOSITORY], check=True, timeout=60)
        final = api('/releases/' + str(release['id']) + '/assets?per_page=100', token)
        for path in (*upload, manifest, manifest.with_suffix('.json.sig')):
            entry = [item for item in final if item.get('name') == path.name]
            checksum = hashlib.sha256(path.read_bytes()).hexdigest()
            if len(entry) != 1 or entry[0].get('digest') != 'sha256:' + checksum:
                raise ValueError('published ready-file digest mismatch')
    print('PASS: immutable Windows update assets published and digest verified')


if __name__ == '__main__':
    main()
