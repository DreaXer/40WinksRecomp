// RT64 frame interpolation: matrix groups for the game's 3D objects (subject E, step 3).
// Every object matrix is a G_MTX MODELVIEW|LOAD|PUSH (0xDA380002) followed later by a G_POPMTX (0xD8380002). The matrix
// lives in the object's data at data + 0x10 + parity * 0x40 (parity = D_8012F36C, double buffer), so the data pointer is a
// stable id across frames. Hooks in 40winks.toml call winks_mtx_group just before the game writes the G_MTX, and
// winks_mtx_group_pop right after it writes the G_POPMTX (both append to the display list cursor D_801D06C0).
#include <cstdint>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include <vector>

#define F3DEX_GBI_2
#include "rt64_extended_gbi.h"

static constexpr uint32_t DL_CURSOR = 0x801D06C0;

static void append(uint8_t* rdram, const GfxCommand* cmds, int count) {
    uint32_t* cursor = (uint32_t*)(rdram + (DL_CURSOR - 0x80000000));
    uint32_t* out = (uint32_t*)(rdram + (*cursor - 0x80000000));
    for (int i = 0; i < count; i++) {
        out[i * 2] = cmds[i].values.word0;
        out[i * 2 + 1] = cmds[i].values.word1;
    }
    *cursor += count * 8;
}

// Some objects wrap their angle: the "propellers" (blades on a horizontal axis, angle at data + 0x6) go from -0x2C0 to
// 0x300 and jump back to -0x2C0, about 130 degrees in one frame. The model looks the same before and after, but RT64
// interpolated the jump backwards. When an object's rotation moves more than MaxTurn in one frame, its id changes
// (generation in bits 24..29), so that frame is not interpolated. The ninja's fireballs spin 87-88 degrees per frame:
// MaxTurn stays above that.
static uint32_t object_id(uint8_t* rdram, uint32_t id) {
    constexpr float MaxTurnCos = 0.0f; // 90 degrees
    struct State {
        float rows[9];
        uint32_t generation;
    };
    static std::unordered_map<uint32_t, State> states;
    if (id == 0x801CD530) { // func_80000450: not an object, no matrix at id + 0x10
        return id;
    }

    const uint32_t parity = *(uint32_t*)(rdram + (0x8012F36C - 0x80000000));
    const uint32_t mtx = id + 0x10 + parity * 0x40;
    float rows[9];
    for (int r = 0; r < 3; r++) {
        float len = 0.0f;
        for (int c = 0; c < 3; c++) {
            const uint32_t a = mtx + (r * 4 + c) * 2;
            const uint16_t i = *(uint16_t*)(rdram + ((a ^ 2) - 0x80000000));
            const uint16_t f = *(uint16_t*)(rdram + (((a + 0x20) ^ 2) - 0x80000000));
            rows[r * 3 + c] = (float)(int32_t)((uint32_t)i << 16 | f) / 65536.0f;
            len += rows[r * 3 + c] * rows[r * 3 + c];
        }
        len = std::sqrt(len);
        for (int c = 0; c < 3; c++) {
            rows[r * 3 + c] = (len > 0.0f) ? rows[r * 3 + c] / len : 0.0f;
        }
    }

    auto it = states.find(id);
    if (it == states.end()) {
        it = states.emplace(id, State{}).first;
    }
    else {
        float trace = 0.0f;
        for (int i = 0; i < 9; i++) {
            trace += rows[i] * it->second.rows[i];
        }
        if ((trace - 1.0f) * 0.5f < MaxTurnCos) {
            it->second.generation++;
        }
    }
    memcpy(it->second.rows, rows, sizeof(rows));
    return (id & 0xC0FFFFFF) | ((it->second.generation & 0x3F) << 24);
}

