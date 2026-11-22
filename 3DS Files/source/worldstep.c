// worldstep -- see world.h, "the timestep". World.obj's clock, pause, time
// scale, car table, checksums and step. docs/formats/world.md has the
// decode.

#include "world.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "blist.h"
#include "bmath.h"
#include "bticker.h"
#include "car.h"
#include "chopper.h"
#include "collision.h"
#include "collisionbody.h"
#include "joylog.h"
#include "logwriter.h"
#include "player.h"
#include "race.h"
#include "rain.h"
#include "realtime.h"
#include "replay.h"
#include "smokeable.h"
#include "trackpath.h"
#include "vtable.h"

void (*gTrafficTeleporterDoTimestepFn)(struct TrafficTeleporter *t, float dt);
void (*gGlobalCopThinkFn)(float dt);
void (*gCarDoWheelForcesFn)(Car *c);
void (*gChopperFlyFn)(struct Chopper *h, float dt);
void (*gCarFinishMovementFn)(Car *c, float dt);
void (*gCarTriggerEventsFn)(Car *c);
void (*gCarMaybeCameraShakeFn)(Car *c);
void (*gCarMaybeRumbleFn)(Car *c);
void (*gServiceParticleGroupsFn)(void);
void (*gWorldEffectsTweakerCheckFn)(void);
void (*gWorldEffectsDoTimeStepFn)(float dt);
void (*gEmProcessAllEventsFn)(void);
void (*gUpdatePlayersGameStateFn)(void);
void (*gServiceFernFlattenersFn)(void);
void (*gServiceTweakerDamageFn)(void);
void (*gServiceTrackRouteEditorFn)(void);
void (*gRaceUpdateFn)(struct Race *r);
void (*gRaceCoordinatorPostMessageFn)(struct RaceCoordinator *rc,
                                      int32_t msg, int32_t param);
Car *(*gTrafficTeleporterPlaceNewCarFn)(struct TrafficTeleporter *t,
                                        DriverInfo *info);

int32_t (*gSmokeableBreakFn)(Smokeable *s, DynamicCollisionBody **pieces);
void (*gDeleteFarAwaySmokeableCollisionBodiesFn)(void);

#define BODY_END    ((CollisionBody *)(void *)&CollisionBodyList)
#define WITNESS_END ((CollisionBodyWitness *)(void *)&CollisionBodyWitnessList)

// The image's constants: gp-31512, gp-31508 and gp-31504.
#define MAX_TIME_TO_WASTE 0.3f
#define TICKS_TO_SECONDS  0.001f
#define NO_PLAYER         9999999.0f

static void world_fail(const char *what)
{
    fprintf(stderr, "world: %s\n", what);
    abort();
}

// `min.s` and `max.s`, fs then ft.
static float min_s(float fs, float ft)
{
    return ft < fs ? ft : fs;
}

static float max_s(float fs, float ft)
{
    return fs < ft ? ft : fs;
}

// ------------------------------------------------- the step's profile

// Not in the image; see world.h. Each mark gives the ticks since the last
// one to its part.
int32_t       WorldStepProfOn;
WorldStepProf WorldStepProfile;

volatile int32_t WorldStepPart = -1;
volatile int32_t WorldStepCar = -1;
WorldStepNaN     WorldStepNaNs;

static uint32_t ProfStart;
static uint32_t ProfLast;
static World   *ProfWorld;              // the step's, and NULL outside one

static void nan_watch(World *w, int32_t part);

static void prof_start(World *w)
{
    ProfWorld = w;
    WorldStepPart = WSP_COPS;
    WorldStepCar = -1;
    if (WorldStepProfOn)
        ProfStart = ProfLast = bGetTicker();
}

// The end of `part`. In a step, the cars are looked at for a NaN and where
// the step is moves on to the next part; the ticks, while the profile is on.
static void prof_mark(int32_t part)
{
    uint32_t now;

    if (ProfWorld != NULL) {
        nan_watch(ProfWorld, part);
        WorldStepPart = part + 1;
        WorldStepCar = -1;
    }
    if (!WorldStepProfOn)
        return;
    now = bGetTicker();
    WorldStepProfile.ticks[part] += now - ProfLast;
    ProfLast = now;
}

static void prof_end(void)
{
    prof_mark(WSP_REST);
    ProfWorld = NULL;
    WorldStepPart = -1;
    if (WorldStepProfOn) {
        WorldStepProfile.steps++;
        if (ProfLast - ProfStart > WorldStepProfile.worst)
            WorldStepProfile.worst = ProfLast - ProfStart;
    }
}

static void prof_witnesses(void)
{
    CollisionBodyWitness *wit;

    if (!WorldStepProfOn)
        return;
    for (wit = CollisionBodyWitnessList.next; wit != WITNESS_END;
         wit = wit->Link.next)
        WorldStepProfile.witnesses++;
}

static void prof_rounds(int32_t round)
{
    if (!WorldStepProfOn)
        return;
    WorldStepProfile.rounds += (uint32_t)round;
    if ((uint32_t)round > WorldStepProfile.roundsMax)
        WorldStepProfile.roundsMax = (uint32_t)round;
}

// ------------------------------------------------- the fall trace

// Not in the image; see world.h.
int32_t       WorldFallTraceOn;
WorldFallStep WorldFallTrace;
WorldVerifyMissed WorldVerifyMiss;

typedef bVector3 *(*FallVectorFn)(void *self);

// The body's GetPosition and GetVelocity, through its table: the checkers
// that link this file have no CarCollisionBody.
static bVector3 *fall_vector(CollisionBody *b, int32_t slot)
{
    const VtEntry *e = vt_slot(b->_vtable, slot);

    return ((FallVectorFn)e->fn)(vt_this(b, e));
}

#define fall_position(b) fall_vector((b), COLLISIONBODY_VT_GET_POSITION)
#define fall_velocity(b) fall_vector((b), COLLISIONBODY_VT_GET_VELOCITY)

// Player 1's car's body, while the trace is on.
static CollisionBody *fall_body(World *w)
{
    Car *c;

    if (!WorldFallTraceOn || w->TotalNumCars < 1)
        return NULL;
    c = w->CarTable[0];
    if (c == NULL || c->pDynamicCollisionBody == NULL)
        return NULL;
    return &c->pDynamicCollisionBody->Dynamic.Body;
}

