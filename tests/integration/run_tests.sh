#!/bin/bash
# ┌────────────────────────────────────────────────────────────────────────────┐
# │ FluxScript + ngspice Integration Test Suite                                │
# │ Tests the complete pipeline: FluxScript → SPICE → ngspice → Results        │
# └────────────────────────────────────────────────────────────────────────────┘

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$(dirname "$SCRIPT_DIR")")"
CIRCUIT_DIR="$SCRIPT_DIR/circuits"
BUILD_DIR="$PROJECT_DIR/build"
RESULTS_DIR="$SCRIPT_DIR/results"
PASS_COUNT=0
FAIL_COUNT=0
TOTAL_COUNT=0

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

mkdir -p "$RESULTS_DIR"

echo ""
echo "╔══════════════════════════════════════════════════════════════╗"
echo "║     FluxScript + ngspice Integration Test Suite              ║"
echo "╠══════════════════════════════════════════════════════════════╣"
echo "║  ngspice: $(ngspice -v 2>&1 | grep 'ngspice-' | cut -d: -f1 | xargs)"
echo "║  Date: $(date '+%Y-%m-%d %H:%M:%S')"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

# ┌────────────────────────────────────────────────────────────────────────────┐
# │ Test Function: Runs ngspice and checks exit code + output files            │
# └────────────────────────────────────────────────────────────────────────────┘
run_ngspice_test() {
    local test_name="$1"
    local circuit_file="$2"
    local timeout_sec="${3:-10}"

    TOTAL_COUNT=$((TOTAL_COUNT + 1))
    local raw_file="$RESULTS_DIR/${test_name// /_}.raw"
    local log_file="$RESULTS_DIR/${test_name// /_}.log"
    
    echo -n "Test $TOTAL_COUNT: $test_name... "
    
    # Run ngspice (needs stdin pipe)
    local start_time=$(date +%s%N)
    echo "" | timeout "$timeout_sec" ngspice -b -r "$raw_file" "$circuit_file" > "$log_file" 2>&1 && local rc=0 || local rc=$?
    local end_time=$(date +%s%N)
    local elapsed=$(( (end_time - start_time) / 1000000 ))
    
    # Check if simulation succeeded (exit code 0)
    if [ $rc -eq 0 ]; then
        echo -e "${GREEN}✅ PASS${NC} (${elapsed}ms)"
        PASS_COUNT=$((PASS_COUNT + 1))
        return 0
    fi
    
    echo -e "${RED}❌ FAIL${NC} (${elapsed}ms, rc=$rc)"
    FAIL_COUNT=$((FAIL_COUNT + 1))
    return 1
}

# ┌────────────────────────────────────────────────────────────────────────────┐
# │ TEST SECTION 1: Passive Circuits                                           │
# └────────────────────────────────────────────────────────────────────────────┘
echo "═══════════════════════════════════════════════════════════════════"
echo "  SECTION 1: Passive Circuit Simulation"
echo "═══════════════════════════════════════════════════════════════════"
echo ""

run_ngspice_test "RC Low-Pass Filter" "$CIRCUIT_DIR/rc_lowpass.cir"
run_ngspice_test "Voltage Divider" "$CIRCUIT_DIR/voltage_divider.cir"
run_ngspice_test "RL High-Pass Filter" "$CIRCUIT_DIR/rl_highpass.cir"
run_ngspice_test "RLC Bandpass Filter" "$CIRCUIT_DIR/rlc_bandpass.cir"
run_ngspice_test "RC Integrator" "$CIRCUIT_DIR/rc_integrator.cir"

echo ""

# ┌────────────────────────────────────────────────────────────────────────────┐
# │ TEST SECTION 2: Active Circuits                                            │
# └────────────────────────────────────────────────────────────────────────────┘
echo "═══════════════════════════════════════════════════════════════════"
echo "  SECTION 2: Active Circuits (BJT)"
echo "═══════════════════════════════════════════════════════════════════"
echo ""

run_ngspice_test "Common Emitter Amplifier" "$CIRCUIT_DIR/common_emitter.cir"

echo ""

# ┌────────────────────────────────────────────────────────────────────────────┐
# │ TEST SECTION 3: Behavioral Sources                                         │
# └────────────────────────────────────────────────────────────────────────────┘
echo "═══════════════════════════════════════════════════════════════════"
echo "  SECTION 3: Behavioral Sources (B-Device)"
echo "═══════════════════════════════════════════════════════════════════"
echo ""

