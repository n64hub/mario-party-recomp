// A 3D model's texture clamped where its triangles' part of it ends, and a part's textures evened where they meet
// (us.toml's hooks in func_8002E488 say where).
//
// The model builder (src/2C0C0.c's func_8002E488) writes a batch of a part's triangles after the texture they are
// drawn with, its tile clamped at the texture's edges. Some models' textures are sheets of pictures stacked one under
// another, a triangle's coordinates on one of them: Crazy Cutter's cones, barricades and poles are quads on the top
// 32 rows of a 32x96 sheet, their coordinates t 0 to 32 (the middle cone's to 31.88). A picture's last rows are clear,
// but at a quad's bottom edge the bilinear filter mixes in row 32, the top row of the picture under it, and drawn above
// the console's resolution that row shows as a thin dark line under the object, a little under the bottom of what is
// drawn. Players don't see it on the console.
//
// So where the batches drawn with a texture reach no further than an edge between two texels inside it (or an eighth
// of a texel short of one), its tile is clamped at that edge, and the filter mixes in nothing past it. A batch whose
// coordinates end further between two texels' edges (most of a character's parts on its texture) keeps the texture's
// own tile, and so does one that runs to the texture's edge. Only a tile's right and bottom are clamped: its top and
// left are where the texture's memory starts.
//
// A picture too big for the texture memory is made of several textures side by side, a part's batches each drawn with
// one, their quads meeting at its edges: Board Selection's picture of each board, 128x64, is four 64x32 CI8 textures,
// each with a palette of its own, quarters of one part meeting across its middle and down it. Each clamped at its own
// edges, the bilinear filter blends nothing across a join, and drawn above the console's resolution, where the filter
// smooths everything else, the join shows as a hard line across the picture.
//
// So where a batch's vertex lies where one of another texture of the part does, each at its texture's opposite edges
// (one's t at its last row's far edge, the other's at 0, their s the same, or the same across), the two textures meet
// there, and along the span such vertices share, the step between the two textures' colours at the join is spread over
// the SPREAD texels either side of it, 2 SPREAD + 1 even steps where there was one, each texel's new colour from its
// own palette (a CI8 texture with an RGBA16 one). Two quarters' palettes may have nothing between their colours (one
// sky, the other ground), so a colour a palette lacks takes a slot no texture of the part drawn with it uses, or, with
// none free, one of the palette's two nearest colours (never one a join made), every texel of it moved to the other, if
// they are nearer each other than the colour wanted is to any, or no more than a step apart in each channel: texels
// here and there a step off show far less than a line across flat sky. The filter's hard step across the join is gone.
//
// The quarters are lit, and not alike: the builder sets the directional light's colour (G_MW_LIGHTCOL) B2B2B2 for
// each picture's first quarter and FFFFFF for its bottom right one, which draws brighter than the quarter above it,
// a line across the picture's right half that the console draws too. So a batch of a later picture of the part, where
// it meets one before it, has its light colours set as the part's first picture's were, and every quarter is lit alike.
//
// A minigame's instructions screen has a soft cloud behind its title (form file 0xB0004), bobbing up and down: a glow
// and five triangles of a 32x32 RGBA32 puff, each with a flat bottom edge across it. The puff fades to clear at its
// bottom (row 29's alpha 62, row 30's 35, row 31's 0), but the triangles' bottoms end on it at t 30.03 to 31.16, where
// it is still 14 to 24% opaque, so each is cut off straight where it can still be seen. Above the console's resolution
// the cuts show as a band with a hard top under the title, moving with the cloud. And at t 30.03 (a sixteenth past row
// 30's edge) the clamp above took the last triangle's tile to rows 0 to 29, holding row 29's alpha down to its bottom
// edge. So while that file is made (us.toml's hooks around the instructions setup's LoadFormFile call for it), a
// batch's vertex whose t lies on its tile's last two rows, short of the last row's middle, is moved to that middle,
// where the puff is clear: the cloud fades out before its edges, which stay where they were, its puff a texel and a
// half taller at most.

