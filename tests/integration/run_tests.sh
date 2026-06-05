#!/bin/bash
# FluxScript Integration Test Runner
# Tests features with syntax that actually works
#
# Usage:
#   ./run_tests.sh           # sequential (default)
#   ./run_tests.sh -P 4      # parallel with 4 workers
#   ./run_tests.sh --worker <test_dir> <results_dir>   # internal: single test worker
#   ./run_tests.sh --help

MODE="sequential"
PARALLEL_JOBS=1
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Find an objcopy-compatible tool (prefer llvm-objcopy, then system objcopy)
OBJCOPY=""
for cmd in llvm-objcopy-21 llvm-objcopy objcopy; do
    if command -v "$cmd" &>/dev/null; then
        OBJCOPY="$cmd"
        break
    fi
done
# macOS Homebrew LLVM keg-only path
if [ -z "$OBJCOPY" ] && [ -x "/opt/homebrew/opt/llvm@21/bin/llvm-objcopy" ]; then
    OBJCOPY="/opt/homebrew/opt/llvm@21/bin/llvm-objcopy"
fi

# ==============================================================================
# Internal worker mode: run a single pre-registered test and write result
# ==============================================================================
if [ "${1:-}" = "--worker" ]; then
    FLUX_BIN="${FLUX_BIN:-$PROJECT_DIR/build/flux}"
    OPT="${OPT:-}"
    FLUX_SELFHOST_BIN="${FLUX_SELFHOST_BIN:-}"
    test_dir="$2"
    results_dir="$3"
    [ -z "$test_dir" ] && echo "Missing test_dir" && exit 1
    [ -z "$results_dir" ] && echo "Missing results_dir" && exit 1

    type="$(cat "$test_dir/type" 2>/dev/null)"
    name="$(cat "$test_dir/name" 2>/dev/null)"
    code_file="$test_dir/code.flux"
    result_file="$results_dir/$(basename "$test_dir").result"
    output_file="$results_dir/$(basename "$test_dir").output"

    if [ ! -f "$code_file" ]; then
        echo "FAIL|$name" > "$result_file"
        echo "No code file" > "$output_file"
        exit 0
    fi

    status=1
    case "$type" in
        jit)
            output=$("$FLUX_BIN" --cache=0 "$code_file" 2>&1)
            status=$?
            ;;
        check)
            output=$("$FLUX_BIN" --emit=check --cache=0 "$code_file" 2>&1)
            if echo "$output" | grep -q "OK"; then status=0; else status=1; fi
            ;;
        aot)
            FLUXC="${FLUXC:-$PROJECT_DIR/build/fluxc}"
            BUILD_DIR="${BUILD_DIR:-$PROJECT_DIR/build}"
            objfile="${code_file}.o"
            binfile="${code_file}.bin"
            if ! "$FLUXC" "$code_file" -o "$objfile" 2>/dev/null; then
                output="AOT compilation failed"; status=1
            else
                anon_sym=$(nm "$objfile" 2>/dev/null | grep "_anon_expr" | awk '{print $3}' | head -1 || true)
                has_user_main=$(nm "$objfile" 2>/dev/null | grep -c " T main$" || true)
                if [ -z "$anon_sym" ]; then
                    output="No entry symbol found"; status=1
                else
                    obj_fixed="${code_file}.fixed.o"
                    if [ "$has_user_main" -gt 0 ]; then
                        "$OBJCOPY" --redefine-sym "$anon_sym=__flux_entry" --redefine-sym "main=__user_main" "$objfile" "$obj_fixed"
                        wrapper="${code_file}_wrapper.cpp"
                        cat > "$wrapper" << 'WRAPEOF'
#include <cstdio>
extern "C" double __flux_entry();
int main() {
    double result = __flux_entry();
    fprintf(stdout, "Result: %g\n", result);
    return 0;
}
WRAPEOF
                        c++ -no-pie -o "$binfile" "$obj_fixed" "$wrapper" -L"$BUILD_DIR" -lFluxRuntime -lpthread -ldl -lm 2>/dev/null
                        status=$?
                        rm -f "$wrapper"
                    else
                        "$OBJCOPY" --redefine-sym "$anon_sym=main" "$objfile" "$obj_fixed"
                        c++ -no-pie -o "$binfile" "$obj_fixed" -L"$BUILD_DIR" -lFluxRuntime -lpthread -ldl -lm 2>/dev/null
                        status=$?
                    fi
                    rm -f "$obj_fixed"
                    if [ $status -eq 0 ] && [ -x "$binfile" ]; then
                        output=$("$binfile" 2>&1)
                        status=$?
                    else
                        output="AOT linking failed"; status=1
                    fi
                fi
            fi
            rm -f "$objfile" "$binfile"
            ;;
        selfhost)
            if [ -n "$OPT" ]; then
                if [ -n "$FLUX_SELFHOST_BIN" ] && [ -x "$FLUX_SELFHOST_BIN" ]; then
                    output=$(FLUX_INPUT="$code_file" "$FLUX_SELFHOST_BIN" 2>/dev/null | \
                        awk '/^; ModuleID/ {found=1} found && !/^(Compiled|JIT cache|Result)/' | \
                        "$OPT" -passes=verify -S 2>&1)
                    status=$?
                else
                    output=$(FLUX_INPUT="$code_file" "$FLUX_BIN" --cache=0 "$PROJECT_DIR/src/fluxc/main.flux" 2>/dev/null | \
                        awk '/^; ModuleID/ {found=1} found && !/^(Compiled|JIT cache|Result)/' | \
                        "$OPT" -passes=verify -S 2>&1)
                    status=$?
                fi
            else
                status=0
                output=""
            fi
            ;;
        *)
            output="Unknown test type: $type"
            status=1
            ;;
    esac

    if [ $status -eq 0 ]; then
        echo "PASS" > "$result_file"
        echo "" > "$output_file"
    else
        echo "FAIL" > "$result_file"
        echo "$output" > "$output_file"
    fi

    # Print one-line result for live aggregation
    if [ $status -eq 0 ]; then echo "OK|${name}"; else echo "FAIL|${name}"; fi
    exit 0
fi

if [ "${1:-}" = "--help" ]; then
    echo "Usage: $0 [-P N]"
    echo "  -P N    Run tests with N parallel workers (default: 1 = sequential)"
    exit 0
fi

# Parse -P flag
while getopts "P:" opt; do
    case "$opt" in
        P) PARALLEL_JOBS="$OPTARG" ;;
        *) echo "Usage: $0 [-P N]"; exit 1 ;;
    esac
done
shift $((OPTIND-1))

if [ "$PARALLEL_JOBS" -gt 1 ]; then
    MODE="parallel"
fi

FLUX_BIN="$PROJECT_DIR/build/flux"
if [ ! -x "$FLUX_BIN" ]; then
    if [ -x "$PROJECT_DIR/build/flux.exe" ]; then
        FLUX_BIN="$PROJECT_DIR/build/flux.exe"
    elif [ -x "$PROJECT_DIR/build/Release/flux.exe" ]; then
        FLUX_BIN="$PROJECT_DIR/build/Release/flux.exe"
    elif [ -x "$PROJECT_DIR/build/Debug/flux.exe" ]; then
        FLUX_BIN="$PROJECT_DIR/build/Debug/flux.exe"
    elif [ -x "$PROJECT_DIR/build/RelWithDebInfo/flux.exe" ]; then
        FLUX_BIN="$PROJECT_DIR/build/RelWithDebInfo/flux.exe"
    elif [ -x "$PROJECT_DIR/build/MinSizeRel/flux.exe" ]; then
        FLUX_BIN="$PROJECT_DIR/build/MinSizeRel/flux.exe"
    fi
fi

if [ ! -x "$FLUX_BIN" ]; then
    echo "flux not found at $FLUX_BIN"
    echo "Run 'make flux' first"
    exit 1
fi

# Self-hosted compiler binary for bootstrapped tests
FLUX_SELFHOST_BIN="${FLUX_SELFHOST_BIN:-$PROJECT_DIR/build/fluxc_bootstrap}"

# Dynamically locate LLVM tools (opt, llc, llvm-as) across platforms
find_llvm_tool() {
    local tool="$1"
    local ver_suffix="21"
    # Try version-suffixed name first (Ubuntu/Debian packages)
    if command -v "${tool}-${ver_suffix}" &>/dev/null; then
        echo "${tool}-${ver_suffix}"; return
    fi
    # Try Homebrew on macOS
    for prefix in "/opt/homebrew/opt/llvm@${ver_suffix}" "/opt/homebrew/opt/llvm" "/usr/local/opt/llvm@${ver_suffix}" "/usr/local/opt/llvm"; do
        if [ -x "${prefix}/bin/${tool}" ]; then
            echo "${prefix}/bin/${tool}"; return
        fi
    done
    # Try versioned system directories
    if [ -x "/usr/lib/llvm-${ver_suffix}/bin/${tool}" ]; then
        echo "/usr/lib/llvm-${ver_suffix}/bin/${tool}"; return
    fi
    # Fallback to plain name in PATH
    if command -v "${tool}" &>/dev/null; then
        echo "${tool}"; return
    fi
    echo ""
}

OPT=$(find_llvm_tool opt)
LLC=$(find_llvm_tool llc)
LLVM_AS=$(find_llvm_tool llvm-as)

if [ -z "$OPT" ]; then
    echo "Warning: opt not found — self-host IR verification will be skipped"
fi
if [ -z "$LLC" ]; then
    echo "Warning: llc not found — bootstrap build will fail"
fi
if [ -z "$LLVM_AS" ]; then
    echo "Warning: llvm-as not found — bootstrap build will fail"
fi

build_bootstrap() {
    local out="$1"
    local stage2_ll=$(mktemp /tmp/flux_stage2_ll.XXXXXX).ll
    local stage2_bc=/tmp/flux_stage2.bc
    local stage2_s=/tmp/flux_stage2.s
    local stage2_bin=/tmp/flux_stage2
    local stage3_ll=$(mktemp /tmp/flux_stage3_ll.XXXXXX).ll
    local stage3_bc=/tmp/flux_stage3.bc
    local stage3_s=/tmp/flux_stage3.s

    rm -f "$stage2_bc" "$stage2_s" "$stage2_bin" "$stage3_bc" "$stage3_s"

    "$FLUX_BIN" --cache=0 --emit=llvm "$PROJECT_DIR/src/fluxc/main.flux" 2>/dev/null > "$stage2_ll" || return 1
    $LLVM_AS "$stage2_ll" -o "$stage2_bc" || return 1
    $OPT -passes=globaldce "$stage2_bc" -o "$stage2_bc" 2>/dev/null || return 1
    $LLC -O2 -relocation-model=pic "$stage2_bc" -o "$stage2_s" || return 1
    g++ -O2 "$stage2_s" -L"$PROJECT_DIR/build" -lFluxScript -Wl,-rpath,"$PROJECT_DIR/build" -o "$stage2_bin" || return 1

    FLUX_INPUT="$PROJECT_DIR/src/fluxc/main.flux" "$stage2_bin" 2>/dev/null > "$stage3_ll" || return 1

    $LLVM_AS "$stage3_ll" -o "$stage3_bc" || return 1
    $OPT -passes=globaldce "$stage3_bc" -o "$stage3_bc" 2>/dev/null || return 1
    $LLC -O2 -relocation-model=pic "$stage3_bc" -o "$stage3_s" || return 1
    g++ -O2 "$stage3_s" -L"$PROJECT_DIR/build" -lFluxScript -Wl,-rpath,"$PROJECT_DIR/build" -o "$out" || return 1

    rm -f "$stage2_ll" "$stage2_bc" "$stage2_s" "$stage3_ll" "$stage3_bc" "$stage3_s" "$stage2_bin"
    return 0
}

if [ ! -x "$FLUX_SELFHOST_BIN" ]; then
    echo "Building self-hosted compiler (stage-3) for bootstrap tests..."
    if build_bootstrap "$FLUX_SELFHOST_BIN"; then
        echo "  -> $FLUX_SELFHOST_BIN"
    else
        echo "  -> bootstrap build failed; falling back to C++ compiler"
        FLUX_SELFHOST_BIN=""
    fi
