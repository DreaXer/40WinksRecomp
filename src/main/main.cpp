// 40 Winks : Dreamcompiled. Runtime glue, adapted from dk64recomp's src/main/main.cpp without the DK64-specific parts
// (patches, co-op, launcher animation, custom config).
#include <cstdio>
#include <cstring>
#include <cassert>
#include <vector>
#include <array>
#include <filesystem>
#include <stdexcept>
#include <cinttypes>
#include <ctime>
#include <chrono>
#include <fstream>
#include <algorithm>
#include <map>
#include <optional>
#include <cmath>
#include <tuple>
#include <atomic>
#include <variant>

#include "nfd.h"

#include "ultramodern/ultra64.h"
#include "ultramodern/ultramodern.hpp"
#include "ultramodern/config.hpp"
#define SDL_MAIN_HANDLED
#ifdef _WIN32
#include "SDL.h"
#else
#include "SDL2/SDL.h"
#include "SDL2/SDL_syswm.h"
#undef None
#undef Status
#undef LockMask
#undef ControlMask
#undef Success
#undef Always
#endif

#include "recompui/recompui.h"
#include "recompui/program_config.h"
#include "recompui/renderer.h"
#include "recompui/config.h"
#include "util/file.h"
#include "recompinput/input_events.h"
#include "recompinput/recompinput.h"
#include "recompinput/profiles.h"
#include "librecomp/game.hpp"
#include "librecomp/mods.hpp"
#include "librecomp/helpers.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <timeapi.h>
#include "SDL_syswm.h"
#define APP_ICON 1
#endif

#include "../src/elements/ui_image.h"
#include "../src/elements/ui_slider.h"
#include "icon_bytes.h"
#include "menu_font.h"

namespace winks {
    void register_overlays();
}

// Defined in lib/rt64/src/render/rt64_vi_renderer.cpp (rt64 headers are not on main's include path).
namespace RT64 {
    extern std::atomic<bool> filmGrainEnabled;
}

const std::string version_string = "0.3.0";

template<typename... Ts>
void exit_error(const char* str, Ts ...args) {
    ((void)fprintf(stderr, str, args), ...);
    assert(false);

    ultramodern::error_handling::quick_exit(__FILE__, __LINE__, __FUNCTION__);
}

ultramodern::gfx_callbacks_t::gfx_data_t create_gfx() {
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
    SDL_SetHint(SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS, "0");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4_RUMBLE, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5_RUMBLE, "1");
    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC) > 0) {
        exit_error("Failed to initialize SDL2: %s\n", SDL_GetError());
    }

    fprintf(stdout, "SDL Video Driver: %s\n", SDL_GetCurrentVideoDriver());

    return {};
}

ultramodern::input::connected_device_info_t get_connected_device_info(int controller_num) {
    if (recompinput::players::is_single_player_mode() || recompinput::players::get_player_is_assigned(controller_num)) {
        return ultramodern::input::connected_device_info_t{
            .connected_device = ultramodern::input::Device::Controller,
            .connected_pak = ultramodern::input::Pak::RumblePak,
        };
    }

    return ultramodern::input::connected_device_info_t{
        .connected_device = ultramodern::input::Device::None,
        .connected_pak = ultramodern::input::Pak::None,
    };
}

SDL_Window* window;

ultramodern::renderer::WindowHandle create_window(ultramodern::gfx_callbacks_t::gfx_data_t) {
    uint32_t flags = SDL_WINDOW_RESIZABLE;

#if defined(__APPLE__)
    flags |= SDL_WINDOW_METAL;
#elif defined(RT64_SDL_WINDOW_VULKAN)
    flags |= SDL_WINDOW_VULKAN;
#endif

    window = SDL_CreateWindow("40 Winks : Dreamcompiled", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1600, 900, flags);

    if (window == nullptr) {
        exit_error("Failed to create window: %s\n", SDL_GetError());
    }

    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    SDL_GetWindowWMInfo(window, &wmInfo);

#if defined(_WIN32)
    HICON new_icon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(APP_ICON));
    SendMessage(wmInfo.info.win.window, WM_SETICON, ICON_SMALL2, (LPARAM)(new_icon));
    return ultramodern::renderer::WindowHandle{ wmInfo.info.win.window, GetCurrentThreadId() };
#elif defined(__linux__) || defined(__ANDROID__)
    return ultramodern::renderer::WindowHandle{ window };
#elif defined(__APPLE__)
    SDL_MetalView view = SDL_Metal_CreateView(window);
    return ultramodern::renderer::WindowHandle{ wmInfo.info.cocoa.window, SDL_Metal_GetLayer(view) };
#else
    static_assert(false && "Unimplemented");
#endif
}

void update_gfx(void*) {
    recompinput::handle_events();
}

static SDL_AudioCVT audio_convert;
static SDL_AudioDeviceID audio_device = 0;

// Samples per channel per second.
static uint32_t sample_rate = 48000;
static uint32_t output_sample_rate = 48000;
// Channel count.
constexpr uint32_t input_channels = 2;
static uint32_t output_channels = 2;

// Terminology: a frame is a collection of samples for each channel. e.g. 2 input samples is one input frame. This is unrelated to graphical frames.

// Number of frames to duplicate for fixing interpolation at the start and end of a chunk.
constexpr uint32_t duplicated_input_frames = 4;
// The number of output frames to skip for playback (to avoid playing duplicate inputs twice).
static uint32_t discarded_output_frames;

constexpr uint32_t bytes_per_frame = input_channels * sizeof(float);

