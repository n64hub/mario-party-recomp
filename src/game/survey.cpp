// The survey's natives (patches/survey.c says what it is for): where its struct is, and the hook at omOvlGotoEx's start
// (us.toml) that sends the game to the overlay a script asked for in place of the one it was going to.

#include "recomp.h"
#include "librecomp/helpers.hpp"

namespace {
    // The struct's place in RDRAM, and its words' offsets (patches/survey.c's struct Survey).
    gpr survey_vram = 0;
    constexpr int REQUEST = 0x08;
    constexpr int FROM = 0x0C;
    constexpr int OVERLAY = 0x10;
    constexpr int EVENT = 0x14;
    constexpr int STAT = 0x18;
    constexpr int WARPS = 0x1C;
}

// recomp_survey_register(survey): the survey's struct, once, at the first frame.
extern "C" void recomp_survey_register(uint8_t* rdram, recomp_context* ctx) {
    survey_vram = ctx->r4;
}

// The hook: omOvlGotoEx(overlay, event, stat)'s arguments, replaced by the request's if there is one and the game is
// going to the overlay it names in `from` (or `from` is -1). An event or stat of -1 keeps the game's own. A request of
// 1 is taken once; one of 2 every time until a script clears it, for a board, which the hub goes to again and again
// with different events (its turns, after each minigame).
extern "C" void mp_survey_goto(uint8_t* rdram, gpr* overlay, gpr* event, gpr* stat) {
    if (survey_vram == 0 || MEM_W(REQUEST, survey_vram) == 0) {
        return;
    }
    int32_t from = MEM_W(FROM, survey_vram);
    if (from != -1 && from != (int32_t)*overlay) {
        return;
    }
    *overlay = (int32_t)MEM_W(OVERLAY, survey_vram);
    if ((int32_t)MEM_W(EVENT, survey_vram) != -1) {
        *event = (int32_t)MEM_W(EVENT, survey_vram);
    }
    if ((int32_t)MEM_W(STAT, survey_vram) != -1) {
        *stat = (int32_t)MEM_W(STAT, survey_vram);
    }
    if (MEM_W(REQUEST, survey_vram) == 1) {
        MEM_W(REQUEST, survey_vram) = 0;
    }
    MEM_W(WARPS, survey_vram) = MEM_W(WARPS, survey_vram) + 1;
}