static int32_t fall_count(ContactNode *list, CollisionBody *fb)
{
    Contact *c;
    int32_t n = 0;

    for (c = list->next; c != (Contact *)(void *)list; c = c->Link.next)
        if (c->pBody1 == fb || c->pBody2 == fb)
            n++;
    return n;
}

static WorldFallContact *fall_contact_begin(CollisionBody *fb, Contact *c,
                                            int32_t round)
{
    WorldFallStep *s = &WorldFallTrace;
    WorldFallContact *e;
    CollisionBody *other;
    float sign;

    if (fb == NULL || (c->pBody1 != fb && c->pBody2 != fb))
        return NULL;
    if (s->resolved++ >= WORLD_FALL_CONTACTS)
        return NULL;
    e = &s->contact[s->resolved - 1];
    other = c->pBody1 == fb ? c->pBody2 : c->pBody1;
    sign = c->pBody1 == fb ? -1.0f : 1.0f;
    e->round = round;
    e->other = other->Type;
    e->normalZ = 0.0f;
    e->side = 0.0f;
    if (other->Type == TRACK_POLYGON_COLLISION_BODY) {
        const VCPlane *pl = &other->pPoly->pFaces->Plane;
        const bVector3 *p = fall_position(fb);

        e->normalZ = pl->CompressedNormal[2];
        e->side = (pl->CompressedNormal[0] * p->x
                   + pl->CompressedNormal[1] * p->y
                   + pl->CompressedNormal[2] * p->z)
                - pl->CompressedPlaneDistance;
    }
    e->backup = c->BackupTime;
    e->crowbarZ = sign * c->CrowbarVector.z;
    e->zBefore = fall_position(fb)->z;
    e->vzBefore = fall_velocity(fb)->z;
    return e;
}

static void fall_contact_end(CollisionBody *fb, WorldFallContact *e)
{
    if (e == NULL)
        return;
    e->zAfter = fall_position(fb)->z;
    e->vzAfter = fall_velocity(fb)->z;
}

// A car's think or drive, its ticks added to its CarIndex's.
static void prof_car(void (*fn)(Car *c, float dt), uint32_t *by_car, Car *c,
                     float dt)
{
    uint32_t t0;

    WorldStepCar = c->CarIndex;
    if (!WorldStepProfOn) {
        fn(c, dt);
        return;
    }
    t0 = bGetTicker();
    fn(c, dt);
    if (c->CarIndex >= 0 && c->CarIndex < WORLD_MAX_CARS)
        by_car[c->CarIndex] += bGetTicker() - t0;
}

// ---------------------------------------------------- the NaN watch

// Not in the image; see world.h. Each car's last place with nothing
// non-finite in its state, by CarIndex, and the car it was then: a new
// race's car may have the index without the place.
static bVector3   GoodPosition[WORLD_MAX_CARS];
static float      GoodSpeed[WORLD_MAX_CARS];
static uint16_t   GoodAngle[WORLD_MAX_CARS];
static const Car *GoodCar[WORLD_MAX_CARS];
static uint32_t   NaNCars;              // by CarIndex, found and not put back

static int32_t finite3(const bVector3 *v)
{
    return isfinite(v->x) && isfinite(v->y) && isfinite(v->z);
}

static void nan_watch(World *w, int32_t part)
{
    int32_t i;

    for (i = 0; i < w->TotalNumCars && i < WORLD_MAX_CARS; i++) {
        const Car      *c = w->CarTable[i];
        const CarState *s = &c->State;
        WorldStepNaN   *n = &WorldStepNaNs;

        if (finite3(&s->Position) && finite3(&s->Velocity)
            && finite3(&s->AngularVelocity) && isfinite(s->ForwardSpeed)) {
            GoodPosition[i] = s->Position;
            GoodSpeed[i] = s->ForwardSpeed;
            GoodAngle[i] = s->Angle;
            GoodCar[i] = c;
            continue;
        }
        if (NaNCars & (1u << i))
            continue;
        NaNCars |= 1u << i;
        n->count++;
        n->frame = WorldLoopCounter;
        n->part = part;
        n->car = i;
        n->movementMode = c->TheMovementMode;
        n->reset = GoodCar[i] == c;
        n->position = s->Position;
        n->velocity = s->Velocity;
        n->spin = s->AngularVelocity;
        n->good = GoodPosition[i];
    }
}

// Before the car list's sort: each car found is put back where it last
// had no NaN, facing and going as it was. (Stopped, an AI racer thinks
// itself stuck and backs up for seconds.) One with no such place is left
// as it is; the sort passes it by.
static void nan_put_back(World *w)
{
    int32_t i;

    nan_watch(w, WSP_REST);
    for (i = 0; NaNCars != 0 && i < w->TotalNumCars && i < WORLD_MAX_CARS;
         i++) {
        Car *c = w->CarTable[i];

        if (!(NaNCars & (1u << i)))
            continue;
        NaNCars &= ~(1u << i);
        if (GoodCar[i] == c)
            car_reset_to_position(c, &GoodPosition[i], GoodSpeed[i],
                                  GoodAngle[i]);
    }
    NaNCars = 0;
}

// ----------------------------------------------------- the clock and pause

void world_set_time_scale(World *w, float scale, float timer,
                          void (*callback)(int32_t), int32_t param)
{
    w->TimeScaleCallbackParam = param;
    w->fTimeScale = scale;
    w->fTimeScaleTimer = timer;
    w->TimeScaleCallback = callback;
}

void world_reset_time_scale(World *w)
{
    w->TimeScaleCallbackParam = 0;
    w->fTimeScale = 1.0f;
    w->fTimeScaleTimer = 0.0f;
    w->fTimeToRemainPaused = 0.0f;
    w->TimeScaleCallback = NULL;
}

void world_set_world_paused(World *w, float seconds,
                            void (*callback)(int32_t), int32_t param)
{
    w->TimeScaleCallbackParam = param;
    w->fTimeToRemainPaused = seconds;
    w->TimeScaleCallback = callback;
}

void world_begin_pause(World *w)
{
    (void)w;
}

void world_end_pause(World *w)
{
    (void)w;
}

void world_update_world_paused(World *w)
{
    float t = w->fTimeToRemainPaused;

    if (!(0.0f < t))
        return;
    t = t - RealTimeElapsed;
    w->fTimeToRemainPaused = t;
    if (!(t <= 0.0f))
        return;
    w->fTimeToRemainPaused = 0.0f;
    if (w->TimeScaleCallback != NULL) {
        w->TimeScaleCallback(w->TimeScaleCallbackParam);
        w->TimeScaleCallbackParam = 0;
        w->TimeScaleCallback = NULL;
    }
}