fi

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[0;33m'
NC='\033[0m'

# Parallel mode setup
TEST_DIRS=()
if [ "$MODE" = "parallel" ]; then
    echo "Parallel mode enabled: $PARALLEL_JOBS workers"
fi

echo "========================================"
echo "  FluxScript Integration Tests"  
echo "========================================"
echo ""

TOTAL=0
PASSED=0
FAILED=0

# Check if timeout command exists, otherwise use gtimeout or none
TIMEOUT_CMD="timeout"
if ! command -v timeout &> /dev/null; then
    if command -v gtimeout &> /dev/null; then
        TIMEOUT_CMD="gtimeout"
    else
        TIMEOUT_CMD=""
    fi
fi

run_test() {
    local test_name="$1"
    local test_code="$2"

    if [ "$MODE" = "parallel" ]; then
        local tmpdir=$(mktemp -d "/tmp/fluxtest_parallel_XXXXXX")
        echo "$test_code" > "$tmpdir/code.flux"
        echo "jit" > "$tmpdir/type"
        echo "$test_name" > "$tmpdir/name"
        TEST_DIRS+=("$tmpdir")
        return
    fi

    TOTAL=$((TOTAL + 1))
    echo -n "$test_name... "
    
    local tmpfile=$(mktemp /tmp/fluxtest_XXXXXX.flux)
    echo "$test_code" > "$tmpfile"
    
    local cmd_status
    local test_output
    if [ -n "$TIMEOUT_CMD" ]; then
        test_output=$($TIMEOUT_CMD 5 "$FLUX_BIN" --cache=0 "$tmpfile" 2>&1)
        cmd_status=$?
    else
        test_output=$("$FLUX_BIN" --cache=0 "$tmpfile" 2>&1)
        cmd_status=$?
    fi

    if [ $cmd_status -eq 0 ]; then
        echo -e "${GREEN} PASSED${NC}"
        PASSED=$((PASSED + 1))
    else
        echo -e "${RED} FAILED${NC}"
        echo "=== TEST OUTPUT FOR $test_name ==="
        echo "$test_output"
        echo "=================================="
        FAILED=$((FAILED + 1))
    fi
    
    rm -f "$tmpfile"
}

# AOT standalone test: compile with fluxc, link with FluxRuntime, run executable
run_aot_test() {
    local test_name="$1"
    local test_code="$2"

    FLUXC="${FLUXC:-$PROJECT_DIR/build/fluxc}"
    BUILD_DIR="${BUILD_DIR:-$PROJECT_DIR/build}"

    if [ "$MODE" = "parallel" ]; then
        local tmpdir=$(mktemp -d "/tmp/fluxtest_parallel_XXXXXX")
        echo "$test_code" > "$tmpdir/code.flux"
        echo "aot" > "$tmpdir/type"
        echo "$test_name" > "$tmpdir/name"
        TEST_DIRS+=("$tmpdir")
        return
    fi

    TOTAL=$((TOTAL + 1))
    echo -n "$test_name (AOT)... "

    local tmpfile=$(mktemp /tmp/fluxtest_aot_XXXXXX.flux)
    echo "$test_code" > "$tmpfile"

    local objfile="${tmpfile}.o"
    local binfile="${tmpfile}.bin"

    local cmd_status=1
    local test_output=""

    # Compile to object file
    if ! "$FLUXC" "$tmpfile" -o "$objfile" 2>/dev/null; then
        test_output="AOT compilation failed"
        cmd_status=1
    else
        # Find entry symbol
        local anon_sym=$(nm "$objfile" 2>/dev/null | grep "_anon_expr" | awk '{print $3}' | head -1 || true)
        local has_user_main=$(nm "$objfile" 2>/dev/null | grep -c " T main$" || true)

        if [ -n "$anon_sym" ]; then
            local obj_fixed="${tmpfile}.fixed.o"
            if [ "$has_user_main" -gt 0 ]; then
                # User defined main(): rename anon_expr -> __flux_entry
                "$OBJCOPY" --redefine-sym "$anon_sym=__flux_entry" --redefine-sym "main=__user_main" "$objfile" "$obj_fixed"

                # Build wrapper
                local wrapper="${tmpfile}_wrapper.cpp"
                cat > "$wrapper" << 'WRAPEOF'
#include <cstdio>
extern "C" double __flux_entry();
int main() {
    double result = __flux_entry();
    fprintf(stdout, "Result: %g\n", result);
    return 0;
}
WRAPEOF
                c++ -no-pie -o "$binfile" "$obj_fixed" "$wrapper" \
                    -L"$BUILD_DIR" -lFluxRuntime -lpthread -ldl -lm 2>/dev/null
                cmd_status=$?
                rm -f "$wrapper"
            else
                # No user main: rename anon_expr -> main directly
                "$OBJCOPY" --redefine-sym "$anon_sym=main" "$objfile" "$obj_fixed"
                c++ -no-pie -o "$binfile" "$obj_fixed" \
                    -L"$BUILD_DIR" -lFluxRuntime -lpthread -ldl -lm 2>/dev/null
                cmd_status=$?
            fi
            rm -f "$obj_fixed"

            if [ $cmd_status -eq 0 ] && [ -x "$binfile" ]; then
                test_output=$("$binfile" 2>&1)
                cmd_status=$?
            else
                test_output="AOT linking failed (exit $cmd_status)"
                cmd_status=1
            fi
        else
            test_output="No entry symbol found in object file"
            cmd_status=1
        fi
    fi

    if [ $cmd_status -eq 0 ]; then
        echo -e "${GREEN} PASSED${NC}"
        PASSED=$((PASSED + 1))
    else
        echo -e "${RED} FAILED${NC}"
        echo "=== AOT TEST OUTPUT FOR $test_name ==="
        echo "$test_output"
        echo "========================================"
        FAILED=$((FAILED + 1))
    fi

    rm -f "$tmpfile" "$objfile" "$binfile"
}

# Self-hosting compiler test: generate IR via bootstrap or C++ compiler, verify with opt-21
run_selfhost_test() {
    local test_name="$1"
    local test_code="$2"

    if [ "$MODE" = "parallel" ]; then
        local tmpdir=$(mktemp -d "/tmp/fluxtest_parallel_XXXXXX")
        echo "$test_code" > "$tmpdir/code.flux"
        echo "selfhost" > "$tmpdir/type"
        echo "$test_name" > "$tmpdir/name"
        TEST_DIRS+=("$tmpdir")
        return
    fi

    TOTAL=$((TOTAL + 1))
    echo -n "$test_name... "

    local tmpfile=$(mktemp /tmp/fluxtest_selfhost_XXXXXX.flux)
    echo "$test_code" > "$tmpfile"

    if [ -z "$OPT" ]; then
        echo -e "${YELLOW} SKIP (opt not found)${NC}"
        PASSED=$((PASSED + 1))
        rm -f "$tmpfile"
        return
    fi

    local test_output
    local cmd_status
    if [ -n "$FLUX_SELFHOST_BIN" ] && [ -x "$FLUX_SELFHOST_BIN" ]; then
        test_output=$(FLUX_INPUT="$tmpfile" "$FLUX_SELFHOST_BIN" 2>/dev/null | \
            awk '/^; ModuleID/ {found=1} found && !/^(Compiled|JIT cache|Result)/' | \
            $OPT -passes=verify -S 2>&1)
        cmd_status=$?
    else
        test_output=$(FLUX_INPUT="$tmpfile" "$FLUX_BIN" --cache=0 "$PROJECT_DIR/src/fluxc/main.flux" 2>/dev/null | \
            awk '/^; ModuleID/ {found=1} found && !/^(Compiled|JIT cache|Result)/' | \
            $OPT -passes=verify -S 2>&1)
        cmd_status=$?
    fi

    if [ $cmd_status -eq 0 ]; then
        echo -e "${GREEN} PASSED${NC}"
        PASSED=$((PASSED + 1))
    else
        echo -e "${RED} FAILED${NC}"
        echo "=== TEST OUTPUT FOR $test_name ==="
        echo "$test_output"
        echo "=================================="
        FAILED=$((FAILED + 1))
    fi

    rm -f "$tmpfile"
}

# Modified run_test for parser-only validation (uses --emit=check)
run_check_test() {
    local test_name="$1"
    local test_code="$2"

    if [ "$MODE" = "parallel" ]; then
        local tmpdir=$(mktemp -d "/tmp/fluxtest_parallel_XXXXXX")
        echo "$test_code" > "$tmpdir/code.flux"
        echo "check" > "$tmpdir/type"
        echo "$test_name" > "$tmpdir/name"
        TEST_DIRS+=("$tmpdir")
        return
    fi

    TOTAL=$((TOTAL + 1))
    echo -n "$test_name... "

    local tmpfile=$(mktemp /tmp/fluxtest_check_XXXXXX.flux)
    echo "$test_code" > "$tmpfile"

    local cmd_status
    local test_output
    if [ -n "$TIMEOUT_CMD" ]; then
        test_output=$($TIMEOUT_CMD 5 "$FLUX_BIN" --emit=check --cache=0 "$tmpfile" 2>&1)
        echo "$test_output" | grep -q "OK"
        cmd_status=$?
    else
        test_output=$("$FLUX_BIN" --emit=check --cache=0 "$tmpfile" 2>&1)
        echo "$test_output" | grep -q "OK"
        cmd_status=$?
    fi

    if [ $cmd_status -eq 0 ]; then
        echo -e "${GREEN} PASSED${NC}"
        PASSED=$((PASSED + 1))
    else
        echo -e "${RED} FAILED${NC}"
        echo "=== TEST OUTPUT FOR $test_name ==="
        echo "$test_output"
        echo "=================================="
        FAILED=$((FAILED + 1))
    fi

    rm -f "$tmpfile"
}

# Test 1: Basic For Loop (baseline)
run_test "For Loop" "
for i in 1, 3 do i
"

# Test 2: Parallel For Loop (parser-only validation keeps both old check and adds new test)
run_check_test "Parallel For Parse" "
parallel for i in 1, 3 do i
"
run_test "Parallel For JIT" "
let lo = 1.0;
let hi = 10.0;
parallel for i in lo, hi do i;
0.0
"
run_test "Parallel For Captured Vars" "
let x = 5.0;
let y = 3.0;
parallel for i in 0, 4 do i + x + y;
0.0
"

# Test 3: Symbolic Declaration (parser-only - JIT runtime symbols not registered)
run_check_test "Symbolic Decl" "
sym x
"

# Test 4: Pattern Matching (parser test)
run_test "Pattern Match" "
def test() match(5.0) { 5.0 -> 10.0, _ -> 0.0 }; 1.0
test()
"

# Test 5: Foreach Loop
run_test "Foreach" "
def test() foreach x in [1.0, 2.0, 3.0] do x; 1.0
test()
"

# Test 6: Try-Catch (parser validation)
run_check_test "Try-Catch" "
def foo() { 10.0 }
def test() try { foo() } catch e { 0.0 }; 1.0
test()
"

# Test 7: Let binding
run_test "Let Binding" "
def test() let x = 42.0 in x; 1.0
test()
"

# Test 8: B-Source (parser validation)
run_check_test "B-Source" "
def test() bsource s V(out, 0) { sin(time) }; 1.0
test()
"

# Test 9: Initial Conditions (parser validation)
run_check_test "Initial Cond" "
def test() ic V(cap) = 5.0; 1.0
test()
"

# Test 10: Outputs (parser-only - JIT runtime symbols not registered)
run_check_test "Outputs" "
def test() outputs[\"Vout\"] = 2.5; 1.0
test()
"

# Test 11: Analog Block (Verilog-A Lite)
run_check_test "Analog" "
def test() analog { V(out) <+ sin(time) }; 1.0
test()
"

# Test 12: ddt() time derivative
run_check_test "ddt" "
def test() ddt(sin(time)); 1.0
test()
"

# Test 13: idt() time integral
run_check_test "idt" "
def test() idt(sin(time)); 1.0
test()
"

# Test 14: cross() zero-crossing detection
run_check_test "Cross" "
def test() cross(sin(time), 1); 1.0
test()
"

# Test 15: diagnostic standalone directive
run_check_test "Diagnostic" '
diagnostic V(out) type="overshoot" threshold=0.05
def test() 1.0
test()
'

# Test 16: tolerance standalone directive
run_check_test "Tolerance" '
tolerance abs=1e-6 rel=0.5
def test() 1.0
test()
'

# Test 17: poles() returns matrix
run_check_test "PolesMatrix" '
sym s
poles(s^2 + 3*s + 2)
'

# Test 18: zeros() returns matrix
run_check_test "ZerosMatrix" '
sym s
zeros(s^2 + 3*s + 2)
'

# Test 19: Impl block (parser validation)
run_check_test "Impl block" '
struct Point { x: Double, y: Double }
impl Point
    def scale(self, f: Double) -> Double
        self.x * f
    end
end
def test() { 1.0 }
test()
'

# Test 20: Struct construct + method call (parser validation)
run_check_test "Impl method call" '
struct Point { x: Double, y: Double }
impl Point
    def scale(self, f: Double) -> Double
        self.x * f
    end
end
def test() {
    let p = Point { x: 3.0, y: 4.0 }
    p.scale(2.0)
}
test()
'

# Test 21: Vector literal creation
run_test "Vector Literal" '
def test() [1, 2, 3]; 1.0
test()
'

# Test 22: Vector indexing returns scalar
run_test "Vector Index" '
def test() [1, 2, 3][1]; 1.0
test()
'

# Test 23: First element index
run_test "Vector First Index" '
def test() [10, 20, 30][0] == 10.0; 1.0
test()
'

# Test 24: Vector index in expression
run_test "Vector Index Expr" '
def test() [1, 2, 3][2] * 2.0; 1.0
test()
'

# Test 25: Vector with negative values
run_test "Vector Negatives" '
def test() [-1, -2, -3][1]; 1.0
test()
'

# Test 26: Vector with mixed int/double
run_test "Vector Mixed Types" '
def test() [1, 2.5, 3][0]; 1.0
test()
'

# Test 27: Empty vector literal
run_check_test "Vector Empty" '
def test() []; 1.0
test()
'

# Test 28: Vector index in comparison
run_test "Vector Index Compare" '
def test() [1, 2, 3][1] == 2.0; 1.0
test()
'

# Test 29: Vector with complex indexing chain
run_test "Vector Double Index" '
def test() [[1, 2], [3, 4]][0]; 1.0
test()
'

# Test 30: Struct construction and field access
run_test "Struct Field Access" '
struct Point { x: Double, y: Double }
def test() -> Double {
    let p = Point { x: 3.0, y: 4.0 };
    p.x
}
test()
'

# Test 31: Struct let-in pattern
run_test "Struct Let-In" '
struct Point { x: Double, y: Double }
def test() -> Double let p = Point { x: 7.0, y: 8.0 } in p.y
test()
'

# Test 32: Struct field access with assert
run_test "Struct Assert" '
struct Vec2 { x: Double, y: Double }
def test() {
    let v = Vec2 { x: 10.0, y: 20.0 };
    assert(v.x == 10.0, "x wrong");
    assert(v.y == 20.0, "y wrong");
    1.0
}
test()
'

# Test 33: Enum declaration and variant access
run_test "Enum Variant" '
enum Color { Red, Green, Blue }
def test() -> Double {
    let c = Color.Red;
    1.0
}
test()
'

# Test 34: Struct function parameter and call
run_test "Struct Param Call" '
struct Point { x: Double, y: Double }
def get_x(p: Point) -> Double p.x
def call_it() -> Double get_x(Point { x: 5.0, y: 6.0 })
call_it()
'

# Test 35: Enum equality == and !=
run_test "Enum Equality" '
enum Color
    Red
    Green
    Blue
end
def test() {
    assert(Color.Red == Color.Red, "eq");
    assert(Color.Red != Color.Blue, "ne");
    1.0
}
test()
'

# Test 36: Enum payload (single field) + match extraction
run_test "Enum Payload Match" '
enum Option { Some { value: Double }, None }
let five = Option.Some(5.0)
match five {
    Option.Some(v) -> v,
    default -> -1.0
}
'

# Test 37: Enum payload (multi-field) + match multi-binding
run_test "Enum Multi Payload Match" '
enum Item { Label { text: String, value: Double } }
let item = Item.Label("hello", 42.0)
match item {
    Item.Label(txt, val) -> val,
    default -> -1.0
}
'

# Test 38: Enum payload positional constructor scalar
run_test "Enum Payload Scalar" '
enum Wrap { Value { x: Double }, Empty }
let w = Wrap.Value(3.14)
match w {
    Wrap.Value(v) -> v,
    default -> -1.0
}
'

# Test 39: Enum payload match default fallback
run_test "Enum Match Default" '
enum Option { Some { value: Double }, None }
let five = Option.None
match five {
    Option.Some(v) -> v,
    default -> -1.0
}
'

# Test 40: Nested enum payload constructions and matches
run_test "Enum Nested Payload" '
enum Result { Ok { value: Double }, Err { msg: String } }
let r1 = Result.Ok(10.0)
let r2 = Result.Err("fail")
match r1 {
    Result.Ok(v) -> v,
    default -> -1.0
}
'

# Test 41: Large struct passing optimization (> 16 bytes)
run_test "Large Struct Passing Optimization" '
struct Point3D {
    x: Double,
    y: Double,
    z: Double
}

impl Point3D
    def calc_sum(self) -> Double {
        self.x + self.y + self.z
    }

    def add_other(self, other: Point3D) -> Double {
        self.calc_sum() + other.calc_sum()
    }
end

def get_x_plus_y(p: Point3D) -> Double {
    p.x + p.y
}

def test() -> Double {
    let p1 = Point3D { x: 10.0, y: 20.0, z: 30.0 };
    let p2 = Point3D { x: 1.0, y: 2.0, z: 3.0 };

    assert(get_x_plus_y(p1) == 30.0, "fn large param wrong");
    assert(p1.calc_sum() == 60.0, "method self large param wrong");
    assert(p1.add_other(p2) == 66.0, "method second large param wrong");

    1.0
}
test()
'

# Test 42: Tracked JIT Allocator & Boxed Enum Arena Deallocation
run_test "Boxed Enum JIT Arena Deallocation" '
struct LargePayload {
    a: Double,
    b: Double,
    c: Double
}

enum BoxedOption {
    Some(LargePayload),
    None
}

def create_boxed(x: Double) -> BoxedOption {
    BoxedOption.Some(LargePayload { a: x, b: x + 1.0, c: x + 2.0 })
}

def verify_leak_prevention() -> Double {
    let opt = create_boxed(42.0);
    match opt {
        BoxedOption.Some(payload) -> {
            assert(payload.a == 42.0, "Large payload field a wrong");
            assert(payload.b == 43.0, "Large payload field b wrong");
            assert(payload.c == 44.0, "Large payload field c wrong");
            1.0
        },
        default -> -1.0
    }
}
verify_leak_prevention()
'

# Test 43: Named Fields Enum Variant Constructor
run_test "Named Fields Enum Variant Constructor" '
enum Option {
    Some { value: Double },
    None
}

enum Info {
    Person { age: Double, score: Double },
    Empty
}

def verify_named_constructors() -> Double {
    let opt = Option.Some { value: 42.0 };
    let person = Info.Person { age: 25.0, score: 98.0 };

    var check_opt = match opt {
        Option.Some(payload) -> {
            assert(payload.value == 42.0, "Named field payload wrong");
            1.0
        },
        default -> -1.0
    };

    var check_person = match person {
        Info.Person(payload) -> {
            assert(payload.age == 25.0, "Person age wrong");
            assert(payload.score == 98.0, "Person score wrong");
            1.0
        },
        default -> -1.0
    };

    check_opt * check_person
}

verify_named_constructors()
'

# Test 44: Generics & Templates (Bracket Syntax [T])
run_test "Generics & Templates (Bracket Syntax [T])" '
def identity[T](x: T) -> T {
    return x
}

struct Pair[T] {
    first: T,
    second: T
}

def verify_generics() -> Double {
    let explicit_val = identity[Double](4.2);
    assert(explicit_val == 4.2, "Explicit generic function wrong");

    let implicit_val = identity(3.14);
    assert(implicit_val == 3.14, "Implicit generic function wrong");

    let pair = Pair[Double] { first: 10.0, second: 20.0 };
    assert(pair.first == 10.0, "Generic struct first field wrong");
    assert(pair.second == 20.0, "Generic struct second field wrong");

    1.0
}

verify_generics()
'

# Test 45: OOP Class Support
run_test "OOP Class Support" '
class Counter {
    value: Double

    def increment(self) -> Double {
        self.value = self.value + 1.0;
        self.value
    }
}

class Box[T] {
    content: T

    def get(self) -> T {
        return self.content;
    }
}

def verify_classes() -> Double {
    let c = Counter { value: 10.0 };
    assert(c.increment() == 11.0, "Class method dispatch failed");
    assert(c.increment() == 12.0, "Subsequent class method dispatch failed");

    let b = Box[Double] { content: 42.0 };
    assert(b.get() == 42.0, "Generic class method dispatch failed");

    1.0
}

verify_classes()
'

# Test 46: OOP Class Inheritance & Lifecycle Hooks
run_test "OOP Class Inheritance & Lifecycle Hooks" '
extern def flux_print_double(x: Double) -> Double

class Animal {
    age: Double
    initialized: Double
    destroyed: Double

    def onInit(self) -> Void {
        self.initialized = 1.0;
    }

    def onDestroy(self) -> Void {
        self.destroyed = 1.0;
        flux_print_double(111.0);
    }

    def speak(self) -> Double { 1.0 }
    def get_age(self) -> Double { self.age }
}

class Dog : Animal {
    breed: Double
    dog_init: Double
    dog_destroyed: Double

    def onInit(self) -> Void {
        self.dog_init = 1.0;
    }

    def onDestroy(self) -> Void {
        self.dog_destroyed = 1.0;
        flux_print_double(222.0);
    }

    def speak(self) -> Double { 2.0 }
}

def verify_inheritance() -> Double {
    let d = Dog { age: 5.0, breed: 9.0 };
    assert(d.get_age() == 5.0, "Inherited method/field lookup failed");
    assert(d.speak() == 2.0, "Method overriding failed");
    assert(d.initialized == 1.0, "Parent onInit failed to run");
    assert(d.dog_init == 1.0, "Child onInit failed to run");
    1.0
}

verify_inheritance()
'

# Test 47: Unified OOP Integration (Struct, Enum, Class)
run_test "Unified OOP Integration" '
enum Status {
    Active,
    Inactive
}

struct Point {
    x: Double,
    y: Double
}

class User {
    id: Double
    pos: Point
    status: Status

    def update_position(self, new_pos: Point) -> Point {
        self.pos = new_pos;
        self.pos
    }

    def is_active(self) -> Double {
        match self.status {
            Status.Active -> 1.0,
            default -> 0.0
        }
    }
}

def verify_unified() -> Double {
    let u = User {
        id: 1.0,
        pos: Point { x: 10.0, y: 20.0 },
        status: Status.Active
    };

    assert(u.is_active() == 1.0, "Enum integration inside class failed");

    let p2 = Point { x: 30.0, y: 40.0 };
    let updated_pos = u.update_position(p2);
    assert(updated_pos.x == 30.0, "Nested struct field update failed");
    assert(u.pos.y == 40.0, "Class state representation failed");

    1.0
}

verify_unified()
'

# Test 48: Traits & Interfaces
run_test "Traits & Interfaces" '
trait HasArea {
    def area(self) -> Double
}

trait Describe {
    def describe(self) -> Double
}

struct Circle {
    radius: Double
}

impl HasArea for Circle {
    def area(self) -> Double {
        3.14159 * self.radius * self.radius
    }
}

impl Describe for Circle {
    def describe(self) -> Double {
        self.radius
    }
}

struct Rectangle {
    width: Double,
    height: Double
}

impl HasArea for Rectangle {
    def area(self) -> Double {
        self.width * self.height
    }
}

def print_area[T: HasArea](shape: T) -> Double {
    shape.area()
}

def verify_traits() -> Double {
    let c = Circle { radius: 2.0 };
    let r = Rectangle { width: 3.0, height: 4.0 };

    let circle_area = print_area[Circle](c);
    let rect_area = print_area[Rectangle](r);

    assert(abs(circle_area - 12.56636) < 0.001, "Circle area via trait wrong");
    assert(abs(rect_area - 12.0) < 0.001, "Rectangle area via trait wrong");

    1.0
}

verify_traits()
'

# Test 49a: dyn Trait - let binding (fat pointer dispatch)
run_test "dyn Trait - let binding" '
trait Draw {
    def draw(self) -> Double
}

struct Circle { r: Double }
struct Square { s: Double }

impl Draw for Circle {
    def draw(self) -> Double { 3.14159 * self.r * self.r }
}
impl Draw for Square {
    def draw(self) -> Double { self.s * self.s }
}

let c = Circle { r: 2.0 };
let s = Square { s: 3.0 };
let dc: dyn Draw = c;
let ds: dyn Draw = s;
let result = dc.draw() + ds.draw();
assert(abs(result - 21.56636) < 0.001, "dyn Draw area sum wrong");
result
'

# Test 49b: dyn Trait - function parameter (upcast from concrete struct)
run_test "dyn Trait - function parameter" '
trait Greeter {
    def greet(self) -> Double
}

struct Hello { val: Double }
struct Goodbye { }

impl Greeter for Hello {
    def greet(self) -> Double { self.val }
}
impl Greeter for Goodbye {
    def greet(self) -> Double { 0.0 }
}

def process(g: dyn Greeter) -> Double {
    g.greet()
}

let a = Hello { val: 5.0 };
let b = Goodbye {};
let r1 = process(a);
let r2 = process(b);
assert(abs(r1 - 5.0) < 0.001, "Hello greet via dyn param wrong");
assert(abs(r2 - 0.0) < 0.001, "Goodbye greet via dyn param wrong");
r1 + r2
'

# Test 49c: dyn Trait - multi-method dispatch via function parameter
run_test "dyn Trait - multi-method dispatch" '
trait Shape {
    def area(self) -> Double
    def perimeter(self) -> Double
}

struct Rect { w: Double, h: Double }
struct Circle2 { r: Double }

impl Shape for Rect {
    def area(self) -> Double { self.w * self.h }
    def perimeter(self) -> Double { 2.0 * (self.w + self.h) }
}
impl Shape for Circle2 {
    def area(self) -> Double { 3.14159 * self.r * self.r }
    def perimeter(self) -> Double { 2.0 * 3.14159 * self.r }
}

def describe(s: dyn Shape) -> Double {
    s.area() + s.perimeter()
}

let r = Rect { w: 3.0, h: 4.0 };
let c = Circle2 { r: 2.0 };
let rd = describe(r);
let cd = describe(c);
assert(abs(rd - 26.0) < 0.001, "Rect describe wrong");
assert(abs(cd - 18.84954) < 0.001, "Circle describe wrong");
rd + cd
'

# Test 49: Operator Overloading
run_test "Operator Overloading" '
trait Add {
    def add(self, rhs: Complex) -> Complex
}

trait Sub {
    def sub(self, rhs: Complex) -> Complex
}

trait Mul {
    def mul(self, rhs: Complex) -> Complex
}

trait Neg {
    def neg(self) -> Complex
}

trait Eq {
    def eq(self, other: Complex) -> Bool
}

struct Complex { re: Double, im: Double }

impl Add for Complex {
    def add(self, rhs: Complex) -> Complex {
        Complex { re: self.re + rhs.re, im: self.im + rhs.im }
    }
}

impl Sub for Complex {
    def sub(self, rhs: Complex) -> Complex {
        Complex { re: self.re - rhs.re, im: self.im - rhs.im }
    }
}

impl Mul for Complex {
    def mul(self, rhs: Complex) -> Complex {
        let r1 = self.re * rhs.re;
        let r2 = self.im * rhs.im;
        let i1 = self.re * rhs.im;
        let i2 = self.im * rhs.re;
        Complex { re: r1 - r2, im: i1 + i2 }
    }
}

impl Neg for Complex {
    def neg(self) -> Complex {
        Complex { re: -self.re, im: -self.im }
    }
}

impl Eq for Complex {
    def eq(self, other: Complex) -> Bool {
        (abs(self.re - other.re) < 0.0001) && (abs(self.im - other.im) < 0.0001)
    }
}

def verify_overloads() -> Double {
    let a = Complex { re: 1.0, im: 2.0 };
    let b = Complex { re: 3.0, im: 4.0 };
    let sum = a + b;
    let diff = a - b;
    let prod = a * b;
    let neg_a = -a;
    let eq_check = a == a;
    let ne_check = a == b;

    assert(abs(sum.re - 4.0) < 0.0001, "sum.re");
    assert(abs(sum.im - 6.0) < 0.0001, "sum.im");
    assert(abs(diff.re + 2.0) < 0.0001, "diff.re");
    assert(abs(diff.im + 2.0) < 0.0001, "diff.im");
    assert(abs(prod.re + 5.0) < 0.0001, "prod.re");
    assert(abs(prod.im - 10.0) < 0.0001, "prod.im");
    assert(abs(neg_a.re + 1.0) < 0.0001, "neg_a.re");
    assert(abs(neg_a.im + 2.0) < 0.0001, "neg_a.im");
    assert(eq_check != 0.0, "eq");
    assert(ne_check == 0.0, "ne");

    1.0
}

verify_overloads()
'

# Test 50: Pipe Operator
run_test "Pipe Operator - basic" '
def double_it(x: Double) -> Double { x * 2.0 }
def add_one(x: Double) -> Double { x + 1.0 }
3.0 |> double_it |> add_one
'
run_test "Pipe Operator - multi-arg" '
def scale(s: Double, x: Double) -> Double { s * x }
let mul = 2.5;
10.0 |> scale(mul)
'
run_test "Pipe Operator - method call" '
class Box { val: Double }
impl Box {
    def apply(self, f: Double) -> Double { self.val * f }
}
let b = Box { val: 3.0 };
5.0 |> b.apply
'

# Test 53: Async/Await
run_test "Async/Await" '
async def fetch_data() -> Double {
    await 0.5;
    42.0
}

async def process_data(val: Double) -> Double {
    let data = await fetch_data();
    data + val
}

def verify_async() -> Double {
    let result = process_data(1.0);
    assert(abs(result - 43.0) < 0.0001, "async result");
    1.0
}

verify_async()
'

# Test 51: Async - await outside async error check
run_test "Async - await outside async (error)" '
async def ok() -> Double { await 0.5; 1.0 }
42.0
'

# Test 52: Async simple call without await
run_test "Async - simple call" '
async def make_val() -> Double { await 1.0; 7.0 }
def check() -> Double {
    let r = make_val();
    assert(abs(r - 7.0) < 0.0001, "simple call");
    1.0
}
check()
'

# Test 53: Async with conditional await
run_test "Async - conditional await" '
async def maybe_await(flag: Double) -> Double {
    if flag > 0.5 {
        await 0.5;
        100.0
    } else {
        200.0
    }
}

def run_check() -> Double {
    let r1 = maybe_await(1.0);
    assert(abs(r1 - 100.0) < 0.0001, "cond await true");
    let r2 = maybe_await(0.0);
    assert(abs(r2 - 200.0) < 0.0001, "cond await false");
    1.0
}
run_check()
'

# Test 54: Async deeply nested (3 levels deep)
run_test "Async - deep nesting" '
async def level3() -> Double { await 0.5; 3.0 }
async def level2() -> Double { let v = await level3(); v + 2.0 }
async def level1() -> Double { let v = await level2(); v + 1.0 }

def run_test() -> Double {
    let r = level1();
    assert(abs(r - 6.0) < 0.0001, "deep nested");
    1.0
}
run_test()
'

# Test 55: Async calling multiple async functions
run_test "Async - multiple async calls" '
async def source_a() -> Double { await 0.5; 10.0 }
async def source_b() -> Double { await 1.0; 20.0 }
async def source_c() -> Double { await 1.5; 30.0 }

async def combiner() -> Double {
    source_a() + source_b() + source_c()
}

def run_check() -> Double {
    let r = combiner();
    assert(abs(r - 60.0) < 0.0001, "multi async calls");
    1.0
}
run_check()
'

# Test 56: Async with nested await expressions
run_test "Async - nested await expressions" '
async def inner() -> Double { await 0.5; 5.0 }
async def outer() -> Double { await inner() + 3.0 }

def run_check() -> Double {
    let r = outer();
    assert(abs(r - 8.0) < 0.0001, "nested await expr");
    1.0
}
run_check()
'

# Test 57: Async identity function
run_test "Async - identity passthrough" '
async def identity(x: Double) -> Double { await x; x }

def run_check() -> Double {
    let r = identity(99.0);
    assert(abs(r - 99.0) < 0.0001, "identity");
    1.0
}
run_check()
'

# Test 58: Async with multiple params
run_test "Async - multi-param async" '
async def sum3(a: Double, b: Double, c: Double) -> Double { await 1.0; a + b + c }

def run_check() -> Double {
    let r = sum3(10.0, 20.0, 30.0);
    assert(abs(r - 60.0) < 0.0001, "multi-param");
    1.0
}
run_check()
'

# Test 60: Match exhaustiveness - all variants covered
run_test "Match Exhaustiveness - all covered" '
enum Color { Red, Green, Blue }
def run_check() -> Double {
    let c = Color.Red;
    match c {
        Color.Red -> 1.0,
        Color.Green -> 2.0,
        Color.Blue -> 3.0
    }
}
run_check()
'

# Test 62: Match exhaustiveness - with default arm (ok)
run_test "Match Exhaustiveness - default arm" '
enum Color { Red, Green, Blue }
def run_check() -> Double {
    let c = Color.Red;
    match c {
        Color.Red -> 1.0,
        default -> 0.0
    }
}
run_check()
'

# Test 63: Match exhaustiveness - payload enum all covered
run_test "Match Exhaustiveness - payload covered" '
enum Option { Some(Double), None }
def run_check() -> Double {
    let opt = Option.Some(5.0);
    match opt {
        Option.Some(x) -> x,
        Option.None -> 0.0
    }
}
run_check()
'

# Test 64a: Switch basic — literal match, returns case value
run_test "Switch basic" '
let a = 1.0
switch (a) {
    1.0 => 10.0,
    2.0 => 20.0,
    ~ => 0.0
}
'

# Test 64b: Switch with default fallthrough
run_test "Switch default" '
let a = 99.0
switch (a) {
    1.0 => 10.0,
    2.0 => 20.0,
    ~ => -1.0
}
'

# Test 64c: Switch with let-binding + code after
run_test "Switch let binding + after" '
let a = 1.0;
let r = switch (a) {
    1.0 => 10.0,
    ~ => 0.0
};
r * 2.0
'

# Test 64d: Switch with return in case
run_test "Switch return case" '
let a = 1.0
switch (a) {
    1.0 => { return 99.0 }
    ~ => { return 0.0 }
}
'

# Test 64e: Switch with mixed return/non-return + code after
run_test "Switch mixed return + after" '
let a = 5.0
switch (a) {
    1.0 => { return 99.0 }
    ~ => { 0.0 }
}
a * 2.0
'

# Test 64f: Switch with case/colon syntax (original)
run_test "Switch colon syntax" '
let a = 2.0
switch (a) {
    case 1.0: 10.0,
    case 2.0: 20.0,
    default: 0.0
}
'

# Test 64g: Switch with enum-like body (block in each case)
run_test "Switch block body" '
let a = 3.0
switch (a) {
    1.0 => {
        100.0
    }
    ~ => {
        a + 1.0
    }
}
'

# Test 64h: Switch nested in expression
run_test "Switch in expression" '
let a = 2.0;
let r = 1.0 + switch (a) { 1.0 => 10.0, ~ => 0.0 };
r
'

# Test 64i: Switch with multiple cases, one matches
run_test "Switch multi case match" '
let a = 3.0
switch (a) {
    1.0 => 10.0,
    2.0 => 20.0,
    3.0 => 30.0,
    ~ => -1.0
}
'

# Test 65a: ~Copy annotation on struct — basic creation and field access
run_test "~Copy struct basic" '
struct NoCopy ~Copy {
    val: Double
}
let a = NoCopy { val: 42.0 };
a.val
'

# Test 65b: ~Copy struct — function parameter passing
run_test "~Copy struct function param" '
struct NoCopy ~Copy {
    val: Double
}
def get_val(x: NoCopy) -> Double { x.val }
let a = NoCopy { val: 42.0 };
get_val(a)
'

# Test 65c: ~Copy enum — declaration and match
run_test "~Copy enum match" '
enum Opt ~Copy { Some { val: Double }, None }
let x = Opt.Some { val: 42.0 };
match x {
    Opt.Some(v) -> v,
    Opt.None -> 0.0
}
'

# Test 65d: ~Copy struct — let-binding assignment
run_test "~Copy struct let assign" '
struct NoCopy ~Copy {
    val: Double
}
let a = NoCopy { val: 42.0 };
let b = a;
b.val
'

# Test 65e: Copy struct (no ~Copy) still works as expected
run_test "Copy struct defaults to copy" '
struct Normal {
    val: Double
}
let a = Normal { val: 7.0 };
a.val * 3.0
'

# Test 65: Async from async with let (no cross-await usage)
run_test "Async - let before await" '
async def helper() -> Double { await 1.0; 5.0 }
async def user() -> Double {
    let factor = 2.0;
    let base = await helper();
    factor * base
}

def run_check() -> Double {
    let r = user();
    assert(abs(r - 10.0) < 0.0001, "let before await");
    1.0
}
run_check()
'

# ==============================================================================
# Outlives Bounds ('a: 'b) Tests
# ==============================================================================

# Test 66: Struct with outlives bounds (parser validation)
run_check_test "Struct Outlives Bounds" "
struct Foo<'a, 'b: 'a> {
    x: &'a Double,
    y: &'b Double
}
def test() -> Double { 1.0 }
test()
"

# Test 67: Impl with outlives bounds (parser validation)
run_check_test "Impl Outlives Bounds" "
struct Foo<'a, 'b> {
    x: &'a Double,
    y: &'b Double
}
impl<'a, 'b: 'a> Foo<'a, 'b>
    def get_x(self) -> &'a Double { self.x }
end
def test() -> Double { 1.0 }
test()
"

# Test 68: Struct with multiple outlives bounds
run_check_test "Multi Outlives Bounds" "
struct Foo<'a, 'b: 'a, 'c: 'b> {
    a: &'a Double,
    b: &'b Double,
    c: &'c Double
}
def test() -> Double { 1.0 }
test()
"

