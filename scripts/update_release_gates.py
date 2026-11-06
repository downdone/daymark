"""Explicit release policies; self-signing never implies Windows public trust."""
import re
import hashlib
import json
from pathlib import Path
import subprocess


def check_candidate_provenance(record, run: dict, source: str, tag: str) -> None:
    if not re.fullmatch('[0-9a-f]{40}', source) or not re.fullmatch(r'v(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)', tag):
        raise ValueError('invalid immutable source identity')
    event = run.get('event')
    direct = event == 'push' and run.get('head_sha') == source and run.get('head_branch') == tag
    manual = event == 'workflow_dispatch' and run.get('head_branch') == 'main'
    if not direct and not manual:
        raise ValueError('only exact tag pushes or main immutable rebuilds are accepted')
    if record is None and direct:
        return  # Earlier exact-tag artifacts predate the explicit tooling record.
    if not isinstance(record, dict):
        raise ValueError('immutable rebuild requires an explicit artifact provenance record')
    expected = {'repository':'downdone/daymark', 'source_revision':source, 'tag':tag,
                'tooling_revision':run.get('head_sha'), 'run_id':str(run.get('id')),
                'event':event, 'trigger_ref':'refs/heads/main' if manual else 'refs/tags/'+tag}
    if (type(record.get('format')) is not int or record['format'] != 1
            or record.get('official_service_configured') is not True
            or any(record.get(key) != value for key,value in expected.items())):
        raise ValueError('artifact source/tooling/run provenance mismatch')


def verify_deployment_report(candidate: Path, info: dict) -> None:
    report = json.loads((candidate / 'deployment-report.json').read_text(encoding='utf-8-sig'))
    if (report.get('format') != 1 or report.get('passed') is not True
            or report.get('kind') != 'independent-windows-hosted-runner'
            or report.get('runtimeOriginsAudited') is not True
            or report.get('sourceRevision') != info.get('sourceRevision')
            or report.get('version') != info.get('version')):
        raise ValueError('independent deployment report identity mismatch')
    expected = {}
    for entry in report.get('files', []):
        path = entry.get('path', '')
        if (not re.fullmatch(r'(payload|native)/[^\\:]+', path) or '..' in Path(path).parts
                or path in expected or not re.fullmatch('[0-9a-f]{64}', entry.get('sha256', ''))):
            raise ValueError('unsafe or duplicate deployment inventory path')
        expected[path] = entry['sha256']
    actual = {}
    for path in candidate.rglob('*'):
        if path.is_symlink() or path.is_junction():
            raise ValueError('link in validated candidate')
        if path.is_file() and path != candidate / 'deployment-report.json':
            with path.open('rb') as source:
                actual[path.relative_to(candidate).as_posix()] = hashlib.file_digest(source, 'sha256').hexdigest()
    if not expected or actual != expected:
        raise ValueError('candidate differs from independently validated files')


def check_gates(info: dict, certificate_sha256: str = '') -> None:
    if any(info.get(gate) is not True for gate in ('fullTestGatePassed', 'cleanMachine', 'officialServiceConfigured')):
        raise ValueError('full tests, independent Windows deployment and official configuration gates must pass first')
    signature = info.get('authenticode', {})
    if not isinstance(signature, dict):
        raise ValueError('invalid Authenticode evidence')
    if not certificate_sha256:
        if info.get('signed') is not True or signature.get('policy') == 'self-signed-pinned':
            raise ValueError('public code-signing gate missing; self-signed policy requires an explicit certificate pin')
        return
    if not re.fullmatch('[0-9a-f]{64}', certificate_sha256):
        raise ValueError('invalid independent self-signed certificate SHA256 pin')
    if (info.get('signed') is not False
            or signature.get('policy') != 'self-signed-pinned'
            or signature.get('publicTrust') is not False
            or signature.get('certificateSha256') != certificate_sha256):
        raise ValueError('self-signed policy must match the independent pin and explicitly deny public trust')


def verify_executables(files, certificate_sha256: str) -> None:
    files = list(files)
    if len(files) not in (4,5):
        raise ValueError('four updater hosts, optionally with the setup helper, are required')
    script = Path(__file__).with_name('Verify-WindowsSignatures.ps1')
    command = ['pwsh', '-NoProfile', '-File', str(script), '-CertificateSha256', certificate_sha256]
    for option, file in zip(('Payload', 'Launcher', 'Cli', 'Updater', 'SetupHelper'), files):
        command += ['-' + option, str(file)]
    subprocess.run(command, check=True, timeout=60)


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description='Check downloaded candidate against independently verified GitHub run metadata')
    parser.add_argument('--run',type=Path,required=True)
    parser.add_argument('--record',type=Path,required=True)
    arguments = parser.parse_args()
    run = json.loads(arguments.run.read_text(encoding='utf-8-sig'))
    record = json.loads(arguments.record.read_text(encoding='utf-8-sig')) if arguments.record.exists() else None
    check_candidate_provenance(record,run,run['source'],run['tag'])
    print('PASS: immutable source, reviewed tooling, triggering run and artifact provenance match')
