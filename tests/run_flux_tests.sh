#!/bin/bash
# Run all FluxScript feature tests
# Use FLUX env var from CTest, or default to ../build/flux
FLUX="${FLUX:-$(dirname "$0")/../build/flux}"
if [ ! -x "$FLUX" ]; then
    echo "flux binary not found at $FLUX" >&2
    exit 1
fi
DIR="$(dirname "$0")"
TEST_FILE="$DIR/flux/test_all_features.flux"
PASSED=0
FAILED=0

run_tests_from() {
    local TEST_FILE="$1"
    for func in $(grep "^def test_" "$TEST_FILE" | sed 's/def //' | sed 's/(.*//'); do
        echo -n "  $func... "
        if timeout 10 "$FLUX" --entry="$func" --cache=false "$TEST_FILE" > /tmp/flux_test_out.txt 2>&1; then
            echo "PASSED"
            PASSED=$((PASSED + 1))
        else
            echo "FAILED"
            cat /tmp/flux_test_out.txt
            FAILED=$((FAILED + 1))
        fi
    done
}

run_tests_from "$DIR/flux/test_all_features.flux"
run_tests_from "$DIR/flux/test_stdlib.flux"
echo ""
echo "  Results: $PASSED/$((PASSED + FAILED)) passed"
exit $FAILED
