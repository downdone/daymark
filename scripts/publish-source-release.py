"""Publish a source-only tagged release using the job-scoped GitHub token."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import urllib.error
import urllib.request
from urllib.parse import urljoin


def publication_api_error(action, error):
    """Expose useful permission categories, never raw bodies or credentials."""
    detail = ""
    try:
        message = json.loads(error.read(8192)).get("message", "")
        for safe_prefix in ("Resource not accessible by integration",
                            "Resource not accessible by personal access token",
                            "API rate limit exceeded", "You have exceeded a secondary rate limit",
                            "Bad credentials", "Repository was archived"):
            if message.startswith(safe_prefix):
                detail = "; " + safe_prefix
                break
    except (ValueError, AttributeError, TypeError):
        pass
    permissions = error.headers.get("X-Accepted-GitHub-Permissions", "") if error.headers else ""
    if permissions and re.fullmatch(r"[a-z_=;, ]{1,180}", permissions):
        detail += "; required permissions: " + permissions
    return RuntimeError(f"Release {action} failed (HTTP {error.code}){detail}")


def source_release_payload(tag, version, notes):
    # The already verified immutable tag owns the source identity. An obsolete
    # target_commitish hint can require workflows:write even for an existing tag.
    return {"tag_name": tag, "name": f"Daymark {version}", "body": notes,
            "draft": False, "prerelease": False, "make_latest": "true"}


def validate_release_record(record, tag, version, notes):
    if (record["tag_name"] != tag or record["name"] != f"Daymark {version}"
            or record["body"] != notes or record["draft"] or record["prerelease"]
            or record["assets"]):
        raise ValueError("An incompatible release exists; it was not modified")


def release_input(root, tag):
    if not re.fullmatch(r"v(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)", tag):
        raise ValueError("A stable vMAJOR.MINOR.PATCH tag is required")
    version = tag[1:]
    cmake = (root / "CMakeLists.txt").read_text(encoding="utf-8")
    if (not re.search(r'project\(Daymark\s+VERSION\s+'+re.escape(version)+r'\s', cmake)
            or f'set(DAYMARK_VERSION "{version}")' not in cmake):
        raise ValueError("Tag, CMake project and application version must match")
    minor = ".".join(version.split(".")[:2])
    notes_path = root / f"docs/release-notes-v{version}.md"
    if not notes_path.exists():
        notes_path = root / f"docs/release-notes-v{minor}.md"  # Legacy 1.0.0 notes remain immutable.
    notes = notes_path.read_text(encoding="utf-8")
    if not notes.splitlines() or notes.splitlines()[0] != f"# Daymark {version}" or "仅发布源码" not in notes:
        raise ValueError("Matching source-only release notes are required")
    return version, notes



def validate_source_tree(root):
    """Reject tracked machine data, distributable runtimes and common secrets."""
    names = subprocess.check_output(["git", "ls-files", "-z"], cwd=root).decode("utf-8").split("\0")
    blocked = {".exe", ".dll", ".db", ".sqlite", ".sqlite3", ".db-wal", ".db-shm",
               ".sqlite-wal", ".sqlite-shm", ".log", ".pem", ".pfx", ".p12", ".key",
               ".bundle", ".zip", ".7z", ".obj", ".o", ".a", ".pdb", ".user", ".pyc"}
    sensitive = re.compile(r"[A-Za-z]:[/\\](?:Users|Code|Qt[0-9]*)[/\\]|"
                           r"-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----|"
                           r"gh[pousr]_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{30,}")
    for name in filter(None, names):
        path = Path(name)
        if (path.parts[0] in {"build", "dist", ".idea", ".vscode"} or "__pycache__" in path.parts
                or path.name.startswith(".env") and path.name != ".env.example"
                or any(name.lower().endswith(suffix) for suffix in blocked)):
            raise ValueError(f"Source release contains a forbidden tracked path: {name}")
        try:
            content = (root / path).read_text(encoding="utf-8")
        except UnicodeError:
            continue  # Original product icon/image assets are allowed.
        if sensitive.search(content):
            raise ValueError(f"Source privacy review required: {name} (value omitted)")


def validate_tag_checkout(root, tag, revision):
    def git(*args):
        return subprocess.check_output(["git", *args], cwd=root, text=True).strip()
    if git("cat-file", "-t", "refs/tags/"+tag) != "tag":
        raise ValueError("An annotated release tag is required")
    if git("rev-parse", "refs/tags/"+tag+"^{}") != revision:
        raise ValueError("Release tag does not identify this checkout")
    if subprocess.run(["git", "merge-base", "--is-ancestor", revision, "origin/main"],
                      cwd=root, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode:
        raise ValueError("Release commit must already be reachable from origin/main")


def resolve_commit(root, revision):
    """Webhook SHAs may name an annotated tag object; compare peeled commits."""
    if not revision or not re.fullmatch(r"[0-9a-f]{40}", revision):
        raise ValueError("Expected an exact source revision")
    return subprocess.check_output(["git", "rev-parse", "--verify", revision+"^{commit}"],
                                   cwd=root, text=True).strip()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    selection = parser.add_mutually_exclusive_group(required=True)
    selection.add_argument("--tag")
    selection.add_argument("--check-current", action="store_true")
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--source-root", type=Path)
    args = parser.parse_args()
    root = (args.source_root or Path(__file__).resolve().parent.parent).resolve()
    if args.check_current:
        args.tag = "v" + re.search(r'set\(DAYMARK_VERSION "([^"]+)"\)',
                                  (root / "CMakeLists.txt").read_text(encoding="utf-8")).group(1)
        args.check = True  # This mode can never access the publication API.
    version, notes = release_input(root, args.tag)
    validate_source_tree(root)
    if args.check:
        print(f"PASS: {args.tag} matches version/notes; tracked source privacy checks passed")
        return
    tag_event = os.environ.get("GITHUB_REF") == f"refs/tags/{args.tag}"
    request_event = os.environ.get("GITHUB_REF") == "refs/heads/main" and os.environ.get("RELEASE_REQUEST_MODE") == "true"
    if not (tag_event or request_event):
        raise ValueError("Publishing requires a matching tag or an explicit main-branch release request")
    repository = os.environ["GITHUB_REPOSITORY"]
    if not re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", repository):
        raise ValueError("Invalid repository")
    revision = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip()
    expected_revision = os.environ.get("RELEASE_SOURCE_SHA") if request_event else os.environ["GITHUB_SHA"]
    if revision != resolve_commit(root, expected_revision):
        raise ValueError("Checkout does not match triggering commit")
    validate_tag_checkout(root, args.tag, revision)
    # Release bodies are outside docs/, so resolve their relative documentation links.
    docs_base = f"https://github.com/{repository}/blob/{args.tag}/docs/"
    notes = re.sub(r"\]\((?!https?://|#)([^)]+)\)",
                   lambda match: "](" + urljoin(docs_base,match.group(1)) + ")", notes)
    base = f"https://api.github.com/repos/{repository}/releases"
    token = os.environ["GH_TOKEN"]

    def request(url, payload=None):
        data = None if payload is None else json.dumps(payload).encode()
        req = urllib.request.Request(url, data=data, headers={
            "Authorization": f"Bearer {token}", "Accept": "application/vnd.github+json",
            "Content-Type": "application/json", "User-Agent": "Daymark-source-release",
            "X-GitHub-Api-Version": "2026-03-10",
        })
        with urllib.request.urlopen(req, timeout=30) as response:
            return json.load(response)

    remote_ref_url = f"https://api.github.com/repos/{repository}/git/ref/tags/{args.tag}"
    expected_tag = subprocess.check_output(["git", "rev-parse", "refs/tags/"+args.tag],
                                           cwd=root, text=True).strip()
    def verify_remote_tag():
        remote = request(remote_ref_url)["object"]
        if remote.get("type") != "tag" or remote.get("sha") != expected_tag:
            raise ValueError("Remote annotated tag differs from verified source; no tag was changed")
    verify_remote_tag()
    try:
        existing = request(base + "/tags/" + args.tag)
    except urllib.error.HTTPError as error:
        if error.code != 404:
            raise publication_api_error("lookup", error) from None
    else:
        validate_release_record(existing, args.tag, version, notes)
        print(existing["html_url"])
        return
    try:
        result = request(base, source_release_payload(args.tag, version, notes))
    except urllib.error.HTTPError as error:
        raise publication_api_error("creation", error) from None
    validate_release_record(result, args.tag, version, notes)
    verify_remote_tag()
    print(result["html_url"])


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        # No token/request headers are included in these sanitized guard/API errors.
        if os.environ.get("GITHUB_ACTIONS") == "true":
            message = str(error).replace("%", "%25").replace("\r", "%0D").replace("\n", "%0A")
            print(f"::error title=Source release rejected::{message}", flush=True)
        raise
