#!/usr/bin/env bash
# Stage the BattleRoyale mod for packing: copies BattleRoyale/ to build/BattleRoyale/ with
# every .p3d binarized (ODOL, model.cfg baked in); scripts, config, layouts, textures and
# materials are copied through as they are. DayZMissions' tools/build_mod.py packs and
# signs build/BattleRoyale as @BattleRoyale (~/Projects/DayZMissions/mods/BattleRoyale).
#
#   tools/make_parachute_textures.py                         # after changing the texture
#   blender --background --python blender/build_parachute.py # after changing the model
#   tools/build.sh
#
# Binarize runs under GE-Proton in its own prefix (shared with img2paa.sh; never the DayZ
# Tools prefix in compatdata/). It scans its whole drive for addons, so the source gets a
# drive of its own: P: is a scratch copy of BattleRoyale/, O: the output.
set -euo pipefail

MOD=BattleRoyale
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
STEAM_ROOT="${STEAM_ROOT:-$HOME/.steam/debian-installation}"
GE="$(ls -d "$STEAM_ROOT"/compatibilitytools.d/GE-Proton* | sort -V | tail -n1)"
BINARIZE="$STEAM_ROOT/steamapps/common/DayZ Tools/Bin/Binarize/binarize.exe"
export WINEPREFIX="${IMG2PAA_PREFIX:-$HOME/.local/share/dayz-img2paa-prefix}/pfx"
export WINEDEBUG=-all

STAGE="$ROOT/build/$MOD"
WORK="$ROOT/build/.binarize"
rm -rf "$WORK"
mkdir -p "$ROOT/build"

if [ -n "$(find "$ROOT/$MOD" -name '*.p3d' -print -quit)" ]; then
	mkdir -p "$WORK/P" "$WORK/O"
	cp -r "$ROOT/$MOD" "$WORK/P/"
	ln -sfn "$WORK/P" "$WINEPREFIX/dosdevices/p:"
	ln -sfn "$WORK/O" "$WINEPREFIX/dosdevices/o:"
	cd "$WORK/P"
	timeout 300 "$GE/files/bin/wine64" "$BINARIZE" -targetBonesInterval=56 -maxProcesses=1 -always \
		"-textures=O:\\tex" "P:\\$MOD" "O:\\$MOD" >"$WORK/binarize.log" 2>&1 || {
		echo "binarize failed, see $WORK/binarize.log" >&2
		exit 1
	}
fi

# Keep a Workshop id recorded in the staged folder (workshop_publish.sh writes it there)
if [ -f "$STAGE/workshop.id" ] && [ ! -f "$ROOT/$MOD/workshop.id" ]; then
	cp "$STAGE/workshop.id" "$ROOT/$MOD/workshop.id"
fi
rm -rf "$STAGE"
cp -r "$ROOT/$MOD" "$STAGE"
find "$STAGE" -name model.cfg -delete   # baked into the binarized models
while IFS= read -r -d '' src; do
	rel="${src#"$ROOT/$MOD/"}"
	out="$WORK/O/$MOD/$rel"
	if [ "$(head -c 4 "$out" 2>/dev/null)" != ODOL ]; then
		echo "binarize produced no ODOL for $rel, see $WORK/binarize.log" >&2
		exit 1
	fi
	cp "$out" "$STAGE/$rel"
done < <(find "$ROOT/$MOD" -name '*.p3d' -print0)

echo "staged $STAGE ($(find "$STAGE" -name '*.p3d' | wc -l) models binarized)"