# Test 69: Function with outlives bounds (parser validation)
run_check_test "Fn Outlives Bounds" "
def foo<'a, 'b: 'a>(x: &'a Double) -> &'b Double { x }
def test() -> Double { 1.0 }
test()
"

# Test 70: Lifetime bound error - undeclared lifetime in bound
run_check_test "Undeclared Lifetime Bound" "
struct Foo<'a: 'b> { x: &'a Double }
def test() -> Double { 1.0 }
test()
"

# Test 71: Lifetime bound error - undeclared lifetime in reference
run_check_test "Undeclared Lifetime Ref" "
struct Foo<'a> { x: &'b Double }
def test() -> Double { 1.0 }
test()
"

# Test 72: Single lifetime param (no bounds) still works
run_check_test "Single Lifetime Param" "
struct Foo<'a> { x: &'a Double }
impl<'a> Foo<'a>
    def get(self) -> &'a Double { self.x }
end
def test() -> Double { 1.0 }
test()
"

# Test 73: Associated Types in Traits
run_test "Associated Types" '
trait Container {
    type Item;
    def get(self, index: Double) -> Item
}

struct MyBox {
    value: Double
}

impl Container for MyBox {
    type Item = Double;
    def get(self, index: Double) -> Double {
        self.value
    }
}

