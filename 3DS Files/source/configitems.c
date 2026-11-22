// configitems -- see configitems.h.

#include "configitems.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "playercam.h"

int32_t TrackSelectedInConfigFile = -1;     // gp-23256
int32_t bVersionE3;                         // gp-23224
int32_t SkipFENumPlayerCars = 1;            // gp-23216
int32_t SkipFENumAICars;                    // gp-23212
int32_t SkipFENumLaps = 8;                  // gp-23208
int32_t SkipFEMaxCops;                      // gp-23204
float   SkipFETimeUntilCopsAppear = 1.0f;   // gp-23200
int32_t SkipFETickets = 1;                  // gp-23196
int32_t SkipFERoadBlocks = 1;               // gp-23192
int32_t SkipFEHelicopter = 1;               // gp-23188
int32_t SkipFESpikeStrips = 1;              // gp-23184
int32_t SkipFENew624 = 1;                   // gp-23180
int32_t SkipFEBeACop;                       // gp-23176
int32_t SkipFERearView = 1;                 // gp-23168
float   SkipFETrafficDensity;               // gp-23164
float   SkipFETrafficOncoming = 0.5f;       // gp-23160
int32_t SkipFERaceType = 1;                 // gp-23156
int32_t SkipFESpeedoMode = 1;               // gp-23152
int32_t SkipFEDifficulty;                   // gp-23148
int32_t SkipFEEnableJumpCam = 1;            // gp-23144
int32_t SkipFEEnableZoneCam = 1;            // gp-23140
int32_t DefaultPlayer1CarType = 37;         // gp-23136
int32_t DefaultPlayer2CarType = 37;         // gp-23132
int32_t DefaultPlayer1SkinIndex = 1;        // gp-23128
int32_t DefaultPlayer2SkinIndex = 1;        // gp-23124
int32_t ForceAllAICarsToBeThisType = -1;    // gp-23116
int32_t MaximumNumAICars = 7;               // gp-23112
int32_t TunePreRaceSequence;                // gp-23108
int32_t ConfigDebugLog;                     // the port's own
char ConfigPlayerName[12];                  // the port's own, "" = none
int32_t ConfigGpuSends = 1;                 // the port's own
int32_t ConfigKernelFlush = 1;              // the port's own
int32_t ConfigClusterCull = 1;              // the port's own
int32_t ConfigForceChopper;                 // the port's own
int32_t ConfigDebugMenu;                    // the port's own
int32_t ConfigWirelessTest;                 // the port's own
int32_t ConfigWirelessTestSeconds = 120;    // the port's own
int32_t ConfigWirelessTestConsoles = 2;     // the port's own
int32_t ConfigWirelessOff;                  // the port's own
int32_t ConfigWirelessTestFinish;           // the port's own
int32_t ConfigWirelessHoldSeconds = 90;     // the port's own
int32_t ConfigWirelessTestDropFrame;        // the port's own
int32_t ConfigWirelessTestDropMs = 5000;    // the port's own
int32_t ConfigWirelessTestBlindFrame;       // the port's own
int32_t ConfigWirelessTestBlindFrames = 300; // the port's own
int32_t ConfigTestFinish;                   // the port's own
int32_t ConfigTestFinishPlace = 1;          // the port's own
int32_t ConfigTestFinishTime;               // the port's own
int32_t ConfigReplayCheck;                  // the port's own
int32_t ConfigDemoReplays;                  // the port's own
int32_t ConfigThreadGuard;                  // the port's own
int32_t ConfigCoreSample;                   // the port's own
int32_t ConfigGpuLog;                       // the port's own
int32_t ConfigPauseLog;                     // the port's own
int32_t ConfigEggCodes;                     // the port's own
int32_t ConfigFallLog;                      // the port's own
int32_t ConfigDemoReplayDrift;              // the port's own
int32_t ConfigDemoReplayFile;               // the port's own

int32_t config_skip_fe(void)
{
    return gPlayerCamSkipFE != NULL ? *gPlayerCamSkipFE : 0;
}

// ------------------------------------------------- the port's config file

typedef struct ConfigItem {
    const char *Name;
    void       *Word;
    int         IsFloat;
} ConfigItem;

