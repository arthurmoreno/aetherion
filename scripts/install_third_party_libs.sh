#!/usr/bin/env bash
# Bootstrap vcpkg (full clone, pinned baseline), run manifest install for entt + flatbuffers,
# then materialize sources under libs/ for unchanged CMakeLists.txt.
# nanobind: git clone using refs from third_party.lock (vcpkg port omits 2.7.x; avoids python3 build chain).
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LIBS_DIR="${1:-$ROOT_DIR/libs}"
LOCK_FILE="${THIRD_PARTY_LOCK_FILE:-$ROOT_DIR/third_party.lock}"
MANIFEST_DIR="$ROOT_DIR"

# Pinned microsoft/vcpkg commit — must match vcpkg.json / vcpkg-configuration.json / vcpkg-lock.json.
VCPKG_BASELINE="${VCPKG_BASELINE:-c27eeddba73f608f10605d80bc0144c1166f8fb7}"
VCPKG_ROOT="${VCPKG_ROOT:-$ROOT_DIR/.vcpkg}"
VCPKG_DEFAULT_TRIPLET="${VCPKG_DEFAULT_TRIPLET:-x64-linux}"

CACHE_ROOT="${VCPKG_CACHE_ROOT:-$ROOT_DIR/.vcpkg_cache}"
DOWNLOADS_ROOT="$CACHE_ROOT/downloads"
BUILDTREES_ROOT="$CACHE_ROOT/buildtrees"
PACKAGES_ROOT="$CACHE_ROOT/packages"
INSTALL_ROOT="$ROOT_DIR/.vcpkg_installed"

if [[ ! -f "$ROOT_DIR/vcpkg.json" ]]; then
  echo "Missing vcpkg.json under $ROOT_DIR" >&2
  exit 1
fi

if [[ ! -f "$LOCK_FILE" ]]; then
  echo "Missing third_party.lock at $LOCK_FILE" >&2
  exit 1
fi

# shellcheck disable=SC1090
source "$LOCK_FILE"

require_cmd() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "Required command is missing: $1" >&2
    exit 1
  fi
}

ensure_vcpkg() {
  require_cmd git
  require_cmd cmake
  require_cmd ninja
  require_cmd curl
  require_cmd sed

  if [[ ! -d "$VCPKG_ROOT/.git" ]]; then
    echo "[vcpkg] Cloning microsoft/vcpkg -> $VCPKG_ROOT"
    git clone https://github.com/microsoft/vcpkg.git "$VCPKG_ROOT"
  fi

  if git -C "$VCPKG_ROOT" rev-parse --is-shallow-repository 2>/dev/null | grep -q true; then
    echo "[vcpkg] Repository is shallow; unshallowing for version database..."
    git -C "$VCPKG_ROOT" fetch --unshallow
  fi

  git -C "$VCPKG_ROOT" fetch --tags origin
  git -C "$VCPKG_ROOT" checkout --detach "$VCPKG_BASELINE"

  if [[ ! -x "$VCPKG_ROOT/vcpkg" ]]; then
    echo "[vcpkg] Bootstrapping..."
    "$VCPKG_ROOT/bootstrap-vcpkg.sh" -disableMetrics
  fi
}

materialize_flatbuffers() {
  local src
  src="$( (find "$BUILDTREES_ROOT/flatbuffers/src" -maxdepth 1 -type d -name 'v24.3.25*.clean' 2>/dev/null || true) | head -1 )"
  rm -rf "$LIBS_DIR/flatbuffers"
  mkdir -p "$LIBS_DIR"
  if [[ -n "$src" && -f "$src/CMakeLists.txt" ]]; then
    echo "[materialize] flatbuffers from vcpkg buildtree"
    cp -a "$src" "$LIBS_DIR/flatbuffers"
  else
    echo "[materialize] flatbuffers buildtree missing (common with binary cache); cloning tag v24.3.25"
    require_cmd git
    git clone --depth 1 -b v24.3.25 https://github.com/google/flatbuffers.git "$LIBS_DIR/flatbuffers"
  fi
  if [[ ! -f "$LIBS_DIR/flatbuffers/CMakeLists.txt" ]]; then
    echo "[materialize] flatbuffers CMakeLists.txt missing under $LIBS_DIR/flatbuffers" >&2
    exit 1
  fi
}

materialize_entt() {
  local src
  src="$( (find "$BUILDTREES_ROOT/entt/src" -maxdepth 1 -type d -name 'v3.13.2*.clean' 2>/dev/null || true) | head -1 )"
  rm -rf "$LIBS_DIR/entt"
  mkdir -p "$LIBS_DIR/entt"
  if [[ -n "$src" && -f "$src/single_include/entt/entt.hpp" ]]; then
    echo "[materialize] entt from vcpkg buildtree"
    cp -a "$src/single_include/entt/entt.hpp" "$LIBS_DIR/entt/entt.hpp"
  else
    echo "[materialize] entt buildtree missing; downloading single header v3.13.2"
    curl -fsSL "https://raw.githubusercontent.com/skypjack/entt/v3.13.2/single_include/entt/entt.hpp" \
      -o "$LIBS_DIR/entt/entt.hpp"
  fi
}

materialize_nanobind() {
  rm -rf "$LIBS_DIR/nanobind"
  echo "[nanobind] Cloning wjakob/nanobind @ ${NANOBIND_REF}"
  git clone --depth 1 --branch "${NANOBIND_REF}" https://github.com/wjakob/nanobind.git "$LIBS_DIR/nanobind"
  if [[ ! -f "$LIBS_DIR/nanobind/cmake/nanobind-config.cmake" ]]; then
    echo "[nanobind] Expected cmake/nanobind-config.cmake missing" >&2
    exit 1
  fi
}

echo "[third-party] Manifest root: $MANIFEST_DIR"
echo "[third-party] libs output:   $LIBS_DIR"
echo "[third-party] vcpkg root:   $VCPKG_ROOT @ $VCPKG_BASELINE"
ensure_vcpkg

mkdir -p "$CACHE_ROOT" "$DOWNLOADS_ROOT" "$BUILDTREES_ROOT" "$PACKAGES_ROOT" "$INSTALL_ROOT"

export VCPKG_ROOT
export VCPKG_DEFAULT_TRIPLET

echo "[vcpkg] install (entt + flatbuffers per vcpkg.json)..."
(
  cd "$MANIFEST_DIR"
  "$VCPKG_ROOT/vcpkg" install \
    --triplet "$VCPKG_DEFAULT_TRIPLET" \
    --x-install-root="$INSTALL_ROOT" \
    --downloads-root="$DOWNLOADS_ROOT" \
    --x-buildtrees-root="$BUILDTREES_ROOT" \
    --x-packages-root="$PACKAGES_ROOT"
)

echo "[materialize] flatbuffers + entt -> $LIBS_DIR"
materialize_flatbuffers
materialize_entt
materialize_nanobind

echo "[third-party] Optional / UI stack (not vcpkg): manage imgui/implot/ImGuizmo in libs per third_party.lock or your existing tree."
echo "[third-party] Done."
