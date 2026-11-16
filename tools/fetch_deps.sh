#!/bin/bash
# Fetch microReticulum and its dependencies into deps/ at the versions this project was built with.
set -eu
cd "$(dirname "$0")/.."
mkdir -p deps
get() { # name url ref
	if [ -d "deps/$1/.git" ]; then echo "deps/$1 present"; return; fi
	git clone --quiet "$2" "deps/$1"
	git -C "deps/$1" checkout --quiet "$3"
	echo "deps/$1 at $(git -C deps/$1 rev-parse --short HEAD)"
}
get microReticulum https://github.com/attermann/microReticulum.git 40fa628809d57140180c1c833559ab96fec992c1
get Crypto         https://github.com/attermann/Crypto.git         984dc891330986c302a86c4e312d4f5abcc28359
get microStore     https://github.com/genemichael/microStore.git   cdfddfad501b4c6f19c311c411a303730b77a7bf
get ArduinoJson    https://github.com/bblanchon/ArduinoJson.git    v7.4.3
get MsgPack        https://github.com/hideakitai/MsgPack.git       v0.4.2
get ArxContainer   https://github.com/hideakitai/ArxContainer.git  v0.7.0
get ArxTypeTraits  https://github.com/hideakitai/ArxTypeTraits.git v0.3.2
get DebugLog       https://github.com/hideakitai/DebugLog.git      v0.8.4