static const ConfigItem kConfigItems[] = {
    { "TrackSelectedInConfigFile",  &TrackSelectedInConfigFile,  0 },
    { "bVersionE3",                 &bVersionE3,                 0 },
    { "SkipFENumPlayerCars",        &SkipFENumPlayerCars,        0 },
    { "SkipFENumAICars",            &SkipFENumAICars,            0 },
    { "SkipFENumLaps",              &SkipFENumLaps,              0 },
    { "SkipFEMaxCops",              &SkipFEMaxCops,              0 },
    { "SkipFETimeUntilCopsAppear",  &SkipFETimeUntilCopsAppear,  1 },
    { "SkipFETickets",              &SkipFETickets,              0 },
    { "SkipFERoadBlocks",           &SkipFERoadBlocks,           0 },
    { "SkipFEHelicopter",           &SkipFEHelicopter,           0 },
    { "SkipFESpikeStrips",          &SkipFESpikeStrips,          0 },
    { "SkipFENew624",               &SkipFENew624,               0 },
    { "SkipFEBeACop",               &SkipFEBeACop,               0 },
    { "SkipFERearView",             &SkipFERearView,             0 },
    { "SkipFETrafficDensity",       &SkipFETrafficDensity,       1 },
    { "SkipFETrafficOncoming",      &SkipFETrafficOncoming,      1 },
    { "SkipFERaceType",             &SkipFERaceType,             0 },
    { "SkipFESpeedoMode",           &SkipFESpeedoMode,           0 },
    { "SkipFEDifficulty",           &SkipFEDifficulty,           0 },
    { "SkipFEEnableJumpCam",        &SkipFEEnableJumpCam,        0 },
    { "SkipFEEnableZoneCam",        &SkipFEEnableZoneCam,        0 },
    { "DefaultPlayer1CarType",      &DefaultPlayer1CarType,      0 },
    { "DefaultPlayer2CarType",      &DefaultPlayer2CarType,      0 },
    { "DefaultPlayer1SkinIndex",    &DefaultPlayer1SkinIndex,    0 },
    { "DefaultPlayer2SkinIndex",    &DefaultPlayer2SkinIndex,    0 },
    { "ForceAllAICarsToBeThisType", &ForceAllAICarsToBeThisType, 0 },
    { "MaximumNumAICars",           &MaximumNumAICars,           0 },
    { "TunePreRaceSequence",        &TunePreRaceSequence,        0 },
    { "DebugLog",                   &ConfigDebugLog,             0 },
    { "GpuSends",                   &ConfigGpuSends,             0 },
    { "KernelFlush",                &ConfigKernelFlush,          0 },
    { "ClusterCull",                &ConfigClusterCull,          0 },
    { "ForceChopper",               &ConfigForceChopper,         0 },
    { "DebugMenu",                  &ConfigDebugMenu,            0 },
    { "WirelessTest",               &ConfigWirelessTest,         0 },
    { "WirelessTestSeconds",        &ConfigWirelessTestSeconds,  0 },
    { "WirelessTestConsoles",       &ConfigWirelessTestConsoles, 0 },
    { "WirelessOff",                &ConfigWirelessOff,          0 },
    { "WirelessTestFinish",         &ConfigWirelessTestFinish,   0 },
    { "WirelessHoldSeconds",        &ConfigWirelessHoldSeconds,  0 },
    { "WirelessTestDropFrame",      &ConfigWirelessTestDropFrame, 0 },
    { "WirelessTestDropMs",         &ConfigWirelessTestDropMs,   0 },
    { "WirelessTestBlindFrame",     &ConfigWirelessTestBlindFrame, 0 },
    { "WirelessTestBlindFrames",    &ConfigWirelessTestBlindFrames, 0 },
    { "TestFinish",                 &ConfigTestFinish,           0 },
    { "TestFinishPlace",            &ConfigTestFinishPlace,      0 },
    { "TestFinishTime",             &ConfigTestFinishTime,       0 },
    { "ReplayCheck",                &ConfigReplayCheck,          0 },
    { "DemoReplays",                &ConfigDemoReplays,          0 },
    { "ThreadGuard",                &ConfigThreadGuard,          0 },
    { "CoreSample",                 &ConfigCoreSample,           0 },
    { "GPULog",                     &ConfigGpuLog,               0 },
    { "PauseLog",                   &ConfigPauseLog,             0 },
    { "EggCodes",                   &ConfigEggCodes,             0 },
    { "FallLog",                    &ConfigFallLog,              0 },
    { "DemoReplayDrift",            &ConfigDemoReplayDrift,      0 },
    { "DemoReplayFile",             &ConfigDemoReplayFile,       0 },
};

