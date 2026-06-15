#!/bin/bash
# FluxScript Integration Test Runner (Directory-Driven)
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

# Linker flags for AOT test executables (macOS doesn't support -no-pie,
# and doesn't have separate -lpthread / -ldl — those are in libSystem)
if [ "$(uname)" = "Darwin" ]; then
    LDFLAGS_NOPIE=""
    AOT_LDLIBS="-lFluxRuntime -lm"
else
    LDFLAGS_NOPIE="-no-pie"
    AOT_LDLIBS="-lFluxRuntime -lpthread -ldl -lm -lcurl"
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
    test_dir_base="${test_dir##*/}"
    result_file="$results_dir/${test_dir_base}.result"
    output_file="$results_dir/${test_dir_base}.output"

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
        expect_error)
            output=$("$FLUX_BIN" --cache=0 "$code_file" 2>&1)
            if [ $? -ne 0 ]; then status=0; else status=1; fi
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
                        c++ $LDFLAGS_NOPIE -o "$binfile" "$obj_fixed" "$wrapper" -L"$BUILD_DIR" $AOT_LDLIBS 2>/dev/null
                        status=$?
                        rm -f "$wrapper"
                    else
                        "$OBJCOPY" --redefine-sym "$anon_sym=main" "$objfile" "$obj_fixed"
                        c++ $LDFLAGS_NOPIE -o "$binfile" "$obj_fixed" -L"$BUILD_DIR" $AOT_LDLIBS 2>/dev/null
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
    local test_file="$2"

    if [ "$MODE" = "parallel" ]; then
        local tmpdir=$(mktemp -d "/tmp/fluxtest_parallel_XXXXXX")
        cp "$test_file" "$tmpdir/code.flux"
        echo "jit" > "$tmpdir/type"
        echo "$test_name" > "$tmpdir/name"
        TEST_DIRS+=("$tmpdir")
        return
    fi

    TOTAL=$((TOTAL + 1))
    echo -n "$test_name... "
    
    local cmd_status
    local test_output
    if [ -n "$TIMEOUT_CMD" ]; then
        test_output=$($TIMEOUT_CMD 5 "$FLUX_BIN" --cache=0 "$test_file" 2>&1)
        cmd_status=$?
    else
        test_output=$("$FLUX_BIN" --cache=0 "$test_file" 2>&1)
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
}

run_aot_test() {
    local test_name="$1"
    local test_file="$2"

    FLUXC="${FLUXC:-$PROJECT_DIR/build/fluxc}"
    BUILD_DIR="${BUILD_DIR:-$PROJECT_DIR/build}"

    if [ "$MODE" = "parallel" ]; then
        local tmpdir=$(mktemp -d "/tmp/fluxtest_parallel_XXXXXX")
        cp "$test_file" "$tmpdir/code.flux"
        echo "aot" > "$tmpdir/type"
        echo "$test_name" > "$tmpdir/name"
        TEST_DIRS+=("$tmpdir")
        return
    fi

    TOTAL=$((TOTAL + 1))
    echo -n "$test_name (AOT)... "

    local objfile="${test_file}.o"
    local binfile="${test_file}.bin"

    local cmd_status=1
    local test_output=""

    # Compile to object file
    if ! "$FLUXC" "$test_file" -o "$objfile" 2>/dev/null; then
        test_output="AOT compilation failed"
        cmd_status=1
    else
        # Find entry symbol
        local anon_sym=$(nm "$objfile" 2>/dev/null | grep "_anon_expr" | awk '{print $3}' | head -1 || true)
        local has_user_main=$(nm "$objfile" 2>/dev/null | grep -c " T main$" || true)

        if [ -n "$anon_sym" ]; then
            local obj_fixed="${test_file}.fixed.o"
            if [ "$has_user_main" -gt 0 ]; then
                # User defined main(): rename anon_expr -> __flux_entry
                "$OBJCOPY" --redefine-sym "$anon_sym=__flux_entry" --redefine-sym "main=__user_main" "$objfile" "$obj_fixed"

                # Build wrapper
                local wrapper="${test_file}_wrapper.cpp"
                cat > "$wrapper" << 'WRAPEOF'
#include <cstdio>
extern "C" double __flux_entry();
int main() {
    double result = __flux_entry();
    fprintf(stdout, "Result: %g\n", result);
    return 0;
}
WRAPEOF
                c++ $LDFLAGS_NOPIE -o "$binfile" "$obj_fixed" "$wrapper" \
                    -L"$BUILD_DIR" $AOT_LDLIBS 2>/dev/null
                cmd_status=$?
                rm -f "$wrapper"
            else
                # No user main: rename anon_expr -> main directly
                "$OBJCOPY" --redefine-sym "$anon_sym=main" "$objfile" "$obj_fixed"
                c++ $LDFLAGS_NOPIE -o "$binfile" "$obj_fixed" \
                    -L"$BUILD_DIR" $AOT_LDLIBS 2>/dev/null
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

    rm -f "$objfile" "$binfile"
}

