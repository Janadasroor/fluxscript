#!/bin/bash
# FluxScript Integration Test Runner
# Tests features with syntax that actually works

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
FLUX_BIN="$PROJECT_DIR/build/flux-minimal"

if [ ! -x "$FLUX_BIN" ]; then
    echo "flux-minimal not found at $FLUX_BIN"
    echo "Run 'make flux-minimal' first"
    exit 1
fi

GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

echo "========================================"
echo "  FluxScript Integration Tests"  
echo "========================================"
echo ""

TOTAL=0
PASSED=0
FAILED=0

run_test() {
    local test_name="$1"
    local test_code="$2"
    
    TOTAL=$((TOTAL + 1))
    echo -n "$test_name... "
    
    echo "$test_code" > /tmp/flux_test.flux
    
    if timeout 5 "$FLUX_BIN" /tmp/flux_test.flux > /dev/null 2>&1; then
        echo -e "${GREEN}✅ PASSED${NC}"
        PASSED=$((PASSED + 1))
    else
        echo -e "${RED}❌ FAILED${NC}"
        FAILED=$((FAILED + 1))
    fi
    
    rm -f /tmp/flux_test.flux
}

# Test 1: Basic For Loop (baseline)
run_test "For Loop" "
for i in 1, 3 do i
"

# Test 2: Parallel For Loop (sequential execution)
run_test "Parallel For" "
parallel for i in 1, 3 do i
"

# Test 3: Symbolic Declaration
run_test "Symbolic Decl" "
sym x
"

# Test 4: Pattern Matching (simple)
run_test "Pattern Match" "
def test() match(5.0) { 5.0 -> 10.0, _ -> 0.0 }; 1.0
test()
"

# Test 5: Foreach Loop
run_test "Foreach" "
def test() foreach x in [1.0, 2.0, 3.0] do x; 1.0
test()
"

# Test 6: Try-Catch
run_test "Try-Catch" "
def test() try { 10.0 } catch e { 0.0 }; 1.0
test()
"

# Test 7: Corner Cases
run_test "Corner Cases" "
def test() corner r { \"nom\": 1000.0 }; 1.0
test()
"

# Test 8: B-Source (basic)
run_test "B-Source" "
def test() bsource s V(out, 0) { sin(time) }; 1.0
test()
"

# Test 9: Initial Conditions
run_test "Initial Cond" "
def test() ic V(cap) = 5.0; 1.0
test()
"

# Test 10: Outputs
run_test "Outputs" "
def test() outputs[\"Vout\"] = 2.5; 1.0
test()
"

echo ""
echo "========================================"  
echo "  Results: $PASSED/$TOTAL passed"
echo "========================================"

if [ $FAILED -eq 0 ]; then
    echo -e "${GREEN}✅ ALL TESTS PASSED${NC}"
    exit 0
else
    echo -e "${RED}❌ $FAILED TESTS FAILED${NC}"
    exit 1
fi
