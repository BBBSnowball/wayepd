#! /usr/bin/env nix-shell
#! nix-shell -i bash -p sway alacritty
set -xe
set -m  # enable job control

d="`dirname "$0"`"
d="`realpath "$d"`"
cd "$d"

swaymsg 'for_window [title="wlroots - X11-1"]' floating enable
swaymsg 'for_window [title="wlroots - X11-1"]' border none
swaymsg 'for_window [title="wlroots - X11-1"]' resize set 280 480
swaymsg 'for_window [title="wlroots - X11-1"]' border normal
swaymsg 'for_window [title="wlroots - X11-1"]' move container to workspace e

# set EPD_TTY before we start sway, so wayvnc will inherit it
export EPD_TTY=/dev/ttyACM0
#export EPD_TTY=NONE

# We want X11 mode so updates don't stop when main screen is locked.
unset WAYLAND_DISPLAY
sway &
export SWAYSOCK=/run/user/$UID/sway-ipc.1000.$!.sock
sleep 1
#swaymsg exec "`dirname "$0"`/run.sh"
swaymsg exec -- "$d/build/wayvnc" -S "$d/sock"
swaymsg exec alacritty
fg 1
