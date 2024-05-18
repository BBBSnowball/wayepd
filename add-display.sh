#! /usr/bin/env nix-shell
#! nix-shell -i bash -p sway jq

set -e

get_our_output() {
    swaymsg -t get_outputs --raw | jq -r '.[] | select(.name | startswith("HEADLESS-")) | select(.current_mode.width == 280 and .current_mode.height == 480) | .name'
}
get_outputs() {
    swaymsg -t get_outputs --raw | jq -r '.[] | .name' | sort
}

IFS=$'\n'
x=( `get_our_output` )
if [ ${#x[@]} -ge 1 ]; then
    echo "${x[0]}"
    exit 0
fi

x="`get_outputs`"
swaymsg create_output >&2
new=( `comm -13 - <<<"$x" <(get_outputs)` )
if [ ${#new[@]} -ne 1 ] ; then
    echo "We couldn't create the output! We had expected one new output but we got ${#new[@]}." >&2
    exit 1
fi
name="${new[0]}"
echo "DEBUG: New display is called $name" >&2

#NOTE It might seem like a good idea to set a low update rate to match the epaper
#     but then we will waste a lot of time painting old frames (e.g. switch to the
#     workspace and we will draw the mouse pointer but waybar isn't updated, yet).
#     Instead, we set a higher rate and we will later add some semi-smart holdoff
#     timer in our code.
#swaymsg output "$name" mode 280x480@0.5Hz >&2
swaymsg output "$name" mode 280x480@5Hz >&2
swaymsg output "$name" enable >&2

if [ "`get_our_output`" != "$name" ] ; then
    echo "WARN: We couldn't find our new output again, so something may be wrong." >&2
fi
echo "$name"
