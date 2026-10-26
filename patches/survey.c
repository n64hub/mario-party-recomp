// The survey: lets a script take the game to any overlay (a board, a minigame, a menu) by itself, for the page's shader
// list (src/rt64_shaders.txt, recorded by tools/shader_plan.json) and its play plans, without anyone playing a board
// to its minigames. It does nothing unless a script asks.
//
// A script finds this struct in RDRAM by its two magic words. It writes an overlay's index, an event and a stat (as
// omOvlGotoEx takes them, src/objmain.c), the overlay to take the place of in `from` (-1 for any), then `request`. The
// next time the game goes to that overlay, it goes to the one asked for instead: a hook at omOvlGotoEx's start (us.toml)
// changes its arguments (src/game/survey.cpp). So the game leaves the overlay it is in its own way, with what it frees
// and sets up as it does, which a call of omOvlGotoEx from elsewhere skips: called from the village, the Mini-Game
// House's setup read through a message window the village had not closed. Going from the overlay a board starts from
// (0x3E, the rules' board, which the village's warp pipe leads to on a new save) to another board or a minigame, the
// four players are set up as a board has them.
//
// `warps` counts the requests taken, and every frame the struct carries the overlay playing (the one omOvlGotoEx last
// went to) and a count of the scheduler's frames (HuPrcCall, patches/process.c, calls survey_tick once a frame), which
// plans hold their waits on.

#include "patches.h"

// omOvlGotoEx's (src/objmain.c, not matched C): the overlay it last went to.
extern s32 D_800F09F4;

void recomp_survey_register(void* survey);

struct Survey {
    u32 magic[2];
    volatile s32 request;
    volatile s32 from;
    volatile s32 overlay;
    volatile s32 event;
    volatile s32 stat;
    volatile u32 warps;
    volatile s32 cur_overlay;
    volatile u32 frames;
};

struct Survey survey = { { 0x4D505254, 0x53525659 }, 0, -1, 0, 0, 0, 0, -1, 0 }; // "MPRT" "SRVY"

void survey_tick(void) {
    if (survey.frames == 0) {
        recomp_survey_register(&survey);
    }
    survey.cur_overlay = D_800F09F4;
    survey.frames++;
}
