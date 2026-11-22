// FRONTB's Flailer chunk with the port's main menu rows, Local Wireless
// and the debug menu, and the debug menu's screens in it (debugmenu.h).
//
// The chunk is written out again rather than patched in place, because
// what is added has to sit between what is there: flailer_object_index
// (flailer.c) binary-searches a project's payload addresses, so the objects
// must rise in file order, and NEW_MAIN_MENU's new objects belong inside
// its container, before the screens after it. Every chunk goes across in
// order and byte for byte, but for the main menu's object count and its
// bottom filler's height; the padding between chunks (type 0, size 0) is
// dropped, which neither reader counts (bchunk_next skips it). Every size
// in the chunk is a multiple of 4, so a malloc'd buffer keeps every
// payload 4-aligned.
//
// The main menu's rows are then laid out in the port's order (TASKS 68):
// only their Ys and visibility change, in the buffer the patch returned,
// which bui reads live. The panel holds five rows; past that they scroll.
//
// Free of <3ds.h>, like bui.c, for tools/debugmenu_check.py.

#include "debugmenu.h"

#include <stdlib.h>
#include <string.h>

#include "bchunk.h"
#include "bui.h"
#include "portlang.h"

// Retail's object fields, as offsets from the payload: flailer.h's
// FlailerObjectChunk, FlailerTextChunk and FlailerScreenChunk.
#define OBJ_NAME_HASH   8
#define OBJ_NAME        16      // char[24], the authoring name
#define OBJ_VISIBILITY  41
#define OBJ_X           48
#define OBJ_Y           52
#define OBJ_SCALE_X     56
#define OBJ_SCALE_Y     60
#define OBJ_TEX_HASH    80      // FlailerBitmapChunk TextureHash
#define TEXT_MAX_CHARS  84
#define TEXT_LANGUAGE   96
#define TEXT_STRING     100
#define SCREEN_NAME     8
#define SCREEN_NUM_OBJS 28

#define CHUNK_TEXT      0x00010212u

// NEW_MAIN_MENU's objects the new rows are made from (gameflow.h).
#define MAIN_MENU       0xae0f8aa1u
#define OPTIONS_MENU    0x95ad096bu   // bhash("OPTIONS")
#define LANGUAGE_MENU   0x632b068eu   // bhash("Language Options Pre FE")
// The flags collage on the Language screen, and the Turkish flag the
// port added to FRONTB.BUN's FRONTEND pack (64x40, 4bpp, FLAG_TR).
#define FLAG_ALL_TEX    0x9ca72111u   // bhash("FLAG_ALL")
#define FLAG_TR_TEX     0xd6339e9eu   // bhash("FLAG_TR")
#define AUDIO_OPTIONS_MENU 0xe4bfe7fdu // bhash("Audio Options"), the Sound menu
#define SCROLL_UP_ARROW    0xedf2e4eeu // BSETUP_OBJ_UP_ARROW
#define SCROLL_DOWN_ARROW  0x20d73d01u // BSETUP_OBJ_DOWN_ARROW
#define MENUITEM(n)     (0x52e039f2u + (uint32_t)(n))
#define MI_1_MASK       0x6e34e350u
#define MI_2_MASK       0x708a0bf1u
#define MI_3_MASK       0x72df3492u
#define MI_4_MASK       0x75345d33u
#define MI_5_MASK       0x778985d4u
#define MI_6_MASK       0x79deae75u   // bhash("MI_6_MASK"), Exit Game row
#define MI_7_MASK       0x7c33d716u   // bhash("MI_7_MASK")
#define MI_8_MASK       0x7e88ffb7u   // bhash("MI_8_MASK"), Wireless row
#define BOTTOM_MASK     0xaa5d7d3fu
// Be The Cop Setup's, for the debug screen.
#define LINE(n)         (0x69a2c756u + (uint32_t)(n))
#define TITLE           0x5e9f4683u   // CHALLENGE_TITLE
#define HELP            0x75980cf0u   // SCROLLING_TEXT
#define WHITEOUT_BAR    0x70a404ecu   // blurb wipe bar over SCROLLING_TEXT

// The mask texture (e2a5c626) is 16 high at ScaleY 1: MI_n_MASK's 2.813
// is the row's 45 pixels, and the two thin BOTTOM_MASKs' 0.625 the 10
// between rows. The third BOTTOM_MASK, at 7.063, fills the panel below
// Options: 113 pixels, room for two more rows and their separators. It
// becomes the separator under the third row, and the fillers the port
// adds under the fourth and fifth each the separator under its row or,
// under the last row shown, the panel down to where the third ended.
#define MASK_UNIT       16.0f
#define SEPARATOR_SCALE 0.625f

// The main menu's order (TASKS 68, the user's ask of 2026-10-15): every
// row above Options but the debug menu's, which is the only one under it.
// Hot Pursuit, World Racing, Local Wireless, Options, then Debug Menu once
// shown. MAIN_SLOTS rows fit the panel, a row's step (55 px) apart; with
// more, the window of rows shown follows the highlight.
// R36S: a sixth row, "Oyundan Cik" (Exit Game), always shown under
// Options; the debug menu, once revealed, becomes the seventh and the
// panel scrolls.
#define MAIN_SLOTS      5
#define MAIN_ROWS       6

// What the debug screens leave out of Be The Cop Setup: its eighth row
// (label, value, mask), the two blooms' spare texts, and the scroll clip.
// A screen with fewer rows loses their labels and values as well, and
// keeps their masks, which darken the panel under its last row.
static const uint32_t kLeftOut[] = {
    0x52e039fau, 0x69a2c75eu, 0x7e88ffb7u, 0xab671c9au, 0x1042eac3u,
    0xc67cfb4au,
};

// The rooms the debug screen's texts get: Be The Cop Setup's are its own
// strings' (10 and 6), and the help line's 100. 128 is all flailer_printf
// writes.
#define LABEL_ROOM 24
#define VALUE_ROOM 40
#define TITLE_ROOM 16
#define HELP_ROOM  128

typedef struct {
    uint8_t* p;
    size_t   len, cap;
    int      failed;
} Out;

// A row added under Options: its label, its mask and the filler under
// it, as offsets of their payloads in the buffer the patch returned.
typedef struct {
    size_t item, mask, fill;
} Row;

