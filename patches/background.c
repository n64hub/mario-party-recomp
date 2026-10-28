// A board's background to a picture wider than 4:3. A board is a map of 64x48 tiles (src/48D90.c's
// LoadBackgroundIndex), which the game streams in from the ROM as its camera moves: func_8004A950 picks the 6x6 tiles
// under the camera's 320x240, func_8004A890 gives each a slot of the 40 it has and sends it to a thread that reads it
// (func_8004A31C) and one that decompresses it (func_8004A394), and func_8004ACEC draws them, each as two 64x24
// rectangles (func_8004AAA8), before the board's 3D. RT64's Aspect Ratio Expand widens the 3D but can't draw tiles the
// game never drew, and RT64 keeps a 4:3 picture's rectangles inside 4:3, so the board drew in a 4:3 box with its 3D
// spaces and characters out past it on black.
//
// Here the game picks 11 columns of tiles, two more on the left and three more on the right (enough for 21:9 at any
// scroll), and a row more above and below (what a frame RT64 draws between two of the game's can reach), from 112 slots
// (the game's 40 and 72 more here), through queues as long, and draws them through lib/mario-party-engine's
// board_back.c: as quads at their places on the map, in a list of their own with black behind them, kept to the
// picture's aspect and moved by an interpolated scroll, so the window's sides show more of the map, and the board pans
// with its 3D spaces. A tile holds its compressed bytes until it is decompressed, on the game's 128 KB temporary heap
// (MallocTemp), which the game sized for its 36: a board's start sends every tile in the picture at once, and on DK's
// Jungle Adventure, whose tiles are the largest, 88 of them filled the heap, MallocTemp gave the next one NULL, and the
// thread that reads it wrote it to address 0. So one in a slot past the game's holds them in board_back_archive's
// buffer for the slot, and the game's func_8004A56C, which frees them once a tile is decompressed, leaves those. A tile
// past the game's 6x6 that still takes the heap (in one of the game's slots) is sent only while fewer than
// EXTRA_IN_FLIGHT are on their way there.
//
// The game keeps its board camera where a 320x240 picture stays on the map (func_8004A1B0, which every board's camera
// goes through), so at the map's edge the wider picture ran past it, where there are no tiles, and a black strip
// opened at the window's side as the camera panned there: the rules tutorial's small board reaches its edges at every
// turn. Here the camera is kept where the wider picture stays on the map (lib/mario-party-engine's
// src/aspect_ratio.cpp gives its width), so it stops short of the edge by as much as the window shows past 4:3, and
// all a 4:3 picture showed there is still on screen. A map narrower than the picture keeps its camera in its middle,
// with black at both sides, and one no wider than 320 is the game's own. Everything else is the game's as it was.

#include "patches.h"
#include "PR/os.h"
#include "engine/mallocblock.h"
#include "engine/data.h"
#include "../lib/mario-party-engine/patches/board_back.h"

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
#define EXTRA_IN_FLIGHT 12

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
f32 mp_aspect_ratio(void);

// The slots past the game's 40, and their tiles.
static unk20 extra_slots[SLOTS - GAME_SLOTS];
static u8 extra_slot_data[SLOTS - GAME_SLOTS][SLOT_BYTES] __attribute__((aligned(16)));
// The tiles to draw, as the game's D_800D6A60 with more columns: [column][row].
static unk20* grid[COLUMNS][ROWS];
// The three queues' messages, as many as there are slots.
static OSMesg read_messages[SLOTS];
static OSMesg decompress_messages[SLOTS];
static OSMesg done_messages[SLOTS];
_Static_assert(SLOTS - GAME_SLOTS <= BOARD_BACK_ARCHIVES, "a buffer for each slot past the game's");

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
            if (!board_back_own_archive(s->archive)) {
                FreeTemp(s->archive);
            }
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

// func_8004A56C: the decompressed tiles' compressed bytes freed.
RECOMP_PATCH void func_8004A56C(void) {
    unk20* s;

    while (osRecvMesg(&D_800D7FD8, (OSMesg*)&s, 0) == 0) {
        if (s != NULL) {
            // @recomp Not board_back_archive's.
            if (!board_back_own_archive(s->archive)) {
                FreeTemp(s->archive);
            }
            s->archive = NULL;
        }
    }
}

