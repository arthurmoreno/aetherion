#!/usr/bin/env bash
# Diagnose why a shared library's call stacks aren't resolving in a profiler
# (Tracy / heaptrack / perf). Reports stripped status, DWARF sections, symbol
# table, and tests a real address->function-name round-trip.
#
# Usage:
#   ./scripts/check-debug-symbols.sh                       # auto-find _aetherion in life-sim-312
#   ./scripts/check-debug-symbols.sh /path/to/lib.so       # check a specific file
#   ./scripts/check-debug-symbols.sh --env aetherion-312   # auto-find in another conda env

set -uo pipefail

SO=""
ENV_NAME="life-sim-312"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --env) ENV_NAME="$2"; shift 2 ;;
        -h|--help)
            sed -n '2,9p' "$0"; exit 0 ;;
        *) SO="$1"; shift ;;
    esac
done

if [[ -z "$SO" ]]; then
    SP=$(conda run --no-capture-output -n "$ENV_NAME" python -c \
         'import site; print(site.getsitepackages()[0])' 2>/dev/null)
    SO=$(find "$SP" -name '_aetherion*.so' 2>/dev/null | head -1)
fi

if [[ -z "$SO" || ! -f "$SO" ]]; then
    echo "ERROR: could not locate target .so (searched env: $ENV_NAME)"
    echo "       pass an explicit path: $0 /path/to/lib.so"
    exit 2
fi

bold() { printf '\033[1m%s\033[0m\n' "$1"; }
ok()   { printf '  \033[32mOK\033[0m  %s\n' "$1"; }
bad()  { printf '  \033[31mFAIL\033[0m %s\n' "$1"; }
warn() { printf '  \033[33mWARN\033[0m %s\n' "$1"; }

bold "Target"
echo "  $SO"
ls -lh "$SO" | awk '{printf "  size: %s  mtime: %s %s %s\n", $5, $6, $7, $8}'
echo

bold "1. Stripped status (file)"
FILE_OUT=$(file "$SO" | sed 's|.*: ||')
echo "  $FILE_OUT"
if echo "$FILE_OUT" | grep -q "not stripped"; then
    ok "binary retains symbol table"
elif echo "$FILE_OUT" | grep -q "stripped"; then
    bad "binary is stripped — Tracy/heaptrack will only see module name"
fi
echo

bold "2. DWARF debug sections (readelf -S)"
DBG=$(readelf -S "$SO" 2>/dev/null | grep -oE '\.debug_(info|line|str|abbrev|aranges|ranges|loc|rnglists|loclists)' | sort -u)
if [[ -n "$DBG" ]]; then
    echo "$DBG" | sed 's/^/  /'
    ok "DWARF present — addr2line and Tracy can symbolicate"
else
    bad "no .debug_* sections — DWARF stripped or never compiled in (-g)"
fi
echo

bold "3. Symbol tables"
DYN_COUNT=$(readelf --dyn-syms "$SO" 2>/dev/null | grep -cE ' FUNC ')
SYM_COUNT=$(nm "$SO" 2>/dev/null | grep -cE ' [Tt] ')
echo "  .dynsym FUNC entries: $DYN_COUNT  (visible symbols, used by dladdr)"
echo "  .symtab T/t entries:  $SYM_COUNT  (full symbols, used by debuggers)"
if (( SYM_COUNT > 0 )); then
    ok ".symtab present — debuggers/profilers can name local functions"
elif (( DYN_COUNT > 0 )); then
    warn "only .dynsym left — Tracy may resolve exported symbols only"
else
    bad "no symbol table — every address renders as ??"
fi
echo

bold "4. Live address -> symbol round-trip (addr2line)"
ADDR=$(nm "$SO" 2>/dev/null | grep -E ' [Tt] ' | head -1 | awk '{print $1}')
if [[ -n "$ADDR" ]]; then
    echo "  picking first text symbol at 0x$ADDR"
    RESOLVED=$(addr2line -fipe "$SO" "0x$ADDR" 2>/dev/null)
    echo "  addr2line -> $RESOLVED"
    if [[ "$RESOLVED" == *"??"* || -z "$RESOLVED" ]]; then
        bad "addr2line returns ?? — DWARF not usable even if sections exist"
    else
        ok "address resolves to a real function name"
    fi
else
    warn "no text symbol to test — symbol table is gone"
fi
echo

bold "5. Build ID (for matching with separate .debug files)"
BUILD_ID=$(file "$SO" | grep -oE 'BuildID\[sha1\]=[a-f0-9]+' | head -1)
echo "  $BUILD_ID"
DEBUG_PATH="/usr/lib/debug/.build-id/${BUILD_ID:18:2}/${BUILD_ID:20}.debug"
if [[ -f "$DEBUG_PATH" ]]; then
    ok "separate debug file found at $DEBUG_PATH"
else
    echo "  (no separate .debug file at /usr/lib/debug — fine if main binary has DWARF)"
fi
echo

bold "Summary"
if [[ -n "$DBG" && $SYM_COUNT -gt 0 ]]; then
    ok "symbolication should work — Tracy bottom-up tree should resolve frames"
else
    bad "symbolication will fail; possible fixes:"
    echo "       - aetherion: rebuild with TRACY=1 (Makefile passes install.strip=false)"
    echo "       - generic: add '-g' to compile flags AND avoid 'cmake --install --strip'"
    echo "       - generic: avoid 'strip <file>' or 'objcopy --strip-debug'"
fi
