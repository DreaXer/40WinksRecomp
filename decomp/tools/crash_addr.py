"""Map a Windows crash "Fault offset" of 40WinksRecompiled.exe to the function containing it.

Usage: python -I decomp/tools/crash_addr.py build-vs/40WinksRecompiled.map 0x1e16fc
"""
import re
import sys

base, target, best = 0x140000000, None, None
for line in open(sys.argv[1], errors="ignore"):
    m = re.match(r"\s*[0-9a-f]{4}:[0-9a-f]{8}\s+(\S+)\s+([0-9a-f]{16})\s+(.*)", line)
    if not m:
        continue
    if target is None:
        target = base + int(sys.argv[2], 16)
    addr = int(m.group(2), 16)
    if addr <= target and (best is None or addr > best[0]):
        best = (addr, m.group(1), m.group(3).strip())
print("%s (%s) +0x%X" % (best[1], best[2], target - best[0]))
