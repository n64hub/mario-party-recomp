#include "patches.h"
#include "engine/ovlman.h"

// Cast Aways (overlay 0x2B)'s sky, the picture of blue sky and clouds behind its sea and islands (sprite file
// 0x3F0002, its frame 160x120 drawn at twice its size, 320x240), which its setup (func_800F71AC_CastAways) centres at
// (160, 130): 10 lines below the screen's middle, so it covers lines 10 to 250. Above it the frame shows its clear
// colour, which the overlay's start sets to (0x21, 0x42, 0xD6), a lighter, flat blue than the sky's top: a band across
// the top of the screen, 10 of its 240 lines, that a TV's overscan hid and a window shows, 4:3 or wider.
//
// The setup is the game's but for the sky's height: drawn 13/12 as tall about the same middle, so its top reaches the
// screen's (130 - 60 x 2 x 13/12 = 0). The sky above its clouds is all but one colour, and the clouds come up a few
// lines at the horizon.

#define SKY_SCALE_Y (2.0f * 13.0f / 12.0f)

extern u16 D_800FBB5C_ovl2B;
extern u16 D_800FBB5E_ovl2B;
extern s32 D_800FBA64_ovl2B;
extern s16 D_800FBB60_CastAways;
void func_800F7308_CastAways(void);

RECOMP_PATCH void func_800F71AC_CastAways(omObjData* arg0) {
    s16 sprite;

    sprite = InitSprite(0x3F0002);
    D_800FBB5C_ovl2B = sprite;
    D_800FBB5E_ovl2B = func_80019060(sprite, 0, 1);
    func_80018D44(D_800FBB5E_ovl2B, 0x4008);
    SetBasicSpritePos(D_800FBB5E_ovl2B, 0xA0, 0x82);
    ShowBasicSprite(D_800FBB5E_ovl2B);
    SetBasicSpriteSize(D_800FBB5E_ovl2B, 2.0f, SKY_SCALE_Y);
    func_80017660(0, 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT);
    func_800176C4(0, 640.0f, 480.0f, 511.0f, 640.0f, 480.0f, 511.0f);
    arg0->model[0] = LoadFormFile(0x3F0000, 0x499);
    arg0->model[3] = func_800174C0(0x3F0001, 0x1499);
    func_80039C48((char*)&D_800FBA64_ovl2B, &D_800FBB60_CastAways);
    func_80026040(arg0->model[3]);
    arg0->func_ptr = &func_800F7308_CastAways;
}

// Its coins, coin bags and chests, which float across in three rows: the front row from the right (z 1800), the middle
// row from the left (z 1050), the back row from the right (z 300). The spawner (func_800F76B8_CastAways) places each at
// its row's fixed x, and the item's frame function (func_800F7A38_CastAways) deletes it once its middle, where
// Convert3DTo2D puts it in 4:3's 320 wide, is past 340 moving right or -20 moving left: 20 past the picture's edge.
// A wider picture shows more than 4:3 on each side (120 x the aspect - 160 of 320 more), so in 16:9 the items vanished
// inside it, 33 short of its edges, and the middle and back rows also appeared inside it, near its left and right.
//
// us.toml's hooks move both out by as much as the window's sides are past 4:3's: the frame function's bounds (the
// two below) and a new item's x, just after the spawner sets it. At 4:3 both are the game's own. Each row keeps its
// cap and spawn timer, so a row fills to its cap a little more often as its items stay on screen a little longer.

f32 mp_aspect_ratio(void);

// How far a side of the picture is past 4:3's, in 4:3's 320 wide: 0 at 4:3, whose aspect in a float is a hair over.
static f32 cast_aways_wider(void) {
    f32 wider = 120.0f * mp_aspect_ratio() - 160.0f;

    return (wider > 0.01f) ? wider : 0.0f;
}

// The screen x an item moving right is gone past (the game's 340).
f32 cast_aways_item_gone_right(void) {
    return 340.0f + cast_aways_wider();
}

// The screen x an item moving left is gone past (the game's -20).
f32 cast_aways_item_gone_left(void) {
    return -20.0f - cast_aways_wider();
}

