"""Inspect the actual face 0 used by FreeType without changing or subsetting its glyphs."""
import argparse
import gzip
import hashlib
import json
import struct
from pathlib import Path

def inspect_font(path, required):
    data = Path(path).read_bytes()
    if len(data) > 64 * 1024 * 1024:
        raise ValueError('Font exceeds the hosted 64 MiB limit')
    u16 = lambda at: struct.unpack_from('>H', data, at)[0]
    u32 = lambda at: struct.unpack_from('>I', data, at)[0]
    face = u32(12) if data[:4] == b'ttcf' else 0
    if data[face:face + 4] not in (b'\x00\x01\x00\x00', b'OTTO', b'true'):
        raise ValueError('Expected SFNT TTF/OTF or TTC face 0')
    tables = {}
    for i in range(u16(face + 4)):
        at = face + 12 + i * 16
        offset, size = u32(at + 8), u32(at + 12)
        if offset + size > len(data):
            raise ValueError('Truncated font table')
        tables[data[at:at + 4]] = offset
    names = {}
    name = tables.get(b'name')
    if name is not None:
        for i in range(u16(name + 2)):
            at = name + 6 + i * 12
            platform, encoding, language, identity, length, offset = struct.unpack_from('>6H', data, at)
            if identity not in (0, 1, 2, 5, 13, 14):
                continue
            raw = data[name + u16(name + 4) + offset:name + u16(name + 4) + offset + length]
            value = raw.decode('utf-16-be' if platform in (0, 3) else 'mac_roman', errors='replace')
            names.setdefault(str(identity), [])
            if value not in names[str(identity)]:
                names[str(identity)].append(value)
    cmap = tables[b'cmap']
    subtables = []
    for i in range(u16(cmap + 2)):
        at = cmap + 4 + i * 8
        platform, encoding = u16(at), u16(at + 2)
        if platform == 0 or (platform == 3 and encoding in (1, 10)):
            subtables.append(cmap + u32(at + 4))
    def covered(code):
        for start in subtables:
            fmt = u16(start)
            if fmt == 12:
                for i in range(u32(start + 12)):
                    first, last, glyph = struct.unpack_from('>3I', data, start + 16 + i * 12)
                    if first <= code <= last and glyph + code - first:
                        return True
            elif fmt == 4 and code <= 0xffff:
                count = u16(start + 6) // 2
                for i in range(count):
                    if u16(start + 16 + count * 2 + i * 2) <= code <= u16(start + 14 + i * 2):
                        delta = u16(start + 16 + count * 4 + i * 2)
                        address = start + 16 + count * 6 + i * 2
                        offset = u16(address)
                        glyph = u16(address + offset + 2 * (code - u16(start + 16 + count * 2 + i * 2))) if offset else code
                        if glyph and ((glyph + delta) & 0xffff):
                            return True
        return False
    chars = sorted(set(required) - set('\n\r\t'))
    missing = [f'U+{ord(c):04X} {c}' for c in chars if not covered(ord(c))]
    return {'file': str(Path(path).resolve()), 'sha256': hashlib.sha256(data).hexdigest(),
            'face': 0, 'bytes': len(data), 'gzipBytes': len(gzip.compress(data, compresslevel=6, mtime=0)),
            'names': names, 'os2FsType': u16(tables[b'OS/2'] + 8) if b'OS/2' in tables else None,
            'requiredCharacters': len(chars), 'missing': missing,
            'licenseStatus': 'Not inferred: font metadata/embedding bits do not grant redistribution rights.',
            'subsetting': 'None; arbitrary user-entered Chinese coverage is preserved.'}

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('font', type=Path)
    parser.add_argument('--text', type=Path, help='UTF-8 application corpus; defaults to the regression baseline')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    required = args.text.read_text(encoding='utf-8') if args.text else ''.join(map(chr, range(32, 127))) + '中文让世界更美好候选'
    report = inspect_font(args.font, required)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps({k: report[k] for k in ('bytes', 'gzipBytes', 'requiredCharacters', 'missing')}, ensure_ascii=False))
    if report['missing']:
        raise SystemExit('Required glyphs are missing')