#include "recomp.h"
#include "librecomp/helpers.hpp"
#include <cstdio>
#include <algorithm>

namespace {
    // The builder's globals (src/2C0C0.c).
    constexpr gpr DL_HEAD = (int32_t)0x800F37DC;        // D_800F37DC, the Gfx* it writes at
    constexpr gpr VERTICES = (int32_t)0x800F50BC;       // D_800F50BC, the part's Vtx
    constexpr gpr BATCH_FIRST = (int32_t)0x800EE32C;    // D_800EE32C, s16: the batch's first vertex
    constexpr gpr BATCH_END = (int32_t)0x800EE992;      // D_800EE992, s16: one past its last
    constexpr gpr TEXTURE = (int32_t)0x800F328C;        // D_800F328C, the texture the tile holds, NULL at a part's start

    constexpr uint32_t G_SETTILE = 0xF5;
    constexpr uint32_t G_SETTILESIZE = 0xF2;
    constexpr int G_TX_RENDERTILE = 0;
    constexpr int G_TX_CLAMP = 2;

    // How far short of a texel's edge, and past it, a coordinate (10.5) may end and still be on it: an eighth of a
    // texel short (Crazy Cutter's middle cone's quad ends at t 31.88), a sixteenth past.
    constexpr int32_t SHORT_OF_EDGE = 4;
    constexpr int32_t PAST_EDGE = 2;

    // Whether the instructions screen's title cloud (form file 0xB0004) is being made.
    bool title_cloud = false;

    gpr batch_start = 0;        // the DL head when the batch started

    // The tile the part's stream draws with now: its SetTileSize command, 0 if none to clamp.
    struct Tile {
        gpr size = 0;
        gpr texture = 0;
        int32_t last_s = 0, last_t = 0;     // its own last texel, the texture's
        bool clamp_s = false, clamp_t = false;
        int32_t reach_s = 0, reach_t = 0;   // the furthest its batches' coordinates go (10.5)
        bool any = false;
    } tile;

    // The last texel to clamp an axis at: the edge the batches reach, or the tile's own last texel.
    int32_t clamped_last(bool clamp, int32_t reach, int32_t last) {
        if (!clamp) {
            return last;
        }
        int32_t edge = (reach + 16) >> 5;
        if (edge < 1 || edge - 1 >= last || reach - edge * 32 > PAST_EDGE || edge * 32 - reach > SHORT_OF_EDGE) {
            return last;
        }
        return edge - 1;
    }

    constexpr uint32_t G_SETTIMG = 0xFD;
    constexpr uint32_t G_LOADTLUT = 0xF0;
    constexpr uint32_t G_SETOTHERMODE_H_TEXTLUT = 0xE3001001;   // its TLUT's format (G_MDSFT_TEXTLUT, 2 bits)
    constexpr uint32_t G_TT_RGBA16 = 0x8000;
    constexpr uint32_t G_IM_FMT_CI = 2;
    constexpr uint32_t G_IM_SIZ_8b = 1;

    // The CI8 texture the part's batches draw with now (0 if it is another kind) and its palette, and where they were
    // loaded: a batch that draws with the texture before it loads nothing.
    struct Picture {
        gpr texture = 0, palette = 0;
        int32_t width = 0, height = 0;      // texels, a row width bytes
        int32_t colours = 0;                // the palette's colours loaded, its first
    } picture;
    gpr picture_palette = 0;                // the last palette loaded
    int32_t picture_colours = 0;            // and how many colours of it
    bool picture_rgba16 = false;            // the TLUT's format the part draws with

    // The part's batches drawn with a CI8 texture so far: each texture and its batch's vertices.
    struct Piece {
        Picture picture;
        int32_t first = 0, end = 0;
    };
    constexpr int MAX_PIECES = 16;
    Piece pieces[MAX_PIECES];
    int piece_count = 0;
    gpr pieces_vertices = 0;

