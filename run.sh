#! /bin/sh
set -eo pipefail
export EPD_TTY=/dev/ttyACM0
cd "`dirname "$0"`"
output="`./add-display.sh`"
exec ./build/wayvnc --output "$output" -S ./sock