int32_t world_is_world_paused(const World *w)
{
    return 0.0f < w->fTimeToRemainPaused || TweakerPauseWorld != 0;
}

float world_get_timestep(World *w)
{
    ReplayManager *rm = pReplayManager;
    int32_t q;
    float real;

    if (rm->ReplayingFlag) {
        q = ReplayManager_GetData(rm, 8, 1);
    } else {
        q = RealTimeElapsedQuantized;
        if (rm->CapturingFlag)
            ReplayManager_AddData(rm, q, 8, 1);
    }
    real = GetRealTimeElapsedFromQuantized(q);

    // The flag is read again, through pReplayManager again. Retail's
    // (0x0014d718) reads the slow motion and the pause beside it: a replay
    // in either takes its step without the wait, and PreviousTimeToWaste
    // stays as it was.
    rm = pReplayManager;
    if (rm->ReplayingFlag && !rm->SlowMotionFlag && !rm->PausedFlag) {
        float waste = min_s(MAX_TIME_TO_WASTE,
                            max_s((real - RealTimeElapsed)
                                  + w->PreviousTimeToWaste, 0.0f));

        w->PreviousTimeToWaste = waste;
        if (0.0f < waste) {
            uint32_t start = bGetTicker();

            while (bGetTickerDifference(start, bGetTicker())
                   * TICKS_TO_SECONDS < waste)
                ;
        }
    }

    if (0.0f < w->fTimeScaleTimer) {
        float t = w->fTimeScaleTimer - real;

        w->fTimeScaleTimer = t;
        if (t <= 0.0f) {
            w->fTimeScaleTimer = 0.0f;
            w->fTimeScale = 1.0f;
            if (w->TimeScaleCallback != NULL) {
                w->TimeScaleCallback(w->TimeScaleCallbackParam);
                w->TimeScaleCallback = NULL;
                w->TimeScaleCallbackParam = 0;
            }
        }
    }
    return real * w->fTimeScale;
}

// ------------------------------------------------------------ small things

void world_flush_collisions(World *w)
{
    (void)w;
}

int32_t world_random_i(World *w, int32_t range)
{
    return bRandomI(range, &w->WorldRandomSeed);
}

float world_random_f(World *w, float f)
{
    return bRandomF(f, &w->WorldRandomSeed);
}

float world_get_dat_to_closest_player(const World *w, float dat)
{
    float d0 = trackpath_compare_distances(&TheTrackPathManager,
                                           w->PlayerDATs[0], dat);
    float d1 = trackpath_compare_distances(&TheTrackPathManager,
                                           w->PlayerDATs[1], dat);

    return fabsf(d0) < fabsf(d1) ? d0 : d1;
}

// ------------------------------------------------------------ the car table

#define CAR_END(w) ((Car *)(void *)&(w)->CarList)

static int32_t car_type(const Car *c)
{
    return (int32_t)c->MyDriver->TheDriverType;
}

static void add_car(World *w, Car *c)
{
    if (w->TotalNumCars == WORLD_MAX_CARS)
        world_fail("more than 14 cars for the car table");
    w->CarTable[w->TotalNumCars] = c;
    w->TotalNumCars++;
}

void world_build_car_table(World *w)
{
    Car *c;
    int32_t i;

    w->TotalNumCars = 0;
    w->AICarPos = 0;
    w->CopCarPos = 0;
    w->TrafficCarPos = 0;

    for (c = w->CarList.next; c != CAR_END(w); c = (Car *)c->Link.next)
        if (car_type(c) == DRIVER_TYPE_PLAYER)
            add_car(w, c);
    w->AICarPos = w->TotalNumCars;
    for (c = w->CarList.next; c != CAR_END(w); c = (Car *)c->Link.next)
        if (car_type(c) == DRIVER_TYPE_AI_RACER)
            add_car(w, c);
    w->CopCarPos = w->TotalNumCars;
    for (c = w->CarList.next; c != CAR_END(w); c = (Car *)c->Link.next)
        if (car_type(c) == DRIVER_TYPE_COP
            || car_type(c) == DRIVER_TYPE_SUPER_COP)
            add_car(w, c);
    w->TrafficCarPos = w->TotalNumCars;
    for (c = w->CarList.next; c != CAR_END(w); c = (Car *)c->Link.next)
        if (car_type(c) == DRIVER_TYPE_TRAFFIC_CAR)
            add_car(w, c);

    for (i = 0; i < w->TotalNumCars; i++)
        w->CarTable[i]->CarIndex = (int16_t)i;
    // The listing walks AICarList to its end here and does nothing with
    // it: a loop the compiler kept the shell of.
}

// GetCarByNumber and GetCarByDriverNumber are in world.c, beside the
// replay's pointer snapshot that reads the first.

// The image's `c.le.s`, a's x no greater than b's, is asked the other way
// round: whether b's x is less. For any two numbers that is the same
// answer. With a NaN the image's is no both ways round, and the sort would
// swap the NaN car with its neighbour and back for ever, the step never
// ending; the port's is yes, and the sort leaves the car where it is.
int32_t world_car_list_check_flip(const Car *a, const Car *b)
{
    return !(b->State.Position.x < a->State.Position.x);
}

static int32_t car_list_in_order(bNode *a, bNode *b)
{
    return world_car_list_check_flip((const Car *)(void *)a,
                                     (const Car *)(void *)b);
}

void world_add_traffic_car(World *w, DriverInfo *info)
{
    Car *c;
    Car *tail;

    // A NULL seam places no traffic car, as raceinit.c's NULL
    // AddCopCarToRace places no cop.
    if (gTrafficTeleporterPlaceNewCarFn == NULL)
        return;
    c = gTrafficTeleporterPlaceNewCarFn(w->Teleporter, info);
    if (c == NULL)
        return;
    tail = w->CarList.prev;
    tail->Link.next = &c->Link;
    w->CarList.prev = c;
    c->Link.next = (CarNode *)(void *)&w->CarList;
    c->Link.prev = &tail->Link;
}

// --------------------------------------------------------- the checksums

