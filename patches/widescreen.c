// What a picture wider than 4:3 needs besides RT64's Aspect Ratio Expand, which widens the 3D: a camera's view drawn to
// the window's edges, its 3D moving with the board under it. The engine's patches/camera_view.c says how; here are
// Mario Party's two functions that start a camera's view, src/1DE40.c's func_8001D658 (its viewport and projection)
// and func_8001D7DC (its scissor). A board pans its 3D by moving its camera's viewport (src/48D90.c's func_8004B208).

#include "patches.h"
#include "../lib/mario-party-engine/patches/camera_view.h"
#include "../lib/mario-party-engine/patches/board_back.h"
#include "rt64_extended_gbi.h"

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
// board_back_scene_widest, at 4:3), and the camera's view and the HUD stay inside 4:3.
// - 0x01, Chance Time: its backdrop, the sky and the sand, is a 2D picture of the screen's size, and its camera's
//   scissor (0, 0, 319, 239) would otherwise be widened as the whole screen's, its 3D out over the black beside it.
// - 0x03, Buried Treasure: its dirt is a 2D picture of the screen's size (sprite 0x150001, drawn by
//   func_800F9700_BuriedTreasure's rectangles, below) that the digging takes pixels out of where a player is
//   (func_800F92D4_BuriedTreasure), the players kept inside 4:3 (func_800F8C84_BuriedTreasure). Stretched over a wide
//   window, each tunnel showed further from the middle than the player digging it, by as much more as the window is
//   wider than 4:3.
// - 0x04, Treasure Divers, and 0x30, Deep Sea Divers: the sky, the sea and the reef far off are a 2D picture of the
//   screen's size, and the sea the players swim in, its surface, the sand and the rocks, is 3D that ends at 4:3's
//   edges. Stretched over a wide window, the picture's sea went on past them beside the water's tinted box, its
//   horizon and reef no longer meeting the 3D's.
// - 0x21, Tipsy Tourney: its clouds are a 2D picture of the screen's size behind the four players' views, and they
//   and the paste timer's lines end at 4:3's edges, so past them the window would show the clear's white and the
//   views' 3D.
// - 0x81, the title: its picture, the logo and the characters on white, is a 2D picture of the screen's size.
//   Stretched over a wide window, the characters and the logo were drawn as much wider as the window is.
static const u8 overlays_4x3[] = { 0x01, 0x03, 0x04, 0x21, 0x30, 0x81 };

// The minigames whose 3D ends short of a wide window's edges, and the widest each is drawn, its width over its height:
// where its stage, floor or sea ends at its narrowest across the minigame's play, at a 32:9 window. The engine draws one
// to the window's edges up to that width and black past it (board_back_scene_widest), the camera's view stopping there,
// so a window no wider shows it as before, and a wider one shows black bars rather than the frame's clear around the
// stage's edges. A floor seen in perspective is narrowest at its far edge, and the widest is that edge's. Three seas
// are widened past theirs once each is made (widen_object and widen_model, below).
// - 0x0C, Pedal Power: the back wall, off the middle, ends at 1.8 on its right.
// - 0x11, Bowl Over: the lane in the lower view ends at 3.1.
// - 0x12, Ground Pound: the hill's top edge, 2.05.
// - 0x17, Box Mountain Mayhem: the floor's near corners, 1.85.
// - 0x1A, Mushroom Mixup: the sea's far edge, off the middle as the camera turns, 1.55 on its right, its sea widened.
// - 0x1B, Hammer Drop: the sea's far edge, 1.65, its sea widened.
// - 0x1C, Grab Bag: the floor's near edge, 2.65 (narrower in the opening zoom, under START).
// - 0x1E, Bumper Balls: the sea's far edge, 1.6, its sea widened.
// - 0x1F, Tightrope Treachery: the sea's far edge under the sky, 1.8.
// - 0x23, Crane Game: the cabinet's back wall, 1.6.
// - 0x24, Flower Shower: the field's edge, 2.15.
// - 0x26, Mario Bandstand: the stage's back wall, 1.55.
// - 0x2A, the 2 VS 2 field's near edge, 1.7.
typedef struct {
    u8 overlay;
    f32 widest;
} ShortScene;

static const ShortScene short_scenes[] = {
    { 0x0C, 1.8f },  { 0x11, 3.1f },  { 0x12, 2.05f }, { 0x17, 1.85f }, { 0x1A, 1.55f }, { 0x1B, 1.65f }, { 0x1C, 2.65f },
    { 0x1E, 1.6f },  { 0x1F, 1.8f },  { 0x23, 1.6f },  { 0x24, 2.15f }, { 0x26, 1.55f }, { 0x2A, 1.7f },
};