extern "C" void winks_mtx_group(uint8_t* rdram, uint32_t id) {
    id = object_id(rdram, id);
    GfxCommand cmds[2];
    // Vertices are interpolated too: animated characters are skinned on the CPU (per-frame vertex buffers, one matrix).
    gEXMatrixGroup(cmds, id, G_EX_INTERPOLATE_DECOMPOSE, G_EX_PUSH, 0, G_EX_COMPONENT_INTERPOLATE,
        G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_INTERPOLATE,
        G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_INTERPOLATE, G_EX_ORDER_LINEAR, G_EX_EDIT_NONE, G_EX_ASPECT_AUTO,
        G_EX_COMPONENT_SKIP, G_EX_COMPONENT_AUTO);
    append(rdram, cmds, 2);
}

extern "C" void winks_mtx_group_pop(uint8_t* rdram) {
    GfxCommand cmd[1];
    gEXPopMatrixGroup(cmd, 0);
    append(rdram, cmd, 1);
}

// Rotated 2D sprites (menu letters, cursor sparkles, ...): func_80000C4C builds a matrix per sprite and loads it with
// G_MTX MODELVIEW | LOAD (no push). Untagged, RT64 matches them by call hash, so repeated letters (same texture) swap
// places between frames. Ids by draw order do not hold either: when the menu cursor moves, the letters of the selected
// entry are drawn by other code, and sparkles appear and vanish. So the matching is done here, with the real identity
// of each sprite: its glyph (6th argument of func_80000FB0) and its position (func_80000C4C's a0 / a1).
// Each sprite gets a group whose id is filled at the end of the frame (func_800030C0, before the list runs): all pairs
// (previous frame sprite, current sprite) of the same glyph are sorted by distance and the closest are matched first,
// so a letter is never taken by a letter of the same glyph on the next line. The distance is measured to the
// predicted position (last position + last move). A sprite already tracked accepts an error of at most 3 x its speed
// + MinTrackedError (letters that bounce back), so a sparkle recreated at the top of its fall is not matched to one
// that vanished at the bottom. Unmatched sprites get a new id, which RT64 does not interpolate.
static constexpr uint32_t SPRITE_ID_BASE = 0x40000000;
static constexpr int max_sprites = 512;
static constexpr int64_t MaxMatchDistance = 64 * 1024; // func_80000C4C divides positions by 1024
static constexpr int64_t MinTrackedError = 8 * 1024;
struct Sprite {
    uint32_t glyph;
    int32_t x, y;
    int32_t vx, vy;     // move since the matched sprite of the previous frame
    bool tracked;       // matched a sprite of the previous frame, so vx / vy are known
    uint32_t id;
    uint32_t id_addr;   // current frame: RDRAM address of the group command's id word
};
static Sprite sprites_prev[max_sprites], sprites_cur[max_sprites];
static int sprites_prev_count = 0, sprites_cur_count = 0;
static uint32_t sprite_new_id = 0;
static uint32_t next_sprite_glyph = 0;
static int32_t next_sprite_x = 0, next_sprite_y = 0;
static bool sprite_group_open = false;

extern "C" void winks_sprite_number(uint32_t number) {
    next_sprite_glyph = number;
}

extern "C" void winks_sprite_position(int32_t x, int32_t y) {
    next_sprite_x = x;
    next_sprite_y = y;
}

extern "C" void winks_sprite_group_close(uint8_t* rdram) {
    if (sprite_group_open) {
        winks_mtx_group_pop(rdram);
        sprite_group_open = false;
    }
}

static void sprite_frame_start() {
    sprites_cur_count = 0;
    sprite_group_open = false;
}

