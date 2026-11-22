# Volkyport testers post (draft, not posted)

For the Volkyport Discord's testing thread, in the format of the THUG
test-build post. Attach `r36s/build/volkyport/nfshp2.zip` where it says
`[LINK]`. Before posting:

- Tick Knulli (RG34XX) and 720x480 only once the first-run install test on the
  card gives a result. Both are unticked now.
- The R36S figure is run 7 (2026-11-14, `docs/R36S_OPTIMISATION_BACKLOG.md`):
  median 27.3 fps, 23.7 to 32.2 from the 10th to the 90th percentile. Builds
  since then are not logged.
- The first-run install through Volkyport's patcher has not run on a device
  yet. The post says so.

The post is 1,707 characters, under the 2,000 of a chat message.

```
# Need for Speed: Hot Pursuit 2 (Test Build)

PORT FILES:

[LINK]

You need the European (PAL) PS2 disc, SLES-50731, as an .iso. The zip does not include the game.

### CFW Tests
[ ] Knulli (RG34XX / H700)
[ ] ArkOS (R36S / RK3326)
[ ] AmberELEC (R36S / RK3326)
[X] dArkOS (R36S / RK3326)
[ ] muOS
ROCKNIX
    -> [ ] Libmali
    -> [ ] Panfrost

### Resolution Tests

[ ] 480x320 (Optional)
[X] 640x480 (R36S)
[ ] 720x480 (RG34XX)
[ ] Higher resolutions (e.g., 1280)

## Install

1. Copy `nfshp2.zip` to Volkyport's `autoinstall` folder and open Volkyport.
2. Copy your `.iso` into `ports/nfshp2/gamedata`.
3. Start **Need for Speed: Hot Pursuit 2** from Ports.
   The first launch installs the game from the `.iso`: about 10 minutes, 3 GB free, needs `python3`. If it stops, launch again. After that you can delete the `.iso`.

## Controls (by button position)

| Button | Action |
|--|--|
| Left stick / D-pad | Steer, menus |
| Bottom (B) | Accelerate, accept |
| Left (Y) | Brake, reverse |
| Right (A) | Horn, back |
| Top (X) | Camera |
| L1 / R1 | Look back / handbrake |
| L2 / R2 | Shift down / up |
| Start | Pause |
| Select + Start (hold) | Quit |

## Show FPS

**Select + L3/R3**, or `NFSHP2_SHOW_FPS=1` in `ports/nfshp2/nfshp2.cfg` (no L3/R3, like the RG34XX).

## Problems?

Send `ports/nfshp2/log.txt` (and `patchlog.txt` if the install failed), your device, your CFW, and what you were doing.

## Devices note

* R36S (640x480): runs, about 27 fps in races.
* RG34XX (720x480, Knulli): installed, no result yet.

## Known issues

* The first-launch install has never run on a device. Tell us if it fails.
* Buttons are mapped for the R36S and RG34XX pads only. Tell us if they are wrong.
```
