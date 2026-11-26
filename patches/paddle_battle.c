#include "patches.h"

// Paddle Battle (overlay 0x33), two things in what it draws: its river's joins and its shafts of sunlight.
//
// Its river is in four sections one after another down the course, each two models: the ground (the river bed, its
// banks and the boulders on them, drawn opaque) and the water over it (file 0x2089, a translucent surface the game draws
// without writing depth). Its setup (func_800FBC00_PaddleBattle) places them from tables of heights and depths, the
// grounds at (0, 7500), (0, 2500), (0, -2495), (0, -7490) and the waters at (0, 7500), (0, 2500), (0, -2497),
// (-3, -7485), and their edges miss each other by a few units. The waters' by a unit or two: the second's and third's
// first edges are 1 unit lower than the last edge of the one before, the fourth's 1 unit higher, and the second's
// starts 2 units down the course from the first's end, the fourth's 2 units up from the third's; where two overlap,
// both blends show. The grounds' by up to 8: the second's first edge stops 5 units down the course from the first's
// last, right through a boulder in the middle of the river, and the third's and fourth's bed and banks start a few
// units to the side of the one before. Through each gap the sky shows, and through the water over it, as a thin bright
// line straight across the river: the N64's 240 lines hid it, a window's resolution shows it.
//
// us.toml's hook calls this once the setup has placed the eight. It moves each water after the first so its first edge
// meets the last edge of the one before it, as that one has moved (measured in the running game), then makes each
// join exact: every vertex of a section within 12 units of one of the section before it, ground to ground and water to
// water, is put on that one, in each of its model's buffers (the game steps through a water's buffers as it flows and
// writes only their texture coordinates; a ground has one). A section's own vertices are further apart than that.

// Each water's move, up the course (z) and up (y). The first stays where it is.
static const f32 river_moves[4][2] = { { 0.0f, 0.0f }, { 2.0f, 1.0f }, { 2.0f, 2.0f }, { 0.0f, 1.0f } };

// The river object's first four models are the grounds, the next four the waters, each down the course.
#define RIVER_GROUND 0
#define RIVER_WATER 4
#define RIVER_SECTIONS 4
// How far a ground's place may be from its water's for the two to be taken as one section.
#define SECTION_NEAR 10.0f

// The display list commands a section's vertices are found by: the G_MOVEWORD that sets segment 1 to the model's
// buffer, which opens each of its lists, G_VTX, G_SETPRIMCOLOR (only the water's lists set one) and G_ENDDL.
#define CMD_SEGMENT_FIRST 0xDB0A0000u
#define CMD_VTX 0x01u
#define CMD_SETPRIMCOLOR 0xFAu
#define CMD_ENDDL 0xDFu
// A ground has up to 1230 vertices in about 30 lists, a water fewer than 140 in 5 to 7.
#define SECTION_LISTS 64
#define SECTION_LIST_LONGEST 1024
#define SECTION_VERTICES 1536
// How far a vertex may be from one of the section before it to be put on it.
#define JOIN_REACH 12.0f

static s32 in_rdram(void* p) {
    return ((u32)p >> 24) == 0x80;
}