extern "C" void winks_sprite_group(uint8_t* rdram) {
    winks_sprite_group_close(rdram);
    const uint32_t cursor = *(uint32_t*)(rdram + (DL_CURSOR - 0x80000000));
    if (sprites_cur_count < max_sprites) {
        sprites_cur[sprites_cur_count++] = { next_sprite_glyph, next_sprite_x, next_sprite_y, 0, 0, false, 0, cursor + 4 };
    }
    // The id is a placeholder until winks_sprites_frame_end.
    GfxCommand cmds[2];
    gEXMatrixGroup(cmds, SPRITE_ID_BASE, G_EX_INTERPOLATE_DECOMPOSE, G_EX_PUSH, 0, G_EX_COMPONENT_INTERPOLATE,
        G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_INTERPOLATE,
        G_EX_COMPONENT_SKIP, G_EX_COMPONENT_INTERPOLATE, G_EX_ORDER_LINEAR, G_EX_EDIT_NONE, G_EX_ASPECT_AUTO,
        G_EX_COMPONENT_SKIP, G_EX_COMPONENT_AUTO);
    append(rdram, cmds, 2);
    sprite_group_open = true;
}

extern "C" void winks_sprites_frame_end(uint8_t* rdram) {
    winks_sprite_group_close(rdram);

    struct Pair {
        int64_t dist;
        int16_t prev, cur;
    };
    static std::vector<Pair> pairs;
    pairs.clear();
    for (int c = 0; c < sprites_cur_count; c++) {
        const Sprite& cur = sprites_cur[c];
        for (int p = 0; p < sprites_prev_count; p++) {
            const Sprite& prev = sprites_prev[p];
            if (prev.glyph != cur.glyph) {
                continue;
            }
            const int64_t dx = int64_t(prev.x) + prev.vx - cur.x, dy = int64_t(prev.y) + prev.vy - cur.y;
            const int64_t dist = dx * dx + dy * dy;
            int64_t max_dist = MaxMatchDistance;
            if (prev.tracked) {
                const int64_t speed = std::max(std::abs(int64_t(prev.vx)), std::abs(int64_t(prev.vy)));
                max_dist = std::min(MaxMatchDistance, 3 * speed + MinTrackedError);
            }
            if (dist <= max_dist * max_dist) {
                pairs.push_back({ dist, int16_t(p), int16_t(c) });
            }
        }
    }
    std::sort(pairs.begin(), pairs.end(), [](const Pair& a, const Pair& b) { return a.dist < b.dist; });

    bool prev_used[max_sprites] = {};
    bool cur_done[max_sprites] = {};
    for (const Pair& pair : pairs) {
        if (prev_used[pair.prev] || cur_done[pair.cur]) {
            continue;
        }
        prev_used[pair.prev] = cur_done[pair.cur] = true;
        const Sprite& prev = sprites_prev[pair.prev];
        Sprite& cur = sprites_cur[pair.cur];
        cur.id = prev.id;
        cur.vx = cur.x - prev.x;
        cur.vy = cur.y - prev.y;
        cur.tracked = true;
    }
    for (int c = 0; c < sprites_cur_count; c++) {
        Sprite& cur = sprites_cur[c];
        if (!cur_done[c]) {
            cur.id = SPRITE_ID_BASE | (sprite_new_id++ & 0x00FFFFFF);
        }
        *(uint32_t*)(rdram + (cur.id_addr - 0x80000000)) = cur.id;
    }

    memcpy(sprites_prev, sprites_cur, sizeof(Sprite) * sprites_cur_count);
    sprites_prev_count = sprites_cur_count;
    sprites_cur_count = 0;
}

// Particles: the game writes their vertices in world space on the CPU, into a shared pool, and draws them in batches with
// func_800244AC (triangles) / func_800245E0 (quads), under the camera matrix only. RT64 only interpolates vertices of a
// tagged transform, so a call can get its own group and an identity G_MTX (which creates the transform); RT64 then
// interpolates each vertex if the transform has as many vertices as last frame, matched by index.
// Each group needs an id that stays with the same element from frame to frame (ids by call order pair the wrong vertices
// once elements are depth-sorted, appear or vanish):
// - rain: one call per frame (call site 0x80001960, func_8000134C), id = the call site;
// - object shadows (func_80011F28, one quad per object of the list D_801D5D08): id = object pointer;
// - billboard objects (func_80026D18, from func_8001B7F8's sorted loop): id = object pointer, whole function grouped;
// - level billboards (func_80026164, rotated to face the camera by func_80024E8C): id = their index, whole function.
// The high byte tags each kind, so an object pointer used by two kinds (or by a 3D object group) never shares an id.
// A per-call id is set by a hook before the jal (winks_particles_id): the recompiled code does not keep $ra.
// - translucent level triangles (func_8001732C: waterfalls...): static vertices (D_801C8B20) but scrolling textures, drawn
//   one triangle per call in depth order. Ungrouped, RT64 paired their tiles with another triangle's of the same texture
//   (order changes every frame) and the scroll flickered. id = triangle index in D_8012E418, tiles interpolated.
static uint32_t identity_phys = 0;
static uint32_t next_id = 0;
static uint8_t next_tile = G_EX_COMPONENT_SKIP;
static bool grouped = false;

