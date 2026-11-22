# Submitting the R36S port to Volkyport

Where it stands (2026-11-14). The staged folder `r36s/build/volkyport/nfshp2/`
is built from `sh r36s/build.sh` then `sh r36s/package.sh`. It passes
Volkyport's own `tools/build_release.py --do-check` in a sparse checkout of
Volkyport-MV-New (New 1, Broken 0, no warnings). The packaging page
(https://volkyport.games/contribute/porting/packaging/) is followed: layout,
name rules, `port.json`, a README with thanks, controls and compile notes,
`gameinfo.xml`, a 640x480 `screenshot.jpg` under 100 KB, the licenses folder,
and the launch script's order (control folder, `control.txt`, `PORT_32BIT`,
the mod file, `get_controls`, Volkyport's patcher, `$GPTOKEYB2`,
`pm_platform_helper`, `pm_finish`). The launcher is about 60 lines, shaped
like Arx's (a disc install through `utils/patcher.txt`) and flaskoman's (armhf).

Lines a reviewer may ask about, and the answer:

- No `.installed` marker: the install is done when `gamedata/NFSHP2/GLOBAL`
  exists, and it carries on from where it stopped.
- `$GPTOKEYB2` runs with no `.ini`: it only serves the Select+Start quit.
  The game reads the pad itself.
- `nfshp2.cfg` is sourced with `set -a`: the player's settings (README) go to
  the game as environment variables. Unset, the game uses its defaults.
- The R36S pad's mapping is appended after Volkyport's: the game's controls
  are by button position, and Volkyport's entry for that pad is by label.
- The launcher needs Volkyport (it stops at `source control.txt` without it),
  like every Volkyport port.

## Test matrix (the guide requires this before a pull request)

| Firmware | Resolution | Result |
| --- | --- | --- |
| dArkOSRE, R36S (Mali-G31, libmali) | 640x480 | played on the device; frame rate: fill in from current measurements (the older logged runs predate later speed work) |
| Knulli, RG34XX (H700) | 720x480 | not tested: installed on a card, not yet run |
| AmberELEC | any | not tested |
| muOS | any | not tested |
| ROCKNIX, libmali | any | not tested |
| ROCKNIX, Panfrost | any | not tested |
| any | 1280 or higher | not tested |
| any | 720x720 (optional) | not tested |

Nothing here has been run under another firmware. Emulated runs (QEMU, Mesa
software GL) cover the launcher's decisions, an ARMv7 CPU, GLES 2 and screen
shapes of 4:3, 16:9, 3:2 and square; they are not device results.

## Discord post for #testing-n-dev

Drafted in `docs/R36S_TESTERS_POST.md` (two messages, with what to send back).
The older draft here named the wrong frame-counter keys, so it was removed.

## Pull request (draft)

Title: `Add Need for Speed: Hot Pursuit 2 (nfshp2)`, against the `main` branch
of https://github.com/PortsMaster-MV/Volkyport-MV-New (decompilation).

> Adds `ports/nfshp2`: the PS2 game's decompilation, by way of its New 3DS
> port, as a 32-bit armhf program for SDL2 and OpenGL ES. Needs the player's
> own PAL PS2 disc; no game data is included. The first launch installs the
> game from an .iso in `gamedata/`.
>
> - Source: https://github.com/manuelpllull/NFSHP2-R36S at `<commit hash>`
>   (GPLv3; the port is in `r36s/`)
> - Testing thread: `<link to the #testing-n-dev thread>`
> - `tools/build_release.py --do-check`: New 1, Broken 0
> - Tested on: see the thread; at the time of writing dArkOSRE on an R36S at
>   640x480
>
> 🤖 Generated with [Claude Code](https://claude.com/claude-code)

## What only you can do

1. Commit and push the source, then put the revision in the pull request.
   Nothing is committed yet.
2. Run the port on the firmwares and resolutions above (or have testers do
   it), post the thread, and fill in the table.
3. Retake `screenshot.jpg` on a device if you can (640x480, under 100 KB);
   the current one is an emulated 640x480 frame with the dial face.
4. Fork Volkyport-MV-New, copy `r36s/build/volkyport/nfshp2/` to
   `ports/nfshp2/`, run `tools/build_release.py --do-check`, open the PR.