// The three rows, which debugmenu_reveal changes.
static uint8_t* sBuf;
static Row      sWireless, sDebug, sExit;
static int      sRevealed;
// The Options screen's eighth row (Wireless, R36S): its label's payload
// in sBuf, rewritten live when the toggle moves.
static Row sWirelessOpt;

// A main menu row in the port's order: its MENUITEM_n, and its label's and
// mask's payloads in sBuf.
typedef struct {
    uint32_t item;
    size_t   label, mask;
} MainRow;

static MainRow sRows[MAIN_ROWS];
static int     sNumRows;
// Each slot's label and mask Y: retail's three rows, then a row's step at
// a time below Options' (put_row's). The filler under each slot's mask
// that the layout sets, 0 under the first two (retail's thin separators,
// left as they are). Where retail's filler ended.
static float   sLabelY[MAIN_SLOTS], sMaskY[MAIN_SLOTS];
static size_t  sFill[MAIN_SLOTS];
static float   sBottom;
// The slots in use (MAIN_SLOTS but in tools/debugmenu_check.py), the
// first row shown, and the highlighted row, whose mask SelectionSet's
// IndicatorMode 1 keeps off; -1 for none.
static int     sSlots = MAIN_SLOTS, sTop, sCurrent = -1;

static uint32_t rd32(const uint8_t* p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16
           | (uint32_t)p[3] << 24;
}