extern "C" void winks_particles_id(uint32_t id) {
    next_id = id;
}

extern "C" void winks_scrolling_id(uint32_t id) {
    next_id = id;
    next_tile = G_EX_COMPONENT_AUTO;
}

// Start of the frame's display list (func_80002F20): enable RT64 extensions, set the game's rate, and store an identity
// matrix in the list, jumped over with a G_DL branch.
extern "C" void winks_frame_start(uint8_t* rdram) {
    uint32_t cursor = *(uint32_t*)(rdram + (DL_CURSOR - 0x80000000));
    GfxCommand cmds[3 + 8];
    gEXEnable(&cmds[0]);
    // The game runs 25 frames/s (2 VI per frame at 50 Hz, measured with trace_mode); RT64 would guess from the VI with a 60 Hz base.
    gEXSetRefreshRate(&cmds[1], 25);
    cmds[2].values.word0 = 0xDE010000;
    cmds[2].values.word1 = cursor + 3 * 8 + 64;
    static const uint32_t identity[16] = { 0x00010000, 0, 1, 0, 0, 0x00010000, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0 };
    for (int i = 0; i < 8; i++) {
        cmds[3 + i].values.word0 = identity[i * 2];
        cmds[3 + i].values.word1 = identity[i * 2 + 1];
    }
    identity_phys = (cursor + 3 * 8) & 0x1FFFFFFF;
    append(rdram, cmds, 11);
    sprite_frame_start();
}

static void particles_group(uint8_t* rdram, uint32_t id, uint8_t tile) {
    // D_80133A7C = free slots left in the last G_VTX load, shared by every call: without a reset, this call would draw with
    // vertices loaded under the previous call's group, and RT64 would compute their motion against another caller's.
    // Zero forces a new G_VTX at the current pool position (halfword, so address ^ 2 in the byteswapped RDRAM).
    *(int16_t*)(rdram + ((0x80133A7C ^ 2) - 0x80000000)) = 0;
    GfxCommand cmds[3];
    gEXMatrixGroup(cmds, id, G_EX_INTERPOLATE_SIMPLE, G_EX_PUSH, 0, G_EX_COMPONENT_SKIP, G_EX_COMPONENT_SKIP,
        G_EX_COMPONENT_SKIP, G_EX_COMPONENT_SKIP, G_EX_COMPONENT_SKIP, G_EX_COMPONENT_INTERPOLATE, tile,
        G_EX_ORDER_LINEAR, G_EX_EDIT_NONE, G_EX_ASPECT_AUTO, G_EX_COMPONENT_SKIP, G_EX_COMPONENT_SKIP);
    cmds[2].values.word0 = 0xDA380000;  // G_MTX MODELVIEW | MUL | PUSH
    cmds[2].values.word1 = identity_phys;
    append(rdram, cmds, 3);
}

extern "C" void winks_particles_group(uint8_t* rdram, uint32_t id) {
    particles_group(rdram, id, G_EX_COMPONENT_SKIP);
}

