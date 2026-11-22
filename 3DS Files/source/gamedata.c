#include "gamedata.h"

#include <stdio.h>
#include <string.h>

static char s_root[GAMEDATA_MAX_PATH];
static bool s_ready;

static const char* const kDefaults[] = {
    "sdmc:/3ds/NFSHP2-3DS/data",
    "sdmc:/NFSHP2",
};

static void set_root(const char* path)
{
    size_t n = strlen(path);
    while (n > 0 && (path[n - 1] == '/' || path[n - 1] == '\\')) n--;
    if (n >= sizeof(s_root)) n = sizeof(s_root) - 1;
    memcpy(s_root, path, n);
    s_root[n] = '\0';
}

static bool file_exists(const char* path)
{
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    fclose(f);
    return true;
}

// A candidate counts only if the sentinel is in it, so a stale pointer
// falls through to a default rather than leaving the game pointed at
// nothing.
static bool candidate_ok(const char* dir)
{
    char path[GAMEDATA_MAX_PATH];
    set_root(dir);
    if (!gamedata_path(GAMEDATA_SENTINEL, path, sizeof(path))) return false;
    return file_exists(path);
}

bool gamedata_init(void)
{
    if (s_ready) return true;

    // The data directory is one of the two defaults. There used to be a
    // third source, a nfshp2_config.txt next to the executable naming the
    // directory; it is gone (2026-11-18, v40 — the settings file of that
    // name was folded into the launcher's nfshp2.cfg, and no .txt is read
    // any more), so the defaults are the whole search.
    for (size_t i = 0; i < sizeof(kDefaults) / sizeof(kDefaults[0]); i++)
        if (candidate_ok(kDefaults[i])) { s_ready = true; return true; }

    // s_root holds the last candidate tried, which is what an error message
    // wants to show.
    return false;
}

const char* gamedata_root(void)
{
    return s_root;
}

char* gamedata_path(const char* rel, char* out, size_t cap)
{
    while (*rel == '/') rel++;
    int n = snprintf(out, cap, "%s/%s", s_root, rel);
    if (n < 0 || (size_t)n >= cap) return NULL;
    return out;
}

bool gamedata_exists(const char* rel)
{
    char path[GAMEDATA_MAX_PATH];
    if (!gamedata_path(rel, path, sizeof(path))) return false;
    return file_exists(path);
}