static void wr32(uint8_t* p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static float rdf(const uint8_t* p)
{
    uint32_t v = rd32(p);
    float    f;
    memcpy(&f, &v, sizeof f);
    return f;
}

static void wrf(uint8_t* p, float f)
{
    uint32_t v;
    memcpy(&v, &f, sizeof v);
    wr32(p, v);
}

// `n` more bytes at the end, zeroed; their offset, or 0 once out of memory
// (no chunk starts at 0 but the container's header, written first).
static size_t grow(Out* o, size_t n)
{
    size_t at = o->len;
    if (o->failed) return 0;
    if (o->len + n > o->cap) {
        size_t   cap = o->cap ? o->cap : 1u << 16;
        uint8_t* p;
        while (cap < o->len + n) cap *= 2;
        p = (uint8_t*)realloc(o->p, cap);
        if (!p) {
            o->failed = 1;
            return 0;
        }
        o->p   = p;
        o->cap = cap;
    }
    memset(o->p + at, 0, n);
    o->len += n;
    return at;
}

// A chunk's header with its size still 0; close() writes it.
static size_t open_chunk(Out* o, uint32_t type)
{
    size_t at = grow(o, 8);
    if (!o->failed) wr32(o->p + at, type);
    return at;
}

static void close_chunk(Out* o, size_t hdr)
{
    if (!o->failed) wr32(o->p + hdr + 4, (uint32_t)(o->len - hdr - 8));
}

// A whole chunk; the payload's offset.
static size_t put_chunk(Out* o, uint32_t type, const uint8_t* payload,
                        uint32_t size)
{
    size_t hdr = open_chunk(o, type);
    size_t at  = grow(o, size);
    if (o->failed) return 0;
    if (size) memcpy(o->p + at, payload, size);
    close_chunk(o, hdr);
    return at;
}

// A text with `room` characters: its 100 bytes of fields, MaxChars set to
// the room, and its string, cut to fit if need be. flailertext_set_string
// writes up to MaxChars, so the chunk has exactly that many.
static size_t put_text(Out* o, const uint8_t* payload, uint32_t size,
                       uint32_t room)
{
    size_t   hdr = open_chunk(o, CHUNK_TEXT);
    size_t   at  = grow(o, TEXT_STRING + room);
    uint32_t old = size > TEXT_STRING ? size - TEXT_STRING : 0;
    if (o->failed) return 0;
    memcpy(o->p + at, payload, TEXT_STRING);
    memcpy(o->p + at + TEXT_STRING, payload + TEXT_STRING,
           old < room ? old : room);
    o->p[at + TEXT_STRING + room - 1] = 0;
    wr32(o->p + at + TEXT_MAX_CHARS, room);
    close_chunk(o, hdr);
    return at;
}

static void set_string(Out* o, size_t at, const char* s)
{
    uint32_t room = rd32(o->p + at + TEXT_MAX_CHARS);
    if (o->failed) return;
    memset(o->p + at + TEXT_STRING, 0, room);
    strncpy((char*)o->p + at + TEXT_STRING, s, room - 1);
    wr32(o->p + at + TEXT_LANGUAGE, 0);
}

static void set_name(Out* o, size_t at, uint32_t hash, const char* name)
{
    if (o->failed) return;
    wr32(o->p + at + OBJ_NAME_HASH, hash);
    memset(o->p + at + OBJ_NAME, 0, 24);
    strncpy((char*)o->p + at + OBJ_NAME, name, 23);
}

static uint32_t screen_hash(const uint8_t* payload, uint32_t size)
{
    BChunkIter it;
    bchunk_begin(&it, payload, 0, size);
    while (bchunk_next(&it))
        if (it.type == BUI_CHUNK_SCREENDEF && it.size >= 40)
            return rd32(it.payload + SCREEN_NAME);
    return BUI_HASH_NONE;
}

// The object of a screen container named `hash`, or NULL. `big` picks,
// among several, the one with the largest ScaleY.
static const uint8_t* find_object(const uint8_t* payload, uint32_t size,
                                  uint32_t hash, int big, uint32_t* outSize)
{
    const uint8_t* best = NULL;
    BChunkIter     it;
    bchunk_begin(&it, payload, 0, size);
    while (bchunk_next(&it)) {
        if (it.type == BUI_CHUNK_SCREENDEF || it.size < 64) continue;
        if (rd32(it.payload + OBJ_NAME_HASH) != hash) continue;
        if (best && !(big && rdf(it.payload + OBJ_SCALE_Y)
                                 > rdf(best + OBJ_SCALE_Y)))
            continue;
        best = it.payload;
        if (outSize) *outSize = it.size;
    }
    return best;
}

// Retail's three rows, the last two of which the new rows are copied from,
// and the filler under them.
typedef struct {
    const uint8_t *item1, *item2, *item3, *mask1, *mask2, *mask3, *fill;
    uint32_t       s3, sm3, sf;
} MenuParts;

// The row `row` below MENUITEM_3 (1 the first): MENUITEM_3 as far below
// itself, at MENUITEM_2's X (3's is two pixels right of 1's and 2's);
// MI_3_MASK as far below itself; and the rest of the filler under the
// mask, down to where the filler ends. Each stays in its group, so it
// slides in with the rest, and is named and shown as asked.
static int put_row(Out* o, const MenuParts* m, int row, Row* r,
                   uint32_t itemHash, const char* itemName,
                   uint32_t maskHash, const char* maskName,
                   uint32_t fillHash, const char* fillName, int shown)
{
    float rowY   = rdf(m->item3 + OBJ_Y)
                 + (rdf(m->item3 + OBJ_Y) - rdf(m->item2 + OBJ_Y)) * row;
    float maskY  = rdf(m->mask3 + OBJ_Y)
                 + (rdf(m->mask3 + OBJ_Y) - rdf(m->mask2 + OBJ_Y)) * row;
    float fillY  = maskY + rdf(m->mask3 + OBJ_SCALE_Y) * MASK_UNIT;
    float bottom = rdf(m->fill + OBJ_Y)
                 + rdf(m->fill + OBJ_SCALE_Y) * MASK_UNIT;

    r->item = put_text(o, m->item3, m->s3, rd32(m->item3 + TEXT_MAX_CHARS));
    set_name(o, r->item, itemHash, itemName);
    if (o->failed) return 0;
    wrf(o->p + r->item + OBJ_X, rdf(m->item2 + OBJ_X));
    wrf(o->p + r->item + OBJ_Y, rowY);
    o->p[r->item + OBJ_VISIBILITY] = (uint8_t)shown;

    r->mask = put_chunk(o, BUI_CHUNK_BITMAP, m->mask3, m->sm3);
    set_name(o, r->mask, maskHash, maskName);
    if (o->failed) return 0;
    wrf(o->p + r->mask + OBJ_Y, maskY);
    o->p[r->mask + OBJ_VISIBILITY] = (uint8_t)shown;

    r->fill = put_chunk(o, BUI_CHUNK_BITMAP, m->fill, m->sf);
    set_name(o, r->fill, fillHash, fillName);
    if (o->failed) return 0;
    wrf(o->p + r->fill + OBJ_Y, fillY);
    wrf(o->p + r->fill + OBJ_SCALE_Y, (bottom - fillY) / MASK_UNIT);
    o->p[r->fill + OBJ_VISIBILITY] = (uint8_t)shown;
    return 1;
}

// NEW_MAIN_MENU as it is, but for its bottom filler cut to a separator,
// then six objects after its own, and its NumObjects up by six: Local
// Wireless's row, shown, and the debug menu's under it, hidden. The
// fourth and fifth slots' rows and fillers, which the layout then moves:
// Local Wireless goes up into the third slot, Options down into the
// fourth. Retail's rows kept in sRows, and the slots.
static int put_main_menu(Out* o, const uint8_t* payload, uint32_t size)
{
    MenuParts  m;
    uint32_t   s2 = 0, sm2 = 0;
    size_t     hdr, def = 0, filler = 0, label[3] = {0}, mask[3] = {0};
    BChunkIter it;
    int        i;

    memset(&m, 0, sizeof m);
    m.item1 = find_object(payload, size, MENUITEM(1), 0, NULL);
    m.item2 = find_object(payload, size, MENUITEM(2), 0, &s2);
    m.item3 = find_object(payload, size, MENUITEM(3), 0, &m.s3);
    m.mask1 = find_object(payload, size, MI_1_MASK, 0, NULL);
    m.mask2 = find_object(payload, size, MI_2_MASK, 0, &sm2);
    m.mask3 = find_object(payload, size, MI_3_MASK, 0, &m.sm3);
    m.fill  = find_object(payload, size, BOTTOM_MASK, 1, &m.sf);
    if (!m.item1 || !m.item2 || !m.item3 || !m.mask1 || !m.mask2
        || !m.mask3 || !m.fill)
        return 0;

    hdr = open_chunk(o, BUI_CHUNK_SCREEN);
    bchunk_begin(&it, payload, 0, size);
    while (bchunk_next(&it)) {
        size_t at = put_chunk(o, it.type, it.payload, it.size);
        if (it.type == BUI_CHUNK_SCREENDEF) def = at;
        if (it.payload == m.fill) filler = at;
        if (it.payload == m.item1) label[0] = at;
        if (it.payload == m.item2) label[1] = at;
        if (it.payload == m.item3) label[2] = at;
        if (it.payload == m.mask1) mask[0] = at;
        if (it.payload == m.mask2) mask[1] = at;
        if (it.payload == m.mask3) mask[2] = at;
    }
    if (o->failed || !def || !filler) return 0;
    o->p[def + SCREEN_NUM_OBJS] += 9;     // 31 on disc, so no carry
    wrf(o->p + filler + OBJ_SCALE_Y, SEPARATOR_SCALE);

    // Local Wireless (TASKS 63): its LanguageID the port's (portlang.h),
    // the inline string the same in English.
    if (!put_row(o, &m, 1, &sWireless, BGF_ITEM_MENUITEM_4, "MENUITEM_4",
                 MI_4_MASK, "MI_4_MASK", DEBUGMENU_FILL_4, "PORT_FILL_4", 1))
        return 0;
    set_string(o, sWireless.item, "Yerel Kablosuz");
    wr32(o->p + sWireless.item + TEXT_LANGUAGE, PORTLANG_LOCAL_WIRELESS);

    // The debug menu, under names the menu does not look for until it is
    // shown.
    if (!put_row(o, &m, 2, &sDebug, DEBUGMENU_ITEM_HIDDEN, "MENUITEM_5",
                 DEBUGMENU_MASK_HIDDEN, "MI_5_MASK", DEBUGMENU_FILL_5,
                 "PORT_FILL_5", 0))
        return 0;
    set_string(o, sDebug.item, "Debug Menu");

    // R36S: "Oyundan Cik" (Exit Game), always shown, under Options (the
    // third row below MENUITEM_3). The inline string is the fallback; the
    // LanguageID is what the renderer draws, so the row follows the
    // selected language (v33: it used to stay Turkish in every language).
    if (!put_row(o, &m, 3, &sExit, BGF_ITEM_MENUITEM_6, "MENUITEM_6",
                 MI_6_MASK, "MI_6_MASK", DEBUGMENU_FILL_6, "PORT_FILL_6", 1))
        return 0;
    set_string(o, sExit.item, "Oyundan Cik");
    wr32(o->p + sExit.item + TEXT_LANGUAGE, PORTLANG_EXIT_GAME);
    // The row's own filler is not a slot's: hide it, so no stray bar
    // shows below the panel. Slot 4's filler stays sDebug.fill, which
    // sits at the slot's Y and runs to the panel bottom.
    o->p[sExit.fill + OBJ_VISIBILITY] = 0;
    close_chunk(o, hdr);
    if (o->failed) return 0;

    // The slots: retail's three rows, then the two put_row put below them.
    for (i = 0; i < 3; i++) {
        sLabelY[i] = rdf(o->p + label[i] + OBJ_Y);
        sMaskY[i]  = rdf(o->p + mask[i] + OBJ_Y);
    }
    sLabelY[3] = rdf(o->p + sWireless.item + OBJ_Y);
    sMaskY[3]  = rdf(o->p + sWireless.mask + OBJ_Y);
    sLabelY[4] = rdf(o->p + sDebug.item + OBJ_Y);
    sMaskY[4]  = rdf(o->p + sDebug.mask + OBJ_Y);
    sFill[2]   = filler;
    sFill[3]   = sWireless.fill;
    sFill[4]   = sDebug.fill;
    sBottom    = rdf(m.fill + OBJ_Y) + rdf(m.fill + OBJ_SCALE_Y) * MASK_UNIT;

    sRows[0] = (MainRow){MENUITEM(1), label[0], mask[0]};
    sRows[1] = (MainRow){MENUITEM(2), label[1], mask[1]};
    sRows[2] = (MainRow){BGF_ITEM_MENUITEM_4, sWireless.item, sWireless.mask};
    sRows[3] = (MainRow){MENUITEM(3), label[2], mask[2]};
    sRows[4] = (MainRow){BGF_ITEM_MENUITEM_6, sExit.item, sExit.mask};
    sNumRows = 5;
    return 1;
}

// The Audio Options screen's scroll arrows, found by a pre-pass over the
// Flailer chunk: each arrow's chunk type, payload and size in the input
// buffer. Both set when the screen and its arrows are there; the Sound
// menu's own arrows are never touched, only copied from.
typedef struct {
    const uint8_t *up, *down;
    uint32_t       upSize, downSize, upType, downType;
} AudioArrows;

// The Audio Options screen's two scroll arrows, by their hashes. 1 when
// both are found; the screen or an arrow missing leaves them NULL, and
// the caller adds no arrows rather than inventing any.
static int find_audio_arrows(const uint8_t* flailer, uint32_t size,
                             AudioArrows* out)
{
    BChunkIter it;

    memset(out, 0, sizeof *out);
    bchunk_begin(&it, flailer, 0, size);
    while (bchunk_next(&it)) {
        BChunkIter jt;
        if (it.type != BUI_CHUNK_SCREEN) continue;
        if (screen_hash(it.payload, it.size) != AUDIO_OPTIONS_MENU)
            continue;
        bchunk_begin(&jt, it.payload, 0, it.size);
        while (bchunk_next(&jt)) {
            uint32_t hash;
            if (jt.type == BUI_CHUNK_SCREENDEF || jt.size < 64) continue;
            hash = rd32(jt.payload + OBJ_NAME_HASH);
            if (hash == SCROLL_UP_ARROW && !out->up) {
                out->up = jt.payload;
                out->upSize = jt.size;
                out->upType = jt.type;
            } else if (hash == SCROLL_DOWN_ARROW && !out->down) {
                out->down = jt.payload;
                out->downSize = jt.size;
                out->downType = jt.type;
            }
        }
        return out->up && out->down;
    }
    return 0;
}

// The Options screen as it is, but for one row added under its seventh
// (Languages): the R36S Wireless toggle, MENUITEM_8, copied from
// MENUITEM_7 a row-step below it, with MI_8_MASK and the panel's filler
// under it. Only the row is new: the screen's own seven items, their
// masks and the rest of the panel are copied byte for byte. The row's
// label is set to the toggle's text by debugmenu_options_wireless_set,
// which the Options screen calls whenever it is entered, and on every
// toggle. When the screen has no seventh row, mask or filler to copy
// from, 0, and the caller leaves the screen unchanged.
// The blurb line (SCROLLING_TEXT and its WHITEOUT_BAR) stays where it
// was; the eighth row sits below the seventh, above the line.
// The Audio Options screen's scroll arrows are copied in as well, the
// same bitmaps at the same place, named so the scroll state drives them
// as it does in the Sound menu; when they are not found, no arrows are
// added. The Sound menu's own arrows are untouched.
static int put_options_menu(Out* o, const uint8_t* payload, uint32_t size,
                            const AudioArrows* arrows)
{
    const uint8_t *item6, *item7, *mask6, *mask7, *fill;
    uint32_t       s7 = 0, sm7 = 0, sf = 0;
    size_t         hdr, def = 0, filler = 0, label7 = 0, maskAt7 = 0;
    float          step, maskStep, rowY, maskY, fillY, bottom;
    BChunkIter     it;

    item6 = find_object(payload, size, MENUITEM(6), 0, NULL);
    item7 = find_object(payload, size, MENUITEM(7), 0, &s7);
    mask6 = find_object(payload, size, MI_6_MASK, 0, NULL);
    mask7 = find_object(payload, size, MI_7_MASK, 0, &sm7);
    fill  = find_object(payload, size, BOTTOM_MASK, 1, &sf);
    if (!item6 || !item7 || !mask6 || !mask7)
        return 0;

    hdr = open_chunk(o, BUI_CHUNK_SCREEN);
    bchunk_begin(&it, payload, 0, size);
    while (bchunk_next(&it)) {
        size_t at = put_chunk(o, it.type, it.payload, it.size);
        if (it.type == BUI_CHUNK_SCREENDEF) def = at;
        if (fill && it.payload == fill) filler = at;
        if (it.payload == item7) label7 = at;
        if (it.payload == mask7) maskAt7 = at;
    }
    if (o->failed || !def) return 0;
    // The wireless row, its mask and its filler (3 with a filler, 2
    // without), plus the two scroll arrows when the Sound menu has them.
    o->p[def + SCREEN_NUM_OBJS] += (uint8_t)((fill ? 3 : 2)
        + ((arrows && arrows->up && arrows->down) ? 2 : 0));

    step     = rdf(item7 + OBJ_Y) - rdf(item6 + OBJ_Y);
    maskStep = rdf(mask7 + OBJ_Y) - rdf(mask6 + OBJ_Y);
    rowY     = rdf(item7 + OBJ_Y) + step;
    maskY    = rdf(mask7 + OBJ_Y) + maskStep;

    // The row, one step below the seventh: its label copied from the
    // seventh's text, its mask from the seventh's, at the sixth's X.
    sWirelessOpt.item = put_text(o, item7, s7, rd32(item7 + TEXT_MAX_CHARS));
    set_name(o, sWirelessOpt.item, MENUITEM(8), "MENUITEM_8");
    if (o->failed) return 0;
    wrf(o->p + sWirelessOpt.item + OBJ_X, rdf(item6 + OBJ_X));
    wrf(o->p + sWirelessOpt.item + OBJ_Y, rowY);
    o->p[sWirelessOpt.item + OBJ_VISIBILITY] = 1;
    set_string(o, sWirelessOpt.item, "Kablosuz: Acik");
    // The LanguageID is what the renderer draws, so the row follows the
    // selected language (v33); debugmenu_options_wireless_set keeps it on
    // the row's state. The inline string is the fallback.
    wr32(o->p + sWirelessOpt.item + TEXT_LANGUAGE, PORTLANG_WIRELESS_ROW_ON);

    sWirelessOpt.mask = put_chunk(o, BUI_CHUNK_BITMAP, mask7, sm7);
    set_name(o, sWirelessOpt.mask, MI_8_MASK, "MI_8_MASK");
    if (o->failed) return 0;
    wrf(o->p + sWirelessOpt.mask + OBJ_Y, maskY);
    o->p[sWirelessOpt.mask + OBJ_VISIBILITY] = 1;

    // The blurb line (SCROLLING_TEXT and its WHITEOUT_BAR) stays at its
    // original place; the eighth row sits below the seventh, above it.

    if (fill) {
        // The panel's filler, cut to a separator under the seventh row,
        // and a copy under the eighth, running to the panel's bottom.
        bottom = rdf(fill + OBJ_Y) + rdf(fill + OBJ_SCALE_Y) * MASK_UNIT;
        fillY  = maskY + rdf(mask7 + OBJ_SCALE_Y) * MASK_UNIT;
        wrf(o->p + filler + OBJ_SCALE_Y, SEPARATOR_SCALE);
        sWirelessOpt.fill = put_chunk(o, BUI_CHUNK_BITMAP, fill, sf);
        set_name(o, sWirelessOpt.fill, OPTIONS_FILL, "PORT_FILL_7");
        if (o->failed) return 0;
        wrf(o->p + sWirelessOpt.fill + OBJ_Y, fillY);
        wrf(o->p + sWirelessOpt.fill + OBJ_SCALE_Y,
            (bottom - fillY) / MASK_UNIT);
        o->p[sWirelessOpt.fill + OBJ_VISIBILITY] = 1;
    }

    // The Sound menu's scroll arrows, copied here so the Options menu
    // shows them as the Sound menu does: the same bitmaps, at the same
    // place (both menus share the panel), named with the same hashes so
    // the scroll state drives them. Shown outright, so they are there
    // even before any scroll state runs. The Sound menu's own arrows
    // are untouched; when they are not found, no arrows are added.
    if (arrows && arrows->up && arrows->down) {
        size_t up = put_chunk(o, arrows->upType, arrows->up, arrows->upSize);
        size_t down;
        set_name(o, up, SCROLL_UP_ARROW, "SCROLL_UP");
        if (o->failed) return 0;
        o->p[up + OBJ_VISIBILITY] = 1;
        down = put_chunk(o, arrows->downType, arrows->down,
                         arrows->downSize);
        set_name(o, down, SCROLL_DOWN_ARROW, "SCROLL_DOWN");
        if (o->failed) return 0;
        o->p[down + OBJ_VISIBILITY] = 1;
    }
    close_chunk(o, hdr);
    return !o->failed;
}

// The Language screen's row labels, in kLanguages order (Turkish first),
// as inline strings: the names are proper nouns, the same in every
// language, so they do not come from the FE file. Latin-1, as retail's
// tables are. The eighth is Load All Languages, as retail's seventh was.
static const char* const kLanguageNames[8] = {
    "Turkce", "English", "Fran\xe7" "ais", "Deutsch",
    "Italiano", "Espa\xf1ol", "Svenska", "Load All Languages",
};
#define LANGUAGE_LABEL_ROOM 24

// The seven row masks, MI_1_MASK..MI_7_MASK, in row order: the highlight
// brackets follow a row's mask, so when the rows are compressed the masks
// move with them.
static const uint32_t kLanguageMasks[7] = {
    MI_1_MASK, MI_2_MASK, MI_3_MASK, MI_4_MASK,
    MI_5_MASK, MI_6_MASK, MI_7_MASK,
};

// The Language screen as it is, but for one row added under its seventh
// (Swedish): Load All Languages becomes MENUITEM_8, copied from
// MENUITEM_7 a row-step below it, with MI_8_MASK and the panel's filler
// under it — the Options screen's eighth row, same pattern. The seven
// language rows are then relabelled with kLanguageNames as inline
// strings (TEXT_LANGUAGE 0, so no FE file's wording shows): MENUITEM_1
// Turkish, MENUITEM_2 English, and so on. The panel only fits six rows
// at the authored step, so the seven rows are compressed into the span
// of the first six (Y1..Y6), each row's mask moving with its row, and
// the eighth row sits one compressed step below the seventh. A Turkish
// flag bitmap is added next to the Turkish row, the flags collage
// retargeted to the FLAG_TR texture the port added to FRONTB.BUN's
// pack. When the screen has no sixth or seventh row, mask or filler to
// copy from, 0, and the caller leaves the screen unchanged. The masks
// are best-effort: a screen without them still gets its eighth text
// row, and the BMenu finds MENUITEM_1..8.
static int put_language_menu(Out* o, const uint8_t* payload, uint32_t size)
{
    const uint8_t *item6, *item7, *mask7, *fill, *flagAll = NULL;
    uint32_t       s7 = 0, sm7 = 0, sf = 0, sFlagAll = 0;
    size_t         hdr, def = 0, filler = 0, label[7] = {0}, maskOut[7] = {0};
    size_t         item8 = 0, mask8 = 0, flagTR = 0;
    float          step, maskStep, rowY, maskY, fillY, bottom;
    float          y1, y6, newStep, newY7;
    BChunkIter     it;
    int            i;

    item6 = find_object(payload, size, MENUITEM(6), 0, NULL);
    item7 = find_object(payload, size, MENUITEM(7), 0, &s7);
    mask7 = find_object(payload, size, MI_7_MASK, 0, &sm7);
    fill  = find_object(payload, size, BOTTOM_MASK, 1, &sf);
    if (!item6 || !item7)
        return 0;

    hdr = open_chunk(o, BUI_CHUNK_SCREEN);
    bchunk_begin(&it, payload, 0, size);
    while (bchunk_next(&it)) {
        uint32_t hash = it.size >= 64 ? rd32(it.payload + OBJ_NAME_HASH) : 0;
        size_t   at;
        if (it.type == BUI_CHUNK_SCREENDEF) {
            at = put_chunk(o, it.type, it.payload, it.size);
            def = at;
            continue;
        }
        // The seven language rows, copied with room for their new names.
        if (it.type == CHUNK_TEXT && hash >= MENUITEM(1)
            && hash <= MENUITEM(7)) {
            int n = (int)(hash - MENUITEM(1));
            at = put_text(o, it.payload, it.size, LANGUAGE_LABEL_ROOM);
            label[n] = at;
            continue;
        }
        at = put_chunk(o, it.type, it.payload, it.size);
        if (fill && it.payload == fill) filler = at;
        // A row's mask: record where its copy landed, so it can move
        // with its row when the rows are compressed. The flags collage,
        // the template the Turkish flag is copied from.
        if (it.type == BUI_CHUNK_BITMAP) {
            int m;
            for (m = 0; m < 7; m++)
                if (hash == kLanguageMasks[m]) maskOut[m] = at;
            if (it.size >= OBJ_TEX_HASH + 4
                && rd32(it.payload + OBJ_TEX_HASH) == FLAG_ALL_TEX) {
                flagAll = it.payload;
                sFlagAll = it.size;
            }
        }
    }
    if (o->failed || !def) return 0;
    for (i = 0; i < 7; i++)
        if (!label[i]) return 0;
    // The eighth row, its mask and its filler (3 with a filler, 2
    // without), plus the Turkish flag bitmap.
    o->p[def + SCREEN_NUM_OBJS] +=
        (uint8_t)((fill ? 3 : 2) + (flagAll ? 1 : 0));

    // The panel fits six rows at the authored step; seven language rows
    // must share that span, so compress the step: rows at Y1 + i*newStep
    // for i = 0..6, Y1 and Y6 the first and sixth rows' authored Y. Each
    // row's mask moves by the same delta as its row, so the highlight
    // brackets (which follow the mask's bounding box) land on the right
    // row. The eighth row (Load All, hidden) sits one compressed step
    // below the seventh, its mask one compressed mask-step below the
    // seventh's.
    y1 = rdf(o->p + label[0] + OBJ_Y);
    y6 = rdf(o->p + label[5] + OBJ_Y);
    newStep = (y6 - y1) / 6.0f;
    newY7 = y1 + 6.0f * newStep;
    for (i = 0; i < 7; i++) {
        float oldY = rdf(o->p + label[i] + OBJ_Y);
        float newY = y1 + (float)i * newStep;
        wrf(o->p + label[i] + OBJ_Y, newY);
        if (maskOut[i]) {
            float mOld = rdf(o->p + maskOut[i] + OBJ_Y);
            wrf(o->p + maskOut[i] + OBJ_Y, mOld + (newY - oldY));
        }
    }
    step = newStep;
    rowY = newY7 + step;
    if (maskOut[5] && maskOut[6]) {
        float m6 = rdf(o->p + maskOut[5] + OBJ_Y);
        float m7 = rdf(o->p + maskOut[6] + OBJ_Y);
        maskStep = m7 - m6;
        maskY = m7 + maskStep;
    } else {
        maskStep = newStep;
        maskY = newY7 + maskStep;
    }

    item8 = put_text(o, item7, s7, LANGUAGE_LABEL_ROOM);
    set_name(o, item8, MENUITEM(8), "MENUITEM_8");
    if (o->failed) return 0;
    wrf(o->p + item8 + OBJ_X, rdf(item6 + OBJ_X));
    wrf(o->p + item8 + OBJ_Y, rowY);
    o->p[item8 + OBJ_VISIBILITY] = 1;

    if (mask7) {
        mask8 = put_chunk(o, BUI_CHUNK_BITMAP, mask7, sm7);
        set_name(o, mask8, MI_8_MASK, "MI_8_MASK");
        if (o->failed) return 0;
        wrf(o->p + mask8 + OBJ_Y, maskY);
        o->p[mask8 + OBJ_VISIBILITY] = 1;
    }

    if (fill) {
        // The panel's bottom edge, from the filler's authored place
        // (the panel does not move). The filler itself, cut to a
        // separator, moves with the seventh row so it stays just under
        // the last visible row.
        float mScale = mask7 ? rdf(mask7 + OBJ_SCALE_Y) : 0.0f;
        bottom = rdf(fill + OBJ_Y) + rdf(fill + OBJ_SCALE_Y) * MASK_UNIT;
        fillY  = maskY + mScale * MASK_UNIT;
        if (filler)
            wrf(o->p + filler + OBJ_Y,
                rdf(o->p + filler + OBJ_Y) + (newY7 - rdf(item7 + OBJ_Y)));
        wrf(o->p + filler + OBJ_SCALE_Y, SEPARATOR_SCALE);
        {
            size_t f8 = put_chunk(o, BUI_CHUNK_BITMAP, fill, sf);
            set_name(o, f8, LANGUAGE_FILL, "PORT_FILL_L7");
            if (o->failed) return 0;
            wrf(o->p + f8 + OBJ_Y, fillY);
            wrf(o->p + f8 + OBJ_SCALE_Y, (bottom - fillY) / MASK_UNIT);
            o->p[f8 + OBJ_VISIBILITY] = 1;
        }
    }

    // The Turkish flag, next to the Turkish row: the flags collage
    // retargeted to the FLAG_TR texture the port added to FRONTB.BUN's
    // pack, at the first row's height, in the gap between the language
    // text and the collage. FLAG_TR is 64x40 at scale 1, so it spans
    // X 350..414, Y 106..146 — inside the panel, clear of the text and
    // of the collage (X 436..640, Y 104..296).
    if (flagAll) {
        flagTR = put_chunk(o, BUI_CHUNK_BITMAP, flagAll, sFlagAll);
        set_name(o, flagTR, FLAG_TR_TEX, "FLAG_TR");
        if (o->failed) return 0;
        wr32(o->p + flagTR + OBJ_TEX_HASH, FLAG_TR_TEX);
        wrf(o->p + flagTR + OBJ_X, 350.0f);
        wrf(o->p + flagTR + OBJ_Y, y1);
        wrf(o->p + flagTR + OBJ_SCALE_X, 1.0f);
        wrf(o->p + flagTR + OBJ_SCALE_Y, 1.0f);
        o->p[flagTR + OBJ_VISIBILITY] = 1;
    }

    // The labels: inline strings, so the row shows its language's name
    // whatever FE file is loaded.
    for (i = 0; i < 7; i++)
        set_string(o, label[i], kLanguageNames[i]);
    set_string(o, item8, kLanguageNames[7]);
    close_chunk(o, hdr);
    return !o->failed;
}

// The Options screen's eighth row, live: its label rewritten to the
// toggle's text. The buffer the patch returned is what bui reads, so the
// new string shows without re-entering the screen. The LanguageID is what
// the renderer draws (object_text: LanguageID wins over the inline
// placeholder), so the row follows the selected language (v33); the
// inline Turkish string is only the fallback if the lookup fails.
void debugmenu_options_wireless_set(int off)
{
    uint32_t room;
    uint32_t id;

    if (!sBuf || !sWirelessOpt.item) return;
    id   = off ? PORTLANG_WIRELESS_ROW_OFF : PORTLANG_WIRELESS_ROW_ON;
    room = rd32(sBuf + sWirelessOpt.item + TEXT_MAX_CHARS);
    memset(sBuf + sWirelessOpt.item + TEXT_STRING, 0, room);
    strncpy((char*)sBuf + sWirelessOpt.item + TEXT_STRING,
            off ? "Kablosuz: Kapali" : "Kablosuz: Acik", room - 1);
    wr32(sBuf + sWirelessOpt.item + TEXT_LANGUAGE, id);
}

// The rows from sTop in the slots, the rest hidden, label and mask: a
// row's label and mask Y are its slot's, its X its own (Options' stays two
// pixels right of the rest, as retail has it). The masks shown but the
// highlighted row's. Under the slots in use, each filler a separator but
// the last row's, which runs down to the panel's bottom; none under a slot
// not in use.
static void layout(void)
{
    int n = sNumRows < sSlots ? sNumRows : sSlots;
    int i;

    if (!sBuf || !sNumRows) return;
    if (sTop > sNumRows - n) sTop = sNumRows - n;
    if (sTop < 0) sTop = 0;
    for (i = 0; i < sNumRows; i++) {
        const MainRow* r    = &sRows[i];
        int            slot = i - sTop;
        int            in   = slot >= 0 && slot < n;
        if (in) {
            wrf(sBuf + r->label + OBJ_Y, sLabelY[slot]);
            wrf(sBuf + r->mask + OBJ_Y, sMaskY[slot]);
        }
        sBuf[r->label + OBJ_VISIBILITY] = (uint8_t)in;
        sBuf[r->mask + OBJ_VISIBILITY]  = (uint8_t)(in && i != sCurrent);
    }
    for (i = 0; i < MAIN_SLOTS; i++) {
        size_t fill = sFill[i];
        if (!fill) continue;
        sBuf[fill + OBJ_VISIBILITY] = (uint8_t)(i < n);
        wrf(sBuf + fill + OBJ_SCALE_Y,
            i < n - 1 ? SEPARATOR_SCALE
                      : (sBottom - rdf(sBuf + fill + OBJ_Y)) / MASK_UNIT);
    }
}

static int main_row(uint32_t itemHash)
{
    int i;
    for (i = 0; i < sNumRows; i++)
        if (sRows[i].item == itemHash) return i;
    return -1;
}

static int left_out(uint32_t hash, int rows)
{
    for (size_t i = 0; i < sizeof kLeftOut / sizeof kLeftOut[0]; i++)
        if (kLeftOut[i] == hash) return 1;
    return (hash > MENUITEM(rows) && hash <= MENUITEM(7))
           || (hash > LINE(rows) && hash <= LINE(7));
}

// Be The Cop Setup again, as one of the port's own screens: renamed and
// titled, its eighth row, the rows past `rows` and the blooms' texts gone,
// and the texts given room.
static int put_debug_screen(Out* o, const uint8_t* payload, uint32_t size,
                            uint32_t name, const char* title, int rows)
{
    size_t     hdr = open_chunk(o, BUI_CHUNK_SCREEN);
    size_t     def = 0;
    uint32_t   count = 0;
    BChunkIter it;

    bchunk_begin(&it, payload, 0, size);
    while (bchunk_next(&it)) {
        uint32_t hash;
        size_t   at;

        if (it.type == BUI_CHUNK_SCREENDEF) {
            def = put_chunk(o, it.type, it.payload, it.size);
            if (!o->failed) wr32(o->p + def + SCREEN_NAME, name);
            continue;
        }
        hash = it.size >= 64 ? rd32(it.payload + OBJ_NAME_HASH) : 0;
        if (left_out(hash, rows)) continue;
        count++;
        if (it.type != CHUNK_TEXT || it.size < TEXT_STRING) {
            put_chunk(o, it.type, it.payload, it.size);
        } else if (hash >= MENUITEM(1) && hash <= MENUITEM(7)) {
            put_text(o, it.payload, it.size, LABEL_ROOM);
        } else if (hash >= LINE(1) && hash <= LINE(7)) {
            put_text(o, it.payload, it.size, VALUE_ROOM);
        } else if (hash == HELP) {
            put_text(o, it.payload, it.size, HELP_ROOM);
        } else if (hash == TITLE) {
            at = put_text(o, it.payload, it.size, TITLE_ROOM);
            set_string(o, at, title);
        } else {
            put_chunk(o, it.type, it.payload, it.size);
        }
    }
    if (o->failed || !def) return 0;
    o->p[def + SCREEN_NUM_OBJS]     = (uint8_t)count;
    o->p[def + SCREEN_NUM_OBJS + 1] = (uint8_t)(count >> 8);
    close_chunk(o, hdr);
    return !o->failed;
}

uint8_t* debugmenu_patch_frontend(const uint8_t* data, size_t len,
                                  size_t* outLen)
{
    Out            o;
    uint32_t       size = 0;
    const uint8_t* flailer, *source = NULL;
    uint32_t       sourceSize = 0;
    size_t         hdr;
    int            mainMenu = 0, optionsMenu = 0, languageMenu = 0;
    AudioArrows    arrows;
    BChunkIter     it;

    memset(&o, 0, sizeof o);
    memset(&sWireless, 0, sizeof sWireless);
    memset(&sDebug, 0, sizeof sDebug);
    memset(&sExit, 0, sizeof sExit);
    memset(&sWirelessOpt, 0, sizeof sWirelessOpt);
    memset(sFill, 0, sizeof sFill);
    sBuf      = NULL;
    sRevealed = 0;
    sNumRows  = 0;
    sTop      = 0;
    sCurrent  = -1;
    flailer = bchunk_find(data, 0, len, BUI_CHUNK_FLAILER, &size);
    if (!flailer) return NULL;

    // The Sound menu's scroll arrows, for the Options screen's copies.
    // Missing arrows mean no arrows in Options, never invented ones.
    find_audio_arrows(flailer, size, &arrows);

    hdr = open_chunk(&o, BUI_CHUNK_FLAILER);
    bchunk_begin(&it, flailer, 0, size);
    while (bchunk_next(&it)) {
        uint32_t screen = it.type == BUI_CHUNK_SCREEN
                              ? screen_hash(it.payload, it.size)
                              : BUI_HASH_NONE;
        if (it.size & 3u) {           // would leave what follows unaligned
            free(o.p);
            return NULL;
        }
        if (screen == MAIN_MENU && !mainMenu) {
            if (!put_main_menu(&o, it.payload, it.size)) break;
            mainMenu = 1;
            continue;
        }
        // R36S: the Options screen's eighth row, the Wireless toggle,
        // and the Sound menu's scroll arrows copied in. A screen the
        // patch does not know is left as it is, so a missing row costs
        // only the toggle, never the main menu.
        if (screen == OPTIONS_MENU && !optionsMenu) {
            optionsMenu = 1;
            if (put_options_menu(&o, it.payload, it.size, &arrows)) continue;
        }
        // R36S: the Language screen's eighth row (Load All Languages
        // moves there) and its seven language rows relabelled, Turkish
        // first. A screen without the rows is left as it is.
        if (screen == LANGUAGE_MENU && !languageMenu) {
            languageMenu = 1;
            if (put_language_menu(&o, it.payload, it.size)) continue;
        }
        if (screen == DEBUGMENU_SOURCE && !source) {
            source     = it.payload;
            sourceSize = it.size;
        }
        put_chunk(&o, it.type, it.payload, it.size);
    }
    if (!o.failed && mainMenu && source) {
        put_debug_screen(&o, source, sourceSize, DEBUGMENU_SCREEN,
                         "Debug Menu", DEBUGMENU_ROWS);
        put_debug_screen(&o, source, sourceSize, DEBUGMENU_RACE_SCREEN,
                         "Debug Race", 7);
    }
    close_chunk(&o, hdr);

    if (o.failed || !mainMenu || !source || !sDebug.item) {
        free(o.p);
        memset(&sWireless, 0, sizeof sWireless);
        memset(&sDebug, 0, sizeof sDebug);
        memset(&sExit, 0, sizeof sExit);
        memset(&sWirelessOpt, 0, sizeof sWirelessOpt);
        sNumRows = 0;
        return NULL;
    }
    sBuf = o.p;
    layout();
    if (outLen) *outLen = o.len;
    return o.p;
}

int debugmenu_reveal(void)
{
    if (!sBuf || !sDebug.item || sRevealed || sNumRows == MAIN_ROWS)
        return 0;
    wr32(sBuf + sDebug.item + OBJ_NAME_HASH, BGF_ITEM_MENUITEM_5);
    wr32(sBuf + sDebug.mask + OBJ_NAME_HASH, MI_5_MASK);
    sRows[sNumRows++] = (MainRow){BGF_ITEM_MENUITEM_5, sDebug.item,
                                  sDebug.mask};
    sRevealed = 1;
    layout();
    return 1;
}

int debugmenu_revealed(void)
{
    return sRevealed;
}

int debugmenu_main_rank(uint32_t itemHash)
{
    int i = main_row(itemHash);
    return i < 0 ? MAIN_ROWS : i;
}

int debugmenu_main_show(uint32_t itemHash)
{
    int i = main_row(itemHash);
    int n = sNumRows < sSlots ? sNumRows : sSlots;

    if (!sBuf || i < 0) return -1;
    if (i < sTop) sTop = i;
    else if (i >= sTop + n) sTop = i - n + 1;
    sCurrent = i;
    layout();
    return sTop;
}

void debugmenu_main_tick(void)
{
    layout();
}

void debugmenu_main_set_slots(int slots)
{
    sSlots = slots < 3 ? 3 : slots > MAIN_SLOTS ? MAIN_SLOTS : slots;
    layout();
}
