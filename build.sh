#!/bin/bash
# Build the RC32-68 HaLow mesh + microReticulum image. Build only.
# Needs: ESP-IDF v5.4.2 (IDF_PATH, default ~/esp/esp-idf), deps/ (tools/fetch_deps.sh),
# and RC_BCF_MBIN pointing at Heltec's 3.3 V calibration file bcf_HC01_V2_3V3.mbin (see README.md).
set -u
cd "$(dirname "$0")" || exit 2
: "${IDF_PATH:=$HOME/esp/esp-idf}"
: "${IDF_TOOLS_PATH:=$HOME/.espressif}"
BOARD="${BOARD:-rc32-68}"
if [ -z "${RC_BCF_MBIN:-}" ] || [ ! -f "$RC_BCF_MBIN" ]; then
	echo "NO-GO: set RC_BCF_MBIN to the path of bcf_HC01_V2_3V3.mbin"; exit 2
fi
export RC_BCF_MBIN
DEFS="$PWD/boards/$BOARD/sdkconfig.defaults;$PWD/examples/mesh/sdkconfig.defaults"
if [ -f "boards/$BOARD/sdkconfig.local" ]; then
	DEFS="$DEFS;$PWD/boards/$BOARD/sdkconfig.local"
	echo "using boards/$BOARD/sdkconfig.local"
else
	echo "no boards/$BOARD/sdkconfig.local: building with PLACEHOLDER network values"
fi
make build APP=mesh BOARD="$BOARD" IDF_PATH="$IDF_PATH" IDF_TOOLS_PATH="$IDF_TOOLS_PATH" SDKCONFIG_DEFAULTS="$DEFS" > build.log 2>&1
RC=$?
echo "exit=$RC" | tee -a build.log
grep -n 'error:\|undefined reference\|multiple definition\|FATAL' build.log | cut -c1-300 | head -20
ls -l "build/mesh/$BOARD/mesh.bin" 2>/dev/null
exit $RC