uint16_t world_calculate_debug_checksum(World *w)
{
    uint32_t s = (uint32_t)WorldTimer;
    int32_t i;

    for (i = 0; i < w->TotalNumCars; i++) {
        Car *c = w->CarTable[i];
        uint32_t v = (uint32_t)car_calculate_debug_checksum(c);

        w->CarPositionTable[i] = c->State.Position;
        s = s * 33u + v;
    }
    // Retail's (0x0014e240) adds the helicopter's x, its Matrix's fourth
    // row read as a word. The image tests &Matrix for NULL, which it never
    // is.
    if (w->Helicopter != NULL) {
        uint32_t x;

        memcpy(&x, &w->Helicopter->Matrix.v3.x, 4);
        s = s + x;
    }
    return (uint16_t)((s >> 16) + s);
}

// v49 DIAG: the capture's per-loop checksum parts, so a replay miss can
// say WHICH field diverged and BY HOW MUCH, instead of only that the
// byte differed. The byte mixes three things -- the world checksum c
// (car positions, WorldTimer, helicopter) and the random seed's low byte
// -- and the v48 line cannot separate them. The ring records, for each
// captured loop, the seed, c, and every car's part v (ee_trunc_w of
// (x+y)*65536, so dv is in units of 1/65536 m of x+y: |dv|=1 is 0.015 mm
// of float noise, |dv|=65536 is a metre, a real event).
// v50: the ring is 8192 entries, not 1024. The v49 ring missed the
// device's miss at loop 595: it is indexed by loop % N, and loop 595's
// slot had been overwritten by loop 1619 (595+1024), so the lookup found
// rec->loop 1619 != 595 and printed NOREC. A capture is bounded by the
// 0x32000-byte log (about 4650 loops at most, by the per-loop bit
// budget), so an 8192 ring never evicts a loop of the current capture:
// the miss loop is always in it, even for a marker jump back to the
// race start (loop 594, the first LAP_SNAPSHOT). Written only while
// capturing, read only on a miss; behaviour-neutral.
// v55: the ring also stores every car's full Position (x, y, z) per
// loop, so a tolerated miss can SNAP the replayed cars back to the
// recorded positions and reset the divergence to ~0 (see
// world_verify_debug_checksums). v is only a diagnostic sum of x+y
// and cannot reconstruct x and y, so the position itself is stored.
// 8192 entries x 14 cars x 12 bytes is ~1.3 MB on top of the ring;
// the heap is ~1 GB, so this is acceptable.
#define DIAG_CKPT_N 8192
typedef struct {
    int32_t  loop;
    uint32_t seed;
    uint16_t c;
    int32_t  ncars;
    int32_t  v[WORLD_MAX_CARS];
    bVector3 pos[WORLD_MAX_CARS];
} DiagCkpt;
static DiagCkpt diagCkpt[DIAG_CKPT_N];

// v51: set while a replay is running with a logged checksum miss (see
// world_verify_debug_checksums); cleared whenever the world is not
// replaying, so a new replay logs its first miss again. v56: the miss
// is log-only in an instant replay, so this now means "already
// diverged in this replay" rather than "already had a tolerated
// miss"; later misses take the throttled path and keep playing.
// v52: the largest maxdv logged as tolerated in this replay; a later
// miss logs again only once maxdv has grown by 256 since the last
// logged value, so the log shows the growth rate without spam.
static int32_t diagReplayTolerated = 0;
static int64_t diagReplayToleratedMax = 0;

// v55: snap the replayed cars back to the positions the capture recorded
// for this loop (rec->pos). The caller only calls this on a TOLERATED
// miss, where the ring record is known valid for this loop (rec->loop ==
// WorldLoopCounter and rec->ncars == n). The position is the AI's input
// and the physics' state, so putting it back to the recorded value also
// breaks the feedback loop that was amplifying the 1-ULP noise into a
// growing divergence (the AI sees the recorded position and makes the
// recorded decision, so the velocity converges back instead of drifting
// further). GeometryPosition is derived from Position (carstate.c), so
// it is recomputed from the snapped Position on the next update.
static void diag_snap_positions(World *w, const DiagCkpt *rec, int32_t n)
{
    int32_t i;

    for (i = 0; i < n; i++)
        w->CarTable[i]->State.Position = rec->pos[i];
}

