// What a picture wider than 4:3 needs besides RT64's Aspect Ratio Expand, which widens the 3D: a camera's view drawn to
// the window's edges.
//
// Each camera has a scissor (src/1DE40.c's func_8001D7DC sets it before the camera's view is drawn), and RT64 keeps a
// scissor that stops short of 4:3's edges as far in from the window's, so a board, whose camera keeps the console's
// overscan border (16, 12, 304, 228), drew in a box with black at its sides. A camera whose scissor is the whole screen
// or that border is drawn to the window's edges (gEXSetScissor, from the left and right): from top to bottom for the
// border, and between the game's own top and bottom while it animates them, as a board's wipe between turns closes it
// to a band across the middle. A screen split between players (a minigame's halves and quarters) keeps the game's.
//
// RT64 reads extended commands only after a gEXEnable in the same display list (it forgets it at each list's start),
// and reads one before as the microcode's own command: gEXSetScissor did nothing, and the scissor the text drawn before
// it left (us.toml, pfDrawFonts) stayed set for the board.

#include "patches.h"
#include "rt64_extended_gbi.h"

#define BORDER_LEFT 16
#define BORDER_TOP 12
#define BORDER_RIGHT 304
#define BORDER_BOTTOM 228

RECOMP_PATCH void func_8001D7DC(s16 index, Gfx** gfx) {
    unk_Struct00* camera = &D_800C3110[index];
    s32 x0 = camera->unkD8.x;
    s32 y0 = camera->unkD8.y;
    s32 x1 = camera->unkD8.z;
    s32 y1 = camera->unkD8.w;

    // @recomp The whole screen, or the border, to the window's edges.
    if (((x0 == 0) && (x1 == SCREEN_WIDTH)) || ((x0 == BORDER_LEFT) && (x1 == BORDER_RIGHT))) {
        if ((y0 == BORDER_TOP) && (y1 == BORDER_BOTTOM)) {
            y0 = 0;
            y1 = SCREEN_HEIGHT;
        }
        gEXEnable((*gfx)++);
        gEXSetScissor((*gfx)++, G_SC_NON_INTERLACE, G_EX_ORIGIN_LEFT, G_EX_ORIGIN_RIGHT, 0, y0, 0, y1);
        return;
    }

    gDPSetScissor((*gfx)++, G_SC_NON_INTERLACE, x0, y0, x1, y1);
}