void queue_samples(int16_t* audio_data, size_t sample_count) {
    // Buffer for holding the output of swapping the audio channels. This is reused across
    // calls to reduce runtime allocations.
    static std::vector<float> swap_buffer;
    static std::array<float, duplicated_input_frames * input_channels> duplicated_sample_buffer;

    // Make sure the swap buffer is large enough to hold the audio data, including any extra space needed for resampling.
    size_t resampled_sample_count = sample_count + duplicated_input_frames * input_channels;
    size_t max_sample_count = std::max(resampled_sample_count, resampled_sample_count * audio_convert.len_mult);
    if (max_sample_count > swap_buffer.size()) {
        swap_buffer.resize(max_sample_count);
    }

    // Copy the duplicated frames from last chunk into this chunk
    for (size_t i = 0; i < duplicated_input_frames * input_channels; i++) {
        swap_buffer[i] = duplicated_sample_buffer[i];
    }

    // Convert the audio from 16-bit values to floats and swap the audio channels into the
    // swap buffer to correct for the address xor caused by endianness handling.
    float cur_main_volume = static_cast<float>(recompui::config::sound::get_main_volume()) / 100.0f; // Get the current main volume, normalized to 0.0-1.0.
    for (size_t i = 0; i < sample_count; i += input_channels) {
        swap_buffer[i + 0 + duplicated_input_frames * input_channels] = audio_data[i + 1] * (0.5f / 32768.0f) * cur_main_volume;
        swap_buffer[i + 1 + duplicated_input_frames * input_channels] = audio_data[i + 0] * (0.5f / 32768.0f) * cur_main_volume;
    }

    // TODO handle cases where a chunk is smaller than the duplicated frame count.
    assert(sample_count > duplicated_input_frames * input_channels);

    // Copy the last converted samples into the duplicated sample buffer to reuse in resampling the next queued chunk.
    for (size_t i = 0; i < duplicated_input_frames * input_channels; i++) {
        duplicated_sample_buffer[i] = swap_buffer[i + sample_count];
    }

    audio_convert.buf = reinterpret_cast<Uint8*>(swap_buffer.data());
    audio_convert.len = (sample_count + duplicated_input_frames * input_channels) * sizeof(swap_buffer[0]);

    int ret = SDL_ConvertAudio(&audio_convert);

    if (ret < 0) {
        printf("Error using SDL audio converter: %s\n", SDL_GetError());
        throw std::runtime_error("Error using SDL audio converter");
    }

    uint64_t cur_queued_microseconds = uint64_t(SDL_GetQueuedAudioSize(audio_device)) / bytes_per_frame * 1000000 / sample_rate;
    uint32_t num_bytes_to_queue = audio_convert.len_cvt - output_channels * discarded_output_frames * sizeof(swap_buffer[0]);
    float* samples_to_queue = swap_buffer.data() + output_channels * discarded_output_frames / 2;

    // Prevent audio latency from building up by skipping samples in incoming audio when too many samples are already queued.
    // Skip samples based on how many microseconds of samples are queued already.
    uint32_t skip_factor = cur_queued_microseconds / 100000;
    if (skip_factor != 0) {
        uint32_t skip_ratio = 1 << skip_factor;
        num_bytes_to_queue /= skip_ratio;
        for (size_t i = 0; i < num_bytes_to_queue / (output_channels * sizeof(swap_buffer[0])); i++) {
            samples_to_queue[2 * i + 0] = samples_to_queue[2 * skip_ratio * i + 0];
            samples_to_queue[2 * i + 1] = samples_to_queue[2 * skip_ratio * i + 1];
        }
    }

    // Queue the swapped audio data.
    // Offset the data start by only half the discarded frame count as the other half of the discarded frames are at the end of the buffer.
    SDL_QueueAudio(audio_device, samples_to_queue, num_bytes_to_queue);
}

size_t get_frames_remaining() {
    constexpr float buffer_offset_frames = 1.0f;
    // Get the number of remaining buffered audio bytes.
    uint64_t buffered_byte_count = SDL_GetQueuedAudioSize(audio_device);

    // Scale the byte count based on the ratio of sample rates and channel counts.
    buffered_byte_count = buffered_byte_count * 2 * sample_rate / output_sample_rate / output_channels;

    // Adjust the reported count to be some number of refreshes in the future, which helps ensure that
    // there are enough samples even if the audio thread experiences a small amount of lag. This prevents
    // audio popping on games that use the buffered audio byte count to determine how many samples
    // to generate.
    uint32_t frames_per_vi = (sample_rate / 50);
    if (buffered_byte_count > (buffer_offset_frames * bytes_per_frame * frames_per_vi)) {
        buffered_byte_count -= (buffer_offset_frames * bytes_per_frame * frames_per_vi);
    }
    else {
        buffered_byte_count = 0;
    }
    // Convert from byte count to sample count.
    // Cap at one VI of samples: on hardware AI_LEN only covers the current DMA buffer, and 40 Winks computes its next
    // buffer size as (samples_per_frame - remaining + extra) with an unsigned minimum check, so a larger value goes negative.
    return std::min(static_cast<uint32_t>(buffered_byte_count / bytes_per_frame), frames_per_vi);
}

void update_audio_converter() {
    int ret = SDL_BuildAudioCVT(&audio_convert, AUDIO_F32, input_channels, sample_rate, AUDIO_F32, output_channels, output_sample_rate);

    if (ret < 0) {
        printf("Error creating SDL audio converter: %s\n", SDL_GetError());
        throw std::runtime_error("Error creating SDL audio converter");
    }

    // Calculate the number of samples to discard based on the sample rate ratio and the duplicate frame count.
    discarded_output_frames = duplicated_input_frames * output_sample_rate / sample_rate;
}

void set_frequency(uint32_t freq) {
    sample_rate = freq;

    update_audio_converter();
}

bool reset_audio(uint32_t output_freq) {
    SDL_AudioSpec spec_desired{
        .freq = (int)output_freq,
        .format = AUDIO_F32,
        .channels = (Uint8)output_channels,
        .silence = 0, // calculated
        .samples = 0x100, // Fairly small sample count to reduce the latency of internal buffering
        .padding = 0, // unused
        .size = 0, // calculated
        .callback = nullptr,
        .userdata = nullptr
    };

    audio_device = SDL_OpenAudioDevice(nullptr, false, &spec_desired, nullptr, 0);
    if (audio_device == 0) {
        std::string audio_error = std::string("No audio device could be found. Please make sure an audio device is available.\nError opening audio device: ") + std::string(SDL_GetError());
        recompui::message_box(audio_error.c_str());
        return false;
    }

    SDL_PauseAudioDevice(audio_device, 0);

    output_sample_rate = output_freq;
    update_audio_converter();

    return true;
}

