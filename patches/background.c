// A board's background to a picture wider than 4:3. A board is a map of 64x48 tiles (src/48D90.c's
// LoadBackgroundIndex), which the game streams in from the ROM as its camera moves: func_8004A950 picks the 6x6 tiles
// under the camera's 320x240, func_8004A890 gives each a slot of the 40 it has and sends it to a thread that reads it
// (func_8004A31C) and one that decompresses it (func_8004A394), and func_8004ACEC draws them, each as two 64x24
// rectangles (func_8004AAA8), before the board's 3D. RT64's Aspect Ratio Expand widens the 3D but can't draw tiles the
// game never drew, and RT64 keeps a 4:3 picture's rectangles inside 4:3, so the board drew in a 4:3 box with its 3D
// spaces and characters out past it on black.
//
// Here the game picks 11 columns of tiles, two more on the left and three more on the right (enough for 21:9 at any
// scroll), and a row more above and below, from 112 slots (the game's 40 and 72 more here), through queues as long.
// Each tile is drawn as two textured quads at its place on the map, in the 320x240 picture's projection kept to its
// aspect (RT64's G_EX_ASPECT_ADJUST, so the window's sides show more of the map, not a stretched 4:3) and moved by the
// scroll's matrix, inside a scissor that reaches the window's edges. RT64 draws frames between the game's at the
// display's rate by interpolating matrices, and never moves a rectangle, so drawn as the game's rectangles the board
// stood still between its frames while the 3D spaces on it moved, and slid under them in a pan: the scroll's matrix
// is interpolated as the 3D camera is (the extra row and columns are what an in-between frame's picture can reach).
// Past the map's own edges there is no tile, and as the game never clears a board's frame (its tiles cover 4:3), a
// black fill goes behind the tiles, or the window's sides would show what an earlier frame left there. Everything else
// is the game's as it was.
//
// The tiles take several times the commands the game's 36 rectangles do, and the display list buffer a frame is built
// in has the size each scene gives it (func_8002B4C0) for what that scene draws, so the tiles go in display lists of
// their own here, one of a ring of them a frame, which the frame's list branches to.

#include "patches.h"
#include "rt64_extended_gbi.h"
#include "PR/os.h"
#include "engine/mallocblock.h"
#include "engine/data.h"

#define TILE_WIDTH 64
#define TILE_HEIGHT 48
#define GAME_COLUMNS 6
#define GAME_ROWS 6
#define LEFT_COLUMNS 2
#define COLUMNS (LEFT_COLUMNS + GAME_COLUMNS + 3)
#define TOP_ROWS 1
#define ROWS (TOP_ROWS + GAME_ROWS + 1)
#define GAME_SLOTS 40
#define SLOTS 112
#define SLOT_BYTES 0x1800

// src/48D90.c's own.
typedef struct {
    /* 0x00 */ u16 unk0;
    /* 0x02 */ s16 unk2;
    /* 0x04 */ void* data;
    /* 0x08 */ HuArchive* archive;
    /* 0x0C */ s32 dirTblSize;
    /* 0x10 */ u8* bytes;
} unk20;

typedef struct unkStruct19 {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ s32 unk4;
    /* 0x08 */ s32 unk8;
    /* 0x0C */ s32 unkC;
    /* 0x10 */ char unk_10[0x2C];
} unkStruct19;

extern unkStruct19* D_800C4F78;
extern u8 D_800C4F7C;
extern s16 D_800D6730;
extern s16 D_800D6732;
extern s16 D_800D6734;
extern s16 D_800D6736;
extern s16 D_800D6738;
extern unk20 D_800D6740[GAME_SLOTS];
extern unk20* D_800D6A60[GAME_COLUMNS][GAME_ROWS];
extern OSMesgQueue D_800D6AF0;
extern OSThread D_800D6BA8;
extern OSThread D_800D7560;
extern s32 D_800D7710;
extern OSMesgQueue D_800D7F20;
extern OSMesgQueue D_800D7FD8;
extern OSMesgQueue D_800D8090;
extern OSMesg D_800D80A8;
extern Gfx D_800C4F80[];

