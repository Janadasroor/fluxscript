# FullCircuit — Circuit Simulation Demo Suite
import dcsolve

# ---------------------------------------------------------------------------
# Linear circuits
# ---------------------------------------------------------------------------

def voltage_divider(c: Circuit, r1: Double, r2: Double, vin: Double) {
    circuit_add_resistor(c, 1.0, 2.0, r1)
    circuit_add_resistor(c, 2.0, 0.0, r2)
    circuit_add_vdc(c, 1.0, 0.0, vin)
}

def rc_lowpass(c: Circuit, r: Double, cap: Double, vin: Double) {
    circuit_add_resistor(c, 1.0, 2.0, r)
    circuit_add_capacitor(c, 2.0, 0.0, cap, 0.0)
    circuit_add_vdc(c, 1.0, 0.0, vin)
}

def rlc_series(c: Circuit, r: Double, l: Double, cap: Double, vin: Double) {
    circuit_add_resistor(c, 1.0, 2.0, r)
    circuit_add_inductor(c, 2.0, 3.0, l, 0.0)
    circuit_add_capacitor(c, 3.0, 0.0, cap, 0.0)
    circuit_add_vdc(c, 1.0, 0.0, vin)
}

def wheatstone_bridge(c: Circuit, r1: Double, r2: Double, r3: Double, r4: Double, r5: Double, vin: Double) {
    circuit_add_resistor(c, 1.0, 2.0, r1)
    circuit_add_resistor(c, 2.0, 0.0, r2)
    circuit_add_resistor(c, 1.0, 3.0, r3)
    circuit_add_resistor(c, 3.0, 0.0, r4)
    circuit_add_resistor(c, 2.0, 3.0, r5)
    circuit_add_vdc(c, 1.0, 0.0, vin)
}

def opamp_inverting(c: Circuit, r1: Double, rf: Double, vin: Double) {
    circuit_add_resistor(c, 1.0, 2.0, r1)
    circuit_add_resistor(c, 2.0, 3.0, rf)
    circuit_add_vcvs(c, 3.0, 0.0, 2.0, 0.0, 1e6)
    circuit_add_vdc(c, 1.0, 0.0, vin)
}

def vcvs_amplifier(c: Circuit, gain: Double, rin: Double, rout: Double, rload: Double, vin: Double) {
    circuit_add_resistor(c, 1.0, 0.0, rin)
    circuit_add_vdc(c, 1.0, 0.0, vin)
    circuit_add_vcvs(c, 3.0, 0.0, 1.0, 0.0, gain)
    circuit_add_resistor(c, 3.0, 2.0, rout)
    circuit_add_resistor(c, 2.0, 0.0, rload)
}

# ---------------------------------------------------------------------------
# Nonlinear circuits — exercise the Newton-Raphson + gmin-stepping solver
# ---------------------------------------------------------------------------

def diode_rectifier(c: Circuit, rload: Double, cfilter: Double, isat: Double, vt: Double) {
    circuit_add_vdc(c, 1.0, 0.0, 5.0)
    circuit_add_diode(c, 1.0, 2.0, isat, 1.0, vt)
    circuit_add_resistor(c, 2.0, 0.0, rload)
    circuit_add_capacitor(c, 2.0, 0.0, cfilter, 0.0)
}

def bjt_ce_amplifier(c: Circuit, rc: Double, r1: Double, r2: Double, re: Double, bf: Double, isat: Double, vaf: Double, vt: Double, vcc: Double) {
    circuit_add_vdc(c, 5.0, 0.0, vcc)
    circuit_add_resistor(c, 5.0, 1.0, rc)
    circuit_add_resistor(c, 5.0, 2.0, r1)
    circuit_add_resistor(c, 2.0, 0.0, r2)
    circuit_add_resistor(c, 3.0, 0.0, re)
    circuit_add_npn(c, 1.0, 2.0, 3.0, bf, isat, vaf, vt)
}