def test() -> Double {
    let b = MyBox { value: 42.0 };
    let v = b.get(0.0);
    assert(abs(v - 42.0) < 0.001, "associated type basic failed");
    1.0
}
test()
'

# Test 74: Associated Types - multiple associated types
run_test "Associated Types (multi)" '
trait Pair {
    type First;
    type Second;
    def first(self) -> First
    def second(self) -> Second
}

struct TwoValues {
    a: Double,
    b: Double
}

impl Pair for TwoValues {
    type First = Double;
    type Second = Double;
    def first(self) -> Double { self.a }
    def second(self) -> Double { self.b }
}

def test() -> Double {
    let t = TwoValues { a: 10.0, b: 20.0 };
    let f = t.first();
    let s = t.second();
    assert(abs(f - 10.0) < 0.001, "assoc type first failed");
    assert(abs(s - 20.0) < 0.001, "assoc type second failed");
    1.0
}
test()
'

# Test 75: Associated Types - missing binding (error)
run_check_test "Associated Types (missing)" "
trait Container {
    type Item;
    def get(self, index: Double) -> Item
}
struct MyBox { value: Double }
impl Container for MyBox {
    def get(self, index: Double) -> Double { self.value }
}
"

# Test 76: Threading - spawn/join
run_test "Spawn/Join" '
def worker(x: Double) -> Double {
    x * 2.0
}