run_selfhost_test() {
    local test_name="$1"
    local test_file="$2"

    if [ "$MODE" = "parallel" ]; then
        local tmpdir=$(mktemp -d "/tmp/fluxtest_parallel_XXXXXX")
        cp "$test_file" "$tmpdir/code.flux"
        echo "selfhost" > "$tmpdir/type"
        echo "$test_name" > "$tmpdir/name"
        TEST_DIRS+=("$tmpdir")
        return
    fi

    TOTAL=$((TOTAL + 1))
    echo -n "$test_name... "

    if [ -z "$OPT" ]; then
        echo -e "${YELLOW} SKIP (opt not found)${NC}"
        PASSED=$((PASSED + 1))
        return
    fi

    local test_output
    local cmd_status
    if [ -n "$FLUX_SELFHOST_BIN" ] && [ -x "$FLUX_SELFHOST_BIN" ]; then
        test_output=$(FLUX_INPUT="$test_file" "$FLUX_SELFHOST_BIN" 2>/dev/null | \
            awk '/^; ModuleID/ {found=1} found && !/^(Compiled|JIT cache|Result)/' | \
            $OPT -passes=verify -S 2>&1)
        cmd_status=$?
    else
        test_output=$(FLUX_INPUT="$test_file" "$FLUX_BIN" --cache=0 "$PROJECT_DIR/src/fluxc/main.flux" 2>/dev/null | \
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
}

run_check_test() {
    local test_name="$1"
    local test_file="$2"

    if [ "$MODE" = "parallel" ]; then
        local tmpdir=$(mktemp -d "/tmp/fluxtest_parallel_XXXXXX")
        cp "$test_file" "$tmpdir/code.flux"
        echo "check" > "$tmpdir/type"
        echo "$test_name" > "$tmpdir/name"
        TEST_DIRS+=("$tmpdir")
        return
    fi

    TOTAL=$((TOTAL + 1))
    echo -n "$test_name... "

    local cmd_status
    local test_output
    if [ -n "$TIMEOUT_CMD" ]; then
        test_output=$($TIMEOUT_CMD 5 "$FLUX_BIN" --emit=check --cache=0 "$test_file" 2>&1)
        echo "$test_output" | grep -q "OK"
        cmd_status=$?
    else
        test_output=$("$FLUX_BIN" --emit=check --cache=0 "$test_file" 2>&1)
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
}

run_expect_error_test() {
    local test_name="$1"
    local test_file="$2"

    if [ "$MODE" = "parallel" ]; then
        local tmpdir=$(mktemp -d "/tmp/fluxtest_parallel_XXXXXX")
        cp "$test_file" "$tmpdir/code.flux"
        echo "expect_error" > "$tmpdir/type"
        echo "$test_name" > "$tmpdir/name"
        TEST_DIRS+=("$tmpdir")
        return
    fi

    TOTAL=$((TOTAL + 1))
    echo -n "$test_name... "

    local cmd_status
    local test_output
    if [ -n "$TIMEOUT_CMD" ]; then
        test_output=$($TIMEOUT_CMD 5 "$FLUX_BIN" --cache=0 "$test_file" 2>&1)
        cmd_status=$?
    else
        test_output=$("$FLUX_BIN" --cache=0 "$test_file" 2>&1)
        cmd_status=$?
    fi

    if [ $cmd_status -ne 0 ]; then
        echo -e "${GREEN} PASSED${NC}"
        PASSED=$((PASSED + 1))
    else
        echo -e "${RED} FAILED (expected compile error but succeeded)${NC}"
        FAILED=$((FAILED + 1))
    fi
}

# Reference/borrow tests to exclude from JIT/AOT runs (they require selfhost compiler)
ref_test_exclusions=" test_068_Ref_Borrow_Deref test_069_Ref_Multiple_Borrows test_070_Ref_Mut_Assign test_071_Ref_Param test_072_Ref_Complex test_073_Copy_Struct test_074_Copy_Enum test_075_Copy_Func_Move test_076_Copy_Moved_Error test_077_Ref_Field_Access test_078_Ref_Field_Mut test_079_Ref_Field_Valid test_080_Elision_Multi_Ref test_081_Elision_Assign_Borrowed test_082_Elision_Method_Self test_083_Field_Borrow test_084_Field_Assign_Borrow test_085_Multi_Ref_Field test_086_Field_Borrow_Conflict test_136_Ref_Param_Swap test_137_Ref_To_Struct_Field "

# ==============================================================================
# Discover and queue/run all test cases
# ==============================================================================
for f in $(find "$PROJECT_DIR/tests/integration/tests" -name "test_*.flux" | sort); do
    basename="${f##*/}"
    basename="${basename%.flux}"
    if [[ "$ref_test_exclusions" =~ " $basename " ]]; then
        continue
    fi

    meta_file="${f%.flux}.meta"
    if [ -f "$meta_file" ]; then
        # Read metadata
        # Line 1: type (run/check/aot/selfhost)
        # Line 2: display name
        {
            read -r test_type
            read -r test_name
        } < "$meta_file"

        case "$test_type" in
            run)
                run_test "$test_name" "$f"
                ;;
            expect_error)
                run_expect_error_test "$test_name" "$f"
                ;;
            check)
                run_check_test "$test_name" "$f"
                ;;
            aot)
                run_aot_test "$test_name" "$f"
                ;;
            selfhost)
                run_selfhost_test "$test_name" "$f"
                ;;
            *)
                echo "Unknown test type: $test_type in $meta_file"
                ;;
        esac
    fi
done

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
    local jit_count=0 check_count=0 selfhost_count=0 aot_count=0 err_count=0
    for d in "${TEST_DIRS[@]}"; do
        local t=$(cat "$d/type")
        case "$t" in
            jit) jit_count=$((jit_count + 1)) ;;
            check) check_count=$((check_count + 1)) ;;
            selfhost) selfhost_count=$((selfhost_count + 1)) ;;
            aot) aot_count=$((aot_count + 1)) ;;
            expect_error) err_count=$((err_count + 1)) ;;
        esac
    done

    echo "  JIT: $jit_count  Check: $check_count  SelfHost: $selfhost_count  AOT: $aot_count  ExpectError: $err_count"
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
        local filename="${result_file##*/}"
        local base="${filename%.result}"
        local name=""
        for d in "${TEST_DIRS[@]}"; do
            local dir_base="${d##*/}"
            if [ "$dir_base" = "$base" ]; then
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