extern RspUcodeFunc aspMain;

RspUcodeFunc* get_rsp_microcode(const OSTask* task) {
    switch (task->t.type) {
    case M_AUDTASK:
        return aspMain;

    default:
        fprintf(stderr, "Unknown task: %" PRIu32 "\n", task->t.type);
        return nullptr;
    }
}

extern "C" void recomp_entrypoint(uint8_t * rdram, recomp_context * ctx);
gpr get_entrypoint_address();

// How much wider than 4:3 RT64 draws the 3D (Aspect Ratio Expand). The game's portal visibility (func_80018ADC) projects
// with its own 4:3 matrix; hooks in 40winks.toml widen that matrix by this factor so rooms seen at the screen edges are drawn.
extern "C" float winks_expand_factor() {
    if (ultramodern::renderer::get_graphics_config().ar_option != ultramodern::renderer::AspectRatio::Expand) {
        return 1.0f;
    }
    int width, height;
    recompui::get_window_size(width, height);
    return height > 0 ? std::max((float(width) / height) / (4.0f / 3.0f), 1.0f) : 1.0f;
}

// Same, for the HUD: 1 when HUD Placement is Original (the HUD stays in the 4:3 frame).
extern "C" float winks_hud_expand_factor() {
    if (ultramodern::renderer::get_graphics_config().hr_option == ultramodern::renderer::HUDRatioMode::Original) {
        return 1.0f;
    }
    return winks_expand_factor();
}

std::vector<recomp::GameEntry> supported_games = {
    {
        // XXH3 of the final release (sha1 282b97c6a6eb79535e618fc2651b6156fff65ea4).
        .rom_hash = 0xa3c1bef7efb721e3ULL,
        .internal_name = "40 WINKS",
        .display_name = "40 Winks",
        .game_id = u8"40winks",
        .mod_game_id = "40winks",
        .discovery_url = "",
        // AllowAll: EEPROM at the start of the save buffer, Controller Pak image at 0x8000 (src/game/pak.cpp).
        // The game never reads EEPROM: osEepromProbe's result (D_8012BEA4) is unused and pak requests 1/2 are never issued.
        .save_type = recomp::SaveType::AllowAll,
        .thumbnail_bytes = std::span<const char>(icon_bytes),
        .is_enabled = false,
        .has_compressed_code = false,
        .entrypoint_address = get_entrypoint_address(),
        .entrypoint = recomp_entrypoint,
    },
};

std::string get_game_thread_name(const OSThread* t) {
    return "[Game] " + std::to_string(t->id);
}

void enable_texture_pack(recomp::mods::ModContext& context, const recomp::mods::ModHandle& mod) {
    recompui::renderer::enable_texture_pack(context, mod);
}

void disable_texture_pack(recomp::mods::ModContext&, const recomp::mods::ModHandle& mod) {
    recompui::renderer::disable_texture_pack(mod);
}

void reorder_texture_pack(recomp::mods::ModContext&) {
    recompui::renderer::trigger_texture_pack_update();
}

// Launcher decor (the user's fan art): fixed background, two children walking in at the bottom right, then the logo
// fading in from above and floating. Sizes are derived from the container size on every update (dp).
namespace {
    struct LauncherDecor {
        recompui::Element *background = nullptr, *child1 = nullptr, *child2 = nullptr, *logo = nullptr;
    } decor;

    constexpr float logo_aspect = 1350.0f / 1165.0f;

