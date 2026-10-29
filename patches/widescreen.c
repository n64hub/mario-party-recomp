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

// Its 14 trees and bushes along the back of the track, forms 1 to 14 of the same object, which the game keeps within
// the track's scroll plus or minus 1500: each frame (func_800FAEE8_PiranhasPursuit's loop at 0x800FAFA8) it moves one
// that has fallen behind the scroll minus 1500 on by 3000, so they repeat every 3000. With the camera aimed up to 300
// behind the scroll, a picture wider than about 2:1 reached past the ring's left end, the jungle's 2D backdrop alone
// beyond it. A copy of each a ring's width to its left and one to its right, placed each frame where the game's own one
// is, carries them on as they repeat, to the scroll plus or minus 4500, past a 32:9 picture's edges. us.toml's hooks
// call these once the setup (func_800FA878_PiranhasPursuit) has made the 14, and after the frame's loop has moved them.
// At 4:3 the copies are out of the view. They take 28 of the game's 128 forms, of which the minigame uses 92.
#define PIRANHAS_TREES 14
#define PIRANHAS_RING 3000.0f

static s16 piranhas_copies[PIRANHAS_TREES][2];

void widescreen_piranhas_trees_copied(omObjData* object) {
    s32 i;
    s32 side;

    for (i = 0; i < PIRANHAS_TREES; i++) {
        for (side = 0; side < 2; side++) {
            piranhas_copies[i][side] = func_80023FC8(object->model[1 + i]);
        }
    }
}

void widescreen_piranhas_trees(omObjData* object) {
    s32 i;
    s32 side;

    for (i = 0; i < PIRANHAS_TREES; i++) {
        unk_ovl_2D_struct* tree = &D_800F2B7C[object->model[1 + i]];

        for (side = 0; side < 2; side++) {
            unk_ovl_2D_struct* copy;

            if (piranhas_copies[i][side] < 0) {
                continue;
            }
            copy = &D_800F2B7C[piranhas_copies[i][side]];
            copy->unk_24 = tree->unk_24 + (side == 0 ? -PIRANHAS_RING : PIRANHAS_RING);
            copy->unk_28 = tree->unk_28;
            copy->unk_2C = tree->unk_2C;
            copy->unk_30 = tree->unk_30;
            copy->unk_34 = tree->unk_34;
            copy->unk_38 = tree->unk_38;
            copy->unk_3C = tree->unk_3C;
            copy->unk_40 = tree->unk_40;
            copy->unk_44 = tree->unk_44;
        }
    }
}