void func_8004A31C(void);
void func_8004A394(void);
void func_8004A510(void);
void func_8004A530(void);
void func_8004A56C(void);
u16 func_8004A814(s32 arg0, s32 arg1);
u32 func_8004A844(u16 arg0);
u8* func_8004A868(u16 arg0);
void func_80023728(void*);
void func_8003A4EC(Gfx** gfx, void* image, s32 arg2, s32 arg3, s32 arg4, s32 arg5, s32 arg6, s32 arg7, s32 arg8,
                   s32 arg9, s32 arg10, s32 arg11, s32 arg12, s32 arg13, s32 arg14, s32 arg15, s32 arg16);
void guOrtho(Mtx* m, f32 l, f32 r, f32 b, f32 t, f32 n, f32 f, f32 scale);
void guTranslate(Mtx* m, f32 x, f32 y, f32 z);

// The slots past the game's 40, and their tiles.
static unk20 extra_slots[SLOTS - GAME_SLOTS];
static u8 extra_slot_data[SLOTS - GAME_SLOTS][SLOT_BYTES] __attribute__((aligned(16)));
// The tiles to draw, as the game's D_800D6A60 with more columns: [column][row].
static unk20* grid[COLUMNS][ROWS];
// The three queues' messages, as many as there are slots.
static OSMesg read_messages[SLOTS];
static OSMesg decompress_messages[SLOTS];
static OSMesg done_messages[SLOTS];
// The tiles' display lists: a tile is two halves of func_8003A4EC's 7 commands, a gSPVertex and a gSP2Triangles, and
// a list is drawn while the frames after it are built, so there are more of them than the game's frames in flight.
// Each has its tiles' corners and the scroll's matrix.
#define TILE_COMMANDS (2 * (7 + 2))
#define LIST_COMMANDS (COLUMNS * ROWS * TILE_COMMANDS + 64)
#define LISTS 8
static Gfx tile_lists[LISTS][LIST_COMMANDS];
static Vtx tile_vertices[LISTS][COLUMNS * ROWS * 2 * 4];
static Mtx tile_scroll[LISTS];
static s32 tile_list_index;
// The tiles' 320x240 picture, as the game's 3D draws in.
static Mtx tile_projection;
static Vp tile_viewport = { { { SCREEN_WIDTH * 2, SCREEN_HEIGHT * 2, G_MAXZ / 2, 0 },
                              { SCREEN_WIDTH * 2, SCREEN_HEIGHT * 2, G_MAXZ / 2, 0 } } };
// The scroll the frame before drew at, to tell a cut from a pan.
static s32 last_left, last_top;
#define TILE_SCROLL_GROUP 0x4D504247

static unk20* slot(s32 i) {
    return (i < GAME_SLOTS) ? &D_800D6740[i] : &extra_slots[i - GAME_SLOTS];
}

RECOMP_PATCH void func_8004A41C(void) {
    // @recomp Queues as long as there are slots.
    osCreateMesgQueue(&D_800D6AF0, read_messages, SLOTS);
    osCreateMesgQueue(&D_800D7F20, decompress_messages, SLOTS);
    osCreateMesgQueue(&D_800D7FD8, done_messages, SLOTS);
    osCreateMesgQueue(&D_800D8090, &D_800D80A8, 2);
    D_800D7710 = 1;
    osCreateThread(&D_800D7560, 0x64, (void*)&func_8004A394, 0, &D_800D7F20, 1);
    osStartThread(&D_800D7560);
    osCreateThread(&D_800D6BA8, 0x65, (void*)&func_8004A31C, 0, &D_800D7560, 4);
    osStartThread(&D_800D6BA8);
}

RECOMP_PATCH void func_8004A5BC(void) {
    s32 i;

    osJamMesg(&D_800D6AF0, 0, 0);
    osJamMesg(&D_800D7F20, 0, 0);
    func_8004A510();
    func_8004A530();
    osRecvMesg(&D_800D8090, 0, 1);
    osRecvMesg(&D_800D8090, 0, 1);
    // @recomp Every slot.
    for (i = 0; i < SLOTS; i++) {
        unk20* s = slot(i);
        if (s->archive != NULL) {
            FreeTemp(s->archive);
            s->archive = NULL;
        }
    }
}

RECOMP_PATCH void func_8004A684(void) {
    s32 i;

    for (i = 0; i < SLOTS; i++) {
        unk20* s = slot(i);
        s->unk0 = 0;
        s->unk2 = -1;
        // @recomp The game's 40 from its heap, as before, the others from here.
        s->data = (i < GAME_SLOTS) ? func_80023668(SLOT_BYTES) : extra_slot_data[i - GAME_SLOTS];
        s->archive = NULL;
    }
    bzero(&D_800D6A60, sizeof(D_800D6A60));
    bzero(grid, sizeof(grid));
}

