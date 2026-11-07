"""Reject Qt/CRT/external imports in all supplied OS-only Windows hosts."""
import argparse
from pathlib import Path
import struct

SYSTEM = {'advapi32.dll', 'bcrypt.dll', 'comctl32.dll', 'comdlg32.dll', 'crypt32.dll',
          'dwmapi.dll', 'gdi32.dll', 'imm32.dll', 'kernel32.dll', 'kernelbase.dll',
          'msvcrt.dll', 'ntdll.dll', 'ole32.dll', 'oleaut32.dll', 'rpcrt4.dll',
          'secur32.dll', 'shell32.dll', 'shlwapi.dll', 'user32.dll', 'userenv.dll',
          'uxtheme.dll', 'version.dll', 'winhttp.dll', 'ws2_32.dll'}


def imports(data: bytes) -> list[str]:
    def u16(offset): return struct.unpack_from('<H', data, offset)[0]
    def u32(offset): return struct.unpack_from('<I', data, offset)[0]
    try:
        if data[:2] != b'MZ': raise ValueError('invalid DOS header')
        pe = u32(0x3c)
        if data[pe:pe+4] != b'PE\0\0' or u16(pe+4) != 0x8664:
            raise ValueError('x64 PE image required')
        count, size = u16(pe+6), u16(pe+20)
        optional = pe+24
        if u16(optional) != 0x20b or size < 240 or not 1 <= count <= 96 or not 14 <= u32(optional+108) <= 16:
            raise ValueError('invalid PE32+ header')
        sections = optional+size
        def raw(rva):
            for i in range(count):
                section = sections+i*40
                address, length, offset = u32(section+12), u32(section+16), u32(section+20)
                if address <= rva < address+length:
                    result = offset+rva-address
                    if result < len(data): return result
            raise ValueError('unmapped import RVA')
        directories = optional+112
        if u32(directories+13*8) or u32(directories+13*8+4):
            raise ValueError('delay-loaded imports require separate review')
        address, length = u32(directories+8), u32(directories+12)
        if not address or not 20 <= length <= 1024*20: raise ValueError('invalid imports')
        start = raw(address)
        result = []
        for position in range(start, start+length, 20):
            descriptor = struct.unpack_from('<5I', data, position)
            if descriptor == (0,0,0,0,0): return result
            name = raw(descriptor[3])
            end = data.find(b'\0', name, min(name+260,len(data)))
            if end < 0: raise ValueError('unterminated DLL name')
            result.append(data[name:end].decode('ascii').lower())
        raise ValueError('unterminated import descriptors')
    except (struct.error, UnicodeError) as error:
        raise ValueError('malformed PE imports') from error


def audit(data: bytes) -> list[str]:
    names = imports(data)
    for name in names:
        if name not in SYSTEM and not name.startswith(('api-ms-win-core-', 'api-ms-win-security-', 'ext-ms-win-')):
            raise ValueError('external runtime import rejected: '+name)
    return names


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('files', type=Path, nargs='+')
    for path in parser.parse_args().files:
        print('PASS:', path.name, ', '.join(audit(path.read_bytes())))
