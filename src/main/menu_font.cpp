// The game's big menu letters, read from the player's ROM for the launcher (the release ships no game data).
// Each letter is an EDL-compressed resource (Eurocom format) described by a 24-byte record of the table at 0x8008AAD0
// in the game's data segment. The game's own recompiled decompressor (func_80013BCC) runs on a small private RDRAM
// holding the main segment. A decompressed sprite is a 16-color RGBA16 palette (0x20 bytes), then CI4 pixels, then a
// 0x48-byte trailer starting with the width and height (16-bit); letters are 32x32.
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include "recomp.h"
#include "menu_font.h"

extern "C" void func_80013BCC(uint8_t* rdram, recomp_context* ctx);

namespace {
    constexpr uint32_t rdram_size = 4 * 1024 * 1024;
    // Main segment: ROM 0x1050..0xB9220 at 0x80000450 (decomp/40winks.yaml); the bss stays zeroed.
    constexpr uint32_t main_rom_start = 0x1050, main_rom_end = 0xB9220, main_vram = 0x80000450;
    // Resource blob in ROM (D_AE4AA0 in func_8001A5B4); each resource is a raw 0x38-byte header then EDL data.
    constexpr uint32_t blob_rom = 0xAE4AA0, resource_header = 0x38;
    constexpr uint32_t src_vram = 0x80300000, dst_vram = 0x80340000, stack_vram = 0x803F0000;
    constexpr uint32_t records_vram = 0x8008AAD0, record_size = 0x18;
    constexpr int upscale = 4, trailer_size = 0x48;

    // Record index of each glyph (the table is not in alphabetical order). 26 is the underline, also the bar of the
    // options menu's sliders (whose knob is the letter I); 28-30 are "1P" / "2P".
    constexpr std::pair<const char *, int> glyph_records[] = {
        {"A", 0}, {"B", 1}, {"C", 2}, {"M", 3}, {"T", 4}, {"U", 5}, {"V", 6}, {"W", 7}, {"X", 8}, {"Y", 9}, {"Z", 10},
        {"D", 11}, {"E", 12}, {"F", 13}, {"G", 14}, {"H", 15}, {"I", 16}, {"J", 17}, {"L", 18}, {"K", 19}, {"N", 20},
        {"O", 21}, {"P", 22}, {"Q", 23}, {"R", 24}, {"S", 25}, {"!", 27}, {"0", 31}, {"1", 32}, {"2", 33}, {"3", 34},
        {"4", 35}, {"5", 36}, {"6", 37}, {"7", 38}, {"8", 39}, {"9", 40}, {"sparkle", 41}, {"underline", 26},
    };

    uint32_t be32(const std::vector<uint8_t> &rom, uint32_t at) {
        return (uint32_t)rom[at] << 24 | (uint32_t)rom[at + 1] << 16 | (uint32_t)rom[at + 2] << 8 | rom[at + 3];
    }

    // RDRAM as the recompiled code sees it: 0x80000000 at index 0, bytes of each 32-bit word reversed.
    uint8_t &rdram_byte(uint8_t *rdram, uint32_t vram) { return rdram[(vram - 0x80000000) ^ 3]; }

    // Decompresses one record's resource. Returns false if the ROM data is out of range or the decompressor fails.
    bool decompress(const std::vector<uint8_t> &rom, uint8_t *rdram, int index, std::vector<uint8_t> &out) {
        uint32_t record = records_vram + index * record_size - main_vram + main_rom_start;
        uint32_t offset = be32(rom, record), size = be32(rom, record + 8), packed = be32(rom, record + 0xC);
        uint32_t start = blob_rom + offset + resource_header;
        if (start + packed > rom.size() || packed > dst_vram - src_vram || size > stack_vram - 0x10000 - dst_vram) {
            return false;
        }
        for (uint32_t i = 0; i < packed; i++) {
            rdram_byte(rdram, src_vram + i) = rom[start + i];
        }
        recomp_context ctx{};
        ctx.f_odd = &ctx.f0.u32h;
        ctx.r4 = (gpr)(int32_t)src_vram;
        ctx.r5 = (gpr)(int32_t)dst_vram;
        ctx.r29 = (gpr)(int32_t)stack_vram;
        func_80013BCC(rdram, &ctx);
        if ((int32_t)ctx.r2 < 0) {
            return false;
        }
        out.resize(size);
        for (uint32_t i = 0; i < size; i++) {
            out[i] = rdram_byte(rdram, dst_vram + i);
        }
        return true;
    }