    // The light colours the part's first picture was lit with, by the moveword's offset (a word each).
    constexpr uint32_t G_MOVEWORD_LIGHTCOL = 0xDB0A;    // G_MOVEWORD, G_MW_LIGHTCOL (F3DEX2)
    constexpr int LIGHT_WORDS = 32;
    bool light_known[LIGHT_WORDS] = {};
    uint32_t light_colour[LIGHT_WORDS] = {};

    // Whether a texture or palette of n bytes lies in RDRAM (a KSEG0 address, not a segment one).
    bool in_rdram(gpr address, int32_t n) {
        uint32_t a = (uint32_t)address;
        return a >= 0x80000000u && a + (uint32_t)n <= 0x80800000u;
    }

    void rgba16_split(uint32_t c, int32_t& r, int32_t& g, int32_t& b) {
        r = (c >> 11) & 31;
        g = (c >> 6) & 31;
        b = (c >> 1) & 31;
    }

    int32_t distance(int32_t r0, int32_t g0, int32_t b0, int32_t r1, int32_t g1, int32_t b1) {
        return (r0 - r1) * (r0 - r1) + (g0 - g1) * (g0 - g1) + (b0 - b1) * (b0 - b1);
    }

    // How many rows (columns, across a join down) either side of a join the step between two textures is spread over.
    constexpr int32_t SPREAD = 2;

    // A palette's slots that no texel of the part's textures drawn with it uses: a join takes one for a colour the
    // palette lacks (two quarters' palettes may share nothing between their colours, one sky, the other ground).
    struct Slots {
        gpr palette = 0;
        bool used[256] = {};
        int32_t top = 255;      // its last slot: those past it are the next palette's
        bool made[256] = {};    // given its colour by a join, so never merged away
    };
    constexpr int MAX_SLOTS = 16;
    Slots slots[MAX_SLOTS];
    int slots_count = 0;

    void mark_used(uint8_t* rdram, Slots& s, const Picture& p) {
        for (int32_t i = 0; i < p.width * p.height; i++) {
            s.used[MEM_BU(i, p.texture)] = true;
        }
    }

    // Every texel of index from in the part's textures drawn with p's palette (p's and also's among them) made to.
    void remap(uint8_t* rdram, const Picture& p, const Picture& also, int32_t from, int32_t to) {
        auto move = [&](const Picture& q) {
            if (q.palette != p.palette) {
                return;
            }
            for (int32_t i = 0; i < q.width * q.height; i++) {
                if (MEM_BU(i, q.texture) == (uint32_t)from) {
                    MEM_B(i, q.texture) = (int8_t)to;
                }
            }
        };
        for (int i = 0; i < piece_count; i++) {
            move(pieces[i].picture);
        }
        move(p);
        move(also);
    }

    // The palette's slots, the part's textures drawn with it read the first time a join asks; nullptr past MAX_SLOTS.
    Slots* slots_for(uint8_t* rdram, const Picture& p, const Picture& also) {
        for (int i = 0; i < slots_count; i++) {
            if (slots[i].palette == p.palette) {
                return &slots[i];
            }
        }
        if (slots_count == MAX_SLOTS) {
            return nullptr;
        }
        Slots& s = slots[slots_count++];
        s = Slots();
        s.palette = p.palette;
        for (int i = 0; i < piece_count; i++) {
            if (pieces[i].picture.palette == p.palette) {
                mark_used(rdram, s, pieces[i].picture);
            }
        }
        mark_used(rdram, s, p);
        if (also.palette == p.palette) {
            mark_used(rdram, s, also);
        }
        // A palette loaded as 256 colours may hold fewer, the next one's straight after it (Board Selection's are
        // 160 to 208): only a slot under the last one its textures use is its own.
        int32_t top = 255;
        while (top > 0 && !s.used[top]) {
            top--;
        }
        for (int32_t i = top + 1; i < 256; i++) {
            s.used[i] = true;
        }
        s.top = top;
        return &s;
    }

