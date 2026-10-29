#ifndef ENGINE_NAMES_H
#define ENGINE_NAMES_H

// Mario Party's names for what lib/mario-party-engine's patches use (its README lists them).

#include "engine/process.h"
#include "engine/pad.h"

void HuUnlinkPrc(Process** root, Process* process);
void HuPrcEnd(void);

#define PRC_SYS_INIT HuPrcInit
#define PRC_CHILD_WAIT HuPrcChildWatch
#define PRC_END HuPrcEnd
#define PRC_UNLINK HuUnlinkPrc
#define PRC_HEAP_FREE HuMemDirectFree
#define PRC_TOP processtop
#define PRC_CURRENT processcur
#define PRC_COUNT processcnt

// The controllers' setup and what it uses (include/engine/pad.h; the setup itself is asm in the decomp, and src/62140.c
// declares RequestSIFunction).
s32 RequestSIFunction(unkMesg* siMessg, HuSiFunc func, void* arg, s32 type);

#define PAD_INIT func_800138DC
#define PAD_INIT_SI func_80013770
#define PAD_REQUEST RequestSIFunction
#define PAD_REQUEST_T unkMesg
#define PAD_AUTO_READ func_80014174
#define PAD_AUTO_READING D_800D12CC
#define PAD_STICK_LIMIT func_80013AEC
#define PAD_LOCK D_800D12D0
#define PAD_LOCK_MESG D_800D12E8
#define PAD_CONTROLLERS D_800F3778

#endif
