"""Original SDK provenance is mandatory for every tagged archive."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location(
    "qt_sbom", Path(__file__).resolve().parents[1] / "scripts/copy-qt-sbom.py")
sbom = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sbom)


class QtSbomTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.kit = self.root / "kit"
        (self.kit / "sbom").mkdir(parents=True)
        self.output = self.root / "package" / "qt-sbom"

    def document(self, module, json_format):
        suffix = ".spdx.json" if json_format else ".spdx"
        path = self.kit / "sbom" / (module + "-6.10.2" + suffix)
        content = (json.dumps({"spdxVersion": "SPDX-2.3", "SPDXID": "SPDXRef-DOCUMENT",
                               "packages": [{"name": module, "versionInfo": "6.10.2"}]})
                   if json_format else "SPDXVersion: SPDX-2.3\nSPDXID: SPDXRef-DOCUMENT\nPackageName: " + module + "\n")
        path.write_bytes(content.encode())
        return path

    def test_preserves_original_bytes_for_both_formats_and_mixed_kits(self):
        for formats in ([False] * 4, [True] * 4, [False, True, False, True]):
            with self.subTest(formats=formats):
                originals = [self.document(m, f) for m, f in zip(sbom.MODULES, formats)]
                destination = self.root / ("result-" + str(formats))
                names = sbom.copy_documents(self.kit, destination)
                for source in originals:
                    self.assertIn(source.name, names)
                    self.assertEqual(source.read_bytes(), (destination / source.name).read_bytes())
                # Remove only synthetic inputs to keep each format fixture independent.
                for source in originals:
                    source.unlink()

    def test_missing_module_copies_nothing(self):
        for module in sbom.MODULES[:-1]:
            self.document(module, False)
        with self.assertRaisesRegex(ValueError, "qtsvg"):
            sbom.copy_documents(self.kit, self.output)
        self.assertFalse(self.output.exists())

    def test_malformed_json_is_not_hidden_by_text_fallback(self):
        for module in sbom.MODULES:
            self.document(module, False)
        self.document("qtbase", True).write_text("{}")
        with self.assertRaises(ValueError):
            sbom.copy_documents(self.kit, self.output)
        self.assertFalse(self.output.exists())

    def test_incomplete_text_document_is_rejected(self):
        for module in sbom.MODULES:
            self.document(module, False)
        self.document("qtsvg", False).write_text("SPDXVersion: SPDX-2.3\n")
        with self.assertRaisesRegex(ValueError, "Invalid SPDX"):
            sbom.copy_documents(self.kit, self.output)
        self.assertFalse(self.output.exists())

    def test_existing_output_is_never_overwritten(self):
        for module in sbom.MODULES:
            self.document(module, True)
        self.output.mkdir(parents=True)
        original = self.output / "qtsvg-6.10.2.spdx.json"
        original.write_bytes(b"existing")
        with self.assertRaisesRegex(ValueError, "overwritten"):
            sbom.copy_documents(self.kit, self.output)
        self.assertEqual([original], list(self.output.iterdir()))
        self.assertEqual(b"existing", original.read_bytes())


if __name__ == "__main__":
    unittest.main()
