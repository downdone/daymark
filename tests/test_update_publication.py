"""Immutable publication gates; synthetic files only, no GitHub or real key."""
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
from update_release_gates import check_gates, verify_deployment_report
spec = importlib.util.spec_from_file_location('finalize_update', ROOT / 'scripts/finalize-update.py')
finalizer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(finalizer)


class UpdatePublicationTest(unittest.TestCase):
    def test_private_payload_paths_are_rejected_without_reading_secrets(self):
        spec = importlib.util.spec_from_file_location('package_update', ROOT / 'scripts/package-update.py')
        package = importlib.util.module_from_spec(spec); spec.loader.exec_module(package)
        for name in ('daymark.sqlite3','backup.key','update-private.json.dpapi','.env.local','config.local.json','profiles/account/metadata.json','backups/local.dmb','update-fixture.json','logs/runtime.jsonl','daymark.log'):
            self.assertTrue(package.private_payload_path(Path(name)),name)
        for name in ('BUILD-INFO.json','qml/QtQuick/qmldir','licenses/QtCore.json'):
            self.assertFalse(package.private_payload_path(Path(name)),name)
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.archive = self.root / 'Daymark-2.0.1-windows-x64.zip'
        self.archive.write_bytes(b'synthetic immutable archive')
        self.package = {'name': self.archive.name, 'size': self.archive.stat().st_size,
                        'sha256': hashlib.sha256(self.archive.read_bytes()).hexdigest(), 'asset_id': '0'}
        self.manifest = {'repository': 'downdone/daymark', 'tag': 'v2.0.1', 'release_id': '0', 'package': self.package}
        self.release = {'id': 123, 'tag_name': 'v2.0.1', 'draft': False, 'prerelease': False,
                        'url': 'https://api.github.com/repos/downdone/daymark/releases/123'}
        self.asset = {'id': 456, 'name': self.archive.name, 'size': self.package['size'], 'state': 'uploaded',
                      'digest': 'sha256:' + self.package['sha256'],
                      'url': 'https://api.github.com/repos/downdone/daymark/releases/assets/456',
                      'browser_download_url': 'https://github.com/downdone/daymark/releases/download/v2.0.1/' + self.archive.name}

    def invoke(self, signer=None):
        paths = []
        for name, value in [('draft', self.manifest), ('asset', self.asset), ('release', self.release)]:
            path = self.root / (name + '.json'); path.write_text(json.dumps(value), encoding='utf-8'); paths.append(path)
        with patch.object(finalizer.subprocess, 'run', signer or (lambda *a, **kw: None)):
            return finalizer.finalize(*paths, self.root / 'signer.exe', self.root / 'synthetic-private.json')

    def test_actual_github_identities_are_signed_and_not_replaceable(self):
        calls = []
        result = self.invoke(lambda arguments, **kwargs: calls.append(arguments))
        manifest = json.loads(result.read_text())
        self.assertEqual(manifest['release_id'], '123')
        self.assertEqual(manifest['package']['asset_id'], '456')
        self.assertEqual(calls[0][1], 'sign')
        with self.assertRaises(ValueError): self.invoke()

    def test_wrong_identity_release_digest_or_content_is_rejected(self):
        original = copy.deepcopy(self.asset)
        for field, wrong in [('id', True), ('size', 999), ('state', 'new'), ('digest', 'sha256:'+'0'*64),
                             ('url', 'https://example.invalid/456'), ('browser_download_url', 'https://github.com/downdone/daymark/releases/download/v2.0.0/' + self.archive.name)]:
            with self.subTest(field=field):
                self.asset = dict(original, **{field: wrong})
                with self.assertRaises(ValueError): self.invoke()
        self.asset = original
        self.archive.write_bytes(b'changed')
        with self.assertRaises(ValueError): self.invoke()

    def test_signer_failure_leaves_no_ready_file(self):
        def failed(*args, **kwargs): raise subprocess.CalledProcessError(20, 'synthetic signer')
        with self.assertRaises(subprocess.CalledProcessError): self.invoke(failed)
        self.assertFalse((self.root / 'update-windows-x64.json').exists())
        self.assertFalse((self.root / 'update-windows-x64.json.sig').exists())


class UpdateReleasePolicyTest(unittest.TestCase):
    def setUp(self):
        self.pin = 'a' * 64
        self.info = {'signed': False, 'fullTestGatePassed': True, 'cleanMachine': True,
                     'authenticode': {'policy': 'self-signed-pinned', 'publicTrust': False,
                                      'certificateSha256': self.pin}}

    def test_self_signed_policy_is_explicit_and_not_public_trust(self):
        check_gates(self.info, self.pin)
        with self.assertRaises(ValueError): check_gates(self.info)
        check_gates({'signed': True, 'fullTestGatePassed': True, 'cleanMachine': True})
        with self.assertRaises(ValueError): check_gates(dict(self.info, signed=True))

    def test_self_signing_never_bypasses_tests_or_deployment_validation(self):
        for gate in ('fullTestGatePassed', 'cleanMachine'):
            for bad in (False, 1, 'true', None):
                with self.subTest(gate=gate, bad=bad):
                    with self.assertRaises(ValueError): check_gates(dict(self.info, **{gate: bad}), self.pin)

    def test_candidate_cannot_supply_its_own_trust_or_claim_public_signing(self):
        for pin in ('b' * 64, '', 'a' * 63, 'A' * 64):
            with self.assertRaises(ValueError): check_gates(self.info, pin)
        for changes in ({'publicTrust': True}, {'publicTrust': 0}, {'policy': 'ignore-errors'},
                        {'certificateSha256': 'b' * 64}):
            changed = dict(self.info, authenticode=dict(self.info['authenticode'], **changes))
            with self.assertRaises(ValueError): check_gates(changed, self.pin)
        with self.assertRaises(ValueError): check_gates(dict(self.info, signed=True), self.pin)

    def test_deployment_report_binds_all_candidate_files_and_identity(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root/'payload').mkdir(); (root/'native').mkdir()
            exe = root/'payload/daymark.exe'; exe.write_bytes(b'synthetic validated image')
            report = dict(format=1,passed=True,kind='independent-windows-hosted-runner',runtimeOriginsAudited=True,
                          sourceRevision='a'*40,version='2.0.1',files=[{'path':'payload/daymark.exe','sha256':hashlib.sha256(exe.read_bytes()).hexdigest()}])
            path = root/'deployment-report.json'; path.write_text(json.dumps(report))
            info = dict(sourceRevision='a'*40,version='2.0.1')
            verify_deployment_report(root,info)
            for changed in (dict(info,version='2.0.2'),dict(info,sourceRevision='b'*40)):
                with self.assertRaises(ValueError): verify_deployment_report(root,changed)
            exe.write_bytes(b'modified after validation')
            with self.assertRaises(ValueError): verify_deployment_report(root,info)
            exe.write_bytes(b'synthetic validated image')
            extra = root/'native/deployment-report.json'; extra.write_text('{}')
            with self.assertRaises(ValueError): verify_deployment_report(root,info)


if __name__ == '__main__': unittest.main()
