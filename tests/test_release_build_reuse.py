"""Validate cross-run archive provenance without network or real credentials."""
import copy
import io
import json
import os
from pathlib import Path
import re
import tempfile
import textwrap
import unittest
from unittest.mock import patch

WORKFLOW = Path(__file__).resolve().parents[1] / ".github/workflows/release-build.yml"
SOURCE = "a" * 40
TOOLS = "b" * 40
REPO = "daymark-tests/repository"


class BuildReuseTests(unittest.TestCase):
    def setUp(self):
        script = re.search(r"python3 - <<'PY'\n(.*?)\n          PY", WORKFLOW.read_text(), re.S)[1]
        self.script = compile(textwrap.dedent(script), "release-resolver", "exec")
        self.request = {"tag": "v1.8.0", "commit": SOURCE,
                        "reuse_assets_run": 42, "reuse_assets_commit": TOOLS}
        self.old_request = {"tag": "v1.8.0", "commit": SOURCE}
        self.responses = {
            "/actions/runs/42": {"head_sha": TOOLS, "status": "completed", "event": "push",
                                 "path": ".github/workflows/release-build.yml",
                                 "repository": {"full_name": REPO}},
            "/actions/runs/42/jobs?per_page=100": {"jobs": [
                {"name": name, "conclusion": "success"}
                for name in ("resolve", "linux-x64", "macos-arm64")]},
            "/actions/runs/42/artifacts?per_page=100": {"artifacts": [
                {"name": name, "expired": False}
                for name in ("release-linux-x64", "release-macos-arm64")]},
        }

    def execute(self, accepted):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / ".github").mkdir()
            (root / ".github/build-request.json").write_text(json.dumps(self.request))
            output = root / "outputs"
            env = {"GITHUB_REF": "refs/heads/main", "GITHUB_REF_NAME": "main",
                   "GITHUB_OUTPUT": str(output), "GITHUB_REPOSITORY": REPO,
                   "GH_TOKEN": "synthetic-unused-token"}
            if hasattr(self, 'manual_tag'):
                event_path = root/'event.json'
                event_path.write_text(json.dumps({'inputs': {'tag':self.manual_tag}}))
                env.update(GITHUB_EVENT_NAME='workflow_dispatch',GITHUB_EVENT_PATH=str(event_path),
                           GITHUB_REF=getattr(self,'manual_ref','refs/heads/main'))
            def git(arguments, **kwargs):
                if arguments[1] == "fetch": return ""
                if arguments[1] == "cat-file": return "tag\n"
                if arguments[1] == "rev-parse": return SOURCE + "\n"
                if arguments[1] == "show": return json.dumps(self.old_request)
                raise AssertionError("Unexpected Git invocation")
            def urlopen(request, **kwargs):
                prefix = "https://api.github.com/repos/" + REPO
                self.assertTrue(request.full_url.startswith(prefix))
                return io.BytesIO(json.dumps(self.responses[request.full_url[len(prefix):]]).encode())
            old_cwd = Path.cwd()
            try:
                os.chdir(root)
                with patch.dict(os.environ, env), patch("subprocess.check_output", git), \
                        patch("subprocess.run"), patch("urllib.request.urlopen", urlopen), \
                        patch("builtins.print"):
                    if accepted:
                        exec(self.script, {})
                    else:
                        with self.assertRaises(ValueError):
                            exec(self.script, {})
                        self.assertFalse(output.exists())
                        return
                return dict(line.split("=", 1) for line in output.read_text().splitlines())
            finally:
                os.chdir(old_cwd)

    def test_only_windows_is_rebuilt_after_complete_verified_unix_archives(self):
        output = self.execute(True)
        self.assertEqual(output["commit"], SOURCE)
        self.assertEqual(output["reuse_run"], "42")
        self.assertEqual([x["label"] for x in json.loads(output["matrix"])["include"]], ["windows-x64"])

    def test_manual_rebuild_uses_immutable_tag_and_ignores_old_recovery_request(self):
        self.manual_tag = 'v1.8.0'
        self.request['tag'] = 'broken legacy request that must not be read'
        output = self.execute(True)
        self.assertEqual(output['commit'],SOURCE)
        self.assertEqual(output['tag'],'v1.8.0')
        self.assertEqual(output['reuse_run'],'')
        self.manual_ref = 'refs/heads/unreviewed'
        self.execute(False)
        self.manual_ref = 'refs/heads/main'
        self.manual_tag = 'v01.8.0'
        self.execute(False)

    def test_normal_request_builds_windows_only(self):
        self.request = {"tag": "v1.8.0", "commit": SOURCE}
        output = self.execute(True)
        self.assertEqual([p["label"] for p in json.loads(output["matrix"])["include"]], ["windows-x64"])

    def test_run_identity_and_completion_are_required(self):
        original = copy.deepcopy(self.responses)
        for field, value in (("head_sha", "c" * 40), ("status", "in_progress"),
                             ("path", "another.yml"), ("event", "pull_request"),
                             ("repository", {"full_name": "another/repository"})):
            with self.subTest(field=field):
                self.responses = copy.deepcopy(original)
                self.responses["/actions/runs/42"][field] = value
                self.execute(False)

    def test_failed_or_missing_platform_gate_is_rejected(self):
        jobs = self.responses["/actions/runs/42/jobs?per_page=100"]["jobs"]
        jobs[1]["conclusion"] = "failure"
        self.execute(False)
        jobs.pop(1)
        self.execute(False)

    def test_expired_incomplete_or_unexpected_release_archives_are_rejected(self):
        original = copy.deepcopy(self.responses)
        artifacts = self.responses["/actions/runs/42/artifacts?per_page=100"]["artifacts"]
        artifacts[0]["expired"] = True
        self.execute(False)
        self.responses = copy.deepcopy(original)
        self.responses["/actions/runs/42/artifacts?per_page=100"]["artifacts"].pop()
        self.execute(False)
        self.responses = copy.deepcopy(original)
        self.responses["/actions/runs/42/artifacts?per_page=100"]["artifacts"].append(
            {"name": "release-windows-x64", "expired": False})
        self.execute(False)

    def test_previous_request_must_identify_the_same_tag_and_commit(self):
        for field, value in (("tag", "v1.7.0"), ("commit", "c" * 40)):
            self.old_request = {"tag": "v1.8.0", "commit": SOURCE}
            self.old_request[field] = value
            self.execute(False)

    def test_exact_numeric_run_and_commit_are_required(self):
        for field, value in (("reuse_assets_run", "42"), ("reuse_assets_run", -1),
                             ("reuse_assets_commit", "main")):
            original = self.request[field]
            self.request[field] = value
            self.execute(False)
            self.request[field] = original


if __name__ == "__main__":
    unittest.main()