    float fade(float seconds, float begin, float length) {
        float t = std::clamp((seconds - begin) / length, 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }

    // The UI renderer does not read files: the PNG bytes are handed to it under the name used by the element.
    // Returns the PNG's width and height (read from its IHDR chunk), 0 x 0 if the file is missing.
    std::pair<int, int> queue_launcher_image(const std::string &path) {
        static std::map<std::string, std::pair<int, int>> queued;
        if (auto it = queued.find(path); it != queued.end()) {
            return it->second;
        }
        std::ifstream file(recompui::file::get_program_path() / "assets" / path, std::ios::binary);
        std::vector<char> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        std::pair<int, int> size{0, 0};
        if (bytes.size() < 24) {
            fprintf(stderr, "Launcher image not found: %s\n", path.c_str());
        }
        else {
            auto be32 = [&](size_t at) { return int((uint8_t)bytes[at] << 24 | (uint8_t)bytes[at + 1] << 16 | (uint8_t)bytes[at + 2] << 8 | (uint8_t)bytes[at + 3]); };
            size = {be32(16), be32(20)};
        }
        recompui::queue_image_from_bytes_file(path, bytes);
        return queued[path] = size;
    }

    // Launcher entries written with the game's big orange menu letters, read from the player's ROM at startup
    // (src/main/menu_font.cpp: the release ships no game data). One Image per letter in a flex row; only A-Z, 0-9 and
    // '!' exist. Each letter is appended to `letters` with its position in the text (spaces count), for the animation.
    // Without a ROM the entries keep their text labels.
    std::map<std::string, std::pair<int, int>> menu_glyph_sizes;

    struct MenuLetter {
        recompui::Image *img;
        int index;
    };
    recompui::Element *menu_font_word(recompui::Element *parent, std::string_view text, float height, std::vector<MenuLetter> &letters) {
        auto context = recompui::get_current_context();
        auto *row = context.create_element<recompui::Element>(parent);
        row->set_display(recompui::Display::Flex);
        row->set_align_items(recompui::AlignItems::Center);
        for (int i = 0; i < (int)text.size(); i++) {
            std::string name(1, (char)std::toupper((unsigned char)text[i]));
            auto it = menu_glyph_sizes.find(name);
            if (it == menu_glyph_sizes.end()) {
                // Space or missing letter: a gap a third of the height wide.
                auto *gap = context.create_element<recompui::Element>(row);
                gap->set_width(height * 0.35f, recompui::Unit::Dp);
                continue;
            }
            auto [w, h] = it->second;
            auto *img = context.create_element<recompui::Image>(row, "menu_font/" + name);
            img->set_width(height * w / h, recompui::Unit::Dp);
            img->set_height(height, recompui::Unit::Dp);
            // The letters have dark outlines that touch: overlap them slightly like the game's menus.
            img->set_margin_right(-height * 0.04f, recompui::Unit::Dp);
            letters.push_back({img, i});
        }
        return row;
    }

    // Loads the letters from the stored ROM once (they are shared by the launcher and the config menu). Returns false
    // while no ROM is available, or if the letters could not be read.
    bool menu_glyphs_ready() {
        static bool tried = false, ready = false;
        if (tried || !recomp::is_rom_valid(supported_games[0].game_id)) {
            return ready;
        }
        tried = true;
        auto glyphs = menu_font_load(recomp::get_config_path() / (supported_games[0].game_id + u8".z64"));
        if (glyphs.size() < 27) {
            fprintf(stderr, "Menu letters not found in the ROM: menus keep their text labels.\n");
            return false;
        }
        for (auto &[name, glyph] : glyphs) {
            menu_glyph_sizes[name] = {glyph.width, glyph.height};
            recompui::queue_image_from_bytes_rgba32("menu_font/" + name, glyph.rgba, glyph.width, glyph.height);
        }
        return ready = true;
    }

    float menu_seconds() {
        static const auto start = std::chrono::steady_clock::now();
        return std::chrono::duration<float>(std::chrono::steady_clock::now() - start).count();
    }

    // Letter animation measured in the game's menus (func_80000F18 arguments, 25 frames/s): every letter swings
    // +-12 degrees in the screen plane over 32 frames, each with its own phase; the active entry's letters bob +-6 px
    // (a glyph is 16 px tall) over 16 frames, each letter 1 frame ahead of the previous one; inactive entries are
    // tinted A0A0A0. `wave` enables the bobbing (launcher entries only); `active_color` tints the active entry (the
    // game's menu titles, e.g. "OPTIONS", use env color FFFF00).
    void animate_letters(std::vector<MenuLetter> &letters, float height, bool active, bool wave,
        recompui::Color active_color = {0xFF, 0xFF, 0xFF, 0xFF}) {
        constexpr float two_pi = 6.2831853f, swing_period = 32.0f / 25.0f, wave_period = 16.0f / 25.0f;
        float seconds = menu_seconds();
        recompui::Color color = active ? active_color : recompui::Color{0xA0, 0xA0, 0xA0, 0xFF};
        for (auto &l : letters) {
            // The game's phases look random: spread them with the golden angle.
            float swing = 12.0f * std::sin(two_pi * seconds / swing_period + l.index * 2.39996f);
            float dy = active && wave ? -height * 6.0f / 16.0f *
                std::sin(two_pi * seconds / wave_period + l.index * two_pi / 16.0f) : 0.0f;
            l.img->set_rotation(swing);
            l.img->set_translate_2D(0.0f, dy, recompui::Unit::Dp);
            l.img->set_image_color(color);
        }
    }

    // Background: cover the container, centered.
    void cover_background(recompui::Element *img, recompui::Element *container) {
        constexpr float bg_aspect = 1672.0f / 940.0f;
        float ratio = container->get_dp_to_pixel_ratio();
        float w = container->get_client_width() / ratio;
        float h = container->get_client_height() / ratio;
        float bg_w = std::max(w, h * bg_aspect), bg_h = bg_w / bg_aspect;
        img->set_width(bg_w, recompui::Unit::Dp);
        img->set_height(bg_h, recompui::Unit::Dp);
        img->set_left((w - bg_w) / 2.0f, recompui::Unit::Dp);
        img->set_top((h - bg_h) / 2.0f, recompui::Unit::Dp);
    }

    struct MenuFontOption {
        recompui::GameOption *option;
        std::string_view title;
        recompui::Element *sparkle = nullptr;
        std::vector<MenuLetter> letters;
    };
    constexpr float menu_letter_height = 64.0f, menu_option_padding = 24.0f, menu_sparkle_size = 56.0f;
    std::vector<MenuFontOption> menu_font_options;
    bool menu_font_built = false;

    // Replaces the launcher entries' labels with the game's letters, at launch or once a ROM has been selected. The
    // first entry is "Start Game" (it only reads "Load ROM" while no ROM is valid).
    void menu_font_build() {
        menu_font_built = true;
        auto context = recompui::get_current_context();
        for (auto &m : menu_font_options) {
            m.option->get_label()->set_display(recompui::Display::None);
            menu_font_word(m.option, m.title, menu_letter_height, m.letters);
            m.sparkle = context.create_element<recompui::Image>(m.option, "menu_font/sparkle");
            m.sparkle->set_position(recompui::Position::Absolute);
            m.sparkle->set_width(menu_sparkle_size, recompui::Unit::Dp);
            m.sparkle->set_height(menu_sparkle_size, recompui::Unit::Dp);
            m.sparkle->set_left(menu_option_padding - menu_sparkle_size - 8.0f, recompui::Unit::Dp);
            m.sparkle->set_top(menu_option_padding + (menu_letter_height - menu_sparkle_size) / 2.0f, recompui::Unit::Dp);
            m.sparkle->set_opacity(0.0f);
        }
    }

    void launcher_decor_setup(recompui::LauncherMenu *menu) {
        recompui::Element *container = menu->get_background_container();
        auto context = recompui::get_current_context();
        auto make = [&](const char *path) {
            queue_launcher_image(path);
            auto *img = context.create_element<recompui::Image>(container, path);
            img->set_position(recompui::Position::Absolute);
            img->set_opacity(0.0f);
            return img;
        };
        // Creation order is the stacking order: child2 sits behind child1.
        decor.background = make("icons/background.png");
        decor.background->set_opacity(1.0f);
        decor.child2 = make("icons/child2.png");
        decor.child1 = make("icons/child1.png");
        decor.logo = make("icons/logo.png");
    }

    void launcher_decor_update(recompui::LauncherMenu *menu) {
        recompui::Element *container = menu->get_background_container();
        cover_background(decor.background, container);
        float seconds = menu_seconds();
        float ratio = container->get_dp_to_pixel_ratio();
        float w = container->get_client_width() / ratio;
        float h = container->get_client_height() / ratio;

        // Children: small, bottom right. child2 is further left, half hidden behind child1.
        float size = h * 0.38f;
        float child1_left = w - size - w * 0.03f;
        float child2_left = child1_left - size * 0.55f;
        for (auto [img, left, begin] : { std::tuple{decor.child1, child1_left, 0.5f}, std::tuple{decor.child2, child2_left, 0.7f} }) {
            img->set_width(size, recompui::Unit::Dp);
            img->set_height(size, recompui::Unit::Dp);
            // Walk in from the right: slide left while fading in, with a small step bounce until they stop.
            float appear = fade(seconds, begin, 1.0f);
            float walk = 1.0f - appear;
            float bounce = std::fabs(std::sin(appear * 3.14159265f * 4.0f)) * size * 0.03f * walk;
            img->set_left(left + walk * size * 0.8f, recompui::Unit::Dp);
            img->set_top(h - size - bounce, recompui::Unit::Dp);
            img->set_opacity(appear);
        }

        // Logo: centered at the top, fades in while sliding down, then floats up and down.
        constexpr float logo_begin = 1.9f, logo_fade = 0.5f, float_period = 3.0f;
        float logo_h = h * 0.38f, logo_w = logo_h * logo_aspect;
        float appear = fade(seconds, logo_begin, logo_fade);
        float bob = std::max(seconds - (logo_begin + logo_fade), 0.0f);
        float offset_y = -h * 0.06f * (1.0f - appear) + h * 0.012f * std::sin(bob * 2.0f * 3.14159265f / float_period);
        decor.logo->set_width(logo_w, recompui::Unit::Dp);
        decor.logo->set_height(logo_h, recompui::Unit::Dp);
        decor.logo->set_left((w - logo_w) / 2.0f, recompui::Unit::Dp);
        decor.logo->set_top(h * 0.03f + offset_y, recompui::Unit::Dp);
        decor.logo->set_opacity(appear);

        // Image letters ignore the option's hover/focus text color: the game's menu sparkle marks the active entry.
        if (!menu_font_built && menu_glyphs_ready()) {
            menu_font_build();
        }
        for (auto &m : menu_font_options) {
            if (m.sparkle == nullptr) {
                continue;
            }
            bool active = m.option->is_enabled() &&
                (m.option->is_style_enabled(recompui::hover_state) || m.option->is_style_enabled(recompui::focus_state));
            m.sparkle->set_opacity(active ? 0.8f + 0.2f * std::sin(menu_seconds() * 6.0f) : 0.0f);
            animate_letters(m.letters, menu_letter_height, active, true);
        }
    }

    // Settings text (option names, choices, slider values) in the game's letters, as raw RML: one <img> per letter,
    // sized in em so it follows each label's font size. Variant 1 (option names) swings like the launcher entries
    // (keyframes `winks-swing-N` in assets/recomp.rcss, one per phase); variants 0 (choices)
    // and 2 (slider values) keep a fixed tilt. Characters the game font lacks (%, :, -, ., ...) are dropped. Plain text while
    // no ROM letters are available.
    std::optional<std::string> game_text_rml(std::string_view text, int variant) {
        if (!menu_glyphs_ready()) {
            return std::nullopt;
        }
        // Choices (variant 0) are 10 % bigger than names (1) and slider values (2).
        const float letter_em = variant == 0 ? 1.65f : 1.5f;
        std::string rml;
        char style[256];
        float total_em = 0.1f;
        for (int i = 0; i < (int)text.size(); i++) {
            std::string name(1, (char)std::toupper((unsigned char)text[i]));
            if (name == " ") {
                rml += "<span style=\"display: inline-block; width: 0.4em;\"></span>";
                total_em += 0.4f;
                continue;
            }
            auto it = menu_glyph_sizes.find(name);
            if (it == menu_glyph_sizes.end() || name == "sparkle") {
                continue;
            }
            auto [w, h] = it->second;
            // No negative margin here (unlike the launcher entries): RmlUi's shrink-to-fit width ignores it and wraps
            // the last letter of radio buttons.
            if (variant == 1) {
                // Same phase as animate_letters: 12 sin(2 pi t / 1.28 + 2.4 i), rounded to the nearest of the 16
                // phase-shifted cycles `winks-swing-N` of assets/recomp.rcss (no delays: every letter moves at once).
                constexpr float two_pi = 6.2831853f, period = 32.0f / 25.0f;
                int cycle = (int)std::lround(std::fmod(i * 2.39996f, two_pi) / two_pi * 16.0f) % 16;
                snprintf(style, sizeof(style), "height: %.2fem; width: %.2fem; animation: %.2fs linear infinite winks-swing-%d;",
                    letter_em, letter_em * w / h, period, cycle);
            }
            else {
                snprintf(style, sizeof(style), "height: %.2fem; width: %.2fem; transform: rotate(%.1fdeg);",
                    letter_em, letter_em * w / h, 8.0f * std::sin(i * 2.39996f));
            }
            rml += "<img src=\"menu_font/" + name + "\" style=\"" + style + "\"/>";
            total_em += letter_em * w / h;
        }
        // One block of explicit width: RmlUi's shrink-to-fit underestimates a run of inline images by about one letter
        // and wraps the last one (radio buttons, long names).
        snprintf(style, sizeof(style), "display: inline-block; white-space: nowrap; width: %.2fem;", total_em);
        return std::string("<span style=\"") + style + "\">" + rml + "</span>";
    }

    // Config menu (General, Graphics, ...): the launcher background behind the panel, and the tab headers written
    // with the game's letters, swinging, dimmed unless selected, hovered or focused, then yellow like the game's menu
    // titles (no bobbing). Built on the first
    // frame the menu is shown with the letters available.
    struct ConfigTabLetters {
        recompui::Tab *tab;
        std::vector<MenuLetter> letters;
    };
    constexpr float config_letter_height = 48.0f;
    recompui::Element *config_background = nullptr;
    std::vector<ConfigTabLetters> config_tabs;
    bool config_tabs_built = false;

    void config_decor_update(recompui::TabbedModal *modal) {
        recompui::Element *holder = modal->get_background_holder();
        if (config_background == nullptr) {
            queue_launcher_image("icons/background.png");
            config_background = recompui::get_current_context().create_element<recompui::Image>(holder, "icons/background.png");
            config_background->set_position(recompui::Position::Absolute);
        }
        cover_background(config_background, holder);

        if (!config_tabs_built && menu_glyphs_ready()) {
            config_tabs_built = true;
            for (recompui::Tab *tab : modal->get_tab_set()->get_tabs()) {
                tab->get_label()->set_display(recompui::Display::None);
                ConfigTabLetters entry{tab, {}};
                menu_font_word(tab, tab->get_text(), config_letter_height, entry.letters);
                config_tabs.push_back(std::move(entry));
            }
        }
        for (auto &t : config_tabs) {
            bool active = t.tab->get_selected() || t.tab->is_style_enabled(recompui::hover_state) ||
                t.tab->is_style_enabled(recompui::focus_state);
            animate_letters(t.letters, config_letter_height, active, false, {0xFF, 0xFF, 0x00, 0xFF});
        }
    }
}

// First launch (no valid ROM stored yet): ask for the ROM before the launcher shows, so it opens with the game's
// letters. A wrong file explains which dump is needed and asks again; Cancel opens the launcher without a ROM
// ("Load ROM" entry, text labels).
namespace {
    constexpr const char *required_dump =
        "40 Winks (Europe) for Nintendo 64, game code N4WZ\n"
        "SHA-1 282b97c6a6eb79535e618fc2651b6156fff65ea4\n"
        "(.z64, .n64 or .v64)";