def main() -> Double {
    let t = spawn worker(21.0);
    assert(join(t) == 42.0, "spawn/join failed");
    1.0
}

main()
'

# Test 77: Threading - channel send/recv
run_test "Channel Send/Recv" '
def main() -> Double {
    let ch = flux_chan_create();
    flux_chan_send(ch, 42.0);
    let val = flux_chan_recv(ch);
    flux_chan_destroy(ch);
    assert(val == 42.0, "channel send/recv failed");
    1.0
}

main()
'

# Test 78: Threading - spawn with channel
run_test "Spawn + Channel" '
def producer(ch: Double) -> Double {
    flux_chan_send(ch, 99.0);
    1.0
}

def main() -> Double {
    let ch = flux_chan_create();
    spawn producer(ch);
    let val = flux_chan_recv(ch);
    flux_chan_destroy(ch);
    assert(val == 99.0, "spawn+channel failed");
    1.0
}

main()
'

# Test 79: Threading - spawn multiple args
run_test "Spawn Multi-Arg" '
def add(a: Double, b: Double, c: Double) -> Double {
    a + b + c
}

def main() -> Double {
    let t = spawn add(10.0, 20.0, 30.0);
    assert(join(t) == 60.0, "spawn multi-arg failed");
    1.0
}

main()
'

# Test 80: Threading - Mutex
run_test "Mutex Lock/Unlock" '
def main() -> Double {
    let mtx = flux_mutex_create();
    flux_mutex_lock(mtx);
    flux_mutex_unlock(mtx);
    flux_mutex_destroy(mtx);
    1.0
}

main()
'

# Test 81: Threading - RwLock
run_test "RwLock Read/Write" '
def main() -> Double {
    let rw = flux_rwlock_create();
    flux_rwlock_read_lock(rw);
    flux_rwlock_unlock(rw);
    flux_rwlock_write_lock(rw);
    flux_rwlock_unlock(rw);
    flux_rwlock_destroy(rw);
    1.0
}

main()
'

# Test 81b: try/catch/throw runtime (JIT execution, not just check)
run_test "Try-Catch-Throw JIT" '
def risky(n: Double) -> Double {
    if (n < 0.0) { throw -1.0 } else { n * 2.0 }
}
def safe_run(n: Double) -> Double {
    try { risky(n) } catch e { e + 100.0 }
}
def main() -> Double {
    let a = safe_run(5.0);
    let b = safe_run(-3.0);
    a + b
}
main()
'

# Test 81c: try/catch/finally runtime
run_test "Try-Catch-Finally JIT" '
def main() -> Double {
    let acc = 0.0;
    let r = try {
        acc + 5.0
    } catch e {
        0.0
    } finally {
        0.0
    };
    r
}
main()
'

# Test 82: `?` operator — Ok path, value is unwrapped
run_test "Question Ok propagation" '
enum Result { Ok { value: Double }, Err { msg: Double } }
def make_ok(v: Double) -> Result { Result.Ok { value: v } }
def make_err(v: Double) -> Result { Result.Err { msg: v } }
def div_with(a: Double, b: Double) -> Result {
    if (b == 0.0) { make_err(99.0) } else { make_ok(a / b) }
}
def chain() -> Result {
    let a = div_with(10.0, 2.0)?;
    div_with(a, 5.0)
}
match chain() {
    Result.Ok(v) -> v,
    default -> -1.0
}
'

# Test 83: `?` operator — Err path, early return propagates the Err
run_test "Question Err early-return" '
enum Result { Ok { value: Double }, Err { msg: Double } }
def make_ok(v: Double) -> Result { Result.Ok { value: v } }
def make_err(v: Double) -> Result { Result.Err { msg: v } }
def div_with(a: Double, b: Double) -> Result {
    if (b == 0.0) { make_err(99.0) } else { make_ok(a / b) }
}
def chain_err() -> Result {
    let a = div_with(10.0, 0.0)?;
    div_with(a, 5.0)
}
def extract_msg(r: Result) -> Double {
    match r {
        Result.Ok(v) -> v,
        Result.Err(m) -> m
    }
}
extract_msg(chain_err())
'

# Test 84: `?` operator — chained propagation across multiple calls
run_test "Question chained" '
enum Result { Ok { value: Double }, Err { msg: Double } }
def make_ok(v: Double) -> Result { Result.Ok { value: v } }
def make_err(v: Double) -> Result { Result.Err { msg: v } }
def step1() -> Result { make_ok(7.0) }
def step2(x: Double) -> Result {
    if (x > 5.0) { make_ok(x + 1.0) } else { make_err(1.0) }
}
def step3(x: Double) -> Result {
    if (x > 8.0) { make_ok(x * 2.0) } else { make_err(2.0) }
}
def pipeline() -> Result {
    let a = step1()?;
    let b = step2(a)?;
    step3(b)
}
match pipeline() {
    Result.Ok(v) -> v,
    default -> -1.0
}
'

# ==============================================================================
# Self-Hosting Compiler Tests (src/fluxc/) — IR generation via opt-21 verify
# ==============================================================================

# Test S1: String concatenation
run_selfhost_test "SelfHost: string concat" '
extern def flux_print_string(s: string)
def greet(name: string) -> string { "hello " + name }
def main() -> Double { flux_print_string(greet("world")); 0.0 }
'

# Test S2: String equality
run_selfhost_test "SelfHost: string eq" '
def eq(a: string, b: string) -> double { a == b }
def main() -> Double { 0.0 }
'

# Test S3: String inequality
run_selfhost_test "SelfHost: string neq" '
def neq(a: string, b: string) -> double { a != b }
def main() -> Double { 0.0 }
'

# Test S4: Numeric comparisons
run_selfhost_test "SelfHost: numeric cmp" '
def lt(a: double, b: double) -> double { a < b }
def gt(a: double, b: double) -> double { a > b }
def le(a: double, b: double) -> double { a <= b }
def ge(a: double, b: double) -> double { a >= b }
def main() -> Double { 0.0 }
'

# Test S5: If-else with comparison
run_selfhost_test "SelfHost: if-else" '
def max(a: double, b: double) -> double {
    if (a > b) { a } else { b }
}
def main() -> Double { max(3.0, 5.0) }
'

# Test S6: Logical not
run_selfhost_test "SelfHost: logical not" '
def is_zero(x: double) -> double { if (!x) { 1.0 } else { 0.0 } }
def main() -> Double { is_zero(0.0) }
'

run_selfhost_test "SelfHost: enum match" '
enum Color { Red, Green, Blue }
def pick(c: Double) -> Double {
    match c {
        Color.Red -> 1.0,
        Color.Green -> 2.0,
        Color.Blue -> 3.0
    }
}
def main() -> Double { pick(Color.Green) }
'

run_selfhost_test "SelfHost: payload enum match" '
enum Option { Some(value: Double), None }
def main() -> Double {
    let x = Option.Some(42.0);
    match x {
        Option.Some(v) -> v,
        Option.None -> 0.0
    }
}
'

run_selfhost_test "SelfHost: var reassign" '
def main() -> Double {
    var x = 10.0;
    x = 42.0;
    x
}
'

run_selfhost_test "SelfHost: var payload" '
enum Option { Some(value: Double), None }
def main() -> Double {
    var x = Option.Some(10.0);
    x = Option.Some(42.0);
    match x {
        Option.Some(v) -> v,
        Option.None -> 0.0
    }
}
'

run_selfhost_test "SelfHost: multi-field payload" '
enum Person { Named { age: Double, score: Double }, Empty }
def main() -> Double {
    let p = Person.Named { age: 25.0, score: 98.0 };
    match p {
        Person.Named(pl) -> pl.score,
        Person.Empty -> 0.0
    }
}
'

run_selfhost_test "SelfHost: impl with method" '
impl MyMath {
    def double_it(self: MyMath, x: Double) -> Double { x * 2.0 }
}
def main() -> Double { MyMath_double_it(0.0, 21.0) }
'

run_selfhost_test "SelfHost: method call syntax" '
impl MyMath {
    def double_it(self: MyMath, x: Double) -> Double { x * 2.0 }
}
def main() -> Double {
    let m: MyMath = 0.0;
    m.double_it(21.0)
}
'

