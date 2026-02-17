#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
OUT_DIR="$ROOT_DIR/dist"
SRC_DIR="$ROOT_DIR/wasm"

mkdir -p "$OUT_DIR"

: "${EMCC:=em++}"

echo "[build:wasm] Using compiler: $EMCC"

"$EMCC" \
  -O3 -std=c++20 \
  -s WASM=1 \
  -s MODULARIZE=1 \
  -s EXPORT_ES6=1 \
  -s ENVIRONMENT=web,node \
  -s ALLOW_MEMORY_GROWTH=1 \
  -s ASSERTIONS=1 \
  -s DISABLE_EXCEPTION_CATCHING=0 \
  -s EXCEPTION_DEBUG=1 \
  -lembind \
  -I "$ROOT_DIR/../.."/aetherion/src \
  -I "$ROOT_DIR/../.."/aetherion/libs \
  -I "$ROOT_DIR/../.."/aetherion/libs/nanobind/include \
  -I "$ROOT_DIR/../.."/aetherion/libs/entt/single_include \
  -I "$ROOT_DIR/../.."/aetherion/libs/flatbuffers/include \
  -I "$ROOT_DIR/../.."/aetherion/libs/ylt \
  "$ROOT_DIR/../.."/aetherion/src/EntityInterface.cpp \
  "$SRC_DIR/aetherion_embind.cpp" \
  -o "$OUT_DIR/aetherion_wasm.mjs"

echo "[build:wasm] Output: $OUT_DIR/aetherion_wasm.mjs and .wasm"