RECOMP_PATCH void func_8004A6F8(void) {
    s32 i;

    for (i = 0; i < SLOTS; i++) {
        unk20* s = slot(i);
        if ((s->archive == NULL) && (s->unk0 == 0)) {
            s->unk2 = -1;
        }
        s->unk0 = 0;
    }
}

RECOMP_PATCH void func_8004A73C(void) {
    s32 i;

    for (i = 0; i < SLOTS; i++) {
        unk20* s = slot(i);
        s->unk0 = 0;
        s->unk2 = -1;
        // @recomp Only the game's 40 came from its heap.
        if ((i < GAME_SLOTS) && (s->data != NULL)) {
            func_80023728(s->data);
        }
        s->data = NULL;
    }
}

RECOMP_PATCH unk20* func_8004A890(u16 arg0) {
    unk20* free_slot = NULL;
    s32 i;

    for (i = 0; i < SLOTS; i++) {
        unk20* s = slot(i);
        if (s->unk2 == -1) {
            free_slot = s;
        } else if (s->unk2 == arg0) {
            s->unk0 = 1;
            return s;
        }
    }

    if (free_slot != NULL) {
        u32 size;
        free_slot->unk0 = 1;
        free_slot->unk2 = arg0;
        free_slot->bytes = func_8004A868(arg0);
        size = func_8004A844(arg0);
        free_slot->dirTblSize = size;
        free_slot->archive = MallocTemp(size);
        osSendMesg(&D_800D6AF0, free_slot, 0);
    }
    return free_slot;
}

// Where the picture's left edge is on the map, and the game's first column of tiles there.
static s32 left_edge(void) {
    return D_800D6736 + D_800D6732 - 160;
}

static s32 first_column(void) {
    return left_edge() / TILE_WIDTH;
}

// Where its top edge is, and the game's first row.
static s32 top_edge(void) {
    return D_800D6738 + D_800D6734 - 120;
}

static s32 first_row(void) {
    return top_edge() / TILE_HEIGHT;
}

RECOMP_PATCH void func_8004A950(void) {
    s32 n, i, j;
    s32 column0 = first_column() - LEFT_COLUMNS;
    s32 row0 = first_row() - TOP_ROWS;

    // @recomp Columns past the game's six on each side and a row above and below them, the game's own first so that
    // they come first when there are too few slots: its rows, then the one below and the one above, and in each its
    // columns, then the ones right and left.
    for (n = 0; n < ROWS; n++) {
        s32 row;
        i = (n + TOP_ROWS) % ROWS;
        row = row0 + i;
        for (j = LEFT_COLUMNS; j < COLUMNS + LEFT_COLUMNS; j++) {
            s32 k = j % COLUMNS;
            s32 column = column0 + k;
            if ((column < 0) || (row < 0) || (column >= D_800C4F78->unk8) || (row >= D_800C4F78->unkC)) {
                grid[k][i] = NULL;
            } else {
                grid[k][i] = func_8004A890(func_8004A814(column, row));
            }
        }
    }
    for (i = 0; i < GAME_ROWS; i++) {
        for (j = 0; j < GAME_COLUMNS; j++) {
            D_800D6A60[j][i] = grid[j + LEFT_COLUMNS][i + TOP_ROWS];
        }
    }
}

// A command as the game writes it (the combiner and render modes func_8004ACEC sets).
static void gfx_words(Gfx** gfx, u32 w0, u32 w1) {
    Gfx* g = (*gfx)++;
    g->words.w0 = w0;
    g->words.w1 = w1;
}

// A corner of a tile, at (x, y) on the map, its texel (s, t).
static void corner(Vtx* v, s32 x, s32 y, s32 s, s32 t) {
    v->v.ob[0] = x;
    v->v.ob[1] = y;
    v->v.ob[2] = 0;
    v->v.flag = 0;
    v->v.tc[0] = s << 6;
    v->v.tc[1] = t << 6;
    v->v.cn[0] = v->v.cn[1] = v->v.cn[2] = v->v.cn[3] = 0xFF;
}