extern "C" void winks_particles_group_end(uint8_t* rdram) {
    // Same reset on the way out, so the next call does not reuse slots of a load made under this group.
    *(int16_t*)(rdram + ((0x80133A7C ^ 2) - 0x80000000)) = 0;
    GfxCommand cmds[2];
    cmds[0].values.word0 = 0xD8380002;  // G_POPMTX MODELVIEW
    cmds[0].values.word1 = 0x40;
    gEXPopMatrixGroup(&cmds[1], 0);
    append(rdram, cmds, 2);
}

// Trails (shout ribbons, candle smoke): func_80012C00 draws slot `offset` (8 bytes each in D_801D21C0) whose node list
// starts at `head` (next at +0x0, the node's two vertices at +0x4..+0x18, floats). RT64 pairs the vertices by index: when a
// trail restarts elsewhere (candle leaving the water), a node jumps far and the interpolated frames show a huge stretched
// triangle. Then the slot gets a new id, so that frame is not interpolated.
extern "C" void winks_trail_group(uint8_t* rdram, uint32_t offset, uint32_t head) {
    constexpr int MaxNodes = 64;
    constexpr float MaxJump = 200.0f;
    static float prev[32][MaxNodes * 6];
    static int prev_count[32];
    static uint32_t generation[32];
    const uint32_t slot = ((offset - 8) / 8) & 31;
    float cur[MaxNodes * 6];
    int count = 0;
    for (uint32_t node = head; (node != 0) && (count < MaxNodes); count++) {
        const uint32_t* words = (const uint32_t*)(rdram + (node - 0x80000000));
        memcpy(&cur[count * 6], &words[1], 6 * sizeof(float));
        node = words[0];
    }

    float max_jump = 0.0f;
    if (count == prev_count[slot]) {
        for (int i = 0; i < count * 6; i++) {
            max_jump = std::max(max_jump, std::abs(cur[i] - prev[slot][i]));
        }
    }

    if (max_jump > MaxJump) {
        generation[slot]++;
    }

    memcpy(prev[slot], cur, count * 6 * sizeof(float));
    prev_count[slot] = count;
    winks_particles_group(rdram, 0x54000000 | ((generation[slot] & 0x7FFF) << 9) | (offset & 0x1FF));
}

// func_80012C48 writes the trail's vertices to the shared pool (write pointer D_801335B8) but loads them 32 at a time
// (func_80024794), so the load also takes the vertices the next object writes after the trail. When the trail grows by a
// node, its new last segment uses 2 vertices that were the next object's last frame: RT64 interpolates them from there,
// hence big white triangles (candle smoke leaving the water). Copy the last vertex pair after the trail and skip it, so
// those 2 slots hold the trail's tail last frame too.
// shortcut: takes 32 more bytes of the pool per drawn trail, fine unless the pool is nearly full.
extern "C" void winks_trail_pad(uint8_t* rdram) {
    uint32_t* write_ptr = (uint32_t*)(rdram + (0x801335B8 - 0x80000000));
    uint8_t* end = rdram + (*write_ptr & 0x1FFFFFFF);
    memcpy(end, end - 0x20, 0x20);
    *write_ptr += 0x20;
}

// 1P HUD on the screen edges in Expand mode (subject I). func_800079D0 draws 11 counters (loop index $s1), each at
// x = D_80082620[i].x * 160 + 160 (x = 0.45: right side, -0.75: left side), then a centered one (func_80007104) and
// two on the left (func_800071D0, x = 28). They are G_TEXRECTs: RT64 moves them to the edge given by gEXSetRectAlign.
// A right-aligned rect's coordinates are relative to the right edge, hence the -320 offset. The scissor is widened too,
// or RT64 clips the moved rects to the 4:3 area.
static bool hud_aligned = false;

static void hud_set_align(uint8_t* rdram, uint16_t origin) {
    GfxCommand cmds[2];
    const int32_t offset = origin == G_EX_ORIGIN_RIGHT ? -320 * 4 : 0;
    gEXSetRectAlign(cmds, origin, origin, offset, 0, offset, 0);
    append(rdram, cmds, 2);
}