    bool ask_ok_cancel(SDL_MessageBoxFlags flags, const char *title, const std::string &message) {
        const SDL_MessageBoxButtonData buttons[] = {
            { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "OK" },
            { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Cancel" },
        };
        const SDL_MessageBoxData data = { flags, nullptr, title, message.c_str(), SDL_arraysize(buttons), buttons, nullptr };
        int pressed = 0;
        return SDL_ShowMessageBox(&data, &pressed) == 0 && pressed == 1;
    }

    void ask_for_rom_on_first_launch() {
        const std::u8string &game_id = supported_games[0].game_id;
        std::string message = std::string("Select your 40 Winks ROM to start.\n"
            "Choisis ta ROM de 40 Winks pour commencer.\n\nRequired dump / Dump requis :\n") + required_dump;
        while (!recomp::is_rom_valid(game_id)) {
            if (!ask_ok_cancel(SDL_MESSAGEBOX_INFORMATION, "40 Winks : Dreamcompiled", message)) {
                return;
            }
            nfdu8filteritem_t filter = { "N64 ROM", "z64,n64,v64" };
            nfdu8char_t *utf8_path = nullptr;
            if (NFD_OpenDialogU8(&utf8_path, &filter, 1, nullptr) != NFD_OKAY) {
                continue;
            }
            std::filesystem::path path{reinterpret_cast<const char8_t *>(utf8_path)};
            NFD_FreePathU8(utf8_path);
            const char *reason = nullptr;
            switch (recomp::select_rom(path, game_id)) {
                case recomp::RomValidationError::Good:
                    recomp::check_all_stored_roms();
                    break;
                case recomp::RomValidationError::FailedToOpen:
                    reason = "The file could not be opened. / Impossible d'ouvrir le fichier.";
                    break;
                case recomp::RomValidationError::NotARom:
                    reason = "This file is not an N64 ROM. / Ce fichier n'est pas une ROM N64.";
                    break;
                case recomp::RomValidationError::IncorrectVersion:
                    reason = "This is 40 Winks, but not the right version (prototype, other region or modified dump).\n"
                        "C'est bien 40 Winks, mais pas la bonne version (prototype, autre région ou dump modifié).";
                    break;
                default:
                    reason = "This ROM is not 40 Winks. / Cette ROM n'est pas 40 Winks.";
                    break;
            }
            if (reason != nullptr) {
                message = std::string(reason) + "\n\nRequired dump / Dump requis :\n" + required_dump +
                    "\n\nOK: choose another file / choisir un autre fichier.";
                if (!ask_ok_cancel(SDL_MESSAGEBOX_ERROR, "Wrong ROM / Mauvaise ROM", message)) {
                    return;
                }
                message = std::string("Select your 40 Winks ROM.\nChoisis ta ROM de 40 Winks.\n\nRequired dump / Dump requis :\n") + required_dump;
            }
        }
    }
}

void on_launcher_init(recompui::LauncherMenu *menu) {
    ask_for_rom_on_first_launch();
    auto game_options_menu = menu->init_game_options_menu(
        supported_games[0].game_id,
        supported_games[0].mod_game_id,
        supported_games[0].display_name,
        supported_games[0].thumbnail_bytes,
        recompui::GameOptionsMenuLayout::Center
    );

    game_options_menu->add_default_options();
    game_options_menu->set_width(30, recompui::Unit::Percent);

    // Same layout as dk64recomp's launcher (options column on the left, big entries, credits bottom right), on a black
    // background for now: no animation, logo or images yet.
    // The labels are replaced by the game's menu letters once a ROM is available (menu_font_build); titles in
    // add_default_options' order.
    constexpr std::array<std::string_view, 5> option_titles = {"Start Game", "Controls", "Settings", "Mods", "Exit"};
    auto context = recompui::get_current_context();
    auto &options = game_options_menu->get_options();
    assert(options.size() == option_titles.size());
    for (size_t i = 0; i < options.size(); i++) {
        auto option = options[i];
        option->set_justify_content(recompui::JustifyContent::FlexStart);
        option->set_border_radius(0);
        option->set_font_family("Suplexmentary Comic NC");
        option->set_padding(menu_option_padding);
        option->get_label()->set_font_size(56.0f);
        option->get_label()->set_letter_spacing(4.0f);

        std::vector<recompui::Style *> hover_focus = {&option->hover_style, &option->focus_style};
        for (auto style : hover_focus) {
            style->set_background_color(recompui::theme::color::Transparent);
        }
        // No mod support yet: the Mods entry stays visible but greyed out and unselectable (like its config tab).
        if (option_titles[i] == "Mods") {
            option->set_enabled(false);
        }
        menu_font_options.push_back({option, option_titles[i]});
    }
    if (menu_glyphs_ready()) {
        menu_font_build();
    }

    constexpr float options_top_offset = 96.0f;
    recompui::Element *menu_container = menu->get_menu_container();
    menu_container->set_width(100, recompui::Unit::Percent);
    menu_container->set_left(0);
    menu_container->set_top(options_top_offset);
    menu_container->set_bottom(-options_top_offset);
    menu_container->unset_right();

    game_options_menu->unset_right();
    game_options_menu->set_bottom(50.0f, recompui::Unit::Percent);
    game_options_menu->set_translate_2D(0.0f, 50.0f, recompui::Unit::Percent);
    game_options_menu->set_left(200.0f);

    menu->remove_default_title();

    recompui::Element *background_container = menu->get_background_container();
    background_container->set_background_color({ 0, 0, 0, 0xFF });
    launcher_decor_setup(menu);

    auto credits_label = context.create_element<recompui::Label>(
        background_container,
        "40 Winks Dreamcompiled by DreaXer and Claude Anthropic",
        recompui::LabelStyle::Small
    );
    credits_label->set_position(recompui::Position::Absolute);
    credits_label->set_bottom(4.0f, recompui::Unit::Dp);
    credits_label->set_right(4.0f, recompui::Unit::Dp);
    credits_label->set_font_family("Suplexmentary Comic NC");
}

std::unique_ptr<ultramodern::renderer::RendererContext> create_render_context(uint8_t* rdram, ultramodern::renderer::WindowHandle window_handle, bool developer_mode) {
    return recompui::renderer::create_render_context(rdram, window_handle, ultramodern::renderer::PresentationMode::PresentEarly, developer_mode);
}

int main(int argc, char** argv) {
    // Without --show-console, write all output (including call traces) to 40winks-log.txt next to the executable.
    bool show_console = false;
    for (int i = 1; i < argc; i++) {
        show_console |= strcmp(argv[i], "--show-console") == 0;
    }
    if (!show_console) {
        std::string log_path = (recompui::file::get_program_path() / "40winks-log.txt").string();
        // Truncate once, then append from both streams (a GUI app has no valid stderr descriptor to dup onto).
        if (FILE* f = fopen(log_path.c_str(), "w")) {
            fclose(f);
        }
        freopen(log_path.c_str(), "a", stdout);
        freopen(log_path.c_str(), "a", stderr);
    }

    // Unbuffered so messages survive a crash, and report uncaught exceptions instead of a silent abort.
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);
    std::set_terminate([]() {
        try {
            if (auto e = std::current_exception()) {
                std::rethrow_exception(e);
            }
            fprintf(stderr, "terminate called without an exception\n");
        }
        catch (const std::exception& e) {
            fprintf(stderr, "Uncaught exception: %s\n", e.what());
        }
        catch (...) {
            fprintf(stderr, "Uncaught non-std exception\n");
        }
        std::abort();
    });