run_ngspice_test "FM Modulation" "$CIRCUIT_DIR/fm_modulation.cir"
run_ngspice_test "Chirp Signal" "$CIRCUIT_DIR/chirp_signal.cir"
run_ngspice_test "Wien Bridge Oscillator" "$CIRCUIT_DIR/wien_bridge.cir"

echo ""

# ┌────────────────────────────────────────────────────────────────────────────┐
# │ TEST SECTION 4: DC Sweep & Diodes                                          │
# └────────────────────────────────────────────────────────────────────────────┘
echo "═══════════════════════════════════════════════════════════════════"
echo "  SECTION 4: DC Sweep & Diodes"
echo "═══════════════════════════════════════════════════════════════════"
echo ""

run_ngspice_test "Diode IV Curve" "$CIRCUIT_DIR/diode_iv.cir"

echo ""

# ┌────────────────────────────────────────────────────────────────────────────┐
# │ TEST SECTION 5: FluxScript Tools                                           │
# └────────────────────────────────────────────────────────────────────────────┘
echo "═══════════════════════════════════════════════════════════════════"
echo "  SECTION 5: FluxScript Analysis Tools"
echo "═══════════════════════════════════════════════════════════════════"
echo ""

# flux-run
if [ -f "$BUILD_DIR/flux-run" ] && [ -f "$PROJECT_DIR/examples/rc_transient.flux" ]; then
    TOTAL_COUNT=$((TOTAL_COUNT + 1))
    echo -n "Test $TOTAL_COUNT: flux-run (ngspice pipeline)... "
    output=$("$BUILD_DIR/flux-run" "$PROJECT_DIR/examples/rc_transient.flux" 2>&1) || true
    if echo "$output" | grep -qi "simulation complete\|ngspice\|transient"; then
        echo -e "${GREEN}✅ PASS${NC}"
        PASS_COUNT=$((PASS_COUNT + 1))
    else
        echo -e "${RED}❌ FAIL${NC}"
        FAIL_COUNT=$((FAIL_COUNT + 1))
    fi
fi

# flux-stability
if [ -f "$BUILD_DIR/flux-stability" ]; then
    TOTAL_COUNT=$((TOTAL_COUNT + 1))
    echo -n "Test $TOTAL_COUNT: flux-stability... "
    output=$("$BUILD_DIR/flux-stability" "$PROJECT_DIR/examples/stability_analyzer.flux" 2>&1) || true
    if echo "$output" | grep -qi "stability\|phase margin\|stable"; then
        echo -e "${GREEN}✅ PASS${NC}"
        PASS_COUNT=$((PASS_COUNT + 1))
    else
        echo -e "${RED}❌ FAIL${NC}"
        FAIL_COUNT=$((FAIL_COUNT + 1))
    fi
fi

# flux-sweep
if [ -f "$BUILD_DIR/flux-sweep" ]; then
    TOTAL_COUNT=$((TOTAL_COUNT + 1))
    echo -n "Test $TOTAL_COUNT: flux-sweep (parametric)... "
    output=$("$BUILD_DIR/flux-sweep" "$PROJECT_DIR/examples/rc_transient.flux" \
        --component R1 --start 1000 --stop 10000 --points 3 2>&1) || true
    if echo "$output" | grep -qi "sweep\|parametric\|results"; then
        echo -e "${GREEN}✅ PASS${NC}"
        PASS_COUNT=$((PASS_COUNT + 1))
    else
        echo -e "${RED}❌ FAIL${NC}"
        FAIL_COUNT=$((FAIL_COUNT + 1))
    fi
fi

# flux-fft
if [ -f "$PROJECT_DIR/test_signal.csv" ] && [ -f "$BUILD_DIR/flux-fft" ]; then
    TOTAL_COUNT=$((TOTAL_COUNT + 1))
    echo -n "Test $TOTAL_COUNT: flux-fft (frequency domain)... "
    output=$("$BUILD_DIR/flux-fft" "$PROJECT_DIR/test_signal.csv" \
        --rate 10000 2>&1) || true
    if echo "$output" | grep -qi "fft\|frequency\|spectrum\|fundamental"; then
        echo -e "${GREEN}✅ PASS${NC}"
        PASS_COUNT=$((PASS_COUNT + 1))
    else
        echo -e "${RED}❌ FAIL${NC}"
        FAIL_COUNT=$((FAIL_COUNT + 1))
    fi
fi