extern "C" void winks_hud_start(uint8_t* rdram) {
    GfxCommand cmds[3];
    gEXPushScissor(&cmds[0]);
    gEXSetScissor(&cmds[1], 0 /* G_SC_NON_INTERLACE */, G_EX_ORIGIN_LEFT, G_EX_ORIGIN_RIGHT, 0, 0, 0, 240);
    append(rdram, cmds, 3);
    hud_aligned = true;
}

// side < 0: left edge, > 0: right edge, 0: centered (4:3 position).
extern "C" void winks_hud_align(uint8_t* rdram, int side) {
    if (hud_aligned) {
        hud_set_align(rdram, side < 0 ? G_EX_ORIGIN_LEFT : side > 0 ? G_EX_ORIGIN_RIGHT : G_EX_ORIGIN_NONE);
    }
}

extern "C" void winks_hud_end(uint8_t* rdram) {
    if (hud_aligned) {
        hud_set_align(rdram, G_EX_ORIGIN_NONE);
        GfxCommand cmd[1];
        gEXPopScissor(cmd);
        append(rdram, cmd, 1);
        hud_aligned = false;
    }
}

// 2P HUD (func_800074C0, one bar shared by both players at the top): drawn larger in Expand. Every HUD sprite / text
// position and size is multiplied by the scales D_80082354 (x), D_80082358 (y position), D_8008235C (height), which
// the game halves around the 2P counters. Multiplying them by k scales the whole HUD about (0, 0); a rect offset of
// -(k - 1) * 160 moves it back to the center. The scissor is widened (func_80024158 sets the 4:3 one).
static constexpr float HUD2P_MAX_SCALE = 1.30f;
static constexpr float HUD2P_RAISE = 32.0f;
static constexpr uint32_t hud_scale_addrs[3] = { 0x80082354, 0x80082358, 0x8008235C };
static float hud2p_scale = 1.0f;
static float hud_scales_saved[3];

extern "C" float winks_hud_expand_factor(void);

// Start of func_800074C0. The scales are saved and restored as is: multiplying and dividing by k every frame would drift.
extern "C" void winks_hud2p_scale(uint8_t* rdram) {
    hud2p_scale = std::min(winks_hud_expand_factor(), HUD2P_MAX_SCALE);
    for (int i = 0; i < 3; i++) {
        float* scale = (float*)(rdram + (hud_scale_addrs[i] - 0x80000000));
        hud_scales_saved[i] = *scale;
        *scale *= hud2p_scale;
    }
}

// After func_80024158 (first iteration of the counter loop).
extern "C" void winks_hud2p_start(uint8_t* rdram) {
    if (hud2p_scale == 1.0f) {
        return;
    }
    // gEXSetScissor and gEXSetRectAlign take 2 commands each.
    GfxCommand cmds[5];
    const int32_t offset = int32_t(-(hud2p_scale - 1.0f) * 160.0f * 4.0f);
    // Scaled about (0, 0), the bar also moved down: raise it by HUD2P_RAISE px per unit of extra scale (~10 px at 1.30).
    const int32_t offset_y = int32_t(-(hud2p_scale - 1.0f) * HUD2P_RAISE * 4.0f);
    gEXPushScissor(&cmds[0]);
    gEXSetScissor(&cmds[1], 0 /* G_SC_NON_INTERLACE */, G_EX_ORIGIN_LEFT, G_EX_ORIGIN_RIGHT, 0, 0, 0, 240);
    gEXSetRectAlign(&cmds[3], G_EX_ORIGIN_NONE, G_EX_ORIGIN_NONE, offset, offset_y, offset, offset_y);
    append(rdram, cmds, 5);
}

