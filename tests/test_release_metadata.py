"""Release guard boundaries only; no Qt, personal data or network calls."""
import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest
import io
import json
from urllib.error import HTTPError

spec = importlib.util.spec_from_file_location("release_metadata", Path(__file__).resolve().parents[1] / "scripts/publish-source-release.py")
release = importlib.util.module_from_spec(spec)
spec.loader.exec_module(release)


class ReleaseGuards(unittest.TestCase):
    def test_publication_api_error_only_exposes_safe_categories(self):
        secret = "synthetic-private-value"
        for message, expected in (("Resource not accessible by integration", "integration"),
                                  ("API rate limit exceeded for " + secret, "rate limit"),
                                  (secret, "HTTP 403")):
            error = HTTPError("https://api.github.com/", 403, "Forbidden",
                              {"X-Accepted-GitHub-Permissions": "contents=write"},
                              io.BytesIO(json.dumps({"message": message}).encode()))
            detail = str(release.publication_api_error("creation", error))
            self.assertIn(expected, detail)
            self.assertIn("contents=write", detail)
            self.assertNotIn(secret, detail)

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.git("init", "-q", "-b", "main")
        self.write("CMakeLists.txt", 'project(Daymark VERSION 1.0.1 LANGUAGES CXX)\nset(DAYMARK_VERSION "1.0.1")\n')
        self.write("docs/release-notes-v1.0.md", "# Daymark 1.0.0\n仅发布源码\n")
        self.write("docs/release-notes-v1.0.1.md", "# Daymark 1.0.1\n仅发布源码\n真实验证状态\n")
        self.git("add", ".")

    def git(self, *args):
        return subprocess.check_output(["git", "-c", "user.name=Daymark Tests", "-c", "user.email=tests@daymark.invalid", *args], cwd=self.root, text=True, stderr=subprocess.PIPE).strip()

    def write(self, name, text):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")

    def test_patch_notes_take_precedence_over_legacy_minor_file(self):
        version, notes = release.release_input(self.root, "v1.0.1")
        self.assertEqual(version, "1.0.1")
        self.assertIn("真实验证状态", notes)
        release.validate_source_tree(self.root)

    def test_existing_100_legacy_notes_remain_compatible(self):
        self.write("CMakeLists.txt", 'project(Daymark VERSION 1.0.0 LANGUAGES CXX)\nset(DAYMARK_VERSION "1.0.0")\n')
        self.assertEqual(release.release_input(self.root, "v1.0.0")[0], "1.0.0")

    def test_invalid_tags_fail_before_any_publication(self):
        for tag in ("v01.0.1", "v1.0", "v1.0.1-rc.1", "../notes", "1.0.1"):
            with self.subTest(tag=tag), self.assertRaises(ValueError):
                release.release_input(self.root, tag)

    def test_both_cmake_versions_must_match(self):
        self.write("CMakeLists.txt", 'project(Daymark VERSION 1.0.2 LANGUAGES CXX)\nset(DAYMARK_VERSION "1.0.1")\n')
        with self.assertRaises(ValueError):
            release.release_input(self.root, "v1.0.1")

    def test_wrong_notes_or_missing_source_only_boundary_fail(self):
        for notes in ("# Daymark 1.0.2\n仅发布源码", "# Daymark 1.0.10\n仅发布源码", "# Daymark 1.0.1\nUnverified package"):
            with self.subTest(notes=notes), self.assertRaises(ValueError):
                self.write("docs/release-notes-v1.0.1.md", notes)
                release.release_input(self.root, "v1.0.1")

    def test_tracked_runtime_or_database_is_rejected(self):
        for name in ("runtime.dll", "sample.db", ".env", "build/cache.txt", "__pycache__/cache.pyc"):
            with self.subTest(name=name):
                self.write(name, "synthetic")
                self.git("add", "-f", name)
                with self.assertRaises(ValueError):
                    release.validate_source_tree(self.root)
                self.git("rm", "--cached", name)
                (self.root / name).unlink()

    def test_untracked_local_data_is_not_part_of_source_archive(self):
        self.write("private.db", "not published")
        release.validate_source_tree(self.root)

    def test_sensitive_values_are_rejected_without_echoing_them(self):
        examples = ["C:" + "/" + "Users/" + "example/local", "gh" + "p_" + "A" * 36]
        for value in examples:
            with self.subTest(kind=value[:2]):
                self.write("docs/internal.md", value)
                self.git("add", "docs/internal.md")
                with self.assertRaises(ValueError) as caught:
                    release.validate_source_tree(self.root)
                self.assertNotIn(value, str(caught.exception))
                self.assertIn("docs/internal.md", str(caught.exception))

    def committed_tag(self):
        self.git("commit", "-qm", "synthetic release")
        revision = self.git("rev-parse", "HEAD")
        self.git("update-ref", "refs/remotes/origin/main", revision)
        self.git("tag", "-a", "v1.0.1", "-m", "synthetic annotated source release")
        return revision

    def test_annotated_tag_matches_published_branch(self):
        revision = self.committed_tag()
        release.validate_tag_checkout(self.root, "v1.0.1", revision)

    def test_webhook_annotated_tag_sha_is_peeled_without_changing_identity(self):
        revision = self.committed_tag()
        self.assertEqual(release.resolve_commit(self.root, self.git("rev-parse", "v1.0.1")), revision)
        self.assertEqual(release.resolve_commit(self.root, revision), revision)
        for invalid in (None, "main", "--help", "v1.0.1"):
            with self.subTest(revision=invalid), self.assertRaises(ValueError):
                release.resolve_commit(self.root, invalid)

    def test_lightweight_tag_and_different_checkout_are_rejected(self):
        revision = self.committed_tag()
        self.git("tag", "lightweight")
        with self.assertRaises(ValueError):
            release.validate_tag_checkout(self.root, "lightweight", revision)
        self.write("another.txt", "synthetic")
        self.git("add", "another.txt")
        self.git("commit", "-qm", "next commit")
        with self.assertRaises(ValueError):
            release.validate_tag_checkout(self.root, "v1.0.1", self.git("rev-parse", "HEAD"))

    def test_tag_ahead_of_main_is_rejected(self):
        self.committed_tag()
        self.write("another.txt", "synthetic")
        self.git("add", "another.txt")
        self.git("commit", "-qm", "not published to main")
        self.git("tag", "-a", "v1.0.2", "-m", "synthetic")
        with self.assertRaises(ValueError):
            release.validate_tag_checkout(self.root, "v1.0.2", self.git("rev-parse", "HEAD"))


if __name__ == "__main__":
    unittest.main()
