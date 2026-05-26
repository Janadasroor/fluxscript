#!/bin/bash
# Run all FluxScript feature tests
# Use FLUX env var from CTest, or default to ../build/flux
FLUX="${FLUX:-$(dirname "$0")/../build/flux}"
if [ ! -x "$FLUX" ]; then
    echo "flux binary not found at $FLUX" >&2
    exit 1
fi
DIR="$(dirname "$0")"
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

run_tests_from() {
    local TEST_FILE="$1"
    for func in $(grep "^def test_" "$TEST_FILE" | sed 's/def //' | sed 's/(.*//'); do
        echo -n "  $func... "
        local cmd_status
        if [ -n "$TIMEOUT_CMD" ]; then
            $TIMEOUT_CMD 10 "$FLUX" --entry="$func" --cache=false "$TEST_FILE" > flux_test_out.txt 2>&1
            cmd_status=$?
        else
            "$FLUX" --entry="$func" --cache=false "$TEST_FILE" > flux_test_out.txt 2>&1
            cmd_status=$?
        fi

        if [ $cmd_status -eq 0 ]; then
            echo "PASSED"
            PASSED=$((PASSED + 1))
        else
            echo "FAILED"
            cat flux_test_out.txt
            FAILED=$((FAILED + 1))
        fi
        rm -f flux_test_out.txt
    done
}

run_tests_from "$DIR/flux/test_all_features.flux"
run_tests_from "$DIR/flux/test_stdlib.flux"
echo ""
echo "  Results: $PASSED/$((PASSED + FAILED)) passed"
exit $FAILED