// func_8004AAA8: a tile, as its two 64x24 halves, at (x, y) on the map. Each half is a quad, not the game's rectangle,
// as RT64 moves a matrix's triangles to the display's rate between the game's frames and never a rectangle: the board
// pans with its 3D spaces on them.
static void draw_tile(Gfx** gfx, Vtx** vertices, u8* data, s32 x, s32 y) {
    s32 half;

    for (half = 0; half < 2; half++) {
        Vtx* v = *vertices;
        s32 top = y + half * (TILE_HEIGHT / 2);
        func_8003A4EC(gfx, data + half * (TILE_WIDTH * (TILE_HEIGHT / 2) * 2), 0, G_IM_SIZ_16b, TILE_WIDTH,
                      TILE_HEIGHT / 2, 0, 0, TILE_WIDTH, TILE_HEIGHT / 2, 0, 2, 2, 0, 0, 0, 0);
        corner(&v[0], x, top, 0, 0);
        corner(&v[1], x + TILE_WIDTH, top, TILE_WIDTH, 0);
        corner(&v[2], x + TILE_WIDTH, top + TILE_HEIGHT / 2, TILE_WIDTH, TILE_HEIGHT / 2);
        corner(&v[3], x, top + TILE_HEIGHT / 2, 0, TILE_HEIGHT / 2);
        *vertices += 4;
        gSPVertex((*gfx)++, v, 4, 0);
        gSP2Triangles((*gfx)++, 0, 1, 2, 0, 0, 2, 3, 0);
    }
}

