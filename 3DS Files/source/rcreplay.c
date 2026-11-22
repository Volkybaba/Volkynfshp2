// rcreplay -- the race coordinator's instant replay. TASKS 72g-3;
// rcreplay.h has the map and docs/formats/replay.md the decode.
#include "rcreplay.h"

#include <stddef.h>
#include <stdio.h>

#include "bmem.h"
#include "fred.h"
#include "logwriter.h"
#include "pausemenu.h"
#include "player.h"
#include "playercam.h"
#include "rcpost.h"
#include "rcstates.h"
#include "realtime.h"
#include "replay.h"
#include "replaymgr.h"
#include "replayscreen.h"

// The pool the screen comes from.
#define RC_POOL 16

// ThePauseMenu's ReplayChooser: player 1, or whoever finished best.
#define RC_CHOOSER_PLAYER_1  1
#define RC_CHOOSER_BEST     -1

// ---------------------------------------------------------- the helpers

static void mute(int32_t on)
{
    if (gSoundMuteFn != NULL)
        gSoundMuteFn(on);
}

// The stats element of the player who finished best, the first of any
// tie, or NULL when no player has one.
static const TransientStatsElement *best_finisher(void)
{
    const TransientStatsElement *best = NULL;
    int32_t i;

    for (i = 0; i < PlayerNumPlayers; i++) {
        const TransientStatsElement *d =
            gTransientStatsGetDriverFn != NULL
                ? gTransientStatsGetDriverFn(
                      PlayerPlayersByIndex[i]->PlayerNumber)
                : NULL;

        if (d != NULL && (best == NULL
                          || d->FinishPosition < best->FinishPosition))
            best = d;
    }
    return best;
}

// The player the replay follows, from the pause menu's ReplayChooser.
// Retail reads ThePauseMenu unchecked; without one, player 0.
static int32_t replay_player(void)
{
    const TransientStatsElement *best;
    int32_t chooser;

    if (ThePauseMenu == NULL)
        return 0;
    chooser = ThePauseMenu->ReplayChooser;
    if (chooser == RC_CHOOSER_PLAYER_1)
        return 1;
    if (chooser != RC_CHOOSER_BEST)
        return 0;
    best = best_finisher();
    return best != NULL ? best->UniqueDriveNumber : 0;
}

// Every player's camera movers off, and the screen made: the player is
// chosen before the allocation, as retail does.
static void make_screen(RaceCoordinator *rc)
{
    int32_t i, pn;
    void *mem;

    for (i = 0; i < PlayerNumPlayers; i++)
        player_disable_camera_movers(PlayerPlayersByIndex[i]);
    pn = replay_player();
    mem = bMalloc(REPLAY_SCREEN_SIZE, RC_REPLAY_SCREEN_NAME, 0, RC_POOL);
    rc->pReplayScreen = replayscreen_construct(mem, pn);
}

// DIAG (v48): the instant replay's window, drained so a crash after the
// restore cannot lose it. pause_loop is the loop at the pause, from_loop
// the snapshot the replay restarts from, end_loop the loop the capture
// reached; the replay should play (end_loop - from_loop) frames.
// snaps is the instant replay snapshots held; ok is 0 when the present
// could not be kept or the replay could not be restored.
static void replay_diag_start(ReplayManager *rm, int32_t pause_loop,
                              int32_t from_loop, int32_t ok)
{
    char buf[160];
    int m = snprintf(buf, sizeof(buf),
        "DIAG replay: start pause_loop=%d from_loop=%d end_loop=%d snaps=%d ok=%d\n",
        (int)pause_loop, (int)from_loop, (int)rm->EndCapturingLoopCount,
        (int)rm->NumInstantReplaySnapshots, (int)ok);
    if (m > 0) {
        logwriter_put(buf, (size_t)m);
        logwriter_drain();
    }
}