    // CI4 with an RGBA5551 palette -> RGBA8, scaled up 4x (nearest), letters cropped to their opaque columns.
    MenuGlyph decode(const std::vector<uint8_t> &data, bool brightness_to_alpha, bool crop) {
        MenuGlyph glyph;
        if (data.size() < 0x20 + trailer_size) {
            return glyph;
        }
        const size_t trailer = data.size() - trailer_size;
        const int w = data[trailer] << 8 | data[trailer + 1], h = data[trailer + 2] << 8 | data[trailer + 3];
        if (w <= 0 || h <= 0 || 0x20 + (size_t)w * h / 2 > trailer) {
            return glyph;
        }
        uint8_t palette[16][4];
        for (int i = 0; i < 16; i++) {
            uint16_t v = data[i * 2] << 8 | data[i * 2 + 1];
            uint8_t r = ((v >> 11) & 31) * 255 / 31, g = ((v >> 6) & 31) * 255 / 31, b = ((v >> 1) & 31) * 255 / 31;
            uint8_t a = (v & 1) ? 255 : 0;
            if (brightness_to_alpha) {
                // The sparkle is drawn additively in the game (opaque dark square): brightness becomes alpha.
                uint8_t m = std::max({r, g, b});
                a = m;
                r = m ? r * 255 / m : 0;
                g = m ? g * 255 / m : 0;
                b = m ? b * 255 / m : 0;
            }
            palette[i][0] = r; palette[i][1] = g; palette[i][2] = b; palette[i][3] = a;
        }
        auto texel = [&](int x, int y) {
            uint8_t byte = data[0x20 + (y * w + x) / 2];
            return palette[(x & 1) ? (byte & 15) : (byte >> 4)];
        };
        int left = crop ? w : 0, right = crop ? 0 : w;
        for (int x = 0; crop && x < w; x++) {
            for (int y = 0; y < h; y++) {
                if (texel(x, y)[3] != 0) {
                    left = std::min(left, x);
                    right = std::max(right, x + 1);
                }
            }
        }
        if (right <= left) {
            left = 0;
            right = w;
        }
        glyph.width = (right - left) * upscale;
        glyph.height = h * upscale;
        glyph.rgba.resize(glyph.width * glyph.height * 4);
        for (int y = 0; y < glyph.height; y++) {
            for (int x = 0; x < glyph.width; x++) {
                std::memcpy(&glyph.rgba[(y * glyph.width + x) * 4], texel(left + x / upscale, y / upscale), 4);
            }
        }
        return glyph;
    }
}

std::map<std::string, MenuGlyph> menu_font_load(const std::filesystem::path &rom_path) {
    std::map<std::string, MenuGlyph> glyphs;
    std::ifstream file(rom_path, std::ios::binary);
    std::vector<uint8_t> rom((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (rom.size() < main_rom_end) {
        return glyphs;
    }
    auto rdram = std::make_unique<uint8_t[]>(rdram_size);
    std::memset(rdram.get(), 0, rdram_size);
    for (uint32_t i = 0; i < main_rom_end - main_rom_start; i++) {
        rdram_byte(rdram.get(), main_vram + i) = rom[main_rom_start + i];
    }
    std::vector<uint8_t> data;
    for (auto [name, index] : glyph_records) {
        if (decompress(rom, rdram.get(), index, data)) {
            bool letter = std::strlen(name) == 1;
            glyphs[name] = decode(data, std::strcmp(name, "sparkle") == 0, letter);
        }
    }
    return glyphs;
}