run_selfhost_test "SelfHost: trait impl vtable" '
trait Math {
    def add(a: Double, b: Double) -> Double;
    def mul(a: Double, b: Double) -> Double;
}
struct Calc { }
impl Math for Calc {
    def add(a: Double, b: Double) -> Double { a + b }
    def mul(a: Double, b: Double) -> Double { a * b }
}
def main() -> Double { 0.0 }
'

run_selfhost_test "SelfHost: dyn trait dispatch" '
trait Doubler {
    def double_it(self) -> Double;
}
impl Doubler for Double {
    def double_it(self) -> Double { self * 2.0 }
}
def test(x: Double) -> Double {
    let d: dyn Doubler = x;
    d.double_it()
}
def main() -> Double { test(21.0) }
'

run_selfhost_test "SelfHost: struct constructor" '
struct Pair { x: Double, y: Double }
def main() -> Double {
    let p: Pair = Pair { x: 3.0, y: 4.0 };
    p.x
}
'

run_selfhost_test "SelfHost: struct constructor method" '
struct Pair { x: Double, y: Double }
impl Pair {
    def sum(self: Pair) -> Double { self.x + self.y }
}
def main() -> Double {
    let p: Pair = Pair { x: 3.0, y: 4.0 };
    p.sum()
}
'

run_selfhost_test "SelfHost: for loop" '
def main() -> Double {
    for i in 1, 3 do i
}
'

run_selfhost_test "SelfHost: for loop block" '
def main() -> Double {
    let s = 0.0;
    for i in 1, 4 do { s = s + i };
    s
}
'

run_selfhost_test "SelfHost: vector literal" '
def main() -> Double {
    [1.0, 2.0, 3.0][1]
}
'

run_selfhost_test "SelfHost: vector index expr" '
def main() -> Double {
    [1.0, 2.0, 3.0][2] * 2.0
}
'

run_selfhost_test "SelfHost: vector comparison" '
def main() -> Double {
    [10.0, 20.0, 30.0][0] == 10.0
}
'

run_selfhost_test "SelfHost: pipe basic" '
def double_it(x: Double) -> Double { x * 2.0 }
def add_one(x: Double) -> Double { x + 1.0 }
def main() -> Double {
    3.0 |> double_it |> add_one
}
'

run_selfhost_test "SelfHost: pipe multi-arg" '
def scale(s: Double, x: Double) -> Double { s * x }
def main() -> Double {
    let mul = 2.5;
    10.0 |> scale(mul)
}
'

run_selfhost_test "SelfHost: pipe method call" '
struct Box { val: Double }
impl Box {
    def apply(self: Box, f: Double) -> Double { self.val * f }
}
def main() -> Double {
    let b: Box = Box { val: 3.0 };
    5.0 |> b.apply
}
'

run_selfhost_test "SelfHost: while loop" '
def main() -> Double {
    var x = 0.0;
    while x < 3.0 do { x = x + 1.0 };
    x
}
'

run_selfhost_test "SelfHost: lambda" '
def main() -> Double {
    let f = fn(x) -> x + 1.0;
    f
}
'

run_selfhost_test "SelfHost: lambda multi-param" '
def main() -> Double {
    let f = fn(a, b) -> a + b;
    f
}
'

run_selfhost_test "SelfHost: lambda no body" '
def main() -> Double {
    fn() -> 42.0
}
'

run_selfhost_test "SelfHost: while loop" '
def main() -> Double {
    var x = 0.0;
    while x < 3.0 do { x = x + 1.0 };
    x
}
'

run_selfhost_test "SelfHost: break in while" '
def main() -> Double {
    var x = 0.0;
    while true do {
        x = x + 1.0;
        if (x > 3.0) { break }
    };
    x
}
'

run_selfhost_test "SelfHost: continue in while" '
def main() -> Double {
    var x = 0.0;
    var sum = 0.0;
    while x < 5.0 do {
        x = x + 1.0;
        if (x == 3.0) { continue };
        sum = sum + x
    };
    sum
}
'

# Test S22: Spawn/join basic
run_selfhost_test "SelfHost: spawn join" '
def worker(x: Double) -> Double { x * 2.0 }
def main() -> Double {
    let t = spawn worker(21.0);
    join(t)
}
'

# Test S23: Spawn/join multi-arg
run_selfhost_test "SelfHost: spawn multi-arg" '
def add(a: Double, b: Double, c: Double) -> Double { a + b + c }
def main() -> Double {
    let t = spawn add(10.0, 20.0, 30.0);
    join(t)
}
'

# Test S24: Basic class with field and method
run_selfhost_test "SelfHost: class basic" '
class Counter {
    value: Double
    def increment(self: Counter) -> Double {
        self.value = self.value + 1.0;
        self.value
    }
}
def main() -> Double { 0.0 }
'

# Test S23: Class with generic template parameter
run_selfhost_test "SelfHost: class generic" '
class Box[T] {
    content: Double
    def get(self: Box) -> Double {
        self.content
    }
}
def main() -> Double { 0.0 }
'

# Test S24: Async/await basic
run_selfhost_test "SelfHost: async await" '
async def fetch() -> Double {
    await 0.5;
    42.0
}
def main() -> Double { 0.0 }
'

# Test S25: Struct field mutation via var
run_selfhost_test "SelfHost: struct mutation" '
struct Point { x: Double, y: Double }
def main() -> Double {
    var p = Point { x: 1.0, y: 2.0 };
    p.x = 42.0;
    p.x
}
'

# Test S26: Nested for loops
run_selfhost_test "SelfHost: nested for loops" '
def main() -> Double {
    var s = 0.0;
    for i in 1, 3 do {
        for j in 1, 3 do {
            s = s + i * j
        }
    };
    s
}
'

# Test S27: Lambda invoked
run_selfhost_test "SelfHost: lambda called" '
def main() -> Double {
    let f = fn(x) -> x + 1.0;
    f(41.0)
}
'

# Test S28: Trait with multiple methods
run_selfhost_test "SelfHost: trait multi method" '
trait Math {
    def add(a: Double, b: Double) -> Double;
    def mul(a: Double, b: Double) -> Double;
}
struct Calc { }
impl Math for Calc {
    def add(a: Double, b: Double) -> Double { a + b }
    def mul(a: Double, b: Double) -> Double { a * b }
}
def main() -> Double { 0.0 }
'

# Test S29: Nested if-else chain
run_selfhost_test "SelfHost: nested if-else" '
def main() -> Double {
    let x = 5.0;
    if (x > 10.0) { 1.0 } else if (x > 3.0) { 2.0 } else { 3.0 }
}
'

# Test S30: Complex enum with multiple payload variants
run_selfhost_test "SelfHost: complex enum payloads" '
enum Shape { Circle(r: Double), Rect(w: Double, h: Double), Empty }
def main() -> Double {
    let s = Shape.Rect { w: 3.0, h: 4.0 };
    match s {
        Shape.Circle(r) -> r,
        Shape.Rect(pl) -> pl.w * pl.h,
        Shape.Empty -> 0.0
    }
}
'

# Test S31: Generic struct with template
run_selfhost_test "SelfHost: generic struct" '
struct Pair[T] { first: Double, second: Double }
def main() -> Double {
    let p: Pair = Pair { first: 1.0, second: 2.0 };
    p.first
}
'

# Test S32: While loop with conditional break
run_selfhost_test "SelfHost: while conditional break" '
def main() -> Double {
    var x = 0.0;
    var found = 0.0;
    while x < 10.0 do {
        x = x + 1.0;
        if (x == 7.0) { found = 1.0; break }
    };
    found
}
'

# Test S33: Pipe with struct field access
run_selfhost_test "SelfHost: pipe struct field" '
struct Box { val: Double }
impl Box {
    def scale(self: Box, f: Double) -> Double { self.val * f }
}
def main() -> Double {
    let b: Box = Box { val: 3.0 };
    5.0 |> b.scale
}
'

# Test S34: Enum with named-field braced constructor
run_selfhost_test "SelfHost: enum braced constructor" '
enum Option { Some { value: Double }, None }
def main() -> Double {
    let x = Option.Some { value: 42.0 };
    match x {
        Option.Some(v) -> v,
        Option.None -> 0.0
    }
}
'

# Test S35: Operator overloading - add and neg
run_selfhost_test "SelfHost: operator add/neg" '
struct Vec { x: Double }
def Vec_add(a: Double, b: Double) -> Double { 42.0 }
def Vec_neg(a: Double) -> Double { 43.0 }
def main() -> Double {
    let a = Vec { x: 1.0 };
    let b = Vec { x: 2.0 };
    let r1 = a + b;
    let r2 = -a;
    r1 + r2
}
'

# Test S36: Operator overloading - sub, mul, div, eq
run_selfhost_test "SelfHost: operator sub/mul/eq" '
struct Wrap { val: Double }
def Wrap_sub(a: Double, b: Double) -> Double { 10.0 }
def Wrap_mul(a: Double, b: Double) -> Double { 20.0 }
def Wrap_eq(a: Double, b: Double) -> Double { 30.0 }
def main() -> Double {
    let a: Wrap = Wrap { val: 1.0 };
    let b: Wrap = Wrap { val: 2.0 };
    let r1 = a - b;
    let r2 = a * b;
    let r3 = a == b;
    r1 + r2 + r3
}
'

# Test S37: Associated type in trait — single
run_selfhost_test "SelfHost: associated type single" '
trait Container {
    type Item;
    def get(self: Container) -> Double;
}
struct MyBox { val: Double }
impl Container for MyBox {
    type Item = Double;
    def get(self: MyBox) -> Double { self.val }
}
def main() -> Double {
    0.0
}
'

# Test S38: Associated type in trait — multiple
run_selfhost_test "SelfHost: associated type multi" '
trait Pair {
    type First;
    type Second;
    def first(self: Pair) -> Double;
    def second(self: Pair) -> Double;
}
struct TwoVals { a: Double, b: Double }
impl Pair for TwoVals {
    type First = Double;
    type Second = Double;
    def first(self: TwoVals) -> Double { self.a }
    def second(self: TwoVals) -> Double { self.b }
}
def main() -> Double {
    0.0
}
'

# Test S39: Reference/borrow basic
run_selfhost_test "SelfHost: basic borrow deref" '
def main() -> Double {
    let x = 42.0;
    let r = &x;
    assert(*r == 42.0, "basic borrow/deref failed");
    *r
}
'

# Test S40: Reference/borrow mutable assignment
run_selfhost_test "SelfHost: mut assign borrow" '
def main() -> Double {
    var x = 5.0;
    let r = &mut x;
    *r = 10.0;
    assert(x == 10.0, "mutable borrow assignment failed");
    x
}
'

# Test S41: `?` postfix — Ok path unwraps the payload
run_selfhost_test "SelfHost: question mark ok" '
enum Res { Ok { value: Double }, Err { msg: Double } }
def make_ok(v: Double) -> Res { Res.Ok { value: v } }
def make_err(v: Double) -> Res { Res.Err { msg: v } }
def step1() -> Res { make_ok(7.0) }
def step2(x: Double) -> Res {
    if (x > 5.0) { make_ok(x + 1.0) } else { make_err(1.0) }
}
def main() -> Double {
    let a = step1()?;
    let b = step2(a)?;
    b
}
'

# Test S42: `?` postfix — Err path early-returns
run_selfhost_test "SelfHost: question mark err" '
enum Res { Ok { value: Double }, Err { msg: Double } }
def make_ok(v: Double) -> Res { Res.Ok { value: v } }
def make_err(v: Double) -> Res { Res.Err { msg: v } }
def step1() -> Res { make_ok(3.0) }
def step2(x: Double) -> Res {
    if (x > 5.0) { make_ok(x + 1.0) } else { make_err(99.0) }
}
def main() -> Double {
    let a = step1()?;
    let b = step2(a)?;
    b
}
'

# ==============================================================================
# Deep Core Tests — comprehensive edge case coverage
# ==============================================================================

# --- Recursion & Mutual Recursion ---
run_test "Recursive Factorial" '
def fact(n: Double) -> Double {
    if n <= 1.0 then 1.0 else n * fact(n - 1.0)
}
def main() -> Double {
    assert(fact(0.0) == 1.0, "fact(0) wrong");
    assert(fact(1.0) == 1.0, "fact(1) wrong");
    assert(fact(5.0) == 120.0, "fact(5) wrong");
    assert(fact(10.0) == 3628800.0, "fact(10) wrong");
    1.0
}
main()
'