static void config_log(ConfigItemsLogFn log, const char *fmt,
                       const char *a, const char *b)
{
    char line[320];

    if (log == NULL)
        return;
    snprintf(line, sizeof line, fmt, a, b);
    log(line);
}

// One line, NUL-terminated and stripped of its comment. The file is
// shell syntax (NFSHP2.sh sources it), so the form is Key=Value: spaces
// around '=' are allowed, and a value may be wrapped in one pair of
// double quotes, which are stripped. The NFSHP2_* lines are the
// launcher's; the game reads those through getenv, so they are ignored
// here, silently — they belong in this file.
static int config_apply_line(char *line, int *skipFE, ConfigItemsLogFn log)
{
    char name[64];
    char *p = line;
    char *eq, *v, *end;
    const ConfigItem *it = NULL;
    size_t i, nlen;
    long n;
    float f;

    while (*p == ' ' || *p == '\t')
        p++;
    if (*p == '\0')
        return 0;
    eq = strchr(p, '=');
    if (eq == NULL) {
        config_log(log, "config: skipped \"%s\"%s", line, "");
        return 0;
    }
    end = eq;
    while (end > p && (end[-1] == ' ' || end[-1] == '\t'))
        end--;
    nlen = (size_t)(end - p);
    if (nlen == 0 || nlen >= sizeof name) {
        config_log(log, "config: skipped \"%s\"%s", line, "");
        return 0;
    }
    memcpy(name, p, nlen);
    name[nlen] = '\0';
    v = eq + 1;
    while (*v == ' ' || *v == '\t')
        v++;
    end = v + strlen(v);
    while (end > v && (end[-1] == ' ' || end[-1] == '\t'))
        end--;
    *end = '\0';
    if (end - v >= 2 && v[0] == '"' && end[-1] == '"') {
        v++;
        end--;
        *end = '\0';
        while (*v == ' ' || *v == '\t')
            v++;
        end = v + strlen(v);
        while (end > v && (end[-1] == ' ' || end[-1] == '\t'))
            end--;
        *end = '\0';
    }
    if (strncmp(name, "NFSHP2_", 7) == 0)
        return 0;
    // "PlayerName=<name>": the only string item in the file, so it is
    // parsed on its own. The value is the rest of the line after '='
    // (spaces allowed, so a name with a space is written quoted:
    // PlayerName="Volkan Tolky"); it is filtered to the Player Names
    // screen's characters (nameentry.c's nameentry_char_ok: a-z A-Z 0-9
    // ! . # space) and cut to 11, like a typed name. Anything else —
    // including Turkish diacritics, which are non-ASCII bytes — is
    // dropped. A '#' cannot reach here: the caller strips comments
    // first. An empty value clears the name; the last PlayerName line
    // in the file wins.
    if (strcmp(name, "PlayerName") == 0) {
        char *dst = ConfigPlayerName;
        int n = 0;

        for (; *v != '\0' && n < 11; v++) {
            unsigned char c = (unsigned char)*v;
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
                || (c >= '0' && c <= '9') || c == '!' || c == '.'
                || c == '#' || c == ' ')
                dst[n++] = (char)c;
        }
        dst[n] = '\0';
        config_log(log, "config: PlayerName = %s%s", ConfigPlayerName, "");
        return 1;
    }
    if (strcmp(name, "SkipFE") == 0) {
        n = strtol(v, &end, 0);
        if (end == v || *end != '\0') {
            config_log(log, "config: %s: bad value %s", name, v);
            return 0;
        }
        *skipFE = (int)n;
        config_log(log, "config: %s = %s", name, v);
        return 1;
    }
    for (i = 0; i < sizeof kConfigItems / sizeof kConfigItems[0]; i++)
        if (strcmp(name, kConfigItems[i].Name) == 0)
            it = &kConfigItems[i];
    if (it == NULL) {
        config_log(log, "config: no item %s%s", name, "");
        return 0;
    }
    if (it->IsFloat) {
        f = strtof(v, &end);
        if (end == v || *end != '\0') {
            config_log(log, "config: %s: bad value %s", name, v);
            return 0;
        }
        *(float *)it->Word = f;
    } else {
        n = strtol(v, &end, 0);
        if (end == v || *end != '\0') {
            config_log(log, "config: %s: bad value %s", name, v);
            return 0;
        }
        *(int32_t *)it->Word = (int32_t)n;
    }
    config_log(log, "config: %s = %s", name, v);
    return 1;
}

