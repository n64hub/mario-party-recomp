# The process scheduler

Hudson's engine runs most of a game as processes, small functions with stacks of their own that a scheduler runs in
turn once a frame and that switch with the game's own setjmp and longjmp, which recompiled code can't do. Here each
live process runs on a native thread of its own, one at a time, from the code the Mario Party recomps share:
`lib/mario-party-engine` ([its docs/processes.md](https://github.com/n64hub/mario-party-engine/blob/main/docs/processes.md)
says how it works and why). This file says what is Mario Party's.

## What is Mario Party's

- `patches/engine_names.h` gives the engine's patches the decomp's names (`src/engine/process.c`): `HuPrcInit`,
  `HuPrcChildWatch` and `HuPrcEnd`, `HuUnlinkPrc`, the list's `processtop`, `processcur` and `processcnt`, and
  `HuMemDirectFree` for the heap a process leaves.
- The natives' lines are in `patches/syms.ld`, and the survey's `survey_tick` in `patches/survey.c`.
- The game calls setjmp and longjmp nowhere else, and `src/game/libultra.cpp`'s setjmp aborts.
- The warp pipe's room, where Toad sends the players off on an adventure, reads s3 in its first message's wait before
  it sets it: a new process starts with the registers its jmp_buf holds, as on a console, and with 0 there it closes
  and reopens Toad's window over and over.
- Its swap chain (`src/engine/graphics.c`) draws each frame in several RSP tasks, so the game presents at the VI
  (`src/main/main.cpp`).

## How much it costs

Built with `-DMP_PROCESS_STATS=ON`, the program prints every 5 seconds how many processes are alive, how many threads
it has made, how many resumes a second the scheduler makes and what each costs beyond the process's own run (the two
handoffs, from the scheduler's thread to the process's and back).

On the rules board Toad shows a new adventure on, which four computer players play with the board engine, a board
keeps 15 to 18 processes alive, and the scheduler makes about 450 resumes a second, 15 a frame at the game's 30. The
title screens keep the most alive: up to 29, with about 900 resumes a second. 30 threads are made in all.

What each resume costs beyond the process:
- On the desktop, on an idle host: 7 to 10 us, which is 3.3 to 4.6 ms a second, under half a percent of a core.
- In Chrome on a desktop (a busy host): 22 to 28 us, which is 10 to 12 ms a second, about 0.4 ms of each of the
  game's frames. A handoff there is a futex wait and wake between two of the page's workers.
