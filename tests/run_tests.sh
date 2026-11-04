#!/bin/sh
# Pemakaian: tests/run_tests.sh <folder-sementara-untuk-file-uji>
# Memakai flag yang mirip build Android (-fno-exceptions -fno-rtti) plus sanitizer.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
SRC="$HERE/../app/src/main/cpp"
OUT="${1:-$HERE/out}"
mkdir -p "$OUT"
FLAGS="-std=c++20 -O1 -g -Wall -Wextra -fno-exceptions -fno-rtti -fsanitize=address,undefined -fno-sanitize-recover=undefined"
M="$SRC/model/model.cpp $SRC/model/anim.cpp $SRC/model/scene.cpp $SRC/model/storage.cpp"

for t in model scene app anim effects fx export; do
    EXTRA=""
    [ "$t" = app ] && EXTRA="$SRC/app.cpp $SRC/effects.cpp $SRC/effects_more.cpp"
    [ "$t" = effects ] && EXTRA="$SRC/effects.cpp $SRC/effects_more.cpp"
    [ "$t" = fx ] && EXTRA="$SRC/effects.cpp $SRC/effects_more.cpp"
    [ "$t" = export ] && EXTRA="$SRC/export.cpp"
    g++ $FLAGS -I"$SRC" "$HERE/test_$t.cpp" $EXTRA $M -o "$OUT/test_$t"
done

"$OUT/test_model" "$OUT"
"$OUT/test_scene"
"$OUT/test_app" "$OUT"
"$OUT/test_anim" "$OUT"
"$OUT/test_effects"
"$OUT/test_fx" "$OUT"
"$OUT/test_export"