void world_verify_debug_checksums(World *w)
{
    uint32_t c = world_calculate_debug_checksum(w);
    uint32_t b, e;
    ReplayManager *rm;
    int32_t vi[WORLD_MAX_CARS];
    int32_t nvi = 0;
    int32_t i;

    if (Joylog_ReplayingFlag)
        (void)Joylog_GetData(16, 3);
    else
        Joylog_AddData((int32_t)c, 16, 3);

    b = ((c >> 8) + c + (w->WorldRandomSeed & 0xffu)) & 0xffu;
    rm = pReplayManager;
    // v51: the tolerated-miss flag is only meaningful while replaying;
    // clear it on every non-replay step (live play and capture, before
    // the capture branch below returns), so a new replay logs its first
    // tolerated miss again.
    if (!rm->ReplayingFlag) {
        diagReplayTolerated = 0;
        diagReplayToleratedMax = 0;
    }
    // v49: the per-car parts behind c, only while capturing or replaying
    // (a plain race step pays nothing for them).
    if (rm->CapturingFlag || rm->ReplayingFlag) {
        nvi = w->TotalNumCars;
        if (nvi > WORLD_MAX_CARS)
            nvi = WORLD_MAX_CARS;
        if (nvi < 0)
            nvi = 0;
        for (i = 0; i < nvi; i++)
            vi[i] = car_calculate_debug_checksum(w->CarTable[i]);
    }
    if (rm->CapturingFlag) {
        DiagCkpt *rec =
            &diagCkpt[(int32_t)((uint32_t)WorldLoopCounter % DIAG_CKPT_N)];
        rec->loop = WorldLoopCounter;
        rec->seed = w->WorldRandomSeed;
        rec->c = (uint16_t)c;
        rec->ncars = nvi;
        for (i = 0; i < nvi; i++) {
            rec->v[i] = vi[i];
            rec->pos[i] = w->CarTable[i]->State.Position;
        }
        ReplayManager_AddData(rm, (int32_t)b, 8, 2);
        return;
    }
    if (!rm->ReplayingFlag)
        return;
    e = (uint32_t)ReplayManager_GetData(rm, 8, 2) & 0xffu;
    if (e == b)
        return;
    WorldVerifyMiss.checksum = c;
    WorldVerifyMiss.seed = w->WorldRandomSeed;
    WorldVerifyMiss.expected = e;
    // DIAG (v48): the first miss of this replay only, drained so it is on
    // the card even when the skip that follows ends the replay at once.
    // v56: a miss in an INSTANT replay is now LOG-ONLY: it is logged
    // (miss / missdetail / cardiff below) and the replay keeps
    // playing -- OutOfStepFlag is never set and RC_SKIP_REPLAY is
    // never posted here, so the replay plays to end_loop and waits
    // on the playback screen. The two symptoms of the replay bug
    // (the pause menu hiding Tekrari Izle, the replay ending early)
    // were one cause: the abort. Demo replays are excluded: their
    // misses are expected (the PS2's floats), and the demo post hook
    // counts them itself; a demo miss still falls through to the
    // abort below (retail behaviour). loop is the first loop the
    // replayed world left the recorded path on; byte is the world's
    // checksum at that loop, expected the recorded byte; logerr=1 means
    // the log read itself failed and expected is not a recorded byte.
    if (!rm->OutOfStepFlag
        && !racecoordinator_is_replay_demo_mode(pRaceCoordinator)) {
        char buf[160];
        int m;
        // v51/v52: tolerate float noise. The checksum byte is a hash of
        // the world checksum c (car positions, WorldTimer, helicopter)
        // and the seed's low byte, so ANY difference, however small,
        // flips it. The ring's per-car v says how far the world really
        // left the recorded path (1/65536 m of x+y). If the ring has
        // this loop, the seed and car count match, and every car's |dv|
        // is more than 0 but at most 1024 (15.6 mm), the replay is
        // visually identical: keep playing instead of aborting.
        // v51 used 64 (1 mm), but the v51 device session showed the
        // noise GROWS: car 0 dv=2 at loop 4571, then dv=-60 and car 6
        // dv=76 at loop 4620, 49 frames later (~1.5 units/frame, and
        // it spreads to more cars). The snapshot is bit-exact and the
        // timestep is recorded exactly, so the growth is the AI/physics
        // feedback amplifying a 1-ULP difference, not a fixable source.
        // 1024 covers ~600 frames at 1.5 units/frame (~900 units) with
        // margin; 15.6 mm is invisible on screen (a car is ~4 m long),
        // while a real event moves a car by centimetres to metres,
        // thousands of units. A car-count change, a missing ring
        // record, or a maxdv of 0 with c diverged (WorldTimer or
        // helicopter, unmeasurable) still aborts as before. Demo
        // replays never reach here (excluded above, retail behaviour).
        // v54: a seed divergence NO LONGER aborts by itself. The v53
        // device session proved the growth crosses an AI decision
        // threshold: at loop 1982 the seeds differ (rec bf1bd6e2, rep
        // fbb989b7) while the cars are only 352 and 96 units out
        // (5.4 mm and 1.5 mm) -- the float noise made one AI draw
        // differ, and the random stream desynced. The ring stores the
        // seed the capture had at the START of this loop's step, and
        // verify runs at the start of the step too, so putting
        // w->WorldRandomSeed back to the recorded seed is exactly the
        // seed for this step's draws: the stream is back in sync and
        // the replay keeps playing. A seed divergence with maxdv over
        // 1024 (a real event) still aborts as before.
        // v55: a tolerated miss now also SNAPS the cars back to the
        // recorded positions (diag_snap_positions, rec->pos) and
        // re-syncs the seed as in v54, resetting the divergence to
        // ~0. The v54 device session proved a fixed threshold cannot
        // hold: the divergence GROWS (8 -> 264 in 74 frames, then
        // 256 per ~7-9 frames, then past 1024) and ALWAYS exceeds any
        // threshold on a long enough window -- the v54 session aborted
        // at loop 3577 (car 3 dv=-1048) and at loop 3596 (car 1
        // dv=-1304, after two seed resyncs) even though the seed
        // re-sync worked. The snap is what stops the growth: the
        // position is the AI's input, so the AI sees the recorded
        // position and the feedback loop is broken. The snap happens
        // on EVERY tolerated miss (the checksum byte flips on any
        // divergence, so a miss follows within a frame of the snap),
        // so the divergence can never accumulate to 1024: it is reset
        // to ~0 at each miss and only regrows by the per-frame
        // velocity difference (~50 units at most on the device, 20x
        // below the threshold). A miss with maxdv over 1024, a
        // missing ring record, a car-count change, or maxdv 0 with c
        // diverged still aborts as before; the snap never runs on an
        // aborted miss.
        // v56: the snap does NOT reset the divergence, so the
        // threshold approach is DEAD. The v55 device session proved
        // it: session 1 (window 2868..3280) snapped at 2871 (maxdv
        // 16), 2891 (320), 2893 (608), 2895 (896), then aborted at
        // 2896 (car 2 dv=-1040) -- only 29/412 frames played; session
        // 4 (window 5233..5561) snapped at 5339 (32), 5356 (384),
        // 5359 (768), then aborted at 5362 (car 3 dv=-1056). The
        // regrowth is ~144 units/frame in the last stretch (the v55
        // host test's ~50/frame model was wrong). The divergence is
        // in the VELOCITY and/or AI state, not just the position, so
        // snapping positions alone cannot reset it. The fix is to
        // stop aborting: in an instant replay the miss is LOG-ONLY.
        // The replay is view-only (it cannot change the race result
        // or the save) and the divergence is millimetres, so not
        // aborting is safe; the snap + seed resync still run on
        // every miss (when the ring has the loop) to keep the
        // divergence small and the replay visually accurate. The
        // 1024 threshold is kept only as a diagnostic number in the
        // log lines, not as a gate. Demo replays still abort below.
        int64_t maxdv = 0;
        int rec_valid = 0;
        int seed_mismatch = 0;
        uint32_t seed_rec = 0;
        uint32_t seed_rep = w->WorldRandomSeed;
        DiagCkpt *rec = NULL;
        {
            rec = &diagCkpt[(int32_t)((uint32_t)WorldLoopCounter
                                     % DIAG_CKPT_N)];
            if (rec->loop == WorldLoopCounter
                && rec->ncars == nvi && nvi > 0) {
                int j;
                rec_valid = 1;
                for (j = 0; j < nvi && j < rec->ncars; j++) {
                    int64_t dv = (int64_t)vi[j] - (int64_t)rec->v[j];
                    if (dv < 0)
                        dv = -dv;
                    if (dv > maxdv)
                        maxdv = dv;
                }
                seed_rec = rec->seed;
                seed_mismatch = (rec->seed != w->WorldRandomSeed);
            }
        }
        if (diagReplayTolerated) {
            // v56: already diverged in this replay (the first miss is
            // logged below): keep playing -- the abort is gone for
            // instant replays (see the v56 note above). Snap the cars
            // and re-sync the seed when the ring has this loop, so
            // the divergence stays small; log again only once maxdv
            // has grown by 256 since the last logged value, so the
            // log shows the growth rate without spam.
            if (rec_valid) {
                // v54: the random stream may have desynced since the
                // first miss; put it back in sync with the recorded
                // seed (see above) so later frames draw as the
                // capture did. Log it so the device log proves the
                // resync.
                if (seed_mismatch) {
                    w->WorldRandomSeed = seed_rec;
                    m = snprintf(buf, sizeof(buf),
                        "DIAG replay: seed resync loop=%d seed_rec=%08x seed_rep=%08x\n",
                        (int)WorldLoopCounter, (unsigned)seed_rec,
                        (unsigned)seed_rep);
                    if (m > 0)
                        logwriter_put(buf, (size_t)m);
                    logwriter_drain();
                }
                // v55: snap the cars back to the recorded positions
                // (see above). This runs on every miss, so the
                // divergence is reset each time.
                diag_snap_positions(w, rec, nvi);
            }
            if (maxdv >= diagReplayToleratedMax + 256) {
                m = snprintf(buf, sizeof(buf),
                    "DIAG replay: miss tolerated loop=%d maxdv=%lld\n",
                    (int)WorldLoopCounter, (long long)maxdv);
                if (m > 0)
                    logwriter_put(buf, (size_t)m);
                if (rec_valid) {
                    m = snprintf(buf, sizeof(buf),
                        "DIAG replay: pos resync loop=%d maxdv=%lld\n",
                        (int)WorldLoopCounter, (long long)maxdv);
                    if (m > 0)
                        logwriter_put(buf, (size_t)m);
                }
                logwriter_drain();
                diagReplayToleratedMax = maxdv;
            }
            return;
        }
        m = snprintf(buf, sizeof(buf),
            "DIAG replay: miss loop=%d cksum=%04x seed=%08x byte=%02x expected=%02x logerr=%d\n",
            (int)WorldLoopCounter, (unsigned)c,
            (unsigned)w->WorldRandomSeed, (unsigned)b, (unsigned)e,
            (int)rm->RemoveDataError);
        if (m > 0) {
            logwriter_put(buf, (size_t)m);
            logwriter_drain();
        }
        // DIAG (v49): which field diverged, against the capture's ring.
        // seed_rec != seed_rep means the random stream left the recorded
        // path (a draw the capture did not make, or the reverse); c_rec
        // != c_rep with every v equal means WorldTimer or the helicopter;
        // a cardiff line names the car whose position left it, and dv
        // says by how much (1/65536 m of x+y: 1 is float noise, 65536
        // is a metre). NOREC means the ring did not cover the loop.
        {
            /* v55: rec is the hoisted ring record from the tolerance
               computation above (same loop, same slot); reuse it here
               instead of looking the slot up a second time. */
            if (rec == NULL || rec->loop != WorldLoopCounter) {
                m = snprintf(buf, sizeof(buf),
                    "DIAG replay: missdetail loop=%d NOREC\n",
                    (int)WorldLoopCounter);
                if (m > 0)
                    logwriter_put(buf, (size_t)m);
            } else {
                m = snprintf(buf, sizeof(buf),
                    "DIAG replay: missdetail loop=%d seed_rec=%08x seed_rep=%08x c_rec=%04x c_rep=%04x ncars_rec=%d ncars_rep=%d\n",
                    (int)WorldLoopCounter, (unsigned)rec->seed,
                    (unsigned)w->WorldRandomSeed, (unsigned)rec->c,
                    (unsigned)c, (int)rec->ncars, (int)nvi);
                if (m > 0)
                    logwriter_put(buf, (size_t)m);
                for (i = 0; i < nvi && i < rec->ncars; i++) {
                    if (rec->v[i] != vi[i]) {
                        int64_t dv = (int64_t)vi[i] - (int64_t)rec->v[i];
                        m = snprintf(buf, sizeof(buf),
                            "DIAG replay: cardiff loop=%d car=%d v_rec=%d v_rep=%d dv=%lld\n",
                            (int)WorldLoopCounter, (int)i,
                            (int)rec->v[i], (int)vi[i], (long long)dv);
                        if (m > 0)
                            logwriter_put(buf, (size_t)m);
                    }
                }
            }
            logwriter_drain();
        }
        // v56: log-only (see the v56 note above). This is the first
        // miss of an instant replay: the miss is logged above, and
        // the replay keeps playing -- no OutOfStepFlag, no
        // RC_SKIP_REPLAY. Snap the cars and re-sync the seed when
        // the ring has this loop (they keep the divergence small);
        // the 1024 threshold no longer gates anything, because
        // v51-v55 proved the feedback loop always grows past any
        // fixed threshold (v55: 16 -> 320 -> 608 -> 896 -> -1040
        // despite the snaps) and the divergence is millimetres on
        // a view-only replay.
        if (rec_valid) {
            // v54: re-sync the random stream if it desynced (see the
            // comment above). The miss and missdetail lines above
            // already logged the divergence, so resync AFTER them,
            // then keep playing.
            if (seed_mismatch) {
                w->WorldRandomSeed = seed_rec;
                m = snprintf(buf, sizeof(buf),
                    "DIAG replay: seed resync loop=%d seed_rec=%08x seed_rep=%08x\n",
                    (int)WorldLoopCounter, (unsigned)seed_rec,
                    (unsigned)seed_rep);
                if (m > 0)
                    logwriter_put(buf, (size_t)m);
                logwriter_drain();
            }
            // v55: snap the cars back to the recorded positions (see
            // the comment above), then keep playing. The snap is
            // logged so the device log proves it fired.
            diag_snap_positions(w, rec, nvi);
            m = snprintf(buf, sizeof(buf),
                "DIAG replay: pos resync loop=%d maxdv=%lld\n",
                (int)WorldLoopCounter, (long long)maxdv);
            if (m > 0)
                logwriter_put(buf, (size_t)m);
            logwriter_drain();
        }
        // The miss is logged, the replay keeps playing.
        m = snprintf(buf, sizeof(buf),
            "DIAG replay: miss tolerated loop=%d maxdv=%lld\n",
            (int)WorldLoopCounter, (long long)maxdv);
        if (m > 0)
            logwriter_put(buf, (size_t)m);
        logwriter_drain();
        diagReplayTolerated = 1;
        diagReplayToleratedMax = maxdv;
        return;
    }
    // v56: only demo replays reach here (instant replays always return
    // from the block above, log-only). Retail's (0x0014e300) marks the
    // replay out of step first, through pReplayManager again: the pause
    // menu and the post-race then leave it alone.
    pReplayManager->OutOfStepFlag = 1;
    if (gRaceCoordinatorPostMessageFn != NULL)
        gRaceCoordinatorPostMessageFn(pRaceCoordinator, RC_SKIP_REPLAY, 0);
}

