// What a picture wider than 4:3 needs besides RT64's Aspect Ratio Expand, which widens the 3D: a camera's view drawn to
// the window's edges, its 3D moving with the board under it. The engine's patches/camera_view.c says how; here are
// Mario Party's two functions that start a camera's view, src/1DE40.c's func_8001D658 (its viewport and projection)
// and func_8001D7DC (its scissor). A board pans its 3D by moving its camera's viewport (src/48D90.c's func_8004B208).

#include "patches.h"
#include "../lib/mario-party-engine/patches/camera_view.h"
#include "../lib/mario-party-engine/patches/board_back.h"

static s32 widened(unk_Struct00* camera) {
    return camera_view_widened(camera->unkD8.x, camera->unkD8.z);
}

RECOMP_PATCH void func_8001D658(s16 index, Gfx** gfx) {
    unk_Struct00* camera = &D_800C3110[index];
    Mtx* projection = &camera->unkF8 + (D_800F3FA8 * 2);
    unk_Struct00* frame = (void*)camera + (D_800F3FA8 * 16);
    Vp* viewport = (Vp*)(&camera->unk58 + (D_800F3FA8 * 8));

    frame->unk58 = camera->unk24.x;
    frame->unk5A = camera->unk24.y;
    frame->unk5C = camera->unk24.z;
    frame->unk60 = camera->unk30.x;
    frame->unk62 = camera->unk30.y;
    frame->unk64 = camera->unk30.z;

    // @recomp A widened camera's viewport in the middle, what it was moved by in its projection.
    camera_view_projection(gfx, index, viewport, camera->unk4C, projection, widened(camera));
    gSPMatrix((*gfx)++, (void*)projection + 0x80000040, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
}

RECOMP_PATCH void func_8001D7DC(s16 index, Gfx** gfx) {
    unk_Struct00* camera = &D_800C3110[index];

    // @recomp The whole screen, or the border, to the window's edges.
    camera_view_scissor(gfx, camera->unkD8.x, camera->unkD8.y, camera->unkD8.z, camera->unkD8.w);
}

// The scenes the game draws at 4:3 whatever the window's width, by their overlay (OvlLoad, overlays.c, calls
// widescreen_overlay as each loads): the engine keeps black at the window's sides while one plays (board_back.h's
// board_back_scene_4x3), and the camera's view and the HUD stay inside 4:3.
// - 0x01, Chance Time: its backdrop, the sky and the sand, is a 2D picture of the screen's size, and its camera's
//   scissor (0, 0, 319, 239) would otherwise be widened as the whole screen's, its 3D out over the black beside it.
// - 0x21, Tipsy Tourney: its clouds are a 2D picture of the screen's size behind the four players' views, and they
//   and the paste timer's lines end at 4:3's edges, so past them the window would show the clear's white and the
//   views' 3D.
static const u8 overlays_4x3[] = { 0x01, 0x21 };

void widescreen_overlay(s32 overlay) {
    s32 picture_4x3 = FALSE;
    u32 i;

    for (i = 0; i < sizeof(overlays_4x3); i++) {
        if (overlays_4x3[i] == overlay) {
            picture_4x3 = TRUE;
        }
    }
    board_back_scene_4x3(picture_4x3);
}

f32 mp_aspect_ratio(void);

// Piranha's Pursuit (overlay 0x31) in a picture wider than 4:3. Its ground, the dirt track and the grass behind it, is
// one object (its models from file 0x450000, set up in func_800FA878_PiranhasPursuit) that the game moves to the
// track's scroll each frame and slides only the texture of (func_800FAEE8_PiranhasPursuit: the scroll over 24 times
// its x scale), and the camera aims up to 300 units behind it, so at 4:3 its left end is just past the picture's edge
// and a wider one showed black beside it, below the jungle. us.toml's hook calls this each frame before the game
// reads the ground's x scale: the ground as many times wider as the picture is than 4:3, which reaches as far past
// each side as the wider view does (its texture stretched as much, and still moving with the track), and the game's
// own scale of 1 at 4:3.
void widescreen_piranhas_ground(omObjData* ground) {
    ground->scale.x = mp_aspect_ratio() * (3.0f / 4.0f);
}