// Marks in drawn the vertices of the model's buffers its display lists draw (not the 8 corners of each list's bounding
// box, the first G_VTX of a list, which the game loads for G_CULLDL), says whether a list sets a primitive colour, and
// returns how far the vertices reach. The list array ends at an entry that is no list.
static s32 section_drawn(unk2C0C0StructC0* data, u8* drawn, s32* translucent) {
    s32 reach = 0;
    s32 i;
    s32 c;
    s32 v;

    bzero(drawn, SECTION_VERTICES);
    *translucent = FALSE;
    if (!in_rdram(data) || !in_rdram(data->unk_00)) {
        return 0;
    }
    for (i = 0; i < SECTION_LISTS; i++) {
        u32* list = (u32*)data->unk_00[i];
        s32 boxed = FALSE;

        if (!in_rdram(list) || (u32)list == (u32)data->unk_00 || list[0] != CMD_SEGMENT_FIRST) {
            break;
        }
        for (c = 0; c < SECTION_LIST_LONGEST && (list[c * 2] >> 24) != CMD_ENDDL; c++) {
            u32 w0 = list[c * 2];
            u32 w1 = list[c * 2 + 1];

            if ((w0 >> 24) == CMD_SETPRIMCOLOR) {
                *translucent = TRUE;
            } else if ((w0 >> 24) == CMD_VTX && (w1 >> 24) == 0x01) {
                // F3DEX2's G_VTX: the count in bits 12 to 19, the vertices from segment 1 (the model's buffer) at the
                // offset in w1's low 24 bits.
                s32 count = (w0 >> 12) & 0xFF;
                s32 first = (w1 & 0xFFFFFF) / sizeof(unk2C0C0StructE0);

                if (!boxed) {
                    boxed = TRUE;
                    continue;
                }
                for (v = first; v < first + count && v < SECTION_VERTICES; v++) {
                    drawn[v] = TRUE;
                }
                if (v > reach) {
                    reach = v;
                }
            }
        }
    }
    return reach;
}

static s16 rounded(f32 x) {
    return (s16)(s32)(x < 0.0f ? x - 0.5f : x + 0.5f);
}

// Puts each drawn vertex of down within JOIN_REACH of a drawn vertex of up (the section before it) on the nearest, in
// each of down's buffers, once each: a model repeats its last buffer in the slots it doesn't use.
static void join_made_exact(unk_ovl_2D_struct* up, unk_ovl_2D_struct* down) {
    static u8 up_drawn[SECTION_VERTICES];
    static u8 down_drawn[SECTION_VERTICES];
    unk2C0C0StructE0* a;
    unk2C0C0StructE0* b;
    s32 translucent;
    s32 up_reach;
    s32 down_reach;
    s32 v;
    s32 w;
    s32 i;
    s32 j;

    up_reach = section_drawn(up->unk_6C, up_drawn, &translucent);
    down_reach = section_drawn(down->unk_6C, down_drawn, &translucent);
    if (up_reach == 0 || down_reach == 0 || !in_rdram(up->unk_6C->unk_08[0]) || !in_rdram(down->unk_6C->unk_08[0])) {
        return;
    }
    a = up->unk_6C->unk_08[0];
    b = down->unk_6C->unk_08[0];
    for (v = 0; v < down_reach; v++) {
        f32 x;
        f32 y;
        f32 z;
        f32 nearest = SQ(JOIN_REACH);
        s32 on = -1;

        if (!down_drawn[v]) {
            continue;
        }
        x = b[v].unk_00 + down->unk_24;
        y = b[v].unk_02 + down->unk_28;
        z = b[v].unk_04 + down->unk_2C;
        for (w = 0; w < up_reach; w++) {
            f32 d;

            if (!up_drawn[w]) {
                continue;
            }
            d = SQ(a[w].unk_00 + up->unk_24 - x) + SQ(a[w].unk_02 + up->unk_28 - y) + SQ(a[w].unk_04 + up->unk_2C - z);
            if (d > 0.0f && d < nearest) {
                nearest = d;
                on = w;
            }
        }
        if (on < 0) {
            continue;
        }
        x = a[on].unk_00 + up->unk_24 - down->unk_24;
        y = a[on].unk_02 + up->unk_28 - down->unk_28;
        z = a[on].unk_04 + up->unk_2C - down->unk_2C;
        for (i = 0; i < ARRAY_COUNT(down->unk_6C->unk_08); i++) {
            unk2C0C0StructE0* buffer = down->unk_6C->unk_08[i];

            if (!in_rdram(buffer)) {
                continue;
            }
            for (j = 0; j < i; j++) {
                if (down->unk_6C->unk_08[j] == buffer) {
                    break;
                }
            }
            if (j == i) {
                buffer[v].unk_00 = rounded(x);
                buffer[v].unk_02 = rounded(y);
                buffer[v].unk_04 = rounded(z);
            }
        }
    }
}

