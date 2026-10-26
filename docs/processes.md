# The process scheduler

Hudson's engine, which Mario Party, Mario Party 2 and 3 and Bomberman 64 are built on, runs most of a game as
processes: small functions, each with a stack of its own in the engine's heap, which a scheduler runs in turn once a
frame. A process runs until it sleeps (`HuPrcSleep`), waits for its children (`HuPrcChildWatch`) or ends
(`HuPrcEnd`), and the scheduler (`HuPrcCall`, called from the game's main thread) then goes on to the next. On a
console they switch with the game's own setjmp and longjmp: `HuPrcCall` longjmps into a process, onto its stack, and
the process setjmps where it stops and longjmps back.

Recompiled code can't do that. A function of the game is a C function, so a process's state is on the native stack as
well as in RDRAM, and a longjmp between two of them would leave the native stack behind.

## How it runs here

Each live process runs on a native thread of its own, and exactly one of them runs at a time, as the console runs one
process at a time:

- `HuPrcCall` is the game's loop, line for line, with `recomp_prc_resume` where it longjmped into a process. The first
  resume of a process starts a thread at its function with its RDRAM stack; a later one wakes its thread. Either way
  the scheduler waits until the process hands the baton back, then goes on with what the longjmp back would have
  returned: 1 when the process slept, 2 when it ended.
- `HuPrcSleep` and `HuPrcChildWatch` hand the baton back (`recomp_prc_yield`) and wait to be resumed, where they
  setjmped and longjmped.
- `HuPrcTerminate` ends the process (`recomp_prc_exit`), which throws out of the recompiled code to its thread's loop,
  as N64ModernRuntime ends a game thread. The scheduler frees the process's heap, on its own stack, as on a console.
- `HuPrcInit` ends every process's thread (`recomp_prc_reset`), since none outlives the list it was on.
- A process killed while it waits (`HuPrcKill`) is resumed in `HuPrcEnd` on a console, and here too: a process that
  wakes checks for it, and one that never ran starts there.

The thread a process runs on takes the identity of the game thread that resumed it (N64ModernRuntime's
`thread_self` and the FPU's rounding mode), so what a process does, a blocking `osRecvMesg` included, is that game
thread's doing, as on a console. A process's thread is kept when the process ends and handed the next new one, so a
board's processes cost no thread creation after the first time.

It is the same code on the desktop and on the web, where the page builds with threads and `-fwasm-exceptions` anyway.
Bomberman 64's and Mario Party 3's recompilations make each process a coroutine instead (minicoro), which in a browser
needs Asyncify over the whole recompiled game: a bigger, slower page.

The code is `patches/process.c` (the five patches) and `src/game/process_threads.cpp` (the natives, with a header
that says how they hand the baton over).

## What a second game reuses

A game on the same engine takes both files:

- `src/game/process_threads.cpp` as it is. Nothing in it is Mario Party's: its natives take a process's key (its
  address), its function and its stack pointer, and the engine's `HuPrcEnd` for a process whose function returns.
- `patches/process.c`, with the game's own names: the five functions it patches are the engine's, and each patch is
  the game's function with its setjmp and longjmp swapped for a native. A game whose decomp has these functions under
  other names, or whose `Process` names its fields differently, changes only those names.
- The five natives' lines in `patches/syms.ld`.

Then build the game with `MP_PROCESS_STATS` (below) and look at what it prints: a board here keeps up to the number in
How much it costs alive.

## How much it costs

Built with `-DMP_PROCESS_STATS=ON`, the program prints every 5 seconds how many processes are alive, how many threads
it has made, how many resumes a second the scheduler makes and what each costs beyond the process's own run (the two
handoffs, from the scheduler's thread to the process's and back).