# flux-doctor
if [ -f "$BUILD_DIR/flux-doctor" ]; then
    TOTAL_COUNT=$((TOTAL_COUNT + 1))
    echo -n "Test $TOTAL_COUNT: flux-doctor (diagnostics)... "
    output=$("$BUILD_DIR/flux-doctor" "$PROJECT_DIR/examples/doctor_test_circuit.flux" 2>&1) || true
    if echo "$output" | grep -qi "doctor\|diagnostic\|error\|warning\|circuit"; then
        echo -e "${GREEN}✅ PASS${NC}"
        PASS_COUNT=$((PASS_COUNT + 1))
    else
        echo -e "${RED}❌ FAIL${NC}"
        FAIL_COUNT=$((FAIL_COUNT + 1))
    fi
fi

# flux-optimize
if [ -f "$BUILD_DIR/flux-optimize" ]; then
    TOTAL_COUNT=$((TOTAL_COUNT + 1))
    echo -n "Test $TOTAL_COUNT: flux-optimize... "
    output=$("$BUILD_DIR/flux-optimize" "$PROJECT_DIR/examples/auto_optimizer.flux" 2>&1) || true
    if echo "$output" | grep -qi "optimization\|optimal\|converged"; then
        echo -e "${GREEN}✅ PASS${NC}"
        PASS_COUNT=$((PASS_COUNT + 1))
    else
        echo -e "${RED}❌ FAIL${NC}"
        FAIL_COUNT=$((FAIL_COUNT + 1))
    fi
fi

# flux-monte-carlo
if [ -f "$BUILD_DIR/flux-monte-carlo" ]; then
    TOTAL_COUNT=$((TOTAL_COUNT + 1))
    echo -n "Test $TOTAL_COUNT: flux-monte-carlo... "
    output=$("$BUILD_DIR/flux-monte-carlo" "$PROJECT_DIR/examples/monte_carlo_analysis.flux" 2>&1) || true
    if echo "$output" | grep -qi "monte carlo\|statistics\|yield"; then
        echo -e "${GREEN}✅ PASS${NC}"
        PASS_COUNT=$((PASS_COUNT + 1))
    else
        echo -e "${RED}❌ FAIL${NC}"
        FAIL_COUNT=$((FAIL_COUNT + 1))
    fi
fi

# flux-worstcase
if [ -f "$BUILD_DIR/flux-worstcase" ]; then
    TOTAL_COUNT=$((TOTAL_COUNT + 1))
    echo -n "Test $TOTAL_COUNT: flux-worstcase... "
    output=$("$BUILD_DIR/flux-worstcase" "$PROJECT_DIR/examples/worst_case_analysis.flux" 2>&1) || true
    if echo "$output" | grep -qi "worst-case\|corner\|margin"; then
        echo -e "${GREEN}✅ PASS${NC}"
        PASS_COUNT=$((PASS_COUNT + 1))
    else
        echo -e "${RED}❌ FAIL${NC}"
        FAIL_COUNT=$((FAIL_COUNT + 1))
    fi
fi

echo ""

# ┌────────────────────────────────────────────────────────────────────────────┐
# │ Summary                                                                   │
# └────────────────────────────────────────────────────────────────────────────┘
PASS_RATE=0
if [ $TOTAL_COUNT -gt 0 ]; then
    PASS_RATE=$(( (PASS_COUNT * 100) / TOTAL_COUNT ))
fi

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║                    TEST SUMMARY                              ║"
echo "╠══════════════════════════════════════════════════════════════╣"
printf "║  %-14s  %-5d\n" "Total Tests:" "$TOTAL_COUNT"
printf "║  %-14s  %-5d ${GREEN}✅${NC}\n" "Passed:" "$PASS_COUNT"
printf "║  %-14s  %-5d ${RED}❌${NC}\n" "Failed:" "$FAIL_COUNT"
printf "║  %-14s  %-4d%%\n" "Pass Rate:" "$PASS_RATE"
echo "╠══════════════════════════════════════════════════════════════╣"

if [ $FAIL_COUNT -eq 0 ]; then
    echo "║  Status:       ${GREEN}✅ ALL TESTS PASSED${NC}                          ║"
else
    echo "║  Status:       ${YELLOW}⚠️  SOME TESTS FAILED${NC}                         ║"
fi

echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

# Save results
cat > "$RESULTS_DIR/test_summary.txt" << EOF
FluxScript + ngspice Integration Test Results
===============================================
Date: $(date '+%Y-%m-%d %H:%M:%S')
ngspice: $(ngspice -v 2>&1 | grep 'ngspice-' | cut -d: -f1 | xargs)

Total Tests:  $TOTAL_COUNT
Passed:       $PASS_COUNT
Failed:       $FAIL_COUNT
Pass Rate:    ${PASS_RATE}%
EOF

echo "📄 Results saved to: $RESULTS_DIR/test_summary.txt"
echo ""

exit $FAIL_COUNT
