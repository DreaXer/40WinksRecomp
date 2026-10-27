"""Print each function's callees, callers and hardware register blocks, with known names substituted.

Usage: python -I tools/callgraph.py asm/1050.s known.txt [vram_lo vram_hi]
known.txt lines: "name = 0xADDR;" (symbol_addrs.txt format).
"""
import re
import sys
from collections import defaultdict

HW = {0xA404: "SP", 0xA410: "DPC", 0xA430: "MI", 0xA440: "VI", 0xA450: "AI", 0xA460: "PI", 0xA470: "RI", 0xA480: "SI", 0xBFC0: "PIF"}
known = {}
for line in open(sys.argv[2]):
    m = re.match(r"\s*(\w+)\s*=\s*0x([0-9A-Fa-f]+)", line)
    if m:
        known[int(m.group(2), 16)] = m.group(1)
lo, hi = (int(sys.argv[3], 16), int(sys.argv[4], 16)) if len(sys.argv) > 4 else (0, 0xFFFFFFFF)
name = lambda a: known.get(a, "func_%08X" % a)
calls, hw, size, cur = defaultdict(list), defaultdict(set), {}, None
for line in open(sys.argv[1], encoding="utf-8"):
    if line.startswith("glabel func_"):
        cur = int(line.split()[1][5:], 16)
        size[cur] = 0
    elif cur is not None:
        m = re.match(r"\s*/\* [0-9A-F]+ [0-9A-F]{8} ([0-9A-F]{8}) \*/", line)
        if not m:
            continue
        w = int(m.group(1), 16)
        size[cur] += 4
        if w >> 26 == 3:
            calls[cur].append(0x80000000 | (w & 0x3FFFFFF) << 2)
        elif w >> 26 == 0xF and (w & 0xFFFF) in HW:
            hw[cur].add(HW[w & 0xFFFF])
        elif w >> 21 == 0x200 or w >> 21 == 0x204:
            hw[cur].add("CP0")
callers = defaultdict(set)
for f, cs in calls.items():
    for c in cs:
        callers[c].add(f)
for f in sorted(size):
    if lo <= f < hi:
        print("%08X %-26s sz=%-5X hw=%-10s calls=[%s] callers=%d" % (
            f, name(f), size[f], ",".join(sorted(hw[f])), " ".join(dict.fromkeys(name(c) for c in calls[f])), len(callers[f])))