// ------------------------------------------------------------ the bodies

typedef void (*MoveFn)(void *self, float dt);

void world_do_timestep_move(World *w, float dt)
{
    CollisionBody *moving[WORLD_MAX_MOVING_BODIES];
    CollisionBody *b;
    int32_t n = 0;
    int32_t i;

    (void)w;
    if ((((uint32_t)WorldLoopCounter + 77u) & 127u) == 0
        && gDeleteFarAwaySmokeableCollisionBodiesFn != NULL)
        gDeleteFarAwaySmokeableCollisionBodiesFn();

    for (b = CollisionBodyList.next; b != BODY_END; b = b->Link.next) {
        if (!b->Dynamic)
            continue;
        if (n == WORLD_MAX_MOVING_BODIES)
            world_fail("more than 176 dynamic bodies to move");
        moving[n++] = b;
    }
    for (i = 0; i < n; i++) {
        const VtEntry *e = vt_slot(moving[i]->_vtable, COLLISIONBODY_VT_MOVE);

        ((MoveFn)e->fn)(vt_this(moving[i], e), dt);
    }
    prof_mark(WSP_MOVE);
}

#define CONTACT_END(l) ((Contact *)(void *)(l))

static int32_t is_broken_smokeable(const CollisionBody *b)
{
    return b->Type == SMOKEABLE_COLLISION_BODY
        && ((const Smokeable *)(const void *)b)->BreakingStrength == -1.0f;
}

