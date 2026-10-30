"""Copy the four original Qt SBOMs, accepting SPDX JSON or tag/value format."""
import argparse
import json
import os
from pathlib import Path
import re
import shutil


MODULES = ("qtbase", "qtdeclarative", "qtshadertools", "qtsvg")


def validate_document(path):
    content = path.read_text(encoding="utf-8-sig")
    if path.suffix == ".json":
        document = json.loads(content)
        valid = (isinstance(document, dict)
                 and re.fullmatch(r"SPDX-2\.[23]", str(document.get("spdxVersion", "")))
                 and document.get("SPDXID") == "SPDXRef-DOCUMENT"
                 and isinstance(document.get("packages"), list)
                 and bool(document["packages"]))
    else:
        valid = (re.search(r"^SPDXVersion:\s*SPDX-2\.[23]\s*$", content, re.M)
                 and re.search(r"^SPDXID:\s*SPDXRef-DOCUMENT\s*$", content, re.M)
                 and re.search(r"^PackageName:\s*\S", content, re.M))
    if not valid:
        raise ValueError(f"Invalid SPDX document: {path.name}")


def copy_documents(qt_root, destination, version="6.10.2"):
    if not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", version):
        raise ValueError("An exact Qt version is required")
    selected = []
    for module in MODULES:
        stem = f"{module}-{version}.spdx"
        candidates = [qt_root / "sbom" / (stem + ".json"), qt_root / "sbom" / stem]
        source = next((path for path in candidates if path.is_file()), None)
        if source is None:
            raise ValueError(f"Required Qt SBOM missing: {stem} or {stem}.json")
        validate_document(source)
        selected.append(source)
    # Validate the complete set before creating a directory or copying anything.
    names = [path.name for path in selected] + ["README.txt"]
    if any((destination / name).exists() for name in names):
        raise ValueError("SBOM destination already contains output; nothing was overwritten")
    destination.mkdir(parents=True, exist_ok=True)
    for source in selected:
        shutil.copyfile(source, destination / source.name)
    (destination / "README.txt").write_text(
        "Original Qt module SBOMs copied without modification from the matching Kit.\n"
        "SPDX tag/value (.spdx) and SPDX JSON (.spdx.json) are supported.\n"
        "See each document for its package copyrights and license references.\n",
        encoding="utf-8")
    return [path.name for path in selected]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--qt-root", type=Path, required=True)
    parser.add_argument("--destination", type=Path, required=True)
    parser.add_argument("--version", default="6.10.2")
    args = parser.parse_args()
    try:
        names = copy_documents(args.qt_root, args.destination, args.version)
    except (ValueError, OSError) as error:
        message = str(error)
        if os.environ.get("GITHUB_ACTIONS") == "true":
            message = message.replace("%", "%25").replace("\r", "%0D").replace("\n", "%0A")
            print(f"::error title=Qt SBOM rejected::{message}")
        parser.exit(1, f"Qt SBOM rejected: {error}\n")
    print("PASS: copied four original Qt SBOMs: " + ", ".join(names))


if __name__ == "__main__":
    main()