    // The index in p's palette for a colour (5 bits a channel) with alpha bit alpha: the nearest colour the texture
    // draws with, or, if that is more than a step off in a channel, a slot given the colour itself (below).
    int32_t colour_index(uint8_t* rdram, const Picture& p, const Picture& also, int32_t r, int32_t g, int32_t b,
                         uint32_t alpha) {
        Slots* s = slots_for(rdram, p, also);
        int32_t best = -1, best_d = 0;
        for (int32_t i = 0; i < p.colours; i++) {
            uint32_t c = MEM_HU(i * 2, p.palette);
            if ((s != nullptr && !s->used[i]) || (c & 1) != alpha) {
                continue;
            }
            int32_t ir, ig, ib;
            rgba16_split(c, ir, ig, ib);
            int32_t d = distance(ir, ig, ib, r, g, b);
            if (best < 0 || d < best_d) {
                best = i;
                best_d = d;
            }
        }
        if (s == nullptr || (best >= 0 && best_d <= 1)) {
            return best;
        }
        int32_t slot = -1;
        for (int32_t i = 0; i <= s->top && slot < 0; i++) {
            if (!s->used[i]) {
                slot = i;
            }
        }
        if (slot < 0) {
            // None free: the palette's two nearest colours made one, if they are nearer each other than the colour
            // wanted is to the nearest it has, or no more than a step apart in each channel (scattered texels a step
            // off show far less than a line a step or three off across flat sky), every
            // texel of the part's textures drawn with it moved to the first. Colours a join made are kept.
            int32_t keep = -1, drop = -1, pair_d = 0;
            for (int32_t i = 0; i <= s->top; i++) {
                uint32_t c = MEM_HU(i * 2, p.palette);
                int32_t ir, ig, ib;
                rgba16_split(c, ir, ig, ib);
                for (int32_t k = i + 1; k <= s->top; k++) {
                    uint32_t d = MEM_HU(k * 2, p.palette);
                    if (s->made[k] || (c & 1) != (d & 1)) {
                        continue;
                    }
                    int32_t kr, kg, kb;
                    rgba16_split(d, kr, kg, kb);
                    int32_t dd = distance(ir, ig, ib, kr, kg, kb);
                    if (keep < 0 || dd < pair_d) {
                        keep = i;
                        drop = k;
                        pair_d = dd;
                    }
                }
            }
            if (keep < 0 || (best >= 0 && pair_d >= best_d && pair_d > 3)) {
                return best;
            }
            remap(rdram, p, also, drop, keep);
            if (best == drop) {
                best = keep;
            }
            slot = drop;
        }
        MEM_H(slot * 2, p.palette) = (int16_t)(uint16_t)((r << 11) | (g << 6) | (b << 1) | alpha);
        s->used[slot] = true;
        s->made[slot] = true;
        return slot;
    }

    int32_t channel(float v) {
        int32_t c = (int32_t)(v + (v < 0 ? -0.5f : 0.5f));
        return std::max(0, std::min(31, c));
    }