// From the end screens, the race from its start: a change back to the
// post-race is queued when there is no lap snapshot, and the screen is
// made all the same. From the pause, the present kept and the instant
// replay begun; when either fails, back to the pause with no screen.
static void instant_replay_activate(RaceCoordinator *rc)
{
    ReplayManager *rm = pReplayManager;

    if (rc->pEndGameScreens != NULL) {
        if (!replaymgr_restore_snapshot_named(rm, RC_REPLAY_LAP_SNAPSHOT))
            racecoordinator_change_state(rc, RCSTATE_POSTRACE);
    } else {
        int32_t pause_loop = WorldLoopCounter;
        ReplaySnapshot *snap =
            replaymgr_find_snapshot(rm, RC_REPLAY_INSTANT_REPLAY, 0);
        int32_t from_loop = snap != NULL ? snap->BeginLoopTime : -1;
        // v53: take and restore separately, so a failed activation says
        // which half failed (take=0 means THE_PRESENT could not be
        // saved, restore=0 means the INSTANT_REPLAY snapshot is gone).
        int32_t take_ok =
            replaymgr_take_snapshot(rm, RC_REPLAY_THE_PRESENT) != NULL;
        int32_t restore_ok = take_ok
            && replaymgr_restore_snapshot_named(rm, RC_REPLAY_INSTANT_REPLAY);
        int32_t ok = take_ok && restore_ok;
        replay_diag_start(rm, pause_loop, from_loop, ok);
        if (!ok) {
            char buf[128];
            int m = snprintf(buf, sizeof(buf),
                "DIAG replay: activate fail take=%d restore=%d snaps=%d\n",
                (int)take_ok, (int)restore_ok,
                (int)rm->NumInstantReplaySnapshots);
            if (m > 0) {
                logwriter_put(buf, (size_t)m);
                logwriter_drain();
            }
            racecoordinator_change_state(rc, RCSTATE_PAUSE);
            return;
        }
    }
    make_screen(rc);
}

// ---------------------------------------------------- RCSTATE_INSTANT_REPLAY

int32_t rc_state_instant_replay_callback(RaceCoordinator *rc, RCMessage msg,
                                         int32_t param)
{
    int32_t i;

    switch (msg) {
    case RC_ACTIVATE:
        instant_replay_activate(rc);
        return 1;
    case RC_UNACTIVATE:
        if (rc->pReplayScreen != NULL)
            replayscreen_destroy(rc->pReplayScreen, 3);
        rc->pReplayScreen = NULL;
        mute(1);
        for (i = 0; i < PlayerNumPlayers; i++)
            player_enable_camera_movers(PlayerPlayersByIndex[i]);
        if (gRestoreTypeChangedHandlersFn != NULL)
            gRestoreTypeChangedHandlersFn();
        return 1;
    case RC_TICK:
        if (rc->pReplayScreen != NULL)
            replayscreen_tick(rc->pReplayScreen);
        return 1;
    case RC_SKIP_REPLAY:
        // DIAG (v48): why the replay ended. outofstep=1 means the world's
        // checksum missed (worldstep.c's DIAG replay: miss line, just
        // before this, has the loop and the bytes); outofstep=0 means the
        // user skipped, or the replay reached its end and was skipped.
        {
            char buf[128];
            int m = snprintf(buf, sizeof(buf),
                "DIAG replay: skip loop=%d outofstep=%d endgame=%d\n",
                (int)WorldLoopCounter,
                (int)pReplayManager->OutOfStepFlag,
                (int)(rc->pEndGameScreens != NULL));
            if (m > 0) {
                logwriter_put(buf, (size_t)m);
                logwriter_drain();
            }
        }
        if (rc->pEndGameScreens != NULL) {
            racecoordinator_change_state(rc, RCSTATE_POSTRACE);
            return 1;
        }
        replaymgr_restore_snapshot_named(pReplayManager,
                                         RC_REPLAY_THE_PRESENT);
        replaymgr_delete_snapshot_named(pReplayManager,
                                        RC_REPLAY_THE_PRESENT);
        // v53: let the replay open again. OutOfStepFlag is the last
        // attempt's divergence; once the replay is over it only hides
        // Tekrari Izle (replay_mode checks it), and the divergence is
        // event-like (a retry may play further), so clear it here --
        // the next attempt sets it again if it diverges.
        pReplayManager->OutOfStepFlag = 0;
        racecoordinator_change_state(rc, RCSTATE_PAUSE);
        return 1;
    case RC_TAKE_AUTOSNAPSHOT:
        return 1;
    case RC_MOVE_REPLAY:
        // The screen's marker, its snapshot posted as the param. Retail
        // makes the camera again with no screen test.
        replaymgr_restore_snapshot(pReplayManager,
                                   (ReplaySnapshot *)(intptr_t)param);
        if (rc->pReplayScreen != NULL)
            replayscreen_change_camera_mover(rc->pReplayScreen);
        return 1;
    default:
        return 0;
    }
}
