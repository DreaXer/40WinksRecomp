#pragma once

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

struct MenuGlyph {
    int width = 0, height = 0;
    std::vector<char> rgba;
};

// Reads the game's menu letters from the ROM file: "A".."Z", "0".."9", "!", "sparkle" (the menu cursor star,
// brightness turned into alpha) and "underline" (64x10, the options sliders' bar). Each is RGBA8, 4x the original;
// letters (32 px) are cropped to their opaque columns (the width is the letter's advance). Returns an empty map if the
// ROM cannot be read.
std::map<std::string, MenuGlyph> menu_font_load(const std::filesystem::path &rom_path);
