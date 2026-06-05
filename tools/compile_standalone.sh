#!/bin/bash
# Compile a .flux file to a standalone executable (AOT mode).
# Usage: ./tools/compile_standalone.sh input.flux [output]
set -euo pipefail

# Find objcopy (prefer llvm-objcopy for macOS compat)
OBJCOPY=""
for cmd in llvm-objcopy-21 llvm-objcopy objcopy; do
    if command -v "$cmd" &>/dev/null; then
        OBJCOPY="$cmd"
        break
    fi
done
if [ -z "$OBJCOPY" ] && [ -x "/opt/homebrew/opt/llvm@21/bin/llvm-objcopy" ]; then
    OBJCOPY="/opt/homebrew/opt/llvm@21/bin/llvm-objcopy"
fi
if [ -z "$OBJCOPY" ]; then
    echo "Error: no objcopy found (install llvm-objcopy or binutils)" >&2
    exit 1
fi

# Linker flags (macOS doesn't support -no-pie or separate -lpthread/-ldl)
if [ "$(uname)" = "Darwin" ]; then
    LDFLAGS_NOPIE=""
    AOT_LIBS_EXTRA=""
else
    LDFLAGS_NOPIE="-no-pie"
    AOT_LIBS_EXTRA="-lpthread -ldl"
fi

FLUXC="${FLUXC:-build/fluxc}"
FLUX="${FLUX:-build/flux}"
BUILD_DIR="${BUILD_DIR:-build}"

# Resolve relative paths to absolute (script may cd later)
if [[ "$FLUXC" != /* ]]; then FLUXC="$(realpath "$FLUXC")"; fi
if [[ "$FLUX" != /* ]]; then FLUX="$(realpath "$FLUX")"; fi
if [[ "$BUILD_DIR" != /* ]]; then BUILD_DIR="$(realpath "$BUILD_DIR")"; fi

export LD_LIBRARY_PATH="$BUILD_DIR:${LD_LIBRARY_PATH:-}"
export LIBRARY_PATH="$BUILD_DIR:${LIBRARY_PATH:-}"

input="$(realpath "$1")"
output="${2:-${input%.flux}.aot}"
input_dir="$(dirname "$input")"

# Prepend source dir + its parent to module search path so imports in
# subdirectories (e.g. demos/run.flux importing from fullcircuit/) resolve.
input_parent="$(dirname "$input_dir")"
export FLUX_MODULE_PATH="$input_dir:$input_parent${FLUX_MODULE_PATH:+:$FLUX_MODULE_PATH}"

echo "=== JIT mode ==="
rm -rf ~/.cache/fluxscript
"$FLUX" --cache=0 "$input" 2>&1
echo ""

echo "=== AOT mode ==="
obj="${output}.o"
"$FLUXC" "$input" -o "$obj"

# Find symbols in the object
anon_sym=$(nm "$obj" 2>/dev/null | grep "_anon_expr" | awk '{print $3}' | head -1 || true)
has_user_main=$(nm "$obj" 2>/dev/null | grep -c " T main$" || true)

if [ -z "$anon_sym" ]; then
    echo "Error: no anon_expr symbol found in object file"
    exit 1
fi
echo "Found entry point: $anon_sym"
echo "User has main: $has_user_main"

obj_fixed="${output}.fixed.o"

if [ "$has_user_main" -gt 0 ]; then
    # User defined main(): rename both anon_expr -> __flux_main, main -> user_main
    "$OBJCOPY" \
        --redefine-sym "$anon_sym=__flux_entry" \
        --redefine-sym "main=__user_main" \
        "$obj" "$obj_fixed"

    # Generate wrapper that calls __flux_entry (which calls __user_main)
    wrapper="/tmp/aot_wrapper_flux.cpp"
    cat > "$wrapper" << 'EOF'
#include <cstdio>
extern "C" double __flux_entry();
int main() {
    double result = __flux_entry();
    fprintf(stdout, "Result: %g\n", result);
    return 0;
}
EOF
    c++ $LDFLAGS_NOPIE -o "$output" "$obj_fixed" "$wrapper" \
        -L"$BUILD_DIR" -lFluxRuntime $AOT_LIBS_EXTRA -lm 2>&1
    rm -f "$wrapper"
else
    # No user main: just rename anon_expr -> main
    "$OBJCOPY" --redefine-sym "$anon_sym=main" "$obj" "$obj_fixed"
    c++ $LDFLAGS_NOPIE -o "$output" "$obj_fixed" \
        -L"$BUILD_DIR" -lFluxRuntime $AOT_LIBS_EXTRA -lm 2>&1
fi

rm -f "$obj_fixed"
echo "Linked: $output ($(stat -c%s "$output") bytes)"

echo ""
echo "=== AOT run ==="
"$output" 2>&1
echo "Exit code: $?"
