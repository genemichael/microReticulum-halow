#!/bin/bash
# Flash the built image. Usage: PORT=/dev/cu.usbmodemXXX ./flash.sh
# This writes the bootloader, the partition table and the app. Know which board is on the port.
set -eu
cd "$(dirname "$0")"
: "${PORT:?set PORT to the serial port of the board}"
: "${IDF_PATH:=$HOME/esp/esp-idf}"
BOARD="${BOARD:-rc32-68}"
source "$IDF_PATH/export.sh" >/dev/null 2>&1
cd "build/mesh/$BOARD"
python -m esptool --chip esp32s3 -p "$PORT" -b 460800 --before default_reset --after hard_reset write_flash "@flash_args"