def differential_pair(c: Circuit, rc: Double, ree: Double, bf: Double, isat: Double, vaf: Double, vt: Double, vcc: Double, vee: Double) {
    circuit_add_vdc(c, 6.0, 0.0, vcc)
    circuit_add_vdc(c, 7.0, 0.0, vee)
    circuit_add_resistor(c, 6.0, 1.0, rc)
    circuit_add_resistor(c, 6.0, 2.0, rc)
    circuit_add_resistor(c, 3.0, 7.0, ree)
    circuit_add_npn(c, 1.0, 4.0, 3.0, bf, isat, vaf, vt)
    circuit_add_npn(c, 2.0, 5.0, 3.0, bf, isat, vaf, vt)
    circuit_add_vdc(c, 4.0, 0.0, 0.0)
    circuit_add_vdc(c, 5.0, 0.0, 0.0)
}

# ---------------------------------------------------------------------------
# Solver wrapper
# ---------------------------------------------------------------------------

def dc_solve_circuit(c: Circuit, max_iter: Double, tol: Double) -> matrix {
    dc_solve(c.get_comps(), c.get_ctrl(), max_iter, tol)
}

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def check_approx(got: Double, expected: Double, tol_val: Double) {
    var diff = got - expected
    if (diff < 0.0) { diff = -diff }
    if (diff < tol_val) {
        println(" -> PASS")
    } else {
        println(" -> FAIL")
    }
}

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() {
    var tol = 1e-9
    var ptol = 0.01

    println(" FullCircuit Simulation Demo")
    println(" ──────────────────────────────")
    println("")

    # ------ 1. Voltage Divider ------
    var c1 = circuit_create(2.0, 500.0)
    voltage_divider(c1, 1000.0, 1000.0, 5.0)
    var V1 = dc_solve_circuit(c1, 100.0, tol)

    println("1. Voltage Divider")
    println("   R1=1k (1→2), R2=1k (2→0), Vdc=5V (1→0)")
    println("   V(2) = ")
    println(matrix_get(V1, 2.0, 0.0))
    check_approx(matrix_get(V1, 2.0, 0.0), 2.5, ptol)

    # ------ 2. RC Low-Pass (DC) ------
    var c2 = circuit_create(2.0, 500.0)
    rc_lowpass(c2, 1000.0, 1e-6, 5.0)
    var V2 = dc_solve_circuit(c2, 100.0, tol)

    println("2. RC Low-Pass (DC)")
    println("   R=1k (1→2), C=1uF (2→0), Vdc=5V (1→0)")
    println("   V(2) = ")
    println(matrix_get(V2, 2.0, 0.0))
    check_approx(matrix_get(V2, 2.0, 0.0), 5.0, ptol)

    # ------ 3. Series RLC (DC) ------
    var c3 = circuit_create(3.0, 500.0)
    rlc_series(c3, 1000.0, 1e-3, 1e-6, 5.0)
    var V3 = dc_solve_circuit(c3, 100.0, tol)

    println("3. Series RLC (DC)")
    println("   R=1k (1→2), L=1mH (2→3), C=1uF (3→0), Vdc=5V (1→0)")
    println("   V(2) = ")
    println(matrix_get(V3, 2.0, 0.0))
    check_approx(matrix_get(V3, 2.0, 0.0), 5.0, ptol)
    println("   V(3) = ")
    println(matrix_get(V3, 3.0, 0.0))
    check_approx(matrix_get(V3, 3.0, 0.0), 5.0, ptol)

    # ------ 4. Wheatstone Bridge ------
    var c4 = circuit_create(3.0, 500.0)
    wheatstone_bridge(c4, 1000.0, 2000.0, 2000.0, 1000.0, 5000.0, 10.0)
    var V4 = dc_solve_circuit(c4, 100.0, tol)

    println("4. Wheatstone Bridge")
    println("   R1=1k (1→2), R2=2k (2→0), R3=2k (1→3), R4=1k (3→0), R5=5k (2→3), Vdc=10V (1→0)")
    println("   V(1) = ")
    println(matrix_get(V4, 1.0, 0.0))
    println("   V(2) = ")
    println(matrix_get(V4, 2.0, 0.0))
    println("   V(3) = ")
    println(matrix_get(V4, 3.0, 0.0))
    println("   Vdiff = ")
    println(matrix_get(V4, 2.0, 0.0) - matrix_get(V4, 3.0, 0.0))

    # ------ 5. Op-Amp Inverting Amplifier ------
    var c5 = circuit_create(3.0, 500.0)
    opamp_inverting(c5, 1000.0, 10000.0, 1.0)
    var V5 = dc_solve_circuit(c5, 100.0, tol)

    println("5. Op-Amp Inverting Amplifier")
    println("   R1=1k (1→2), Rf=10k (2→3), VCVS gain=1e6 (3→0 ctrl:2→0), Vdc=1V (1→0)")
    println("   V(3) = ")
    println(matrix_get(V5, 3.0, 0.0))
    check_approx(matrix_get(V5, 3.0, 0.0), -10.0, ptol)

    # ------ 6. VCVS Amplifier ------
    var c6 = circuit_create(3.0, 500.0)
    vcvs_amplifier(c6, 100.0, 1000.0, 100.0, 10000.0, 1.0)
    var V6 = dc_solve_circuit(c6, 100.0, tol)

    println("6. VCVS Amplifier")
    println("   Rin=1k, VCVS gain=100, Rout=100, Rload=10k, Vdc=1V")
    println("   V(3) = ")
    println(matrix_get(V6, 3.0, 0.0))
    check_approx(matrix_get(V6, 3.0, 0.0), 100.0, ptol)

    # ====== Nonlinear demos ======

    println("")
    println(" --- Nonlinear Demos (Newton-Raphson + gmin stepping) ---")
    println("")

    # ------ 7. Diode Rectifier (DC op) ------
    var c7 = circuit_create(2.0, 500.0)
    diode_rectifier(c7, 1000.0, 1e-6, 1e-14, 0.025)
    var V7 = dc_solve_circuit(c7, 100.0, tol)

    println("7. Diode Rectifier (DC Operating Point)")
    println("   Vdc=5V (1→0), D (1→2, Is=1e-14), Rload=1k (2→0)")
    println("   V(1) = ")
    println(matrix_get(V7, 1.0, 0.0))
    println("   V(2) = ")
    println(matrix_get(V7, 2.0, 0.0))
    println("   Diode drop = ")
    println(matrix_get(V7, 1.0, 0.0) - matrix_get(V7, 2.0, 0.0))
    println("   (expected ~0.65-0.75V)")

    # ------ 8. DC Sweep of Voltage Divider ------
    println("8. DC Sweep — Voltage Divider Transfer")
    println("   R1=R2=1k, sweeping Vin from 0V to 10V")
    var c8 = circuit_create(2.0, 500.0)
    voltage_divider(c8, 1000.0, 1000.0, 0.0)
    var sweep8 = dc_sweep(c8.get_comps(), c8.get_ctrl(), 2.0, 0.0, 10.0, 10.0, 100.0, tol)
    println("   Vin  Vout")
    var si8 = 0.0
    while (si8 <= 10.0) {
        println(matrix_get(sweep8, si8, 0.0))
        println(" ")
        println(matrix_get(sweep8, si8, 3.0))
        si8 = si8 + 1.0
    }

    # ------ 9. BJT Differential Pair (DC bias) ------
    var c9 = circuit_create(7.0, 500.0)
    differential_pair(c9, 10000.0, 5000.0, 200.0, 1e-14, 100.0, 0.025, 5.0, -5.0)
    var V9 = dc_solve_circuit(c9, 100.0, tol)

    println("9. BJT Differential Pair (DC Bias)")
    println("   Vcc=5V, Vee=-5V, Rc=10k (both), Ree=5k")
    println("   NPN pair: Bf=200, Is=1e-14, Vaf=100, Vt=0.025")
    println("   Both inputs at 0V")
    println("   Vc1 = ")
    println(matrix_get(V9, 1.0, 0.0))
    println("   Vc2 = ")
    println(matrix_get(V9, 2.0, 0.0))
    println("   Vdiff = ")
    println(matrix_get(V9, 1.0, 0.0) - matrix_get(V9, 2.0, 0.0))
    println("   (expected ~0V)")

    println("")
    println(" All demos complete.")
}

main()