// The river object's grounds, down the course, if its first four models are four opaque sections each at its water's
// place; else NULL in the first.
static void grounds_found(omObjData* river, unk_ovl_2D_struct** grounds) {
    static u8 drawn[SECTION_VERTICES];
    s32 translucent;
    s32 i;
    s32 j;

    for (i = 0; i < RIVER_SECTIONS; i++) {
        grounds[i] = &D_800F2B7C[river->model[RIVER_GROUND + i]];
        if (section_drawn(grounds[i]->unk_6C, drawn, &translucent) == 0 || translucent) {
            grounds[0] = NULL;
            return;
        }
    }
    // Down the course is down z: sorted so, then each matched with its water.
    for (i = 1; i < RIVER_SECTIONS; i++) {
        for (j = i; j > 0 && grounds[j]->unk_2C > grounds[j - 1]->unk_2C; j--) {
            unk_ovl_2D_struct* t = grounds[j];

            grounds[j] = grounds[j - 1];
            grounds[j - 1] = t;
        }
    }
    for (i = 0; i < RIVER_SECTIONS; i++) {
        f32 d = grounds[i]->unk_2C - D_800F2B7C[river->model[RIVER_WATER + i]].unk_2C;

        if (d > SECTION_NEAR || d < -SECTION_NEAR) {
            grounds[0] = NULL;
            return;
        }
    }
}

void paddle_battle_river_joined(omObjData* river) {
    unk_ovl_2D_struct* grounds[RIVER_SECTIONS];
    s32 i;

    for (i = 0; i < RIVER_SECTIONS; i++) {
        unk_ovl_2D_struct* water = &D_800F2B7C[river->model[RIVER_WATER + i]];

        water->unk_2C += river_moves[i][0];
        water->unk_28 += river_moves[i][1];
    }
    for (i = 1; i < RIVER_SECTIONS; i++) {
        join_made_exact(&D_800F2B7C[river->model[RIVER_WATER + i - 1]], &D_800F2B7C[river->model[RIVER_WATER + i]]);
    }
    grounds_found(river, grounds);
    if (grounds[0] != NULL) {
        for (i = 1; i < RIVER_SECTIONS; i++) {
            join_made_exact(grounds[i - 1], grounds[i]);
        }
    }
}

// Its shafts of sunlight, the pale diagonal strips over the jungle. The setup makes eight
// (func_800FBAE0_PaddleBattle calls func_800FC39C_PaddleBattle eight times), each a model of file 0x120007 at a scale
// of 0.5 with two quads of its own: a faint layer of dust whose texture scrolls, and over it a pale quad, about 120 of
// its units across and 700 long, whose 16x16 IA16 texture is the shaft's light. That texture, which all eight share,
// is the colour 0xFE with an alpha of 180 - 10 x its row: the same across each row to its sides, so the strip had hard
// straight edges, starting at 70% at its top and still at 12% at its bottom, so it looked cut off in mid-air. In a wide
// window, where the shafts reach out beside the jungle, those quads read as quads.
//
// Each model's light is softened once it is made (us.toml's hook in func_800FC39C_PaddleBattle, with the model's index
// from func_800174C0): the texture fades to nothing at its sides (a smoothstep over its outer 5 columns), fades in over
// its top 3 rows and out over its bottom 4, keeping 180 - 10 x its row between, and the light's quad is half as wide
// again, so that with its outer columns faded the shaft still reads about as wide as the old strip.
// Each model has vertex buffers of its own (up to 8, its unk_08, which the dust's scrolling steps through), the light's
// four vertices the same in each, and the game never writes their x again. The light is found through the model's
// display lists, not at a fixed address, and changed only while it is the texture above: the first shaft's model finds
// it so, and the seven after it find it changed and widen their quads alone.

#define LIGHT_SIZE 16
#define LIGHT_COLOR 0xFE
#define LIGHT_WIDER 1.5f
// The display list commands that find the light: its G_SETTIMG (IA, 16-bit) and its tile's size (16x16), then (with
// G_VTX, above) the four vertices it is drawn with.
#define CMD_SETTIMG_IA16 0xFD700000u
#define CMD_SETTILESIZE 0xF2000000u
#define LIGHT_TILE_SIZE 0x0003C03Cu
// The light's model has two display lists, the dust's and the light's, each about 24 commands long.
#define SHAFT_LISTS 2
#define LIST_LONGEST 64