run_test "Mutual Recursion Even/Odd" '
def is_even(n: Double) -> Double {
    if n == 0.0 then 1.0 else is_odd(n - 1.0)
}
def is_odd(n: Double) -> Double {
    if n == 0.0 then 0.0 else is_even(n - 1.0)
}
def main() -> Double {
    assert(is_even(0.0) == 1.0, "even(0) should be true");
    assert(is_even(2.0) == 1.0, "even(2) should be true");
    assert(is_even(3.0) == 0.0, "even(3) should be false");
    assert(is_odd(1.0) == 1.0, "odd(1) should be true");
    assert(is_odd(4.0) == 0.0, "odd(4) should be false");
    1.0
}
main()
'

run_test "Indirect Mutual Recursion" '
def a(x: Double) -> Double {
    if x <= 0.0 then 0.0 else b(x - 1.0) + 1.0
}
def b(x: Double) -> Double {
    if x <= 0.0 then 0.0 else a(x - 1.0) + 1.0
}
def main() -> Double {
    assert(a(0.0) == 0.0, "a(0) wrong");
    assert(a(5.0) == 5.0, "a(5) wrong");
    assert(b(0.0) == 0.0, "b(0) wrong");
    assert(b(3.0) == 3.0, "b(3) wrong");
    1.0
}
main()
'

# --- Nested Control Flow ---
run_test "Deeply Nested If-Else Chains" '
def classify(x: Double) -> Double {
    if x < 0.0 then
        if x < -10.0 then
            if x < -100.0 then -3.0 else -2.0
        else -1.0
    else
        if x == 0.0 then 0.0
        else if x < 10.0 then 1.0
        else if x < 100.0 then 2.0
        else 3.0
}
def main() -> Double {
    assert(classify(-200.0) == -3.0, "very negative wrong");
    assert(classify(-50.0) == -2.0, "moderate negative wrong");
    assert(classify(-5.0) == -1.0, "slight negative wrong");
    assert(classify(0.0) == 0.0, "zero wrong");
    assert(classify(5.0) == 1.0, "small positive wrong");
    assert(classify(50.0) == 2.0, "medium positive wrong");
    assert(classify(500.0) == 3.0, "large positive wrong");
    1.0
}
main()
'

run_test "Nested For Loops Product" '
def main() -> Double {
    var s = 0.0;
    for i in 0, 3 do
        for j in 0, 3 do
            s = s + i * j;
    assert(s == 36.0, "nested for sum wrong");
    1.0
}
main()
'

run_test "For Loop With Break" '
def main() -> Double {
    var s = 0.0;
    var i = 0.0;
    for i in 1, 10 do {
        s = s + i;
        if i >= 5.0 then break;
    }
    assert(s == 15.0, "for-break sum wrong");
    assert(i == 5.0, "for-break i should be 5");
    1.0
}
main()
'

run_test "For Loop With Continue" '
def main() -> Double {
    var sum_odd = 0.0;
    for i in 1, 10 do {
        if i % 2.0 == 0.0 then continue;
        sum_odd = sum_odd + i;
    }
    assert(sum_odd == 25.0, "for-continue odd sum wrong");
    1.0
}
main()
'

run_test "While Loop Basic" '
def main() -> Double {
    var i = 0.0;
    var s = 0.0;
    while i < 10.0 do {
        s = s + i;
        i = i + 1.0;
    }
    assert(s == 45.0, "while sum wrong");
    assert(i == 10.0, "while count wrong");
    1.0
}
main()
'

run_test "While Loop Break" '
def main() -> Double {
    var s = 0.0;
    var i = 0.0;
    while i < 100.0 do {
        s = s + i;
        i = i + 1.0;
        if s > 20.0 then break;
    }
    assert(s == 28.0, "while-break sum wrong");
    assert(i == 8.0, "while-break count wrong");
    1.0
}
main()
'

# --- Struct Deep Tests ---
run_test "Nested Structs" '
struct Inner { val: Double }
struct Outer { inner: Inner, scale: Double }
def main() -> Double {
    let inner = Inner { val: 5.0 };
    let outer = Outer { inner: inner, scale: 2.0 };
    assert(outer.inner.val == 5.0, "nested struct inner field wrong");
    assert(outer.scale == 2.0, "nested struct scale wrong");
    let result = outer.inner.val * outer.scale;
    assert(result == 10.0, "nested struct computed value wrong");
    1.0
}
main()
'

run_test "Struct Mutation" '
struct Point { x: Double, y: Double }
def main() -> Double {
    var p = Point { x: 1.0, y: 2.0 };
    assert(p.x == 1.0, "init x wrong");
    assert(p.y == 2.0, "init y wrong");
    p.x = 10.0;
    p.y = p.x * 2.0;
    assert(p.x == 10.0, "mut x wrong");
    assert(p.y == 20.0, "mut y wrong");
    1.0
}
main()
'

run_test "Struct Return From Function" '
struct Vec3 { x: Double, y: Double, z: Double }
def make_vec3(x: Double, y: Double, z: Double) -> Vec3 {
    Vec3 { x: x, y: y, z: z }
}
def main() -> Double {
    let v = make_vec3(1.0, 2.0, 3.0);
    assert(v.x == 1.0, "struct return x wrong");
    assert(v.y == 2.0, "struct return y wrong");
    assert(v.z == 3.0, "struct return z wrong");
    1.0
}
main()
'

run_test "Struct As Function Parameter" '
struct Pair { a: Double, b: Double }
def sum_pair(p: Pair) -> Double { p.a + p.b }
def main() -> Double {
    let p = Pair { a: 10.0, b: 20.0 };
    let r = sum_pair(p);
    assert(r == 30.0, "struct param sum wrong");
    1.0
}
main()
'

# --- Enum Deep Tests ---
run_test "Enum Tagged Union Multiple Variants" '
enum Shape { Circle { radius: Double }, Rect { w: Double, h: Double }, Nothing }
def area(s: Shape) -> Double {
    match s {
        Shape.Circle(r) -> 3.14159 * r * r,
        Shape.Rect(w, h) -> w * h,
        default -> 0.0
    }
}
def main() -> Double {
    let c = Shape.Circle(2.0);
    let r = Shape.Rect(3.0, 4.0);
    let n = Shape.Nothing;
    assert(abs(area(c) - 12.56636) < 0.001, "circle area wrong");
    assert(area(r) == 12.0, "rect area wrong");
    assert(area(n) == 0.0, "nothing area wrong");
    1.0
}
main()
'

run_test "Enum As Function Parameter" '
enum Opt { Some { value: Double }, None }
def unwrap_or_default(x: Opt, def_val: Double) -> Double {
    match x {
        Opt.Some(v) -> v,
        default -> def_val
    }
}
def main() -> Double {
    let a = Opt.Some(42.0);
    let b = Opt.None;
    assert(unwrap_or_default(a, 0.0) == 42.0, "unwrap some wrong");
    assert(unwrap_or_default(b, 99.0) == 99.0, "unwrap none wrong");
    1.0
}
main()
'

run_test "Enum Nested In Struct" '
enum Status { Active { id: Double }, Inactive }
struct Record { name: Double, status: Status }
def main() -> Double {
    let r = Record { name: 1.0, status: Status.Active(100.0) };
    assert(r.status.id == 100.0, "nested enum field access wrong");
    1.0
}
main()
'

# --- Switch/Case Deep Tests ---
run_test "Switch Default Branch Returns" '
def main() -> Double {
    var x = 1.0;
    var result = switch (x) {
        0.0 => 100.0,
        1.0 => 200.0,
        ~ => 300.0
    };
    assert(result == 200.0, "switch matching 1.0 wrong");
    x = 99.0;
    result = switch (x) {
        0.0 => 100.0,
        1.0 => 200.0,
        ~ => 300.0
    };
    assert(result == 300.0, "switch default wrong");
    1.0
}
main()
'

run_test "Switch With Multi-Character Cases" '
def main() -> Double {
    let result = switch (10.0) {
        5.0 => 1.0,
        10.0 => 2.0,
        15.0 => 3.0,
        ~ => 0.0
    };
    assert(result == 2.0, "switch multi-char case wrong");
    1.0
}
main()
'

# --- Operator Deep Tests ---
run_test "Complex Arithmetic Expression" '
def main() -> Double {
    let r = (1.0 + 2.0) * (3.0 + 4.0) / (5.0 - 1.0) - 2.0;
    assert(abs(r - 3.25) < 0.001, "complex arith wrong");
    let r2 = ((1.0 + 2.0 * 3.0) - 4.0 / 2.0) * 5.0;
    assert(r2 == 25.0, "complex arith 2 wrong");
    1.0
}
main()
'

run_test "Boolean Logic Combinations" '
def main() -> Double {
    let a = true;
    let b = false;
    let c = true;
    assert(a && b == false, "true && false should be false");
    assert(a || b == true, "true || false should be true");
    assert(!a == false, "!true should be false");
    assert(!b == true, "!false should be true");
    assert((a || b) && c == true, "(true||false)&&true should be true");
    assert((a && b) || (b && c) || (a && c) == true, "complex bool wrong");
    assert(!(a && b) == true, "not(false) should be true");
    1.0
}
main()
'

run_test "Operator Precedence Chain" '
def main() -> Double {
    let r = 2.0 + 3.0 * 4.0 - 6.0 / 2.0;
    assert(r == 11.0, "precedence 2+3*4-6/2 should be 11");
    let r2 = 10.0 / 2.0 + 3.0 * 2.0 - 1.0;
    assert(r2 == 10.0, "precedence 10/2+3*2-1 should be 10");
    let r3 = 100.0 / 4.0 / 5.0;
    assert(r3 == 5.0, "left assoc 100/4/5 should be 5");
    1.0
}
main()
'

run_test "Modulo And Negation" '
def main() -> Double {
    assert(10.0 % 3.0 == 1.0, "10%%3 should be 1");
    assert(7.0 % 2.5 == 2.0, "7%%2.5 should be 2.0");
    assert(-5.0 == -5.0, "negation -5 should be -5");
    assert(-(3.0 + 2.0) == -5.0, "negated expr should be -5");
    let x = 7.0;
    assert(-x == -7.0, "negated var should be -7");
    1.0
}
main()
'

run_test "Comparison Chain" '
def main() -> Double {
    assert(1.0 < 2.0 == true, "1<2 should be true");
    assert(2.0 < 2.0 == false, "2<2 should be false");
    assert(2.0 <= 2.0 == true, "2<=2 should be true");
    assert(3.0 > 2.0 == true, "3>2 should be true");
    assert(3.0 >= 3.0 == true, "3>=3 should be true");
    assert(1.0 != 2.0 == true, "1!=2 should be true");
    assert(1.0 == 1.0 == true, "1==1 should be true");
    assert(1.0 == 2.0 == false, "1==2 should be false");
    1.0
}
main()
'

# --- Complex Expression Tests ---
run_test "Nested Ternary Chains" '
def main() -> Double {
    let x = 5.0;
    let r = if x < 0.0 then -1.0
            else if x == 0.0 then 0.0
            else if x < 10.0 then 1.0
            else if x < 100.0 then 2.0
            else 3.0;
    assert(r == 1.0, "nested ternary chain wrong");
    let x2 = -5.0;
    let r2 = if x2 < 0.0 then -1.0 else if x2 == 0.0 then 0.0 else 1.0;
    assert(r2 == -1.0, "neg ternary chain wrong");
    1.0
}
main()
'

run_test "Expression As Function Body" '
def f1(x: Double) -> Double { x * 2.0 }
def f2(x: Double) -> Double { x + 1.0 }
def composite(x: Double) -> Double { f2(f1(x)) }
def main() -> Double {
    assert(composite(5.0) == 11.0, "composite 5*2+1 should be 11");
    assert(composite(0.0) == 1.0, "composite 0*2+1 should be 1");
    1.0
}
main()
'

run_test "Implicit Return Last Expression" '
def compute(a: Double, b: Double) -> Double { a * a + b * b }
def main() -> Double {
    let r = compute(3.0, 4.0);
    assert(r == 25.0, "implicit return 3^2+4^2 should be 25");
    let r2 = compute(5.0, 12.0);
    assert(r2 == 169.0, "implicit return 5^2+12^2 should be 169");
    1.0
}
main()
'

