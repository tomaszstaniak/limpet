#!/usr/bin/env python3
"""Match a dos_identity_probe report against the dos.library variant files.

The probe dumps resident code at fixed .text offsets. In each variant file
those bytes sit in the relocatable ELF's .text section; bytes covered by a
relocation are placeholders in the file and resolved in memory, so they are
masked. A variant matches when every unmasked byte agrees in both windows.

Usage: dos_identity_compare.py OBJDUMP REPORT VARIANT_FILE...
"""
import hashlib
import re
import subprocess
import sys
from pathlib import Path

# Must equal the offsets compiled into tests/dos_identity_probe.c.
WINDOWS = {'dos_open_bytes': (0xaff0, 64), 'fs_open_bytes': (0x13560, 160)}
RELOC_SIZE = {'R_X86_64_64': 8, 'R_X86_64_PC32': 4, 'R_X86_64_PLT32': 4,
              'R_X86_64_32': 4, 'R_X86_64_32S': 4, 'R_X86_64_GOTPCREL': 4}


def text_section(objdump, path):
    out = subprocess.run([objdump, '-h', str(path)], capture_output=True, text=True, check=True).stdout
    for line in out.splitlines():
        parts = line.split()
        if len(parts) >= 7 and parts[1] == '.text':
            return int(parts[2], 16), int(parts[5], 16)
    raise ValueError('no .text in ' + str(path))


def relocations(objdump, path):
    out = subprocess.run([objdump, '-r', '-j', '.text', str(path)], capture_output=True, text=True, check=True).stdout
    masked = set()
    for line in out.splitlines():
        m = re.match(r'^([0-9a-f]{8,16})\s+(R_X86_64_\w+)', line)
        if not m:
            continue
        size = RELOC_SIZE.get(m.group(2))
        if size is None:
            raise ValueError('unhandled relocation type ' + m.group(2))
        start = int(m.group(1), 16)
        masked.update(range(start, start + size))
    return masked


def reference(objdump, path):
    size, offset = text_section(objdump, path)
    data = Path(path).read_bytes()[offset:offset + size]
    masked = relocations(objdump, path)
    windows = {}
    for key, (start, length) in WINDOWS.items():
        windows[key] = [(None if start + i in masked else data[start + i]) for i in range(length)]
    return windows


def parse_report(path):
    report = {}
    for line in Path(path).read_text(errors='replace').splitlines():
        key, _, value = line.partition(' ')
        report[key] = value.strip()
    return report


def main():
    if len(sys.argv) < 4:
        print(__doc__)
        return 2
    objdump, report_path, variants = sys.argv[1], sys.argv[2], sys.argv[3:]
    report = parse_report(report_path)
    if report.get('probe_end', None) is None and 'probe_end' not in Path(report_path).read_text():
        print('report incomplete: no probe_end line')
        return 1
    print('report', report_path)
    for key in ('file', 'file_bytes', 'file_sha256', 'resident_version', 'resident_idstring'):
        print(' ', key, report.get(key))
    matches = []
    for variant in variants:
        digest = hashlib.sha256(Path(variant).read_bytes()).hexdigest()
        ref = reference(objdump, variant)
        verdicts = []
        for key, expected in ref.items():
            dumped = bytes.fromhex(report[key])
            differ = [i for i, e in enumerate(expected) if e is not None and dumped[i] != e]
            verdicts.append((key, len(differ), sum(e is None for e in expected)))
        ok = all(d == 0 for _, d, _ in verdicts)
        file_ok = report.get('file_sha256') == digest
        detail = ', '.join('%s: %d differing, %d masked' % v for v in verdicts)
        print('  %-28s resident %-8s file %-8s (%s)' % (Path(variant).name,
              'MATCH' if ok else 'differs', 'MATCH' if file_ok else 'differs', detail))
        if ok:
            matches.append(Path(variant).name)
    print('resident matches:', ', '.join(matches) if matches else 'none')
    return 0


if __name__ == '__main__':
    sys.exit(main())
