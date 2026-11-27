#include "patches.h"

// Bumper Balls' (overlay 0x1E) and Hammer Drop's (overlay 0x1B) sea is a grid the overlay builds itself: 6 x 6 quads,
// each of its own 4 vertices (144 in all, 24 a row: the 12 of its far side, then the 12 of its near side, two to a
// quad), 400 x 400 in Bumper Balls (x -1200 to 1200, z -1550 to 850) and 800 x 820 in Hammer Drop (x -2400 to 2400,
// z -4060 to 860). Its setup (func_800FB2EC_BumperBalls, func_800FA2E0_HammerDrop) writes the grid into two buffers
// for each of the game's frame buffers: one the game rewrites each frame (its waves' heights and its scrolling
// texture's coordinates, func_800FC618_BumperBalls and func_800FB050_HammerDrop, the current frame's copy only), and
// one it never moves (a second, still layer that takes the waves' heights from the first). Past the grid's far edge,
// at the top of the screen, the frame shows its clear colour, dark blue in Bumper Balls and black in Hammer Drop: a
// band the waves make ragged, up to 20 of 1080 lines in Bumper Balls and 12 in Hammer Drop, that a TV's overscan hid
// and a window shows, 4:3 or wider.
//
// The grid's outer quads reach WATER_EDGE_MORE times further out, on all four sides: each outer vertex moves away from
// the quad's inner one along that axis, and its texture coordinate with it, so the pattern keeps its size and the
// quad its texels. The positions and the still layer's coordinates are set once, after the setup; the moving layer's
// coordinates are set again after the game writes them each frame. Its waves are the game's: the outer vertices keep
// the heights it gives them.

#define WATER_EDGE_MORE 4
#define WATER_VERTICES 144
#define WATER_ROW 24
#define WATER_SIDE 6

typedef struct {
    s16 ob[3];
    u16 flag;
    s16 tc[2];
    u8 cn[4];
} WaterVtx;

extern u8 D_800F37DA;
extern u8 D_800F37F0;

static s16 water_clamp(s32 value) {
    return value > 0x7FFF ? 0x7FFF : value < -0x8000 ? -0x8000 : (s16)value;
}

// Moves `outer` away from `inner`: its position along ob[axis] (when `position`) and its texture coordinate tc[st].
static void water_stretch(WaterVtx* outer, WaterVtx* inner, s32 axis, s32 st, s32 position) {
    if (position) {
        outer->ob[axis] = water_clamp(inner->ob[axis] + (outer->ob[axis] - inner->ob[axis]) * WATER_EDGE_MORE);
    }
    outer->tc[st] = water_clamp(inner->tc[st] + (outer->tc[st] - inner->tc[st]) * WATER_EDGE_MORE);
}

// Each outer quad of one buffer: its far row's far vertices, its near row's near ones, its left column's left ones
// and its right column's right ones. x is ob[0] with tc[0], z ob[2] with tc[1].
static void water_edges(WaterVtx* v, s32 position) {
    s32 i;

    if (v == NULL) {
        return;
    }
    for (i = 0; i < WATER_SIDE; i++) {
        WaterVtx* far = &v[2 * i];
        WaterVtx* near = &v[(WATER_SIDE - 1) * WATER_ROW + WATER_ROW / 2 + 2 * i];
        WaterVtx* left = &v[i * WATER_ROW];
        WaterVtx* right = &v[i * WATER_ROW + 2 * (WATER_SIDE - 1)];

        water_stretch(&far[0], &far[WATER_ROW / 2], 2, 1, position);
        water_stretch(&far[1], &far[WATER_ROW / 2 + 1], 2, 1, position);
        water_stretch(&near[0], &near[-WATER_ROW / 2], 2, 1, position);
        water_stretch(&near[1], &near[-WATER_ROW / 2 + 1], 2, 1, position);
        water_stretch(&left[0], &left[1], 0, 0, position);
        water_stretch(&left[WATER_ROW / 2], &left[WATER_ROW / 2 + 1], 0, 0, position);
        water_stretch(&right[1], &right[0], 0, 0, position);
        water_stretch(&right[WATER_ROW / 2 + 1], &right[WATER_ROW / 2], 0, 0, position);
    }
}

// After the setup: every buffer's positions, and the still layer's coordinates.
static void water_setup(WaterVtx** moving, WaterVtx** still) {
    s32 i;

    for (i = 0; i < D_800F37DA; i++) {
        water_edges(moving[i], TRUE);
        water_edges(still[i], TRUE);
    }
}

// After the game's frame: the moving layer's coordinates, which it has just written.
static void water_frame(WaterVtx** moving) {
    water_edges(moving[D_800F37F0], FALSE);
}

// Bumper Balls.

extern WaterVtx* D_800FCFA8_BumperBalls[];
extern WaterVtx* D_800FCFC8_BumperBalls[];
void func_800FB2EC_BumperBalls(void);
void func_800FC618_BumperBalls(void);
void func_800F6E10_BumperBalls(omObjData* arg0);

RECOMP_PATCH void func_800F6D98_BumperBalls(omObjData* arg0) {
    arg0->model[0] = LoadFormFile(0x310000, 0x20289);
    arg0->model[1] = LoadFormFile(0x380002, 0x48D);
    arg0->model[2] = LoadFormFile(0x380003, 0x48D);
    func_800FB2EC_BumperBalls();
    water_setup(D_800FCFA8_BumperBalls, D_800FCFC8_BumperBalls);
    arg0->func_ptr = &func_800F6E10_BumperBalls;
}

RECOMP_PATCH void func_800F6E10_BumperBalls(omObjData* arg0) {
    func_800FC618_BumperBalls();
    water_frame(D_800FCFA8_BumperBalls);
}

// Hammer Drop.

extern WaterVtx* D_800FB938_HammerDrop[];
extern WaterVtx* D_800FB958_HammerDrop[];
void func_800FA2E0_HammerDrop(void);
void func_800FB050_HammerDrop(void);
void func_800F6ADC_HammerDrop(void);

RECOMP_PATCH void func_800F6A54_HammerDrop(omObjData* arg0) {
    arg0->unk_50 = func_80023684(sizeof(unkGlobalStruct_02), 0x7918);
    arg0->model[0] = LoadFormFile(0x380002, 0x48D);
    arg0->model[1] = LoadFormFile(0x380003, 0x48D);
    omSetTra(arg0, 0.0f, 1000.0f, 1000.0f);
    func_800FA2E0_HammerDrop();
    water_setup(D_800FB938_HammerDrop, D_800FB958_HammerDrop);
    arg0->func_ptr = (void*)&func_800F6ADC_HammerDrop;
}

RECOMP_PATCH void func_800F6ADC_HammerDrop(void) {
    func_800FB050_HammerDrop();
    water_frame(D_800FB938_HammerDrop);
}
