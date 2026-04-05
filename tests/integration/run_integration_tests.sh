#!/bin/bash
# FluxScript Integration Test Runner
# Runs all integration tests and generates report

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"
FLUX_BIN="$PROJECT_ROOT/build/flux-minimal"

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "========================================"
echo "  FluxScript Integration Test Suite"
echo "========================================"
echo ""

# Check if flux-minimal binary exists
if [ ! -f "$FLUX_BIN" ]; then
    echo -e "${RED}❌ Error: flux-minimal binary not found${NC}"
    echo "Please run ./scripts/build_guard.sh first"
    exit 1
fi

# Test counter
TOTAL=0
PASSED=0
FAILED=0

# Run each test file
for test_file in "$SCRIPT_DIR"/test_*.flux; do
    test_name=$(basename "$test_file" .flux)
    TOTAL=$((TOTAL + 1))
    
    echo -n "Running $test_name... "
    
    # Run test with timeout
    if timeout 10 "$FLUX_BIN" "$test_file" > /dev/null 2>&1; then
        echo -e "${GREEN}✅ PASSED${NC}"
        PASSED=$((PASSED + 1))
    else
        echo -e "${RED}❌ FAILED${NC}"
        FAILED=$((FAILED + 1))
    fi
done

echo ""
echo "========================================"
echo "  Test Summary"
echo "========================================"
echo -e "Total:  $TOTAL"
echo -e "Passed: ${GREEN}$PASSED${NC}"
echo -e "Failed: ${RED}$FAILED${NC}"
echo ""

if [ $FAILED -eq 0 ]; then
    echo -e "${GREEN}✅ ALL TESTS PASSED${NC}"
    exit 0
else
    echo -e "${RED}❌ SOME TESTS FAILED${NC}"
    exit 1
fi