// End of func_800074C0.
extern "C" void winks_hud2p_end(uint8_t* rdram) {
    if (hud2p_scale == 1.0f) {
        return;
    }
    for (int i = 0; i < 3; i++) {
        *(float*)(rdram + (hud_scale_addrs[i] - 0x80000000)) = hud_scales_saved[i];
    }
    hud_set_align(rdram, G_EX_ORIGIN_NONE);
    GfxCommand cmd[1];
    gEXPopScissor(cmd);
    append(rdram, cmd, 1);
    hud2p_scale = 1.0f;
}

// Hooks of func_800244AC / func_800245E0: group this call if a hook before its jal gave it an id.
extern "C" void winks_particles_begin(uint8_t* rdram) {
    grouped = next_id != 0;
    if (grouped) {
        particles_group(rdram, next_id, next_tile);
    }
    next_id = 0;
    next_tile = G_EX_COMPONENT_SKIP;
}

extern "C" void winks_particles_end(uint8_t* rdram) {
    if (grouped) {
        winks_particles_group_end(rdram);
        grouped = false;
    }
}

// Portal scissors (func_80019220): each room seen through a portal is drawn inside the portal's screen rectangle, a
// G_SETSCISSOR computed with the game frame's camera. Interpolated frames draw with a camera between the previous and
// the current game frame, so the room slides out of its rectangle and black slivers show along portal edges. Use the
// union of this frame's and last frame's rectangle for the same room (plus a margin), clamped to the viewport.
static constexpr int PortalMargin = 4 * 4; // 10.2 fixed point
struct PortalRect { int32_t ulx, uly, lrx, lry; bool valid; };
static PortalRect portal_prev[2][256], portal_cur[2][256];

// word0 = rdram address of the G_SETSCISSOR command (first word written), word1 = its second word; returns the new one.
extern "C" uint32_t winks_portal_scissor(uint8_t* rdram, uint32_t index, uint32_t room, uint32_t cmd, uint32_t word1) {
    auto rd = [rdram](uint32_t addr) { return *(int32_t*)(rdram + (addr - 0x80000000)); };
    int32_t vp_ulx = rd(0x8012AAF0) * 4, vp_uly = rd(0x8012E410) * 4;
    int32_t vp_lrx = rd(0x8012BFC8) * 4, vp_lry = rd(0x80132270) * 4;
    int slot = vp_ulx > 0 ? 1 : 0; // 2P: right view
    if (index == 0) {
        std::memcpy(portal_prev[slot], portal_cur[slot], sizeof(portal_cur[slot]));
        std::memset(portal_cur[slot], 0, sizeof(portal_cur[slot]));
    }
    uint32_t* word0 = (uint32_t*)(rdram + (cmd - 0x80000000));
    PortalRect r = { (int32_t)((*word0 >> 12) & 0xFFF), (int32_t)(*word0 & 0xFFF),
                     (int32_t)((word1 >> 12) & 0xFFF), (int32_t)(word1 & 0xFFF), true };
    PortalRect& cur = portal_cur[slot][room & 0xFF];
    if (cur.valid) {
        cur.ulx = std::min(cur.ulx, r.ulx); cur.uly = std::min(cur.uly, r.uly);
        cur.lrx = std::max(cur.lrx, r.lrx); cur.lry = std::max(cur.lry, r.lry);
    } else {
        cur = r;
    }
    const PortalRect& prev = portal_prev[slot][room & 0xFF];
    if (prev.valid) {
        r.ulx = std::min(r.ulx, prev.ulx); r.uly = std::min(r.uly, prev.uly);
        r.lrx = std::max(r.lrx, prev.lrx); r.lry = std::max(r.lry, prev.lry);
    }
    r.ulx = std::max(r.ulx - PortalMargin, vp_ulx); r.uly = std::max(r.uly - PortalMargin, vp_uly);
    r.lrx = std::min(r.lrx + PortalMargin, vp_lrx); r.lry = std::min(r.lry + PortalMargin, vp_lry);
    *word0 = (*word0 & 0xFF000000) | ((uint32_t)r.ulx << 12) | (uint32_t)r.uly;
    return ((uint32_t)r.lrx << 12) | (uint32_t)r.lry;
}
