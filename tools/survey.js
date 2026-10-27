// Evaluated in the page by N64Recomp-browser's tools/record_shaders.mjs (tools/shader_plan.json) and play_page.mjs
// (tools/play_plan.json): finds patches/survey.c's struct in RDRAM by its magic words and offers overlay(), the overlay
// playing (the one omOvlGotoEx last went to: 0x69 Mushroom Village, 0x3E the board Toad teaches the rules on, 0x36-0x3D
// the other boards, 0x00-0x34 the minigames; the decomp's src/overlays and its splat.yaml number them all);
// untilOverlay(overlay, ms), which resolves true once that overlay plays, false after ms; ask(from, overlay, event,
// stat, every), which asks the game to go to overlay (with omOvlGotoEx's event and stat, -1 for the game's own) the
// next time it leaves for `from` (-1: for anywhere), or every time with `every` (a board: patches/survey.c); warps(),
// how many times it has; go(from, overlay, event, stat, ms), ask() once and resolve true once the game has gone there,
// false after ms; frames(), the scheduler's frame count (one a frame the game makes: its own rate); mark() and rate(),
// how many frames a second the game made from the one to the other; human(player), that player (0-3) played from the
// keyboard and pad 1 rather than by the computer (the CPU bit of the player's flags in GwPlayer, 0x800F32B0, 0x30 bytes
// a player), which a board's start sets back from the save; teams(...groups), each player's group (GwPlayer's first
// byte: a minigame's sides, set as a board starts one); and minigame(id, ...groups), which opens a minigame as a board
// does, by its instructions screen (0x6F), which A (Space) pages through and Start (Enter) leaves for the minigame: it
// asks for 0x6F in place of the hub's trip to the rules' board, and while the game is on the overlay before it (0x35)
// sets the minigame's ID (GwSystem's at 0x800ED5DE, which ovl_6F's table turns into an overlay and a kind) and the
// groups the kind needs: 1P and 1 vs 3 the player alone 0 and the others 1, 2 vs 2 0 0 1 1, 4P 0 1 2 3. A minigame the
// survey sends the game to directly, past that screen, can trap (Bobsled Run's did, with its groups set or not).
(() => {
  const magic = (a) => n64r(a) === 0x4d505254 && n64r(a + 4) === 0x53525659;
  if (!window.__survey) {
    // The patches' memory first (patches/patches.ld), then the rest of RDRAM.
    for (let a = 0x80801000; a < 0x81000000 && !window.__survey; a += 4) if (magic(a)) window.__survey = a;
    for (let a = 0x80000000; a < 0x80801000 && !window.__survey; a += 4) if (magic(a)) window.__survey = a;
  }
  const s = window.__survey;
  const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
  window.overlay = () => n64r(s + 32) | 0;
  window.frames = () => n64r(s + 36) >>> 0;
  window.untilOverlay = async (overlay, ms) => {
    const end = Date.now() + ms;
    while (window.overlay() !== overlay && Date.now() < end) await sleep(100);
    return window.overlay() === overlay;
  };
  window.warps = () => n64r(s + 28) >>> 0;
  window.ask = (from, overlay, event, stat, every) => {
    n64w(s + 12, from >>> 0);
    n64w(s + 16, overlay);
    n64w(s + 20, event);
    n64w(s + 24, stat);
    n64w(s + 8, every ? 2 : 1);
    const when = every ? ' every time' : '';
    return `asked for ${overlay.toString(16)} in place of ${from.toString(16)}${when}, ${window.warps()} taken before`;
  };
  window.go = async (from, overlay, event, stat, ms) => {
    const warps = window.warps();
    window.ask(from, overlay, event, stat);
    const end = Date.now() + ms;
    while ((window.warps() === warps || window.overlay() !== overlay) && Date.now() < end) await sleep(100);
    return window.warps() !== warps && window.overlay() === overlay;
  };
  window.mark = () => { window.__mark = [window.frames(), performance.now()]; return 'frames ' + window.__mark[0]; };
  window.rate = () => {
    const [frames, ms] = [window.frames() - window.__mark[0], performance.now() - window.__mark[1]];
    return `${frames} game frames in ${(ms / 1000).toFixed(1)} s: ${(frames * 1000 / ms).toFixed(1)} a second`;
  };
  window.teams = (...groups) => {
    groups.forEach((group, player) => {
      const a = 0x800f32b0 + player * 0x30;
      n64w(a, ((n64r(a) & 0x00ffffff) | group << 24) >>> 0);
    });
    return 'groups ' + [0, 1, 2, 3].map((player) => n64r(0x800f32b0 + player * 0x30) >>> 24).join(' ');
  };
  window.minigame = (id, ...groups) => {
    clearInterval(window.__minigame);
    window.__minigame = setInterval(() => {
      if (window.overlay() !== 0x35) return;
      window.teams(...groups);
      n64w(0x800ed5dc, ((n64r(0x800ed5dc) & 0xffff0000) | id) >>> 0);
    }, 5);
    return window.ask(0x3e, 0x6f, 0, 0x94) + `, minigame ${id.toString(16)} for groups ${groups.join(' ')}`;
  };
  window.human = (player) => {
    const a = 0x800f32b4 + player * 0x30;
    n64w(a, (n64r(a) & ~1) >>> 0);
    return `player ${player} ${n64r(a) & 1 ? 'computer' : 'human'}`;
  };
  return s ? 'survey at ' + s.toString(16) : 'no survey struct: a page built without patches/survey.c';
})()