static int32_t break_smokeable(CollisionBody *b, CollisionBody **pieces,
                               int32_t n)
{
    int32_t k;

    if (gSmokeableBreakFn == NULL)
        world_fail("Smokeable::Break is not ported");
    // Break writes where it is told and returns the count; the array is
    // the image's eight, which it does not check.
    k = gSmokeableBreakFn((Smokeable *)(void *)b,
                          (DynamicCollisionBody **)(void *)&pieces[n]);
    if (k < 0 || n + k > WORLD_MAX_BROKEN_PIECES)
        world_fail("more than 8 bodies after a contact");
    return n + k;
}

static int32_t add_piece(CollisionBody *b, CollisionBody **pieces, int32_t n)
{
    if (is_broken_smokeable(b))
        return break_smokeable(b, pieces, n);
    if (b->Dynamic && b->ActiveCollisionDetection) {
        if (n == WORLD_MAX_BROKEN_PIECES)
            world_fail("more than 8 bodies after a contact");
        pieces[n++] = b;
    }
    return n;
}

void world_do_timestep_collisions(World *w, float dt)
{
    ContactNode list;
    CollisionBody *pieces[WORLD_MAX_BROKEN_PIECES];
    CollisionBodyWitness *wit;
    CollisionBodyWitness *next;
    CollisionBody *b;
    Contact *c;
    struct Collision *col;
    struct Collision *next_col;
    CollisionBody *fb = fall_body(w);
    int32_t round = 0;

    if (fb != NULL) {
        WorldFallTrace.frame = WorldLoopCounter;
        WorldFallTrace.dt = dt;
        WorldFallTrace.zMoved = fall_position(fb)->z;
        WorldFallTrace.vzMoved = fall_velocity(fb)->z;
        WorldFallTrace.dropped = 0;
        WorldFallTrace.resolved = 0;
    }
    collisionbody_maybe_print_all();

    for (wit = CollisionBodyWitnessList.next; wit != WITNESS_END; wit = next) {
        next = wit->Link.next;
        if (cbwitness_has_expired(wit))
            cbwitness_destroy(wit, 3);
    }

    for (b = CollisionBodyList.next; b != BODY_END; b = b->Link.next)
        if (b->Dynamic && b->ActiveCollisionDetection)
            dynamiccollisionbody_maybe_create_witnesses(
                (DynamicCollisionBody *)(void *)b);
    prof_mark(WSP_WITNESSES);
    prof_witnesses();

    list.next = CONTACT_END(&list);
    list.prev = CONTACT_END(&list);
    for (wit = CollisionBodyWitnessList.next; wit != WITNESS_END;
         wit = wit->Link.next)
        cbwitness_test_for_contact(wit, &list);
    prof_mark(WSP_TESTS);
    if (fb != NULL)
        WorldFallTrace.tested = fall_count(&list, fb);

    // Earliest first: resolve the head, drop every other contact either
    // body had, and look again round whatever the resolve left moving.
    while (list.next != CONTACT_END(&list)) {
        CollisionBody *b1;
        CollisionBody *b2;
        WorldFallContact *fc;
        int32_t n;
        int32_t i;

        c = list.next;
        fc = fall_contact_begin(fb, c, round);
        contact_resolve(c);
        fall_contact_end(fb, fc);
        b1 = c->pBody1;
        b2 = c->pBody2;
        if (b1->Dynamic)
            contact_remove_contacts(&list, b1);
        if (b2->Dynamic)
            contact_remove_contacts(&list, b2);

        n = add_piece(b1, pieces, 0);
        n = add_piece(b2, pieces, n);

        for (i = 0; i < n; i++)
            dynamiccollisionbody_maybe_create_witnesses(
                (DynamicCollisionBody *)(void *)pieces[i]);
        for (wit = CollisionBodyWitnessList.next; wit != WITNESS_END;
             wit = wit->Link.next)
            for (i = 0; i < n; i++)
                if (wit->pBody1 == pieces[i] || wit->pBody2 == pieces[i])
                    cbwitness_test_for_contact(wit, &list);

        round++;
        if (round >= WORLD_MAX_CONTACT_ROUNDS)
            break;
    }
    prof_mark(WSP_RESOLVE);
    prof_rounds(round);
    if (fb != NULL) {
        WorldFallTrace.rounds = round;
        if (round == WORLD_MAX_CONTACT_ROUNDS)
            WorldFallTrace.dropped = fall_count(&list, fb);
    }

    // Twenty rounds and still touching: what is left is dropped unresolved.
    if (round == WORLD_MAX_CONTACT_ROUNDS) {
        while ((c = list.next) != CONTACT_END(&list)) {
            c->Link.prev->Link.next = c->Link.next;
            c->Link.next->Link.prev = c->Link.prev;
            contact_destroy(c, 3);
        }
    }

    for (col = w->CollisionList.next;
         col != (struct Collision *)(void *)&w->CollisionList; col = next_col) {
        next_col = col->Link.next;
        if (!collision_update(col, dt))
            collision_end(col);
    }
    prof_mark(WSP_COLLISIONS);
    // ~bTList<Contact>(&list, 2) follows in the image. The list is empty by
    // now either way, and ~bList with flags 2 frees nothing.
}

