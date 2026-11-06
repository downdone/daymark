import importlib.util
from pathlib import Path
import struct
import unittest

spec = importlib.util.spec_from_file_location('native_imports', Path(__file__).resolve().parents[1] / 'scripts/audit-native-imports.py')
pe = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pe)


def image(name=b'KERNEL32.dll'):
    data = bytearray(1024)
    data[:2] = b'MZ'
    struct.pack_into('<I', data, 0x3c, 0x80)
    data[0x80:0x84] = b'PE\0\0'
    struct.pack_into('<HH', data, 0x84, 0x8664, 1)
    struct.pack_into('<H', data, 0x94, 240)
    struct.pack_into('<H', data, 0x98, 0x20b)
    struct.pack_into('<I', data, 0x98+108, 16)
    struct.pack_into('<II', data, 0x98+112+8, 0x1000, 40)
    struct.pack_into('<III', data, 0x98+240+12, 0x1000, 512, 512)
    struct.pack_into('<5I', data, 512, 0, 0, 0, 0x1060, 0)
    data[608:608+len(name)] = name
    return data


class NativeImportsTest(unittest.TestCase):
    def test_system_imports_allowed_external_qt_and_crt_rejected(self):
        self.assertEqual(pe.audit(image()), ['kernel32.dll'])
        for name in (b'Qt6Core.dll', b'VCRUNTIME140.dll', b'msvcp140.dll', b'api-ms-win-crt-runtime-l1-1-0.dll', b'evil.dll'):
            with self.subTest(name=name), self.assertRaises(ValueError): pe.audit(image(name))

    def test_bad_rva_truncation_and_delay_imports_are_rejected(self):
        bad = image(); struct.pack_into('<I', bad, 512+12, 0x9000)
        delay = image(); struct.pack_into('<II', delay, 0x98+112+13*8, 0x1000, 32)
        for data in (b'', image()[:520], bad, delay):
            with self.assertRaises(ValueError): pe.audit(data)


if __name__ == '__main__': unittest.main()
