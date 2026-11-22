#!/bin/bash

XDG_DATA_HOME=${XDG_DATA_HOME:-$HOME/.local/share}

if [ -d "/opt/system/Tools/PortMaster/" ]; then
  controlfolder="/opt/system/Tools/PortMaster"
elif [ -d "/opt/tools/PortMaster/" ]; then
  controlfolder="/opt/tools/PortMaster"
elif [ -d "$XDG_DATA_HOME/PortMaster/" ]; then
  controlfolder="$XDG_DATA_HOME/PortMaster"
else
  controlfolder="/roms/ports/PortMaster"
fi

source $controlfolder/control.txt
export PORT_32BIT="Y"
[ -f "${controlfolder}/mod_${CFW_NAME}.txt" ] && source "${controlfolder}/mod_${CFW_NAME}.txt"
get_controls

GAMEDIR=/$directory/ports/nfshp2
BINARY=nfshp2.armhf

> "$GAMEDIR/log.txt" && exec > >(tee "$GAMEDIR/log.txt") 2>&1

cd $GAMEDIR

# First run: install the game's files from the disc image in gamedata/.
export NFSHP2_ROOT="$GAMEDIR/gamedata"
if [ ! -d "$NFSHP2_ROOT/NFSHP2/GLOBAL" ]; then
  export NFSHP2_ISO=$(find "$NFSHP2_ROOT" -maxdepth 1 -type f -iname '*.iso' | head -n 1)
  if [ -z "$NFSHP2_ISO" ]; then
    pm_message "Copy your PAL (SLES-50731) or NTSC (SLUS-20362) disc image (.iso) into ports/nfshp2/gamedata. See README.md."
    sleep 10
    pm_finish
    exit 1
  fi
  export GAMEDIR
  export PATCHER_FILE="$GAMEDIR/tools/patchscript"
  export PATCHER_GAME="Need for Speed: Hot Pursuit 2"
  export PATCHER_TIME="about 10 minutes"
  $ESUDO chmod a+x "$GAMEDIR/tools/patchscript"
  source "$controlfolder/utils/patcher.txt"
  [ -d "$NFSHP2_ROOT/NFSHP2/GLOBAL" ] || { pm_finish; exit 1; }
fi

# The Turkish language file is not on the disc: the port ships it in
# tr/, and it belongs in the game's LANGUAGES folder. Copy it in when
# it is missing (a fresh install gets it from the installer; this
# covers installs done before that). Quiet when it is already there.
if [ ! -f "$NFSHP2_ROOT/NFSHP2/LANGUAGES/FE_TURKISH.BIN" ] && [ -f "$GAMEDIR/tr/FE_TURKISH.BIN" ]; then
  mkdir -p "$NFSHP2_ROOT/NFSHP2/LANGUAGES"
  cp "$GAMEDIR/tr/FE_TURKISH.BIN" "$NFSHP2_ROOT/NFSHP2/LANGUAGES/FE_TURKISH.BIN"
fi

# The Turkish flag FRONTB.BUN is not on the disc: the port ships it in
# tr/, and it belongs in the game's FRONTEND folder. Copy it in when the
# installed file is still the disc's original (14,385,132 bytes),
# keeping the original as FRONTB.BUN.orig. Quiet when it is already
# the Turkish one, or when the file was changed by hand.
if [ -f "$GAMEDIR/tr/FRONTB_TR.BUN" ] && [ -f "$NFSHP2_ROOT/NFSHP2/FRONTEND/FRONTB.BUN" ] && [ "$(stat -c %s "$NFSHP2_ROOT/NFSHP2/FRONTEND/FRONTB.BUN" 2>/dev/null)" = "14385132" ]; then
  cp "$NFSHP2_ROOT/NFSHP2/FRONTEND/FRONTB.BUN" "$NFSHP2_ROOT/NFSHP2/FRONTEND/FRONTB.BUN.orig"
  cp "$GAMEDIR/tr/FRONTB_TR.BUN" "$NFSHP2_ROOT/NFSHP2/FRONTEND/FRONTB.BUN"
fi

# The widened grid names INGAMEB.BUN is not on the disc: the port ships it
# in tr/, and it belongs in the game's GLOBAL folder. Copy it in when the
# installed file is still the disc's original, keeping the original as
# INGAMEB.BUN.orig. Quiet when it is already the widened one, or when the
# file was changed by hand. (Unlike FRONTB.BUN, the widened INGAMEB.BUN is
# the same size as the disc original, so size alone cannot tell them
# apart: the md5 below is the disc original's.)
if [ -f "$GAMEDIR/tr/INGAMEB_TR.BUN" ] && [ -f "$NFSHP2_ROOT/NFSHP2/GLOBAL/INGAMEB.BUN" ] && [ "$(stat -c %s "$NFSHP2_ROOT/NFSHP2/GLOBAL/INGAMEB.BUN" 2>/dev/null)" = "1983296" ] && [ "$(md5sum "$NFSHP2_ROOT/NFSHP2/GLOBAL/INGAMEB.BUN" 2>/dev/null | cut -d' ' -f1)" = "035af997b50797ce314e44190d24d6fd" ]; then
  cp "$NFSHP2_ROOT/NFSHP2/GLOBAL/INGAMEB.BUN" "$NFSHP2_ROOT/NFSHP2/GLOBAL/INGAMEB.BUN.orig"
  cp "$GAMEDIR/tr/INGAMEB_TR.BUN" "$NFSHP2_ROOT/NFSHP2/GLOBAL/INGAMEB.BUN"
fi

# Settings from nfshp2.cfg (README.md).
if [ -f "$GAMEDIR/nfshp2.cfg" ]; then
  set -a; source "$GAMEDIR/nfshp2.cfg"; set +a
fi

# Player name: the in-game Player Names screen cannot type a name
# (the device has no keyboard). Set it in nfshp2.cfg with a line
# "PlayerName=Volkan" (up to 11 characters, no Turkish letters; quote
# a name with a space: PlayerName="Volkan Tolky"). See README.md,
# "Player name".

export LD_LIBRARY_PATH="/usr/lib32:$LD_LIBRARY_PATH"
# The game's controls are by button position. PortMaster maps these pads by label, so each pad gets its own line here (R36S, RG34XX).
export SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig
190000004b4800000011000000010000,GO-Super Gamepad,a:b0,b:b1,x:b3,y:b2,back:b12,start:b13,leftstick:b14,rightstick:b15,guide:b16,dpleft:b10,dpdown:b9,dpright:b11,dpup:b8,leftshoulder:b4,lefttrigger:b6,rightshoulder:b5,righttrigger:b7,leftx:a0,lefty:a1,rightx:a2,righty:a3,platform:Linux,
19000000010000000100000000010000,Deeplay-keys,a:b4,b:b3,x:b5,y:b6,back:b9,start:b10,guide:b11,leftstick:b12,rightstick:b15,leftshoulder:b7,rightshoulder:b8,lefttrigger:b13,righttrigger:b14,dpleft:h0.8,dpdown:h0.4,dpright:h0.2,dpup:h0.1,leftx:a0,lefty:a1,rightx:a2,righty:a3,platform:Linux,"

$ESUDO chmod +x "$GAMEDIR/$BINARY"

$GPTOKEYB2 "$BINARY" -c "$GAMEDIR/nfshp2.gptk" > /dev/null 2>&1 &
pm_platform_helper "$GAMEDIR/$BINARY"

./$BINARY

pm_finish
