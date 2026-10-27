"""Rebuild the ROM from the splat output and check it against the original.

Usage (from decomp/): python -I tools/build_rom.py
Needs MSYS2 mingw64 clang (MIPS backend), ld.lld and llvm-objcopy.
"""
import hashlib
import pathlib
import subprocess
import sys

BIN = pathlib.Path(r"C:\msys64\mingw64\bin")
CLANG, OBJCOPY = BIN / "clang.exe", BIN / "llvm-objcopy.exe"
LLD = pathlib.Path(r"C:\Program Files\LLVM\bin\ld.lld.exe")  # ELF flavour; the MSYS2 one is MinGW-only
ROM, ELF, OUT = pathlib.Path("../baserom.eur.z64"), pathlib.Path("build/40winks.elf"), pathlib.Path("build/40winks.z64")
AS_FLAGS = ["--target=mips-unknown-elf", "-march=mips3", "-mabi=32", "-mno-abicalls", "-fno-pic", "-G0", "-modd-spreg",
            "-c", "-x", "assembler-with-cpp", "-Iinclude"]


def run(cmd):
    r = subprocess.run([str(c) for c in cmd], capture_output=True, text=True)
    if r.returncode:
        sys.exit("FAILED: %s\n%s%s" % (" ".join(map(str, cmd)), r.stdout, r.stderr))


def main():
    for src in sorted(pathlib.Path("asm").rglob("*.s")):
        obj = pathlib.Path("build") / src.with_suffix(".o")
        obj.parent.mkdir(parents=True, exist_ok=True)
        # LLVM's assembler rejects the GNU-only ".set gp=64"; it does not change the encoding here
        pre = obj.with_suffix(".pre.s")
        pre.write_text(src.read_text(encoding="utf-8").replace(".set gp=64\n", ""), encoding="utf-8")
        run([CLANG, *AS_FLAGS, pre, "-o", obj])
    for src in sorted(pathlib.Path("assets").rglob("*.bin")):
        obj = pathlib.Path("build") / src.with_suffix(".o")
        obj.parent.mkdir(parents=True, exist_ok=True)
        inc = obj.with_suffix(".s")  # .incbin keeps the same ELF flags as the asm objects
        inc.write_text('.section .data\n.incbin "%s"\n' % src.resolve().as_posix(), encoding="utf-8")
        run([CLANG, *AS_FLAGS, inc, "-o", obj])
    run([LLD, "-T", "40winks.ld", "-T", "undefined_syms_auto.txt", "-T", "undefined_funcs_auto.txt", "-T", "libultra_syms.ld", "-T", "extra_syms.ld",
         "--no-check-sections", "-Map", "build/40winks.map", "-o", ELF,
         *sorted(p.as_posix() for p in pathlib.Path("build").rglob("*.o"))])  # posix paths: the ld script matches build/asm/x.o
    run([OBJCOPY, "-O", "binary", ELF, OUT])

    built, orig = OUT.read_bytes(), ROM.read_bytes()
    print("built  %s %s" % (hashlib.sha1(built).hexdigest(), len(built)))
    print("target %s %s" % (hashlib.sha1(orig).hexdigest(), len(orig)))
    if built == orig:
        print("OK: ROM matches")
        return
    diffs = [i for i in range(0, min(len(built), len(orig)), 4) if built[i:i + 4] != orig[i:i + 4]]
    print("MISMATCH: %d differing words, size diff %d" % (len(diffs), len(built) - len(orig)))
    for i in diffs[:20]:
        print("  0x%06X built %s orig %s" % (i, built[i:i + 4].hex(), orig[i:i + 4].hex()))
    sys.exit(1)


if __name__ == "__main__":
    main()