RECOMP_PATCH void func_8004ACEC(Gfx** game_gfx, s32 arg1, u8 arg2) {
    s32 left, top, column0, row0, i, j;
    s16 saved;
    Gfx* list;
    Gfx** gfx;
    Vtx* vertices;
    Mtx* scroll;

    if ((arg2 != 0) || !(D_800D6730 & 4)) {
        return;
    }
    func_8004A56C();
    if (!(D_800D6730 & 1)) {
        return;
    }
    func_8004A6F8();
    saved = func_80061228(0, 0, 0);
    func_8004A950();
    func_80061264(saved);

    left = left_edge();
    top = top_edge();
    column0 = first_column() - LEFT_COLUMNS;
    row0 = first_row() - TOP_ROWS;
    if (tile_projection.m[0][0] == 0) {
        guOrtho(&tile_projection, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0, -1, 1, 1);
    }

    // @recomp In a list of the tiles' own.
    list = tile_lists[tile_list_index];
    vertices = tile_vertices[tile_list_index];
    scroll = &tile_scroll[tile_list_index];
    tile_list_index = (tile_list_index + 1) % LISTS;
    gSPDisplayList((*game_gfx)++, list);
    gfx = &list;
    gEXEnable((*gfx)++);
    // @recomp The tiles past 4:3's edges are drawn to the window's.
    gEXPushScissor((*gfx)++);
    gEXSetScissor((*gfx)++, G_SC_NON_INTERLACE, G_EX_ORIGIN_LEFT, G_EX_ORIGIN_RIGHT, 0, 0, 0, SCREEN_HEIGHT);

    // @recomp Black behind them, for past the map's edges: the game never clears a board's frame (its tiles cover
    // 4:3), so the window's sides would show what an earlier frame left there.
    gDPPipeSync((*gfx)++);
    gEXPushOtherMode((*gfx)++);
    gEXPushFillColor((*gfx)++);
    gDPSetCycleType((*gfx)++, G_CYC_FILL);
    gDPSetRenderMode((*gfx)++, G_RM_NOOP, G_RM_NOOP2);
    gDPSetFillColor((*gfx)++, (GPACK_RGBA5551(0, 0, 0, 1) << 16) | GPACK_RGBA5551(0, 0, 0, 1));
    gEXSetRectAlign((*gfx)++, G_EX_ORIGIN_CENTER, G_EX_ORIGIN_CENTER, -SCREEN_WIDTH * 4, 0, SCREEN_WIDTH * 4,
                    SCREEN_HEIGHT * 4);
    gDPFillRectangle((*gfx)++, 0, 0, 0, 0);
    gEXSetRectAlign((*gfx)++, G_EX_ORIGIN_NONE, G_EX_ORIGIN_NONE, 0, 0, 0, 0);
    gDPPipeSync((*gfx)++);
    gEXPopFillColor((*gfx)++);
    gEXPopOtherMode((*gfx)++);

    gSPDisplayList((*gfx)++, D_800C4F80);
    gfx_words(gfx, 0xFB000000, 0xFFFFFF00 | D_800C4F7C);
    if (D_800C4F7C < 0xFF) {
        if (D_800D6730 & 8) {
            gfx_words(gfx, 0xFC52FEA5, 0x11FFFBFD);
        } else {
            gfx_words(gfx, 0xFCFFFFFF, 0xFFFCFA7D);
        }
        gfx_words(gfx, 0xE200001C, 0x00504240);
    } else {
        if (D_800D6730 & 8) {
            gfx_words(gfx, 0xFC52FEA5, 0x11FFF3F9);
        }
        gfx_words(gfx, 0xE200001C, 0x00552048);
    }

    // @recomp The tiles at their places on the map, in the 320x240 picture's projection, kept to its aspect so that
    // the window's sides show more of the map, moved by the scroll: a matrix RT64 interpolates, unless the scroll
    // jumped (a cut, not a pan).
    gEXPushViewport((*gfx)++);
    gSPViewport((*gfx)++, &tile_viewport);
    gEXPushProjectionMatrix((*gfx)++);
    gEXMatrixGroup((*gfx)++, G_EX_ID_IGNORE, G_EX_INTERPOLATE_SIMPLE, G_EX_PUSH, G_MTX_PROJECTION, G_EX_COMPONENT_SKIP,
                   G_EX_COMPONENT_SKIP, G_EX_COMPONENT_SKIP, G_EX_COMPONENT_SKIP, G_EX_COMPONENT_SKIP,
                   G_EX_COMPONENT_SKIP, G_EX_COMPONENT_SKIP, G_EX_ORDER_LINEAR, G_EX_EDIT_NONE, G_EX_ASPECT_ADJUST,
                   G_EX_COMPONENT_SKIP, G_EX_COMPONENT_SKIP);
    gSPMatrix((*gfx)++, &tile_projection, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
    gEXPushGeometryMode((*gfx)++);
    gSPClearGeometryMode((*gfx)++, G_ZBUFFER | G_CULL_BOTH | G_LIGHTING | G_FOG | G_TEXTURE_GEN | G_TEXTURE_GEN_LINEAR);
    gSPSetGeometryMode((*gfx)++, G_SHADE | G_SHADING_SMOOTH);
    gSPTexture((*gfx)++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);
    guTranslate(scroll, -left, -top, 0);
    if ((left - last_left <= TILE_WIDTH) && (last_left - left <= TILE_WIDTH) && (top - last_top <= TILE_HEIGHT) &&
        (last_top - top <= TILE_HEIGHT)) {
        gEXMatrixGroupSimple((*gfx)++, TILE_SCROLL_GROUP, G_EX_PUSH, G_MTX_MODELVIEW, G_EX_COMPONENT_INTERPOLATE,
                             G_EX_COMPONENT_SKIP, G_EX_COMPONENT_SKIP, G_EX_COMPONENT_SKIP, G_EX_COMPONENT_SKIP,
                             G_EX_ORDER_LINEAR, G_EX_EDIT_NONE, G_EX_COMPONENT_SKIP, G_EX_COMPONENT_SKIP);
    } else {
        gEXMatrixGroupNoInterpolate((*gfx)++, G_EX_PUSH, G_MTX_MODELVIEW, G_EX_EDIT_NONE);
    }
    last_left = left;
    last_top = top;
    gSPMatrix((*gfx)++, scroll, G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_PUSH);

    for (i = 0; i < ROWS; i++) {
        for (j = 0; j < COLUMNS; j++) {
            if (grid[j][i] != NULL) {
                draw_tile(gfx, &vertices, grid[j][i]->data, (column0 + j) * TILE_WIDTH, (row0 + i) * TILE_HEIGHT);
            }
        }
    }

    gSPPopMatrix((*gfx)++, G_MTX_MODELVIEW);
    gEXPopMatrixGroup((*gfx)++, G_MTX_MODELVIEW);
    gEXPopMatrixGroup((*gfx)++, G_MTX_PROJECTION);
    gEXPopGeometryMode((*gfx)++);
    gEXPopProjectionMatrix((*gfx)++);
    gEXPopViewport((*gfx)++);
    gEXPopScissor((*gfx)++);
    gSPEndDisplayList((*gfx)++);
    func_8004A530();
}