    // Where piece p's vertices lie on q's at their opposite edges, the texels either side of that join evened, along
    // the span the shared vertices cover. Down (across false): p's last row on q's first, or q's last on p's first;
    // across: the same for columns. Whether the two meet.
    bool join(uint8_t* rdram, gpr vertices, const Piece& p, const Piece& q, bool across) {
        // Which of the two is first (above, or to the left), and the span along the join (10.5), from either's
        // shared vertices.
        const Piece* first = nullptr;
        int32_t low = 0, high = 0, shared = 0;
        for (int32_t i = p.first; i < p.end; i++) {
            gpr vi = vertices + i * 16;
            for (int32_t k = q.first; k < q.end; k++) {
                gpr vk = vertices + k * 16;
                if (MEM_H(0, vi) != MEM_H(0, vk) || MEM_H(2, vi) != MEM_H(2, vk) || MEM_H(4, vi) != MEM_H(4, vk)) {
                    continue;
                }
                // The coordinate along the join (s down, t across) and the one through it.
                int32_t along_p = MEM_H(across ? 10 : 8, vi), along_q = MEM_H(across ? 10 : 8, vk);
                int32_t through_p = MEM_H(across ? 8 : 10, vi), through_q = MEM_H(across ? 8 : 10, vk);
                int32_t size_p = (across ? p.picture.width : p.picture.height) * 32;
                int32_t size_q = (across ? q.picture.width : q.picture.height) * 32;
                const Piece* here;
                if (through_p == size_p && through_q == 0) {
                    here = &p;
                } else if (through_q == size_q && through_p == 0) {
                    here = &q;
                } else {
                    continue;
                }
                if (along_p != along_q || (first != nullptr && first != here)) {
                    continue;
                }
                first = here;
                low = (shared == 0) ? along_p : std::min(low, along_p);
                high = (shared == 0) ? along_p : std::max(high, along_p);
                shared++;
            }
        }
        if (first == nullptr || high <= low) {
            return false;
        }
        const Picture& a = first->picture;
        const Picture& b = (first == &p) ? q.picture : p.picture;
        int32_t length = std::min(across ? a.height : a.width, across ? b.height : b.width);
        int32_t from = std::max(low >> 5, 0), to = std::min((high + 31) >> 5, length);
        int32_t depth = std::min({ SPREAD, (across ? a.width : a.height) / 2, (across ? b.width : b.height) / 2 });
        for (int32_t x = from; x < to; x++) {
            // Texel j in from the join on either side (0 at it), a byte offset in its texture.
            auto at_a = [&](int32_t j) { return across ? x * a.width + a.width - 1 - j : (a.height - 1 - j) * a.width + x; };
            auto at_b = [&](int32_t j) { return across ? x * b.width + j : j * b.width + x; };
            uint32_t ia[SPREAD], ib[SPREAD];
            for (int32_t j = 0; j < depth; j++) {
                ia[j] = MEM_BU(at_a(j), a.texture);
                ib[j] = MEM_BU(at_b(j), b.texture);
            }
            int32_t ar, ag, ab, br, bg, bb;
            rgba16_split(MEM_HU(ia[0] * 2, a.palette), ar, ag, ab);
            rgba16_split(MEM_HU(ib[0] * 2, b.palette), br, bg, bb);
            // The step across the join spread over the texels either side: the one at it moves depth / (2 depth + 1)
            // of the step toward the other side, the next one less, so the colours climb in 2 depth + 1 even steps.
            float dr = (float)(br - ar), dg = (float)(bg - ag), db = (float)(bb - ab);
            for (int32_t j = 0; j < depth; j++) {
                float f = (float)(depth - j) / (float)(2 * depth + 1);
                uint32_t ca = MEM_HU(ia[j] * 2, a.palette), cb = MEM_HU(ib[j] * 2, b.palette);
                int32_t r, g, bl;
                rgba16_split(ca, r, g, bl);
                int32_t na = colour_index(rdram, a, b, channel(r + dr * f), channel(g + dg * f), channel(bl + db * f), ca & 1);
                rgba16_split(cb, r, g, bl);
                int32_t nb = colour_index(rdram, b, a, channel(r - dr * f), channel(g - dg * f), channel(bl - db * f), cb & 1);
                if (na >= 0) {
                    MEM_B(at_a(j), a.texture) = (int8_t)na;
                }
                if (nb >= 0) {
                    MEM_B(at_b(j), b.texture) = (int8_t)nb;
                }
            }
        }
        return true;
    }

    // A batch's light colours (G_MW_LIGHTCOL movewords, each word by its offset): the part's first picture's recorded,
    // or a later picture's that meets one before it made those, so every quarter of the picture is lit alike.
    void light_like_first(uint8_t* rdram, gpr start, gpr end, bool record) {
        for (gpr cmd = start; cmd < end; cmd += 8) {
            uint32_t w0 = (uint32_t)MEM_W(0, cmd);
            int32_t word = (int32_t)(w0 & 0xFFFF) / 4;
            if ((w0 >> 16) != G_MOVEWORD_LIGHTCOL || (w0 & 3) != 0 || word >= LIGHT_WORDS) {
                continue;
            }
            if (record) {
                light_known[word] = true;
                light_colour[word] = (uint32_t)MEM_W(4, cmd);
            } else if (light_known[word]) {
                MEM_W(4, cmd) = (int32_t)light_colour[word];
            }
        }
    }

