// What a picture wider than 4:3 needs besides RT64's Aspect Ratio Expand, which widens the 3D: a camera's view drawn to
// the window's edges, its 3D moving with the board under it. The engine's patches/camera_view.c says how; here are
// Mario Party's two functions that start a camera's view, src/1DE40.c's func_8001D658 (its viewport and projection)
// and func_8001D7DC (its scissor). A board pans its 3D by moving its camera's viewport (src/48D90.c's func_8004B208).

#include "patches.h"
#include "../lib/mario-party-engine/patches/camera_view.h"

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
