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


def release_input(root, tag):
    if not re.fullmatch(r"v[0-9]+\.[0-9]+\.[0-9]+", tag):
        raise ValueError("A stable vMAJOR.MINOR.PATCH tag is required")
    version = tag[1:]
    cmake = (root / "CMakeLists.txt").read_text(encoding="utf-8")
    if f'set(DAYMARK_VERSION "{version}")' not in cmake:
        raise ValueError("Tag does not match application version")
    minor = ".".join(version.split(".")[:2])
    notes = (root / f"docs/release-notes-v{minor}.md").read_text(encoding="utf-8")
    if not notes.startswith(f"# Daymark {version}") or "仅发布源码" not in notes:
        raise ValueError("Matching source-only release notes are required")
    return version, notes


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tag", required=True)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    version, notes = release_input(root, args.tag)
    if args.check:
        print(f"PASS: {args.tag} matches application version and source-only notes")
        return
    if os.environ.get("GITHUB_REF") != f"refs/tags/{args.tag}":
        raise ValueError("Publishing is restricted to the matching tag workflow")
    repository = os.environ["GITHUB_REPOSITORY"]
    if not re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", repository):
        raise ValueError("Invalid repository")
    revision = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip()
    if revision != os.environ["GITHUB_SHA"]:
        raise ValueError("Checkout does not match triggering commit")
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

    try:
        existing = request(base + "/tags/" + args.tag)
    except urllib.error.HTTPError as error:
        if error.code != 404:
            raise RuntimeError(f"Release lookup failed (HTTP {error.code})") from None
    else:
        if (existing["target_commitish"] != revision or existing["draft"]
                or existing["prerelease"] or existing["assets"]):
            raise ValueError("An incompatible release already exists; it was not modified")
        print(existing["html_url"])
        return
    try:
        result = request(base, {"tag_name": args.tag, "target_commitish": revision,
            "name": f"Daymark {version}", "body": notes, "draft": False,
            "prerelease": False, "make_latest": "true"})
    except urllib.error.HTTPError as error:
        raise RuntimeError(f"Release creation failed (HTTP {error.code})") from None
    if result["tag_name"] != args.tag or result["assets"]:
        raise RuntimeError("Unexpected release response")
    print(result["html_url"])


if __name__ == "__main__":
    main()