// func_8004A890: the slot holding a tile, or a free one it is sent to be read into, or NULL if there is none or it
// isn't the game's own and too many are on their way on the heap.
static unk20* tile_slot(u16 arg0, s32 games) {
    unk20* free_slot = NULL;
    void* archive = NULL;
    s32 free_index = 0;
    s32 in_flight = 0;
    u32 size;
    s32 i;

    for (i = 0; i < SLOTS; i++) {
        unk20* s = slot(i);
        if (s->unk2 == -1) {
            free_slot = s;
            free_index = i;
        } else if (s->unk2 == arg0) {
            s->unk0 = 1;
            return s;
        }
        in_flight += (s->archive != NULL) && !board_back_own_archive(s->archive);
    }

    if (free_slot == NULL) {
        return NULL;
    }
    size = func_8004A844(arg0);
    // @recomp A slot past the game's has its own buffer for them, and one past the game's 6x6 takes the heap only while
    // few are on their way there.
    if (free_index >= GAME_SLOTS) {
        archive = board_back_archive(free_index - GAME_SLOTS, size);
    }
    if ((archive != NULL) || games || (in_flight < EXTRA_IN_FLIGHT)) {
        free_slot->unk0 = 1;
        free_slot->unk2 = arg0;
        free_slot->bytes = func_8004A868(arg0);
        free_slot->dirTblSize = size;
        free_slot->archive = (archive != NULL) ? archive : MallocTemp(size);
        osSendMesg(&D_800D6AF0, free_slot, 0);
        return free_slot;
    }
    return NULL;
}

RECOMP_PATCH unk20* func_8004A890(u16 arg0) {
    return tile_slot(arg0, TRUE);
}

// The picture's half-width, in the 320x240 screen's pixels, that the camera keeps on the map: 160 at 4:3, more in a
// wider window, as far as the map has room for.
static f32 camera_half_width(void) {
    f32 half = 120.0f * mp_aspect_ratio();

    if (half > D_800D6736) {
        half = D_800D6736;
    }
    if (half < 160.0f) {
        half = 160.0f;
    }
    return half;
}

// func_8004A1B0: the camera's place on the screen at the scale the map is drawn at, kept where the picture stays on the
// map; what it was kept from, a bit for each edge.
RECOMP_PATCH u16 func_8004A1B0(Vec2f* arg0, f32 arg1) {
    // @recomp The picture's half-width for 4:3's 160 across.
    f32 half = camera_half_width();
    u16 ret = 0;

    if (arg0->x <= ((-D_800D6736 + half) / arg1) + 160.0f) {
        arg0->x = ((-D_800D6736 + half) / arg1) + 160.0f;
        ret |= 1;
    }
    if (arg0->x >= ((D_800D6736 - half) / arg1) + 160.0f) {
        arg0->x = ((D_800D6736 - half) / arg1) + 160.0f;
        ret |= 2;
    }
    if (arg0->y <= ((-D_800D6738 + 120.0f) / arg1) + 120.0f) {
        arg0->y = ((-D_800D6738 + 120.0f) / arg1) + 120.0f;
        ret |= 4;
    }
    if (arg0->y >= ((D_800D6738 - 120.0f) / arg1) + 120.0f) {
        arg0->y = ((D_800D6738 - 120.0f) / arg1) + 120.0f;
        ret |= 8;
    }
    return ret;
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
            s32 games = (k >= LEFT_COLUMNS) && (k < LEFT_COLUMNS + GAME_COLUMNS) && (i >= TOP_ROWS) &&
                        (i < TOP_ROWS + GAME_ROWS);
            if ((column < 0) || (row < 0) || (column >= D_800C4F78->unk8) || (row >= D_800C4F78->unkC)) {
                grid[k][i] = NULL;
            } else {
                grid[k][i] = tile_slot(func_8004A814(column, row), games);
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

// The tile at (j, i) in the grid, or NULL past the grid or where there is none.
static u8* tile_data(s32 j, s32 i) {
    return ((j < COLUMNS) && (i < ROWS) && (grid[j][i] != NULL)) ? grid[j][i]->data : NULL;
}

RECOMP_PATCH void func_8004ACEC(Gfx** game_gfx, s32 arg1, u8 arg2) {
    s32 column0, row0, i, j;
    s16 saved;
    Gfx** gfx;

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

    // @recomp In a list of the tiles' own, black behind them, to the window's edges.
    gfx = board_back_begin(game_gfx);
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

    // @recomp The tiles at their places on the map, moved by the scroll.
    board_back_view(gfx, left_edge(), top_edge(), 1.0f, FALSE, D_800C4F78->unk8 * TILE_WIDTH);
    column0 = first_column() - LEFT_COLUMNS;
    row0 = first_row() - TOP_ROWS;
    for (i = 0; i < ROWS; i++) {
        for (j = 0; j < COLUMNS; j++) {
            if (grid[j][i] != NULL) {
                // func_8004AAA8's two 64x24 halves, each a quad (board_back.c says why).
                board_back_tile(gfx, (column0 + j) * TILE_WIDTH, (row0 + i) * TILE_HEIGHT, grid[j][i]->data,
                                tile_data(j + 1, i), tile_data(j, i + 1), tile_data(j + 1, i + 1));
            }
        }
    }
    board_back_end(gfx);
    func_8004A530();
}