    // A batch's commands read for its CI8 texture: the palette it loads, its TLUT's format and its render tile.
    void read_picture(uint8_t* rdram, gpr start, gpr end, gpr settile, gpr settilesize) {
        gpr image = 0;
        for (gpr cmd = start; cmd < end; cmd += 8) {
            uint32_t w0 = (uint32_t)MEM_W(0, cmd), w1 = (uint32_t)MEM_W(4, cmd);
            if ((w0 >> 24) == G_SETTIMG) {
                image = (gpr)(int32_t)w1;
            } else if ((w0 >> 24) == G_LOADTLUT) {
                picture_palette = image;
                picture_colours = std::min((int32_t)((w1 >> 14) & 0x3FF) + 1, 256);
            } else if (w0 == G_SETOTHERMODE_H_TEXTLUT) {
                picture_rgba16 = (w1 == G_TT_RGBA16);
            }
        }
        if (settilesize == 0) {
            return;
        }
        picture = Picture();
        if (settile == 0 || settile > settilesize) {
            return;
        }
        uint32_t t0 = (uint32_t)MEM_W(0, settile), t1 = (uint32_t)MEM_W(4, settile);
        uint32_t z0 = (uint32_t)MEM_W(0, settilesize), z1 = (uint32_t)MEM_W(4, settilesize);
        int32_t width = (int32_t)(((z1 >> 12) & 0xFFF) >> 2) + 1, height = (int32_t)((z1 & 0xFFF) >> 2) + 1;
        int32_t line = (int32_t)((t0 >> 9) & 0x1FF) * 8;
        bool ci8 = ((t0 >> 21) & 7) == G_IM_FMT_CI && ((t0 >> 19) & 3) == G_IM_SIZ_8b && ((t1 >> 20) & 0xF) == 0;
        if (!ci8 || !picture_rgba16 || picture_palette == 0 || (z0 & 0x00FFFFFF) != 0 || line != width) {
            return;
        }
        picture.texture = (gpr)(int32_t)MEM_W(0, TEXTURE);
        picture.palette = picture_palette;
        picture.colours = picture_colours;
        picture.width = width;
        picture.height = height;
    }
}

// The instructions setup about to make its title cloud (form file 0xB0004), and back from it (above).
extern "C" void mp_model_title_cloud(uint8_t* rdram, int32_t making) {
    title_cloud = making != 0;
}

// At func_8002E488's start: where the batch's commands start, and whether the tile tracked is still the stream's.
extern "C" void mp_model_batch_start(uint8_t* rdram) {
    batch_start = (gpr)(int32_t)MEM_W(0, DL_HEAD);
    if (tile.size != 0 && (gpr)(int32_t)MEM_W(0, TEXTURE) != tile.texture) {
        tile.size = 0;
    }
}