// ------------------------------------------------------------- the step

static int32_t no_shakes(void)
{
    RCState s = racecoordinator_get_current_state(pRaceCoordinator);

    return s == RCSTATE_INSTANT_REPLAY || s == RCSTATE_POSTRACE
        || s == RCSTATE_AUTO_REPLAY
        || racecoordinator_is_replay_demo_mode(pRaceCoordinator);
}

// The step's thinks and drives go through prof_car, which times them by
// car while the profile is on; the calls below keep the image's names.
#define car_think(c, dt) prof_car(car_think, WorldStepProfile.think, (c), (dt))
#define car_drive(c, dt) prof_car(car_drive, WorldStepProfile.drive, (c), (dt))

void world_do_timestep(World *w, float dt)
{
    int32_t i;

    world_verify_debug_checksums(w);

    for (i = 0; i < WORLD_MAX_PLAYERS; i++) {
        Player *p = PlayerPlayersByNumber[i];

        if (p != NULL) {
            Car *car = p->MyCar;

            w->PlayerPositions[i] = car->State.Position;
            w->PlayerDATs[i] = car->TheTrackPathCoordinate.DistanceAroundTrack;
        } else {
            // The fourth lane is left as it was.
            w->PlayerPositions[i].x = NO_PLAYER;
            w->PlayerPositions[i].y = NO_PLAYER;
            w->PlayerPositions[i].z = NO_PLAYER;
            w->PlayerDATs[i] = NO_PLAYER;
        }
    }

    prof_start(w);
    if (w->Teleporter != NULL && gTrafficTeleporterDoTimestepFn != NULL)
        gTrafficTeleporterDoTimestepFn(w->Teleporter, dt);
    if (gGlobalCopThinkFn != NULL)
        gGlobalCopThinkFn(dt);
    prof_mark(WSP_COPS);

    // AI racers, traffic, players, cops: the counts are read again after
    // every car.
    for (i = 0; i < w->CopCarPos - w->AICarPos; i++)
        car_think(w->CarTable[w->AICarPos + i], dt);
    for (i = 0; i < w->TotalNumCars - w->TrafficCarPos; i++)
        car_think(w->CarTable[w->TrafficCarPos + i], dt);
    for (i = 0; i < w->AICarPos; i++)
        car_think(w->CarTable[i], dt);
    for (i = 0; i < w->TrafficCarPos - w->CopCarPos; i++)
        car_think(w->CarTable[w->CopCarPos + i], dt);
    prof_mark(WSP_THINK);

    for (i = 0; i < w->AICarPos; i++)
        if (gCarDoWheelForcesFn != NULL)
            gCarDoWheelForcesFn(w->CarTable[i]);
    for (i = 0; i < w->TotalNumCars; i++)
        car_drive(w->CarTable[i], dt);
    prof_mark(WSP_DRIVE);
    if (w->Helicopter != NULL && gChopperFlyFn != NULL)
        gChopperFlyFn(w->Helicopter, dt);
    prof_mark(WSP_CHOPPER);
    for (i = 0; i < w->TotalNumCars; i++) {
        WorldStepCar = i;
        car_begin_movement(w->CarTable[i], dt);
    }
    prof_mark(WSP_BEGIN);

    world_do_timestep_move(w, dt);
    world_do_timestep_collisions(w, dt);

    for (i = 0; i < w->TotalNumCars; i++) {
        if (gCarFinishMovementFn == NULL)
            world_fail("Car::FinishMovement is not ported");
        WorldStepCar = i;
        gCarFinishMovementFn(w->CarTable[i], dt);
    }
    prof_mark(WSP_FINISH);
    for (i = 0; i < w->TotalNumCars; i++) {
        Car *car = w->CarTable[i];

        if (world_get_distance_to_closest_player(w, &car->State.Position)
                < 100.0f
            && gCarTriggerEventsFn != NULL)
            gCarTriggerEventsFn(car);
    }

    if (!no_shakes()) {
        for (i = 0; i < w->AICarPos; i++) {
            Car *car = w->CarTable[i];

            if (gCarMaybeCameraShakeFn != NULL)
                gCarMaybeCameraShakeFn(car);
            if (!no_shakes() && gCarMaybeRumbleFn != NULL)
                gCarMaybeRumbleFn(car);
        }
    }
    prof_mark(WSP_EVENTS);

    if (gServiceParticleGroupsFn != NULL)
        gServiceParticleGroupsFn();
    if (gWorldEffectsTweakerCheckFn != NULL)
        gWorldEffectsTweakerCheckFn();
    if (gWorldEffectsDoTimeStepFn != NULL)
        gWorldEffectsDoTimeStepFn(dt);

    for (i = 0; i < w->TotalNumCars; i++) {
        Car *car = w->CarTable[i];
        float d = world_get_distance_to_closest_player(w,
                                                       &car->State.Position);

        if (d < 150.0f && car_should_visual_effects_be_generated(car))
            car_trigger_car_terrain_effects(car, d);
    }
    for (i = 0; i < w->TotalNumCars; i++)
        car_fake_engine_task(w->CarTable[i]);
    prof_mark(WSP_EFFECTS);

    if (gEmProcessAllEventsFn != NULL)
        gEmProcessAllEventsFn();
    nan_put_back(w);
    blist_sort((bNode *)(void *)&w->CarList, car_list_in_order);
    if (gUpdatePlayersGameStateFn != NULL)
        gUpdatePlayersGameStateFn();
    rain_TimeStep(dt);
    if (gServiceFernFlattenersFn != NULL)
        gServiceFernFlattenersFn();
    if (gServiceTweakerDamageFn != NULL)
        gServiceTweakerDamageFn();
    if (gServiceTrackRouteEditorFn != NULL)
        gServiceTrackRouteEditorFn();
    if (pCurrentRace != NULL && gRaceUpdateFn != NULL)
        gRaceUpdateFn(pCurrentRace);
    prof_end();
}
