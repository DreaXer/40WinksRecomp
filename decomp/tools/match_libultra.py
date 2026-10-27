"""Identify libultra/libc functions in 40 Winks by matching against a reference ELF with symbols.

Usage: python -I tools/match_libultra.py <reference.elf> asm/1050.s > libultra_matches.txt

Instructions are compared with relocatable fields masked (jal/j targets, lui and
%lo immediates), like n64sym. Only names that look like library code are kept.
"""
import re
import struct
import sys
from collections import defaultdict

LIB_NAME = re.compile(
    r"^(os|__os|__ll|__d_to|__f_to|__ull|__ul|__ll|_Lito|_Ldto|_Litob|_Ldtob|_Printf|_Putfld|_Genld|_Ldunscale|"
    r"al[A-Z]|_al|n_al|__al|alSyn|__n_|gu[A-Z]|bcopy|bzero|bcmp|memcpy|memset|memmove|strlen|strchr|strcmp|strcpy|"
    r"sprintf|vsprintf|sqrtf|sinf|cosf|sins|coss|__cosf|__sinf|send_mesg|__send_mesg|__Vi|__vi|__Pi|__pi|__Si|__si|__Cont|__cont|"
    r"__Ai|__ai|__Sp|__sp|__Rdb|__rdb|__dp|__Dp|_VirtualToPhysical|__ptExceptionHandler|__ptException|"
    r"osVirtualToPhysical|osPhysicalToVirtual|__createSpeedParam|__osRealloc|__osMalloc|__osFree)"
)
MASK_IMM_OPS = {0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F} | set(range(0x20, 0x40))


def normalize(words):
    words = list(words)
    while words and words[-1] == 0:  # alignment padding after jr ra; nop
        words.pop()
    out = []
    for w in words:
        op = w >> 26
        if op in (0x02, 0x03):  # j / jal
            w &= 0xFC000000
        elif op in MASK_IMM_OPS:  # immediates that may carry %hi/%lo relocations
            w &= 0xFFFF0000
        out.append(w)
    return tuple(out)


def read_ref(path):
    d = open(path, "rb").read()
    assert d[:4] == b"\x7fELF" and d[4] == 1 and d[5] == 2, "expected 32-bit big-endian ELF"
    shoff, = struct.unpack_from(">I", d, 0x20)
    shentsize, shnum = struct.unpack_from(">HH", d, 0x2E)
    secs = [struct.unpack_from(">IIIIIIIIII", d, shoff + i * shentsize) for i in range(shnum)]
    symtab = next(s for s in secs if s[1] == 2)
    strtab = secs[symtab[6]]
    progbits = [s for s in secs if s[1] == 1 and s[3] and (s[2] & 4)]  # PROGBITS, executable
    funcs = {}
    for i in range(symtab[5] // 16):
        name_off, value, size, info, _, _ = struct.unpack_from(">IIIBBH", d, symtab[4] + i * 16)
        if (info & 0xF) != 2 or size < 8 or size % 4:
            continue
        end = d.index(b"\0", strtab[4] + name_off)
        name = d[strtab[4] + name_off:end].decode()
        if "." in name or not LIB_NAME.match(name):
            continue
        sec = next((s for s in progbits if s[3] <= value and value + size <= s[3] + s[5]), None)
        if sec is None:
            continue
        off = sec[4] + value - sec[3]
        words = struct.unpack_from(">%dI" % (size // 4), d, off)
        funcs.setdefault(normalize(words), set()).add(name)
    return funcs


def read_target(path):
    funcs, cur, words, start = [], None, [], 0
    for line in open(path, encoding="utf-8"):
        if line.startswith("glabel "):
            cur, words = line.split()[1], []
        elif line.startswith("endlabel ") and cur:
            funcs.append((cur, words, start))
            cur = None
        elif cur:
            m = re.match(r"\s*/\* [0-9A-F]+ ([0-9A-F]{8}) ([0-9A-F]{8}) \*/", line)
            if m:
                if not words:
                    start = int(m.group(1), 16)
                words.append(int(m.group(2), 16))
    return funcs


def main():
    ref = read_ref(sys.argv[1])
    by_name = defaultdict(list)
    for name, words, vram in read_target(sys.argv[2]):
        if len(words) < 3:
            continue
        names = ref.get(normalize(words))
        if names:
            for n in sorted(names):
                by_name[n].append(vram)
            print("%s 0x%08X size:0x%X %s" % (name, vram, len(words) * 4, "|".join(sorted(names))))
    dup = {n: v for n, v in by_name.items() if len(v) > 1}
    for n, v in dup.items():
        print("# WARNING %s matched %d times: %s" % (n, len(v), " ".join("0x%08X" % x for x in v)), file=sys.stderr)
    print("# %d functions matched, %d reference signatures" % (len(by_name), len(ref)), file=sys.stderr)


if __name__ == "__main__":
    main()