static f32 smooth(f32 t) {
    if (t <= 0.0f) {
        return 0.0f;
    }
    if (t >= 1.0f) {
        return 1.0f;
    }
    return t * t * (3.0f - 2.0f * t);
}

static f32 at_most_one(f32 t) {
    return t < 1.0f ? t : 1.0f;
}

static s32 light_unchanged(u16* light) {
    s32 x;
    s32 y;

    for (y = 0; y < LIGHT_SIZE; y++) {
        for (x = 0; x < LIGHT_SIZE; x++) {
            if (light[y * LIGHT_SIZE + x] != ((LIGHT_COLOR << 8) | (180 - 10 * y))) {
                return FALSE;
            }
        }
    }
    return TRUE;
}

static void light_soften(u16* light) {
    s32 x;
    s32 y;

    for (y = 0; y < LIGHT_SIZE; y++) {
        f32 row = (180 - 10 * y) * at_most_one(y / 3.0f) * at_most_one((LIGHT_SIZE - 1 - y) / 4.0f);

        for (x = 0; x < LIGHT_SIZE; x++) {
            f32 alpha = row * smooth(x / 5.0f) * smooth((LIGHT_SIZE - 1 - x) / 5.0f);

            light[y * LIGHT_SIZE + x] = (LIGHT_COLOR << 8) | (u8)(s32)(alpha + 0.5f);
        }
    }
}

// The light's vertices wider in each of the model's buffers, once each: a model repeats its last buffer in the slots
// it doesn't use.
static void quad_widen(unk2C0C0StructC0* data, s32 first, s32 count) {
    s32 i;
    s32 j;
    s32 v;

    for (i = 0; i < ARRAY_COUNT(data->unk_08); i++) {
        unk2C0C0StructE0* buffer = data->unk_08[i];

        if (buffer == NULL) {
            continue;
        }
        for (j = 0; j < i; j++) {
            if (data->unk_08[j] == buffer) {
                break;
            }
        }
        if (j < i) {
            continue;
        }
        for (v = first; v < first + count; v++) {
            buffer[v].unk_00 = (s16)(s32)(buffer[v].unk_00 * LIGHT_WIDER);
        }
    }
}

void paddle_battle_shaft(s16 model) {
    unk2C0C0StructC0* data = D_800F2B7C[model].unk_6C;
    s32 i;
    s32 c;

    if (data == NULL || data->unk_00 == NULL) {
        return;
    }
    for (i = 0; i < SHAFT_LISTS; i++) {
        u32* list = (u32*)data->unk_00[i];
        u16* light = NULL;
        s32 sized = FALSE;

        if (((u32)list >> 24) != 0x80) {
            continue;
        }
        for (c = 0; c < LIST_LONGEST && (list[c * 2] >> 24) != CMD_ENDDL; c++) {
            u32 w0 = list[c * 2];
            u32 w1 = list[c * 2 + 1];

            if (w0 == CMD_SETTIMG_IA16 && (w1 >> 24) == 0x80) {
                light = (u16*)w1;
            } else if (w0 == CMD_SETTILESIZE && w1 == LIGHT_TILE_SIZE) {
                sized = TRUE;
            } else if ((w0 >> 24) == CMD_VTX && light != NULL && sized) {
                // F3DEX2's G_VTX: the count in bits 12 to 19, the first index plus the count in bits 1 to 7, the
                // vertices from segment 1 (the model's buffer) at the offset in w1's low 24 bits.
                s32 count = (w0 >> 12) & 0xFF;
                s32 first = (w1 & 0xFFFFFF) / sizeof(unk2C0C0StructE0);

                if (light_unchanged(light)) {
                    light_soften(light);
                }
                quad_widen(data, first, count);
                return;
            }
        }
    }
}