int config_items_apply_text(const char *text, int *skipFE,
                            ConfigItemsLogFn log)
{
    char line[256];
    int set = 0;

    while (*text != '\0') {
        size_t len = strcspn(text, "\r\n");
        size_t keep = len < sizeof line - 1 ? len : sizeof line - 1;
        char *hash;

        memcpy(line, text, keep);
        line[keep] = '\0';
        hash = strchr(line, '#');
        if (hash != NULL)
            *hash = '\0';
        set += config_apply_line(line, skipFE, log);
        text += len;
        while (*text == '\r' || *text == '\n')
            text++;
    }
    return set;
}

// R36S: write the Options screen's Wireless toggle back into the config
// file, so it holds across boots. Every other line is kept byte for byte
// (a "# ..." comment, a blank line, and the launcher's NFSHP2_* lines
// included); a "WirelessOff=..." line's value is replaced, or the line is
// appended when there is none. The file is only read at boot otherwise,
// so a write here is the toggle's persistence. The new content is built
// whole in memory first and the file is opened only to write it, so a
// failure cannot leave a half-written file for NFSHP2.sh to source at
// the next launch. 1 when the file was written, 0 when it could not be
// (the toggle still holds for this session; the next boot reads the old
// value).
int config_save_wireless_off(void)
{
    static char text[8192];
    static char out[8192 + 32];
    FILE*       f = fopen(CONFIG_ITEMS_FILE, "rb");
    size_t      n = 0, at = 0, found = 0;
    const char* p;

    if (f != NULL) {
        n = fread(text, 1, sizeof text - 1, f);
        fclose(f);
        if (n >= sizeof text - 1) return 0;   // too big to rewrite safely
        text[n] = '\0';
    } else {
        text[0] = '\0';
    }
    p = text;
    while (*p != '\0') {
        size_t len = strcspn(p, "\r\n");
        const char *k = p, *eq;
        size_t klen;

        // A "WirelessOff=..." line: replace the whole line with the
        // current value. A "# ..." comment line never matches, and a
        // bare "WirelessOff" with no '=' is left alone (the appended
        // line below is the one the reader takes).
        while (*k == ' ' || *k == '\t')
            k++;
        eq = memchr(k, '=', len - (size_t)(k - p));
        klen = 0;
        if (eq != NULL) {
            const char *ke = eq;
            while (ke > k && (ke[-1] == ' ' || ke[-1] == '\t'))
                ke--;
            klen = (size_t)(ke - k);
        }
        if (eq != NULL && klen == strlen("WirelessOff")
            && memcmp(k, "WirelessOff", klen) == 0) {
            at += (size_t)snprintf(out + at, sizeof out - at,
                                   "WirelessOff=%d\n",
                                   (int)ConfigWirelessOff);
            found = 1;
        } else {
            if (at + len + 1 >= sizeof out) return 0;
            memcpy(out + at, p, len);
            at += len;
            out[at++] = '\n';
        }
        p += len;
        // One line ending only: a blank line is a lone '\n' of its own,
        // and swallowing it here would drop it from the file.
        if (*p == '\r') p++;
        if (*p == '\n') p++;
    }
    if (!found)
        at += (size_t)snprintf(out + at, sizeof out - at, "WirelessOff=%d\n",
                               (int)ConfigWirelessOff);
    out[at] = '\0';
    f = fopen(CONFIG_ITEMS_FILE, "wb");
    if (f == NULL) return 0;
    if (fwrite(out, 1, at, f) != at) {
        fclose(f);
        return 0;
    }
    if (fclose(f) != 0) return 0;
    return 1;
}
