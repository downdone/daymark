"""Collect the selected Go module licence/notice texts without local paths."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT / "server/licenses"


def modules():
    required = {match[0] for match in re.findall(r"^\s*(\S+)\s+(v\S+)", (ROOT / "server/go.mod").read_text(), re.M)}
    raw = subprocess.check_output(["go", "list", "-m", "-json", "all"], cwd=ROOT / "server", text=True, encoding="utf-8")
    decoder = json.JSONDecoder()
    selected = {}
    while raw.strip():
        module, end = decoder.raw_decode(raw.lstrip())
        raw = raw.lstrip()[end:]
        if module["Path"] in required:
            selected[module["Path"]] = module
    assert set(selected) == required, "module list differs from go.mod"
    yield from (selected[name] for name in sorted(selected))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    inventory = []
    for module in modules():
        assert "Replace" not in module, "review replaced modules explicitly"
        name = re.sub(r"[^a-zA-Z0-9.-]", "_", module["Path"] + "@" + module["Version"])
        notices = []
        for source in sorted(Path(module["Dir"]).iterdir()):
            if source.is_file() and re.match(r"^(LICENSE|LICENCE|COPYING|NOTICE)([._-]|$)", source.name, re.I):
                content = source.read_bytes()
                destination = DEST / name / source.name
                if args.check:
                    assert destination.read_bytes() == content, f"notice drift: {module['Path']}"
                else:
                    destination.parent.mkdir(parents=True, exist_ok=True)
                    destination.write_bytes(content)
                notices.append({"file": destination.relative_to(ROOT / "server").as_posix(),
                                "sha256": hashlib.sha256(content).hexdigest()})
        assert notices, f"missing notice: {module['Path']}"
        inventory.append({"module": module["Path"], "version": module["Version"],
                          "sum": module["Sum"], "notices": notices})
    manifest = json.dumps({"modules": inventory}, ensure_ascii=True, indent=2) + "\n"
    target = DEST / "modules.json"
    if args.check:
        assert target.read_text(encoding="utf-8") == manifest, "module inventory drifted"
    else:
        target.write_text(manifest, encoding="utf-8", newline="\n")
    print(f"PASS: {len(inventory)} selected module notices and checksums")


if __name__ == "__main__":
    main()