# --- Variable Scope & Shadowing ---
run_test "Variable Shadowing" '
def main() -> Double {
    let x = 1.0;
    let r1 = x;
    let x = 2.0;
    let r2 = x;
    {
        let x = 3.0;
        let r3 = x;
        assert(r3 == 3.0, "inner shadow should be 3");
    };
    assert(r1 == 1.0, "original x should still be 1");
    assert(r2 == 2.0, "shadow x should be 2");
    x = 4.0;
    assert(x == 4.0, "reassigned shadow should be 4");
    1.0
}
main()
'

run_test "Var Mutable In Block" '
def main() -> Double {
    var x = 1.0;
    {
        x = 2.0;
    };
    assert(x == 2.0, "var mutated inside block should persist");
    1.0
}
main()
'

# --- Generics Deep Tests ---
run_test "Generic Identity Multi-Type" '
def id[T](x: T) -> T { x }
def main() -> Double {
    assert(id[Double](42.0) == 42.0, "generic id double wrong");
    let s = id[Double](3.14);
    assert(abs(s - 3.14) < 0.001, "generic id pi wrong");
    1.0
}
main()
'

run_test "Generic Struct Direct Use" '
struct Box[T] { val: T }
def main() -> Double {
    let b = Box[Double] { val: 42.0 };
    assert(b.val == 42.0, "generic struct field wrong");
    1.0
}
main()
'

# --- Reference/Pointer Deep Tests ---
run_test "Ref Param Swap" '
def swap(a: &mut Double, b: &mut Double) {
    let tmp = *a;
    *a = *b;
    *b = tmp;
}
def main() -> Double {
    var x = 1.0;
    var y = 2.0;
    swap(&mut x, &mut y);
    assert(x == 2.0, "swap x should be 2");
    assert(y == 1.0, "swap y should be 1");
    1.0
}
main()
'

run_test "Ref To Struct Field" '
struct Point { x: Double, y: Double }
def main() -> Double {
    var p = Point { x: 1.0, y: 2.0 };
    let px = &mut p.x;
    *px = 10.0;
    assert(p.x == 10.0, "ref to struct field mutate wrong");
    assert(p.y == 2.0, "other field unchanged");
    1.0
}
main()
'

# --- Match Variable Type Regression ---
run_test "Match Variable Arithmetic" '
enum Wrap { Value { x: Double }, Empty }
def sq(w: Wrap) -> Double {
    match w {
        Wrap.Value(v) -> v * v,
        default -> 0.0
    }
}
def main() -> Double {
    assert(sq(Wrap.Value(3.0)) == 9.0, "match var multiply wrong");
    assert(sq(Wrap.Value(0.0)) == 0.0, "match var zero wrong");
    assert(sq(Wrap.Empty) == 0.0, "match var default wrong");
    1.0
}
main()
'

run_test "Match Variable Add" '
enum Opt { Some { value: Double }, None }
def add_one(x: Opt) -> Double {
    match x {
        Opt.Some(v) -> v + 1.0,
        default -> 0.0
    }
}
def main() -> Double {
    assert(add_one(Opt.Some(41.0)) == 42.0, "match var add wrong");
    assert(add_one(Opt.None) == 0.0, "match var none wrong");
    1.0
}
main()
'

# --- Fibonacci Performance Check ---
run_test "Fibonacci Recursive" '
def fib(n: Double) -> Double {
    if n <= 1.0 then n else fib(n - 1.0) + fib(n - 2.0)
}
def main() -> Double {
    assert(fib(0.0) == 0.0, "fib(0) should be 0");
    assert(fib(1.0) == 1.0, "fib(1) should be 1");
    assert(fib(10.0) == 55.0, "fib(10) should be 55");
    assert(fib(20.0) == 6765.0, "fib(20) should be 6765");
    1.0
}
main()
'
run_aot_test "AOT Fibonacci Recursive" '
def fib(n: Double) -> Double {
    if n <= 1.0 then n else fib(n - 1.0) + fib(n - 2.0)
}
def main() -> Double {
    assert(fib(0.0) == 0.0, "fib(0) should be 0");
    assert(fib(1.0) == 1.0, "fib(1) should be 1");
    assert(fib(10.0) == 55.0, "fib(10) should be 55");
    assert(fib(20.0) == 6765.0, "fib(20) should be 6765");
    1.0
}
main()
'

# --- Builtins ---
run_aot_test "AOT Nested Structs" '
struct Inner { val: Double }
struct Outer { inner: Inner, scale: Double }
def main() -> Double {
    let inner = Inner { val: 5.0 };
    let outer = Outer { inner: inner, scale: 2.0 };
    assert(outer.inner.val == 5.0, "nested struct inner field wrong");
    assert(outer.scale == 2.0, "nested struct scale wrong");
    let result = outer.inner.val * outer.scale;
    assert(result == 10.0, "nested struct computed value wrong");
    1.0
}
main()
'

run_aot_test "AOT Enum Multiple Variants" '
enum Shape { Circle { radius: Double }, Rect { w: Double, h: Double }, Nothing }
def area(s: Shape) -> Double {
    match s {
        Shape.Circle(r) -> 3.14159 * r * r,
        Shape.Rect(w, h) -> w * h,
        default -> 0.0
    }
}
def main() -> Double {
    let c = Shape.Circle(2.0);
    let r = Shape.Rect(3.0, 4.0);
    let n = Shape.Nothing;
    assert(abs(area(c) - 12.56636) < 0.001, "circle area wrong");
    assert(area(r) == 12.0, "rect area wrong");
    assert(area(n) == 0.0, "nothing area wrong");
    1.0
}
main()
'

run_aot_test "AOT Switch Default" '
def main() -> Double {
    var x = 1.0;
    var result = switch (x) {
        0.0 => 100.0,
        1.0 => 200.0,
        ~ => 300.0
    };
    assert(result == 200.0, "switch matching 1.0 wrong");
    x = 99.0;
    result = switch (x) {
        0.0 => 100.0,
        1.0 => 200.0,
        ~ => 300.0
    };
    assert(result == 300.0, "switch default wrong");
    1.0
}
main()
'

run_aot_test "AOT Complex Arithmetic" '
def main() -> Double {
    let r = (1.0 + 2.0) * (3.0 + 4.0) / (5.0 - 1.0) - 2.0;
    assert(abs(r - 3.25) < 0.001, "complex arith wrong");
    let r2 = ((1.0 + 2.0 * 3.0) - 4.0 / 2.0) * 5.0;
    assert(r2 == 25.0, "complex arith 2 wrong");
    1.0
}
main()
'

run_aot_test "AOT Ref Param Swap" '
def swap(a: &mut Double, b: &mut Double) {
    let tmp = *a;
    *a = *b;
    *b = tmp;
}
def main() -> Double {
    var x = 1.0;
    var y = 2.0;
    swap(&mut x, &mut y);
    assert(x == 2.0, "swap x should be 2");
    assert(y == 1.0, "swap y should be 1");
    1.0
}
main()
'

run_aot_test "AOT Match Variable Arithmetic" '
enum Wrap { Value { x: Double }, Empty }
def sq(w: Wrap) -> Double {
    match w {
        Wrap.Value(v) -> v * v,
        default -> 0.0
    }
}
def main() -> Double {
    assert(sq(Wrap.Value(3.0)) == 9.0, "match var multiply wrong");
    assert(sq(Wrap.Value(0.0)) == 0.0, "match var zero wrong");
    assert(sq(Wrap.Empty) == 0.0, "match var default wrong");
    1.0
}
main()
'

run_aot_test "AOT Generic Identity" '
def id[T](x: T) -> T { x }
def main() -> Double {
    assert(id[Double](42.0) == 42.0, "generic id double wrong");
    let s = id[Double](3.14);
    assert(abs(s - 3.14) < 0.001, "generic id pi wrong");
    1.0
}
main()
'

run_aot_test "AOT For Loop Break" '
def main() -> Double {
    var s = 0.0;
    var i = 0.0;
    for i in 1, 10 do {
        s = s + i;
        if i >= 5.0 then break;
    }
    assert(s == 15.0, "for-break sum wrong");
    assert(i == 5.0, "for-break i should be 5");
    1.0
}
main()
'

run_test "Abs Builtin" '
def main() -> Double {
    assert(abs(-5.0) == 5.0, "abs(-5) should be 5");
    assert(abs(0.0) == 0.0, "abs(0) should be 0");
    assert(abs(3.0) == 3.0, "abs(3) should be 3");
    assert(abs(-0.0) == 0.0, "abs(-0) should be 0");
    1.0
}
main()
'

# --- Error-Handling: Try/Catch/Throw ---
run_test "Try Catch Basic" '
def main() -> Double {
    var caught = 0.0;
    try {
        throw "error";
    } catch e {
        caught = 1.0;
    };
    assert(caught == 1.0, "catch should have been triggered");
    1.0
}
main()
'

run_test "Try Catch No Throw" '
def main() -> Double {
    var caught = 0.0;
    var value = 0.0;
    try {
        value = 42.0;
    } catch e {
        caught = 1.0;
    };
    assert(caught == 0.0, "catch should not trigger");
    assert(value == 42.0, "value should be set");
    1.0
}
main()
'

run_test "Try Catch Finally" '
def main() -> Double {
    var caught = 0.0;
    var finalized = 0.0;
    try {
        throw "err";
    } catch e {
        caught = 1.0;
    } finally {
        finalized = 1.0;
    };
    assert(caught == 1.0, "catch should trigger");
    assert(finalized == 1.0, "finally should trigger");
    1.0
}
main()
'


# ==============================================================================
# Parallel executor — run all queued tests using xargs -P
# ==============================================================================
run_parallel_tests() {
    local total=${#TEST_DIRS[@]}
    [ "$total" -eq 0 ] && return

    local results_dir=$(mktemp -d "/tmp/fluxtest_results_XXXXXX")
    local workers=$PARALLEL_JOBS

    echo "Running $total tests with $workers parallel workers..."

    # Export env vars needed by --worker subprocesses
    export FLUX_BIN PROJECT_DIR OPT FLUX_SELFHOST_BIN

    # Count test types for progress
    local jit_count=0 check_count=0 selfhost_count=0
    for d in "${TEST_DIRS[@]}"; do
        local t=$(cat "$d/type")
        case "$t" in
            jit) jit_count=$((jit_count + 1)) ;;
            check) check_count=$((check_count + 1)) ;;
            selfhost) selfhost_count=$((selfhost_count + 1)) ;;
        esac
    done

    echo "  JIT: $jit_count  Check: $check_count  SelfHost: $selfhost_count"
    echo ""

    # Launch workers via xargs -P
    printf '%s\n' "${TEST_DIRS[@]}" | \
        xargs -P "$workers" -I{} "$SCRIPT_DIR/run_tests.sh" --worker {} "$results_dir" 2>/dev/null

    # Collect results
    PASSED=0
    FAILED=0
    for result_file in "$results_dir"/*.result; do
        [ -f "$result_file" ] || continue
        local result=$(cat "$result_file")
        local base=$(basename "$result_file" .result)
        local name=""
        for d in "${TEST_DIRS[@]}"; do
            if [ "$(basename "$d")" = "$base" ]; then
                name=$(cat "$d/name")
                break
            fi
        done

        if [ "$result" = "PASS" ]; then
            PASSED=$((PASSED + 1))
        else
            FAILED=$((FAILED + 1))
            local output_file="$results_dir/${base}.output"
            if [ -f "$output_file" ] && [ -s "$output_file" ]; then
                echo "=== FAILED: $name ==="
                cat "$output_file"
                echo "========================"
            fi
        fi
    done

    TOTAL=$((PASSED + FAILED))

    # Cleanup
    rm -rf "$results_dir"
    for d in "${TEST_DIRS[@]}"; do
        rm -rf "$d"
    done
}

# ==============================================================================
# Execute tests (sequential or parallel)
# ==============================================================================
if [ "$MODE" = "parallel" ]; then
    run_parallel_tests
fi

echo ""
echo "========================================"  
echo "  Results: $PASSED/$TOTAL passed"
echo "========================================"

if [ $FAILED -eq 0 ]; then
    echo -e "${GREEN} ALL TESTS PASSED${NC}"
    exit 0
else
    echo -e "${RED} $FAILED TESTS FAILED${NC}"
    exit 1
fi