// At its end, the batch written: a texture it loaded is tracked, and the tile it draws with clamped to the edges
// its batches reach.
extern "C" void mp_model_batch_end(uint8_t* rdram) {
    gpr end = (gpr)(int32_t)MEM_W(0, DL_HEAD);
    gpr settile = 0, settilesize = 0;
    for (gpr cmd = batch_start; cmd < end; cmd += 8) {
        uint32_t w0 = (uint32_t)MEM_W(0, cmd), w1 = (uint32_t)MEM_W(4, cmd);
        if ((w0 >> 24) == G_SETTILE && ((w1 >> 24) & 7) == G_TX_RENDERTILE) {
            settile = cmd;
        } else if ((w0 >> 24) == G_SETTILESIZE && ((w1 >> 24) & 7) == G_TX_RENDERTILE) {
            settilesize = cmd;
        }
    }

    if (settilesize != 0) {
        tile = Tile();
        uint32_t w0 = (uint32_t)MEM_W(0, settilesize), w1 = (uint32_t)MEM_W(4, settilesize);
        if (settile != 0 && settile < settilesize && (w0 & 0x00FFFFFF) == 0) {
            uint32_t t1 = (uint32_t)MEM_W(4, settile);
            tile.size = settilesize;
            tile.texture = (gpr)(int32_t)MEM_W(0, TEXTURE);
            tile.last_s = ((w1 >> 12) & 0xFFF) >> 2;
            tile.last_t = (w1 & 0xFFF) >> 2;
            // cms, shifts and cmt, shiftt: a shift scales the coordinates, which then aren't the texture's texels.
            tile.clamp_s = (((t1 >> 8) & 3) & G_TX_CLAMP) && (t1 & 0xF) == 0;
            tile.clamp_t = (((t1 >> 18) & 3) & G_TX_CLAMP) && ((t1 >> 10) & 0xF) == 0;
        }
    }

    // The part's CI8 pictures: this batch's checked against those before it in the part, for joins.
    {
        gpr vs = (gpr)(int32_t)MEM_W(0, VERTICES);
        int32_t first = MEM_H(0, BATCH_FIRST), last = MEM_H(0, BATCH_END);
        if (first == 0 || vs != pieces_vertices) {
            piece_count = 0;
            slots_count = 0;
            pieces_vertices = vs;
            std::fill(std::begin(light_known), std::end(light_known), false);
            if (settilesize == 0) {
                picture = Picture();
            }
        }
        read_picture(rdram, batch_start, end, settile, settilesize);
        if (picture.texture != 0 && in_rdram(picture.texture, picture.width * picture.height) &&
            in_rdram(picture.palette, 512) && last > first && piece_count < MAX_PIECES) {
            Piece piece{ picture, first, last };
            bool meets = false;
            for (int i = 0; i < piece_count; i++) {
                if (pieces[i].picture.texture != piece.picture.texture) {
                    meets |= join(rdram, vs, piece, pieces[i], false);
                    meets |= join(rdram, vs, piece, pieces[i], true);
                }
            }
            if (piece_count == 0 || meets) {
                light_like_first(rdram, batch_start, end, piece_count == 0);
            }
            for (int i = 0; i < slots_count; i++) {
                if (slots[i].palette == piece.picture.palette) {
                    mark_used(rdram, slots[i], piece.picture);
                }
            }
            pieces[piece_count++] = piece;
        }
    }

    if (tile.size == 0) {
        return;
    }

    gpr vertices = (gpr)(int32_t)MEM_W(0, VERTICES);
    int32_t first = MEM_H(0, BATCH_FIRST), last = MEM_H(0, BATCH_END);
    for (int32_t i = first; i < last; i++) {
        int32_t s = MEM_H(8, vertices + i * 16), t = MEM_H(10, vertices + i * 16);
        // The title cloud's bottom edges moved to the middle of its puff's last row, where it is clear (above).
        int32_t clear = tile.last_t * 32 + 16;
        if (title_cloud && tile.clamp_t && t >= (tile.last_t - 1) * 32 && t < clear) {
            t = clear;
            MEM_H(10, vertices + i * 16) = (int16_t)t;
        }
        if (!tile.any || s > tile.reach_s) {
            tile.reach_s = s;
        }
        if (!tile.any || t > tile.reach_t) {
            tile.reach_t = t;
        }
        tile.any = true;
    }
    if (!tile.any) {
        return;
    }

    int32_t last_s = clamped_last(tile.clamp_s, tile.reach_s, tile.last_s);
    int32_t last_t = clamped_last(tile.clamp_t, tile.reach_t, tile.last_t);
    uint32_t w1 = (uint32_t)MEM_W(4, tile.size);
    MEM_W(4, tile.size) = (int32_t)((w1 & 0xFF000000) | ((uint32_t)(last_s << 2) << 12) | (uint32_t)(last_t << 2));
}
