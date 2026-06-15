#!/bin/bash
# Compile a .flux file to a shared library (.so / .dylib).
# The resulting .so has NO dependency on FluxScript or LLVM — only
# libc, libstdc++, libm, libcurl.
#
# Usage: ./tools/compile_shared.sh input.flux [output.so]
set -euo pipefail

FLUXC="${FLUXC:-build/fluxc}"
input="$(realpath "$1")"
output="${2:-${input%.flux}.so}"

echo "=== AOT compile + link shared library ==="
"$FLUXC" "$input" --shared -o "$output" 2>&1

echo ""
echo "=== Dependencies (no LLVM!) ==="
ldd "$output" 2>/dev/null | grep -v "linux-vdso\|ld-linux" | head -10

echo ""
echo "=== Exported FluxScript functions ==="
nm -D "$output" 2>/dev/null | grep " T " | grep -v "^0.* T [a-z]" | head -10

echo ""
echo "Built: $output ($(stat -c%s "$output" 2>/dev/null || stat -f%z "$output") bytes)"
echo ""
echo "Usage from Python:"
echo "  lib = ctypes.CDLL('./$output')"
echo "  result = lib.square(5.0)"
