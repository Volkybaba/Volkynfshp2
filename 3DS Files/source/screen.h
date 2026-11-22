// screen -- the size of the screen the game's frame is drawn on.
//
// Not a port of any object. The PS2 drew one 640x480 frame on a 4:3 TV.
// The 3DS draws it on the top screen, 400x240, and splits the race HUD
// with the bottom screen (hudsplit.h); its 3D view keeps retail's width
// and crops the height to 5:3. The R36S has one 640x480 4:3 screen, as the
// PS2 had, so there the frame is drawn 1:1 (SCREEN_TOP_W/H), the HUD keeps
// retail's layout on that one screen (SCREEN_ONE), and the 3D view is
// retail's own framing, width and height (SCREEN_PS2_FRAMING).
//
// The other handhelds' screens have other shapes and sizes, known only when
// the game runs. SCREEN_TOP_W/H stay the 640x480 every menu and HUD object
// was authored in. SCREEN_FULL_W/H are the whole screen in those same units,
// wider or taller than 640x480 with that 640x480 in its middle, so nothing
// is stretched: 853x480 on a 16:9 screen, 720x480 on a 3:2 one, 640x640 on a
// square one, and 640x480 on a 4:3 one, which is the frame as it always was.
// The 3D view shows the extra width of the world (the same vertical view,
// more to the sides), and the render target is the screen's own pixels
// (r36s_video.c). NFSHP2_ASPECT=0 keeps 640x480 and bars. A frame without
// the race's world (menus, movies, loading screens) is the 640x480 alone, and
// the present fills the room around it with its edges mirrored
// (SCREEN_WORLD_FRAME below).

#ifndef SCREEN_H
#define SCREEN_H

#if defined(NFSHP2_R36S)
#define SCREEN_TOP_W        640.0f
#define SCREEN_TOP_H        480.0f
#define SCREEN_ONE          1
#define SCREEN_PS2_FRAMING  1
float r36s_screen_full_w(void);
float r36s_screen_full_h(void);
#define SCREEN_FULL_W       r36s_screen_full_w()
#define SCREEN_FULL_H       r36s_screen_full_h()
#define SCREEN_OFF_X        ((SCREEN_FULL_W - SCREEN_TOP_W) * 0.5f)
#define SCREEN_OFF_Y        ((SCREEN_FULL_H - SCREEN_TOP_H) * 0.5f)
// The race's world is in this frame, so its picture fills the screen. Every
// other frame is the 4:3 frame in the middle, the room around it its edges
// mirrored (r36s/compat/src/c3d_gles.c).
void r36s_screen_world(void);
#define SCREEN_WORLD_FRAME() r36s_screen_world()
#else
#define SCREEN_WORLD_FRAME() ((void)0)
#define SCREEN_TOP_W        400.0f
#define SCREEN_TOP_H        240.0f
#endif

// The top render target as citro3d makes it: the 3DS's screens are
// portrait panels, so its width is the screen's height.
#define SCREEN_TOP_TARGET_W ((int)SCREEN_TOP_H)
#define SCREEN_TOP_TARGET_H ((int)SCREEN_TOP_W)

#endif