void widescreen_overlay(s32 overlay) {
    f32 widest = 0.0f;
    u32 i;

    for (i = 0; i < sizeof(overlays_4x3); i++) {
        if (overlays_4x3[i] == overlay) {
            widest = 4.0f / 3.0f;
        }
    }
    for (i = 0; i < sizeof(short_scenes) / sizeof(short_scenes[0]); i++) {
        if (short_scenes[i].overlay == overlay) {
            widest = short_scenes[i].widest;
        }
    }
    board_back_scene_widest(widest);
}

// The sprites the sprite manager has loaded, each's first frame's texels first.
typedef struct {
    u8* texels;
} LoadedFrame;
typedef struct {
    LoadedFrame* frame;
} LoadedSprite;
extern LoadedSprite* D_800EC700[];
extern s16 D_800FC198_BuriedTreasure[6];

// Buried Treasure's dirt, a sprite of 320x244 the game draws as 61 rectangles 4 lines high, each the screen's width.
// RT64 stretches a rectangle that covers the screen's whole width over a wide window by itself, and the scene's black
// sides covered only so much of it: over a window as wide as 32:9 the dirt still showed stretched inside 4:3, its
// tunnels away from the players digging them. Its rectangles are kept to 4:3 (G_EX_ASPECT_ADJUST), as the console
// draws them. With their three commands the list is 622 of the 1024 the game allocates for it.
RECOMP_PATCH void func_800F9700_BuriedTreasure(Gfx* arg0) {
    u8* timg = D_800EC700[D_800FC198_BuriedTreasure[1]]->frame->texels;
    s16 i;

    gDPPipeSync(arg0++);
    gDPSetCycleType(arg0++, G_CYC_1CYCLE);
    gSPSetGeometryMode(arg0++, G_TEXTURE_GEN_LINEAR);
    gDPSetCombineMode(arg0++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPSetRenderMode(arg0++, G_RM_ZB_XLU_SURF, G_RM_NOOP2);
    gDPSetTextureFilter(arg0++, G_TF_POINT);
    gDPSetTexturePersp(arg0++, G_TP_NONE);
    gDPSetColorDither(arg0++, G_CD_DISABLE);

    // @recomp The rectangles kept to 4:3.
    gEXEnable(arg0++);
    gEXSetRectAspect(arg0++, G_EX_ASPECT_ADJUST);

    for (i = 0; i < 61; i++) {
        gDPLoadTextureTile(arg0++, timg, G_IM_FMT_RGBA, G_IM_SIZ_16b, 320, 0, 0, i * 4, 319, i * 4 + 3, 0,
                           G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, 2, G_TX_NOLOD,
                           G_TX_NOLOD);
        gSPTextureRectangle(arg0++, 0, (i * 4) * 4, 320 * 4, (i * 4 + 4) * 4, G_TX_RENDERTILE, 0, 0, 0x0400, 0x0400);
    }

    gEXSetRectAspect(arg0++, G_EX_ASPECT_AUTO);
    gSPEndDisplayList(arg0++);
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

// The seas and fields of short_scenes that are widened in x as they are made, so that they reach a wide window's edges
// (board_back_stage_widened, which moves the scene's widest out with them): a sea or a field with nothing standing at
// its edges, which reads the same wider. A room's walls, a stage built for 4:3 and a painted backdrop keep their cap, and
// so does Ground Pound's forest floor, whose texture showed smeared stretched that far, bare of the trees past the
// middle, Flower Shower's field, whose rocky rim around the pond showed stretched with it, and Tightrope Treachery's sea,
// which keeps its width whatever its object's or its model's scale. us.toml's hooks call these in each one's setup, once
// the stage is made; the game never writes these scales again.
static void widen_object(omObjData* object, f32 widest) {
    object->scale.x *= board_back_stage_widened(widest);
}

static void widen_model(s16 model, f32 widest) {
    D_800F2B7C[model].unk_3C *= board_back_stage_widened(widest);
}

// Hammer Drop's sea, a grid of 6x6 squares the game builds in code (func_800FA2E0_HammerDrop) from x -2400 to 2400.
void widescreen_hammer_sea(s16 model) {
    widen_model(model, 1.65f);
}

// Bumper Balls' sea, a grid built the same way (func_800FB2EC_BumperBalls) from x -1200 to 1200, the island apart.
void widescreen_bumper_sea(s16 model) {
    widen_model(model, 1.6f);
}

// Mushroom Mixup's sea, its first model (file 0x2C0003), whose texture the game scrolls.
void widescreen_mushroom_sea(omObjData* sea) {
    widen_object(sea, 1.55f);
}
