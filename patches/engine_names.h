#ifndef ENGINE_NAMES_H
#define ENGINE_NAMES_H

// Mario Party's names for what lib/mario-party-engine's patches use (its README lists them).

#include "engine/process.h"

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

#endif
