# The R36S port

The New 3DS port, built for the R36S (RK3326: 4x Cortex-A35, Mali-G31,
1 GB, 640x480) as a 32-bit (armhf) Linux program for Volkyport. Started
2026-11-06 (HANDOFF.md, "Session 158").

## Where it stands

| Step | State |
|---|---|
| armhf build in Docker (`r36s/Dockerfile.armhf`), every game file compiled with the headers' layout checks on | done, 2026-11-06 |
| Disc files extracted (SLES-50731, the user's ISO) and a card folder made | done, 2026-11-06 |
| Runs on the disc's files, headless under QEMU: boot, the frontend's loads, the language screen, the title and its movie, the main menu, a Cops On race (track loaded, 14 cars, AI, traffic and cops running) for 31,805 world frames, with scripted input and no crash | done, 2026-11-06 |
| aarch64 build | compiles; can't load the disc's records (§1) |
| Drawing (citro3d and citro2d on GLES 2) | done: the menus and races draw on an R36S (dArkOSRE), and in emulated frames (Mesa softpipe) |
| Sound (NDSP mixer on SDL) | written, runs (patches, jukebox, movie sound queued); how it sounds on a device isn't recorded here |
| Input (SDL game controller) | done: the R36S's pad plays the game. Select + R1 swaps the D-pad and left stick, Select + L1 (or L3 or R3) is the frame counter (`r36s/tests/hid_swap_test.sh` passes); the swap isn't tried on a device with no sticks |
| One screen in the PS2's layout (HUD, minimap, blurbs, 4:3 framing) | seen in an emulated race frame |
| Volkyport package and launcher | done, 2026-11-07: `sh r36s/package.sh`; launcher runs on an R36S with dArkOSRE (no Volkyport installed there) and under QEMU with a stand-in `control.txt`; not run with Volkyport installed, or on other firmware |
| Measured on an R36S | `docs/R36S_OPTIMISATION_BACKLOG.md` has each logged run (run 1, 2026-11-07, gave 10 to 22 FPS; later runs and builds are faster). Frame rate varies by track and traffic; no current summary figure is recorded here |

QEMU's frame times mean nothing for the device: the frame pacer holds 60 Hz
and nothing is drawn.

## How it is built

The game's sources are the 3DS build's, unchanged in what they do, compiled
with `__3DS__` still defined plus `NFSHP2_R36S`. What they ask of the 3DS
comes from `r36s/compat`, which declares the parts of libctru, citro3d and
citro2d the port uses and implements them on SDL2, pthreads and GLES 2. The
3DS-only diagnostics and Local Wireless (`r36s/replaced.txt`) have R36S
versions in `r36s/platform` with the same headers.

```
sh r36s/build.sh                    # r36s/build/armhf/nfshp2.armhf
ARCH=aarch64 sh r36s/build.sh       # r36s/build/aarch64/nfshp2.aarch64
sh r36s/build.sh clean
```

`build.sh` runs `r36s/Makefile` in a Debian 10 image (glibc 2.28) with the
cross compiler, the GLES headers and SDL 2.28 for the arch:
`r36s/Dockerfile.armhf` builds `nfshp2-r36s-build-armhf`; for aarch64,
`r36s/Dockerfile` (`nfshp2-r36s-build`) or the Burnout 3 port's
`takedawn-r36s-build:debian10`. Both builds compile with 64-bit file offsets;
on armhf the path wrappers cover the `*64` names glibc then calls.

A headless run under QEMU (Docker Desktop runs ARM binaries), with the
card folder from `tools/make_card.py`:

```
sh r36s/run_qemu.sh <card> [seconds]
NFSHP2_INPUT='300:A*60/90' sh r36s/run_qemu.sh <card> 560
```

`NFSHP2_INPUT` presses keys by input frame (`compat/src/ctru_hid.c`):
pressing A every 90 frames from frame 300 walks from the language screen
into a Cops On race. `DebugLog 1` in the card folder's `nfshp2_config.txt`
turns on the game's log (`nfshp2_debug.txt`); stderr, with any crash
report, goes to `nfshp2_stderr.txt`, which the script prints with the
report's addresses named.

### The Volkyport package

```
sh r36s/package.sh     # r36s/build/volkyport/nfshp2.zip, and the port folder
```

It follows Volkyport-New's layout and launcher template (read from
volkyport.games/packaging.html and the repository's AGENTS.md, 2026-11-07):
`port.json` version 4 (arch `armhf`, `rtr` false, `exp` false, min glibc
2.17), `README.md`, a 640x480 `screenshot.jpg` under 100 KB, `gameinfo.xml`,
the `NFSHP2.sh` launcher (control.txt, `PORT_32BIT=Y`, `get_controls`,
log.txt through tee, the patcher, gptokeyb2 for the Select+Start exit,
`pm_platform_helper`, `pm_finish`) and `nfshp2/` with the stripped binary,
`licenses/` and an empty `gamedata/`.

Volkyport is required (2026-11-15, to Volkyport-New's AGENTS.md: about 60
lines, no clocks, swap, trap or loader checks). Until then the launcher also
ran without Volkyport, with its own pad mapping, library check and
console installer. The R36S pad's mapping by position is still appended
after Volkyport's, which maps that pad by label. The game also quits on
Select+Start held for a second (`compat/src/ctru_hid.c`):
`aptMainLoop` answers false and the game leaves its main loop and shuts
down as the 3DS's does from HOME, with a 5-second backstop that ends the
process. Nothing is bundled: SDL2, GLES and libc are the
system's, as Volkyport asks. The game's data root is `NFSHP2_ROOT`, the
port's `gamedata/`, which holds the NFSHP2 folder from `make_card.py`,
the saves and the game's logs. The sources are in `r36s/volkyport/`; the
zip's permissions are set by `r36s/scripts/make_zip.py`.

The first run installs the game from the player's disc image
(`gamedata/*.iso`), as Volkyport ports that need a console disc do:
`r36s/installer/patchscript` behind Volkyport's patcher screen
(`utils/patcher.txt`, with PATCHER_FILE, PATCHER_GAME and PATCHER_TIME).
It runs
`r36s/installer/nfshp2_install.py`, which reads the ISO 9660 image in
place and writes what make_card.py copies, using the repository's tools'
own code. Its output on this PC was the card's 299 files byte for byte.
It needs the firmware's python3.

For a catalogue submission, copy the assembled
`r36s/build/volkyport/nfshp2/` folder to `ports/nfshp2/` in a fork of
[Volkyport-MV-New](https://github.com/PortsMaster-MV/Volkyport-MV-New).
The adjacent ZIP is the manual install for testers. Keep the disc and
extracted data out of both uploads.

The staged folder passes `tools/build_release.py --do-check` in a sparse
checkout of Volkyport-MV-New. Hardware testing so far is on dArkOSRE on
the R36S at 640×480. Before opening a pull request against the upstream
`main` branch, publish the matching GPL source in the project repository,
then post the ZIP and record the firmware, device and resolution results
in Volkyport's Discord `#testing-n-dev` channel. Include the source
revision and the testing thread in the pull request. See the
[Volkyport packaging guide](https://volkyport.games/contribute/porting/packaging/)
for its firmware and resolution checklist. The state (re-checked 2026-11-14),
the test matrix and drafts of the Discord post and the pull request are in
[R36S_SUBMISSION.md](R36S_SUBMISSION.md).

`sdmc:/X` opens `$NFSHP2_ROOT/X` (else the working directory's `X`), and a
name that does not exist as written is found case blind, as on the 3DS's FAT
card. So the card's `NFSHP2` folder from `tools/make_card.py` goes in the
port's folder as it is.

## Decisions

- **32-bit (armhf), the 3DS's own word size** (2026-11-06; the user's
  first choice was 64-bit, then left the switch to Claude once the 64-bit
  cost was measured). The game overlays the disc's records in place, and
  91 of them hold pointers or a `bNode`, laid out for 4-byte pointers; on
  aarch64 the first run on the disc's files read the car table 8 bytes
  short per car (§1 below has the sizes). On armhf they are the image's
  layouts, which the headers' layout checks, compiled only on 32-bit
  targets, now prove at every build. The A35 runs 32-bit ARM code at full
  speed; what an A64 build gains (more registers, a little speed) is small
  next to what limits this port, the draws (§3). The R36S firmwares ship
  32-bit libraries for Volkyport's armhf ports; which ones this port runs
  on is checked when it is packaged.
- **The aarch64 build is kept** (`ARCH=aarch64`): it compiles and boots to
  the disc's car table, and holds the work below for a later 64-bit port.
- **On aarch64, everything the game points at stays below 4 GB.** The
  image keeps pointers in 32-bit slots, and the 3DS code casts pointers to
  `u32`. So:
  - the executable is linked non-PIE (code and data at 0x400000);
  - the game's malloc, calloc, realloc, memalign and free go to
    `platform/lowheap.c`, a heap in one region reserved between the brk heap
    and 4 GB (the link wraps them; SDL, the GL driver and glibc keep glibc's
    malloc). glibc's own malloc can't be held there: other threads' arenas
    and big blocks are mmap'd high, and under QEMU its brk can't grow at
    all. `r36s/tests/run.sh` stress-tests the heap (random operations with
    a full walk of its chunks after each, and four threads at once);
  - every thread's stack comes from the heap, with a 64 KB guard at its
    foot, the game's main thread too (`main` is wrapped to run on such a
    thread). Both builds do this;
  - `p32.h` is a checked cast on the R36S and aborts on an address that
    does not fit.
- **A crash prints a report:** the signal, pc, lr, the registers and a
  backtrace, on a stack of its own so a stack overflow is reported too
  (`r36s_platform.c`). `r36s/run_qemu.sh <card>` runs the build headless
  under QEMU and names the addresses.
- **On aarch64, objects allocated by a literal size get room to grow.** The
  image allocates some objects by size (`bMalloc(1136,
  "TrackCarCameraMover")`). A 64-bit object is at most twice its 32-bit size,
  so on aarch64 `bMalloc` doubles every block of 8 KB or less (`objsize.h`,
  `bmem.c`). On armhf, the 3DS and the checkers' hosts the literals stand.
- **The world step rounds toward zero, as on the 3DS.** `fpctl.h` reads and
  writes FPSCR on armhf, as on the 3DS, and FPCR on aarch64 (the same
  bits), and every
  thread starts in RunFast (flush-to-zero, default NaN), as libctru starts
  them.
- **One screen, laid out as on the PS2** (the user's, 2026-11-06).
  `screen.h` sets the top screen to 640x480, which the PS2's 640x480
  frame maps onto 1:1, and `SCREEN_ONE` turns the 3DS's two-screen
  changes off:
  - The HUD split (`hudsplit_install`) is not installed. Every HUD
    object draws where the disc puts it: the race position at the top
    left, the live split list, the lap block, the splits, the score and
    Be The Cop's panel.
  - The minimap draws on the one screen where Render puts it, every
    frame after the HUD's layers, as `eDisplayFrame` does (`0x00113c38`).
  - The frontend's scrolling blurb, its white-out bar and the event
    tree's scroller are no longer hidden. The PS2 showed its blurbs
    there; the 3DS moved them to the bottom screen.
  - The bottom screen is still drawn, but shown (`r36s_bottom_popup`)
    only on the port's own screens, as a panel over the main one: the
    controller setup, the replay's keys and the debug menus. Elsewhere,
    and in every race, it is not shown.
  - The 3D view is retail's framing (`SCREEN_PS2_FRAMING`, `trackview.c`'s
    `view_fovy`/`view_aspect`). The 3DS keeps retail's width and crops
    the height to 5:3. Here the height is retail's too, its scissor over
    H times AspectRatioY, on a 4:3 screen as the TV showed it. The sun's
    and the mirror's places on screen follow the same mapping.
- **No Local Wireless.** `wireless_start` answers 0, as a 3DS with its
  wireless switched off does.
- **No software keyboard.** `swkbdInputText` keeps the name the game
  proposes.

## What is left, in order

### 1. Disc records on 64-bit (the aarch64 build only)

The armhf build doesn't need this. It is what a 64-bit build would.

Measured on the aarch64 build (2026-11-06), against the disc's sizes:

| Record | aarch64 | disc |
|---|---|---|
| `CarTypeInfo` | 1384 | 1376 |
| `eSolid` | 240 | 192 |
| `eStripEntry` | 72 | 64 |
| `eTextureTable` | 16 | 8 |
| `eRigidAnimation` | 120 | 112 |

The first run on the disc's files stops at the first of these: `init_fred`
walks `CarTypeInfoArray`, which `loader_car_info` points at the car data
chunk, and with a stride 8 bytes long every car after the first is read
from the wrong place. `bNode`, the list link at the front of `eSolid` and
146 other structs, holds two real pointers, so it moves every field after
it. Of the 130 structs with a 32-bit-only layout check, 91 hold a pointer
or a `bNode`; the disc records among them all need 32-bit slots, and
`bNode`'s 145 `next`/`prev` uses with them.

The game uses some of the disc's records in place: `esolid.c` casts a
chunk's bytes to `eSolid*` and writes pointers into its fields. On the PS2 and
the 3DS those fields are 4 bytes. On aarch64 they are 8, so every field after
the first pointer is read from the wrong offset, and `size /
sizeof(eTextureTable)` counts the wrong number of entries. The port's
layout checks (`#if UINTPTR_MAX == 0xFFFFFFFFu`) are written for 32-bit
targets only, so they don't catch this.

The fix, record by record:
1. Declare each pointer field of a record that lives in disc data as a
   32-bit slot (`uint32_t`, read and written through `p32_ptr`/`p32_of`,
   which are casts on the R36S).
2. Turn its layout check on for every target, so the compiler proves the
   offsets.
3. On the 3DS this changes nothing: a slot holds the same address.

Runtime-only objects (camera movers, cops, menus) may widen. Only the
records the game overlays on loaded bytes must keep the image's layout. The
first known ones are `eSolid`, `eTextureTable`, `eStripEntry`,
`eLightMaterialEntry`, `eRigidAnimation` (`esolid.c`'s loader). Candidates
to check, from the structs under 32-bit-only checks that have pointers and are
cast from loaded data: `TextureInfo`, `TexturePack(Header)`,
`TextureAnimPack(Header)`, `TextureAnim`, `ScenerySectionHeader`,
`SceneryInfo`, `VisibleZone`, `eModel` (scenery's), `eLightPack`,
`vAABBTree`, `EventTriggerPack`, `emEvent`, `AcidEmitterData`, `CarTypeInfo`,
`eAnimation`, `eAnimationSolid`, `SkidSet`, `CameraDataItems`. Each loader in
`bchunkload.c`'s table is the place to look.

### 2. Drawing: done, then making it fast

The GL layer draws (2026-11-07). Under emulation it draws the language
screen and a race exactly as the game lays them out: the world, the cars,
the rear-view mirror (a render target), and the HUD in the PS2's places on
one 640x480 screen. A race frame is about 1,700 3D draws and 110 2D quads.
None of it has run on an R36S yet.

How it works (`r36s/compat/src/c3d_gles.c`, `c2d_gles.c`, `c3d_shaders.h`):

- **Shaders:** a GLSL twin of each `.v.pica`, line for line, picked by the
  shader binary's name; the game's uniform registers map onto the twin's
  uniforms by name and upload when their values change (a shadow copy per
  program).
- **Depth:** the PICA's clip z runs from -w (near) to 0 (far) and its depth
  is `zScale * z / w + zOffset` (`C3D_DepthMap`); the twins' last line
  turns that into GL's z, so GREATER and GEQUAL hold as the game wrote them.
- **Combiner:** one fragment shader per configuration of the six stages,
  the combiner buffer's update masks and the alpha test, in Citra's order
  (the buffer lags a stage), cached by a hash; the stage colours, buffer
  colour and alpha reference are uniforms.
- **Vertices:** read in place from the game's buffers with the PICA's
  packing (each attribute aligned to its element, padding nibbles), as
  client arrays. `georender.c`'s raw write of
  `GPUREG_ATTRIBBUFFER0_OFFSET` moves buffer 0, as on the PICA.
- **Textures:** untiled from 8x8 Morton tiles on first bind after a
  change, the memory's first row at v = 1; mip chains completed to 1x1 for
  GLES. Every GL name has an owner generation: the game copies `C3D_Tex`
  structs, and a stale copy must not delete or bind a recycled name (one
  did delete the top screen's colour buffer).
- **Targets:** an FBO each; a screen's is landscape. One made from a
  texture renders into that texture's GL image, a cube's face included.
  The presenter shows the target last linked to each screen
  (`C3D_RenderTargetSetOutput`), as citro3d does: the game links two to
  the top screen over its life.
- **citro2d:** batched quads with citro2d's three tint modes, through
  citro3d's depth and blend state, and texenv stage 0's scale applied, which
  `uirender.c` doubles for the GS's colours.

Next, for speed (none measured on a device yet; the profile's
cost model puts ~1,700 draws at the Mali driver's 22-34 µs a draw well past a
frame, see §3):

- **Static vertex data in VBOs:** the world's meshes are uploaded once
  and drawn from GPU memory, instead of client arrays copied every draw.
- **Fewer state changes and uniform uploads per draw,** batching what the
  game draws in runs.
- **Texture memory:** each texture is held twice, the PICA image the game
  wrote and GL's copy; freeing the first after upload where nothing reads
  it again would save up to half of it.
- **Read-back effects:** the radial blur, black and white, depth of field
  and the sun's depth probe still skip themselves; each needs an FBO
  version.

#### Checking frames without a device

`r36s/run_qemu.sh` with `NFSHP2_GL=pbuffer` draws headless, in the armhf
run image (`r36s/Dockerfile.armhf-run`, Debian 12's Mesa). Use
`GALLIUM_DRIVER=softpipe NFSHP2_PRESENT=sample` for frames that must be
right: Mesa's llvmpipe under QEMU loses what is drawn into or uploaded to a
texture while it has work pending (a 40-line GLES program reproduces it;
softpipe does not), so llvmpipe runs present with a blit and their textures
can show empty. Softpipe takes seconds a frame on the menus and longer in a
race: `NFSHP2_GL_FROM=N` skips drawing until frame N, and
`NFSHP2_INPUT='300:A*60/90'` reaches a race by about frame 1,600.
`NFSHP2_SHOTS` saves frames (PPM) to the card folder; `NFSHP2_GLDEBUG=N`
prints what the draws did every N frames, and on frame N+1 every 2D batch
with the pixels it changed.

### 3. Then

Sound and input on the device; the Volkyport launcher and package; the
first device measurements: per-frame game, render and present times and
draws per frame. The 3DS log has 120–1,090 model draws per frame plus about
205 for the car reflections. At the Mali driver's ~22–34 µs a draw, that is
the likely limit (the r36s-port-perf skill's profile for this port).

## Files

- `r36s/Makefile`, `r36s/build.sh`, `r36s/Dockerfile.armhf`,
  `r36s/Dockerfile` (aarch64): the build. `r36s/run_qemu.sh`: a headless
  run.
- `r36s/tests/run.sh`: the low heap's stress test (aarch64).
- `r36s/compat/include`: `3ds.h`, `citro3d.h`, `citro2d.h` for the R36S.
- `r36s/compat/src`:
  - `ctru_sys.c`: clocks, threads, locks, the linear heap, applets.
  - `ctru_hid.c`: input, and `NFSHP2_INPUT`'s scripted presses.
  - `ndsp_sdl.c`: the mixer.
  - `c3d_gles.c`, `c2d_gles.c`, `c3d_maths.c`: graphics.
- `r36s/platform`:
  - `r36s_platform.c`: the FPU, sdmc paths, thread stacks, the crash
    report.
  - `lowheap.c`: the game's heap below 4 GB (aarch64 only).
  - `r36s_video.c`: the window and GL context.
  - `diag_r36s.c`, `wireless_r36s.c`, `c3dwrap_r36s.c`: the replaced 3DS
    files.
- `r36s/scripts/gen_embedded.py`: the shaders' and data's stand-ins for
  picasso and bin2s.
- `r36s/scripts/syntax_all.sh`: every game file syntax-checked for aarch64.
- `3DS Files/source/fpctl.h`, `objsize.h`, `screen.h`: shared with the 3DS
  build, identities there. `mv3.c`'s saturating byte adds use inline
  `uqadd8`/`uqsub8` on the R36S: GCC 8 lacks their intrinsics.
