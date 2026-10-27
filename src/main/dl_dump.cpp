// Debug only (subject E, matrix tags): walks the frame's display list and writes the G_MTX / G_DL structure to
// dl_dump.txt (in the current directory), once every 250 frames. Not called by default; to use it, add to 40winks.toml:
//   [[patches.hook]] func = "func_800030C0", text = "{ extern void winks_dl_dump(uint8_t*); winks_dl_dump(rdram); }",
//   before_vram = 0x80003158 (after G_ENDDL), then regenerate RecompiledFuncs/ and rebuild.
#include <cstdint>
#include <cstdio>

static uint32_t rd(const uint8_t* rdram, uint32_t phys) {
    return *(const uint32_t*)(rdram + (phys & 0x7FFFFC));
}

extern "C" void winks_dl_dump(uint8_t* rdram) {
    static int frame = 0;
    if (frame++ % 250 != 100) {
        return;
    }
    static FILE* f = fopen("dl_dump.txt", "w");
    if (f == nullptr) {
        return;
    }
    uint32_t seg[16] = {};
    uint32_t start = rd(rdram, 0x1D0760) + 0x300;
    uint32_t stack[16];
    int depth = 0;
    uint32_t pc = start & 0x7FFFFF;
    uint32_t dl_start = pc;
    int vtx = 0, tri = 0;
    uint32_t vmin = 0xFFFFFFFF, vmax = 0;
    fprintf(f, "==== frame %d, list 0x%08X\n", frame, start);
    for (int n = 0; n < 200000; n++, pc += 8) {
        uint32_t w0 = rd(rdram, pc), w1 = rd(rdram, pc + 4);
        uint8_t op = w0 >> 24;
        auto resolve = [&](uint32_t a) { return (seg[(a >> 24) & 0xF] + (a & 0xFFFFFF)) & 0x7FFFFF; };
        if (op == 0xDB && ((w0 >> 16) & 0xFF) == 6) {
            seg[((w0 & 0xFFFF) / 4) & 0xF] = w1 & 0xFFFFFF;
        } else if (op == 0x01) {
            vtx++;
            uint32_t v = resolve(w1);
            vmin = v < vmin ? v : vmin;
            vmax = v > vmax ? v : vmax;
            if (depth == 0) {
                uint32_t a = rd(rdram, v), b = rd(rdram, v + 4);
                fprintf(f, "  [+%06X] VTX n=%d %08X first=(%d,%d,%d)\n", pc, (w0 >> 12) & 0xFF, w1, (int16_t)(a >> 16),
                    (int16_t)a, (int16_t)(b >> 16));
            }
        } else if (op == 0xE4 || op == 0x64) {
            fprintf(f, "%*s[+%06X] %s %08X %08X\n", depth * 2, "", pc, op == 0xE4 ? "TEXRECT" : "EX", w0, w1);
        } else if (op == 0x05 || op == 0x06 || op == 0x07) {
            tri++;
        } else if (op == 0xDA || op == 0xD8) {
            fprintf(f, "%*s[dl %06X +%04X] %s %08X %08X (since last: vtx %d [%06X-%06X] tri %d)\n", depth * 2, "", dl_start,
                pc - dl_start, op == 0xDA ? "MTX" : "POP", w0, w1, vtx, vtx ? vmin : 0, vmax, tri);
            vtx = tri = 0;
            vmin = 0xFFFFFFFF, vmax = 0;
        } else if (op == 0xDE) {
            uint32_t target = resolve(w1);
            fprintf(f, "%*s[dl %06X +%04X] DL%s -> %08X (%06X)\n", depth * 2, "", dl_start, pc - dl_start,
                (w0 >> 16) & 0xFF ? " branch" : "", w1, target);
            if (((w0 >> 16) & 0xFF) == 0) {
                if (depth == 16) break;
                stack[depth++] = pc;
            }
            pc = target - 8;
            dl_start = target;
        } else if (op == 0xDF) {
            if (depth == 0) break;
            pc = stack[--depth];
            dl_start = 0;
        }
    }
    fprintf(f, "==== end (vtx %d tri %d after last)\n", vtx, tri);
    fflush(f);
}
