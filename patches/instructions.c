#include "patches.h"

// A minigame's instructions screen (overlay 0x6F) in a picture wider than 4:3. Behind it the game draws a sprite of sky
// and clouds over the whole screen, which RT64 stretches to the window's edges, and in front of that two strips of
// cloud along the bottom, 3D forms (files 0xB000A and 0xB000B) that its setup (func_800F6B14_MinigameInstructions)
// places in the middle and scales to cover 4:3. RT64's Aspect Ratio Expand widens the view, not the strips, so they
// stopped at 4:3's edges with the sky beside them.
//
// Each strip is one quad as wide as STRIP_WIDTH at a scale of 1, its texture repeated a whole number of times across
// it, so a copy of it a strip's width to its left and one to its right carry its clouds on to the window's edges (to
// 32:9) without a seam. us.toml's hooks call this after the setup has placed and scaled each strip. The copies are
// out of a 4:3 picture, but for the 7 pixels at either side the farther strip leaves bare, which a TV's overscan hides.

// Both forms' quad: 3200 of its vertex units across at their scale (their VTX1's, 0.067625), its corners at -108 and
// 108 once the game rounds them to whole units for the RSP (216.4 left a gap of 2 pixels at 1920 wide).
#define STRIP_WIDTH 216.0f
// The flags the setup loads the strips with.
#define STRIP_FLAGS 0x299

// The strip form, loaded from file, once more on each side of it.
void instructions_clouds_beside(s16 form, s32 file) {
    unk_ovl_2D_struct* strip = &D_800F2B7C[form];
    f32 x = strip->unk_24;
    f32 y = strip->unk_28;
    f32 z = strip->unk_2C;
    f32 scale_x = strip->unk_3C;
    f32 scale_y = strip->unk_40;
    f32 scale_z = strip->unk_44;
    s32 side;

    for (side = -1; side <= 1; side += 2) {
        s16 copy = LoadFormFile(file, STRIP_FLAGS);
        func_80025798(copy, x + side * STRIP_WIDTH * scale_x, y, z);
        func_80025830(copy, scale_x, scale_y, scale_z);
    }
}