    recomp::Version project_version{};
    if (!recomp::Version::from_string(version_string, project_version)) {
        ultramodern::error_handling::message_box(("Invalid version string: " + version_string).c_str());
        return EXIT_FAILURE;
    }

#ifdef _WIN32
    // Set up high resolution timing period.
    timeBeginPeriod(1);

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--show-console") == 0) {
            if (GetConsoleWindow() == nullptr) {
                AllocConsole();
                freopen("CONIN$", "r", stdin);
                freopen("CONOUT$", "w", stderr);
                freopen("CONOUT$", "w", stdout);
            }
            break;
        }
    }

    // Set up console output to accept UTF-8 on windows
    SetConsoleOutputCP(CP_UTF8);

    // Force wasapi on Windows, as there seems to be some issue with sample queueing with directsound currently.
    SDL_setenv("SDL_AUDIODRIVER", "wasapi", true);
#endif

    NFD_Init();

    recompui::programconfig::set_program_name("40 Winks : Dreamcompiled");
    recompui::programconfig::set_program_id(u8"40WinksRecompiled");

    // Initialize SDL audio and set the output frequency.
    SDL_InitSubSystem(SDL_INIT_AUDIO);
    if (!reset_audio(48000)) {
        // It is not possible to initialize without an audio device.
        return EXIT_FAILURE;
    }

    std::u8string controller_db_path = (recompui::file::get_program_path() / "recompcontrollerdb.txt").u8string();
    if (SDL_GameControllerAddMappingsFromFile(reinterpret_cast<const char *>(controller_db_path.c_str())) < 0) {
        fprintf(stderr, "Failed to load controller mappings: %s\n", SDL_GetError());
    }

    recompui::register_primary_font("InterVariable.ttf", "Inter Variable");
    recompui::register_extra_font("Suplexmentary Comic NC.ttf");

    recomp::register_config_path(recompui::file::get_app_folder_path());

    for (const auto& game : supported_games) {
        recomp::register_game(game);
    }

    recompui::register_ui_exports();

    winks::register_overlays();

    // The game reads at most 2 controllers; the Controls tab shows a player selector for them.
    recompinput::players::set_single_player_mode(false);
    recompinput::players::set_player_count_range(1, 2);

    // RecompFrontend requires the prefab tabs (at least General) to exist before the launcher starts.
    std::filesystem::create_directories(recompui::file::get_app_folder_path());
    recompui::config::create_general_tab(recompui::config::GeneralTabOptions{});
    {
        recomp::config::Config& graphics_config = recompui::config::create_graphics_tab();
        // Film grain drawn by lib/rt64 (VI shader and RDP noise dither), see RT64::filmGrainEnabled.
        graphics_config.add_enum_option("film_grain", "Film Grain",
            "Adds the grain the game shows on its boot screen to the whole game.",
            { {0u, "Off", "OFF"}, {1u, "On", "ON"} }, 1u);
        graphics_config.add_option_change_callback("film_grain",
            [](recomp::config::ConfigValueVariant cur_value, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext) {
                RT64::filmGrainEnabled = std::get<uint32_t>(cur_value) != 0;
            });
    }
    recompui::config::create_controls_tab();
    recompui::config::create_sound_tab();
    recompui::config::create_mods_tab();
    // Manage Mods stays visible but greyed out and unselectable for now. Find Mods only exists with a discovery URL (none here).
    recompui::config::set_tab_selectable(recompui::config::mods::id, false);
    recompui::config::finalize();

    // Not adapted to 40 Winks yet: only Resolution, Aspect Ratio, HUD Ratio, MSAA, Downsampling and High Precision Framebuffer (shown in lib/RecompFrontend) stay editable in Graphics, nothing in Sound.
    // Downsampling is greyed by RecompFrontend itself at Auto resolution.
    // Aspect Ratio Expand: RT64 widens the projection matrices and keeps 2D rects centered in 4:3, except the 1P HUD,
    // moved to the edges with gEXSetRectAlign (src/game/interpolation.cpp), as far as HUD Ratio allows.
    {
        using namespace recompui::config;
        auto& graphics_config = get_graphics_config();
        for (const auto& id : {
                graphics::options::developer_mode,
                graphics::options::api_option }) {
            if (graphics_config.has_option(id)) {
                graphics_config.update_option_disabled(id, true);
            }
        }
        get_sound_config().update_option_disabled(sound::options::main_volume, true);
    }

    recompui::register_launcher_init_callback(on_launcher_init);
    recompui::register_launcher_update_callback(launcher_decor_update);
    recompui::config::set_update_callback(config_decor_update);
    recompui::set_game_text_converter(game_text_rml);
    // Options sliders like the game's (func_80000F18 trace of the Options menu): the underline sprite as the bar and the
    // unrotated letter I as the knob. Model sizes from the resource headers: a letter quad is 158 units, the bar
    // 474 x 38 (3 x 0.24 letters); the knob moves +-24 px around the bar's center (69 % of its 69 px).
    recompui::set_game_slider_styler([](recompui::Slider *slider) {
        if (!menu_glyphs_ready()) {
            return;
        }
        constexpr float knob_height = 48.0f;
        auto [iw, ih] = menu_glyph_sizes["I"];
        slider->use_game_images("menu_font/underline", "menu_font/I", knob_height * 474.0f / 158.0f,
            knob_height * 38.0f / 158.0f, knob_height * iw / ih, knob_height, 0.69f);
    });

    recomp::rsp::callbacks_t rsp_callbacks{
        .get_rsp_microcode = get_rsp_microcode,
    };

    ultramodern::renderer::callbacks_t renderer_callbacks{
        .create_render_context = create_render_context,
    };

    ultramodern::gfx_callbacks_t gfx_callbacks{
        .create_gfx = create_gfx,
        .create_window = create_window,
        .update_gfx = update_gfx,
    };

    ultramodern::audio_callbacks_t audio_callbacks{
        .queue_samples = queue_samples,
        .get_frames_remaining = get_frames_remaining,
        .set_frequency = set_frequency,
    };

    ultramodern::input::callbacks_t input_callbacks{
        .poll_input = recompinput::poll_inputs,
        .get_input = recompinput::profiles::get_n64_input,
        .set_rumble = recompinput::set_rumble,
        .get_connected_device_info = get_connected_device_info,
    };

    ultramodern::events::callbacks_t thread_callbacks{
        .vi_callback = recompinput::update_rumble,
        .gfx_init_callback = nullptr,
    };

    ultramodern::error_handling::callbacks_t error_handling_callbacks{
        .message_box = recompui::message_box,
    };

    ultramodern::threads::callbacks_t threads_callbacks{
        .get_game_thread_name = get_game_thread_name,
    };

    // Register the texture pack content type with rt64.json as its content file.
    recomp::mods::ModContentType texture_pack_content_type{
        .content_filename = "rt64.json",
        .allow_runtime_toggle = true,
        .on_enabled = enable_texture_pack,
        .on_disabled = disable_texture_pack,
        .on_reordered = reorder_texture_pack,
    };
    auto texture_pack_content_type_id = recomp::mods::register_mod_content_type(texture_pack_content_type);
    recomp::mods::register_mod_container_type("rtz", std::vector{ texture_pack_content_type_id }, false);

    recomp::Configuration cfg {
        .argc = argc,
        .argv = argv,
        .project_version = project_version,
        .window_handle = {},
        .rsp_callbacks = rsp_callbacks,
        .renderer_callbacks = renderer_callbacks,
        .audio_callbacks = audio_callbacks,
        .input_callbacks = input_callbacks,
        .gfx_callbacks = gfx_callbacks,
        .events_callbacks = thread_callbacks,
        .error_handling_callbacks = error_handling_callbacks,
        .threads_callbacks = threads_callbacks,
        .message_queue_control = {
            .requeue_timer = false
        },
    };

    recomp::start(cfg);

    NFD_Quit();

#ifdef _WIN32
    timeEndPeriod(1);
#endif

    return EXIT_SUCCESS;
}