// A new item, after the spawner sets its x: moved out from its row's x so it starts as far past the picture's edge as
// the game's bound is (20), or as far out as the picture is wider than 4:3 if that is less. The front row starts past
// the edge already and is not moved. `work` is its work (the object's unk_50), whose 0x08 is how far it moves
// along x each frame.
void cast_aways_item_spawned(omObjData* item, u8* work) {
    f32 velocity = *(f32*)(work + 0x08);
    f32 wider = cast_aways_wider();
    Vec3f at;
    Vec2f start;
    Vec2f along;
    f32 per_unit;
    f32 out;

    if (wider <= 0.0f) {
        return;
    }
    at.x = item->trans.x;
    at.y = item->trans.y;
    at.z = item->trans.z;
    Convert3DTo2D(0, &at, &start);
    at.x += 100.0f;
    Convert3DTo2D(0, &at, &along);
    per_unit = (along.x - start.x) / 100.0f;
    if (per_unit == 0.0f) {
        return;
    }
    // Moving left, it starts on the right and goes out to the right; moving right, out to the left.
    if (velocity < 0.0f) {
        out = cast_aways_item_gone_right() - start.x;
    } else {
        out = start.x - cast_aways_item_gone_left();
    }
    if (out <= 0.0f) {
        return;
    }
    if (out > wider) {
        out = wider;
    }
    item->trans.x += ((velocity < 0.0f) ? out : -out) / per_unit;
}

// The back row (z 300), moving left, at a picture wider than 4:3: past 4:3's left edge, where the game deleted it and
// now goes on to the window's, it passes the near-left island, whose shore there is nearer the camera than the row,
// so an item sank into the sand to all but its underwater part (n64hub/mario-party-recomp#190). At 4:3 the row
// passes in front of the island's tip, the rest of it outside the picture.
//
// Drawn, not moved: omMain (objmain.c) sets each object's first model to its place and scale right after its frame
// function, and us.toml's hook just after that calls this with the object. An item of the back row still floating
// (its work's 0x04 is 0) and past 4:3's left edge has its model moved toward the camera along its line of sight, to t
// of the way there, and scaled by t: the same place and size on the screen, but at a depth in front of the shore.
// t comes in over the first CAST_AWAYS_PULL_RAMP units past the edge. The object's own place, which the rods'
// catching and the game's bounds read, is the game's, and at 4:3 nothing is moved.
#define CAST_AWAYS_OVERLAY 0x2B
#define CAST_AWAYS_BACK_ROW_UNDER 600.0f
#define CAST_AWAYS_PULLED_Z 600.0f
#define CAST_AWAYS_PULL_RAMP 16.0f

void func_800F7A38_CastAways(omObjData* item);

static s32 cast_aways_loaded;

// Each overlay that loads (OvlLoad, overlays.c): Cast Aways, or another in its place.
void cast_aways_overlay(s32 overlay) {
    if (overlay == CAST_AWAYS_OVERLAY) {
        cast_aways_loaded = TRUE;
    } else if (overlay_table[overlay].ram_start == overlay_table[CAST_AWAYS_OVERLAY].ram_start) {
        cast_aways_loaded = FALSE;
    }
}

void cast_aways_item_drawn(omObjData* item) {
    Vec3f* camera;
    Vec3f at;
    Vec2f screen;
    f32 t;
    f32 ramp;
    s16 model;

    if (!cast_aways_loaded || item->func_ptr != (void*)func_800F7A38_CastAways || (item->stat & 3) != 0 ||
        item->model == NULL || item->unk_50 == NULL || cast_aways_wider() <= 0.0f) {
        return;
    }
    model = item->model[0];
    if (model < 0 || *(u16*)((u8*)item->unk_50 + 0x04) != 0 || item->trans.z >= CAST_AWAYS_BACK_ROW_UNDER) {
        return;
    }
    at.x = item->trans.x;
    at.y = item->trans.y;
    at.z = item->trans.z;
    Convert3DTo2D(0, &at, &screen);
    if (screen.x >= 0.0f) {
        return;
    }
    camera = &D_800C3110[0].pos;
    if (camera->z <= CAST_AWAYS_PULLED_Z) {
        return;
    }
    t = (camera->z - CAST_AWAYS_PULLED_Z) / (camera->z - at.z);
    ramp = -screen.x / CAST_AWAYS_PULL_RAMP;
    if (ramp < 1.0f) {
        t = 1.0f + (t - 1.0f) * ramp;
    }
    func_80025798(model, camera->x + (at.x - camera->x) * t, camera->y + (at.y - camera->y) * t,
                  camera->z + (at.z - camera->z) * t);
    func_80025830(model, item->scale.x * t, item->scale.y * t, item->scale.z * t);
}
