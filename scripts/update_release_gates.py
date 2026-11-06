"""Explicit release policies; self-signing never implies Windows public trust."""
import re
import hashlib
import json
from pathlib import Path
import subprocess


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
    if any(info.get(gate) is not True for gate in ('fullTestGatePassed', 'cleanMachine')):
        raise ValueError('full tests and independent Windows deployment gates must pass first')
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
    if len(files) != 4:
        raise ValueError('exactly four executable paths are required')
    script = Path(__file__).with_name('Verify-WindowsSignatures.ps1')
    command = ['pwsh', '-NoProfile', '-File', str(script), '-CertificateSha256', certificate_sha256]
    for option, file in zip(('Payload', 'Launcher', 'Cli', 'Updater'), files):
        command += ['-' + option, str(file)]
    subprocess.run(command, check=True, timeout=60)
