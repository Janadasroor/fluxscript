# FullCircuit — Circuit Simulation Demo Suite
import circuit
import mna
import dcsolve
import examples

def main() {
    var max_comps = 500
    var tol = 1e-9

    println("FullCircuit Simulation Demo")
    println("========================")
    println("")

    # --- 1. Voltage Divider (nodes: 0,1,2 → need max_nodes=2) ---
    var comps = matrix_zeros(max_comps, 12)
    var ctrl = circuit_create(2, max_comps)
    ex_voltage_divider(comps, ctrl, 1000.0, 1000.0, 5.0)
    var V = dc_solve(comps, ctrl, 100, 1e-9)

    println("1. Voltage Divider (R1=1k, R2=1k, Vin=5V):")
    println("   V(1) = ")
    println(matrix_get(V, 1, 0))
    println("   V(2) = ")
    println(matrix_get(V, 2, 0))
    println("")

    # --- 2. RC Low Pass (nodes: 0,1,2 → max_nodes=2) ---
    var comps2 = matrix_zeros(max_comps, 12)
    var ctrl2 = circuit_create(2, max_comps)
    ex_rc_lowpass(comps2, ctrl2, 1000.0, 1e-6, 5.0)
    var V2 = dc_solve(comps2, ctrl2, 100, 1e-9)

    println("2. RC Low-Pass DC (R=1k, C=1uF, Vin=5V):")
    println("   V(2) = ")
    println(matrix_get(V2, 2, 0))
    println("")

    # --- 3. Series RLC (nodes: 0,1,2,3 → max_nodes=3) ---
    var comps3 = matrix_zeros(max_comps, 12)
    var ctrl3 = circuit_create(3, max_comps)
    ex_rlc_series(comps3, ctrl3, 1000.0, 1e-3, 1e-6, 5.0)
    var V3 = dc_solve(comps3, ctrl3, 100, 1e-9)

    println("3. Series RLC DC (R=1k, L=1mH, C=1uF, Vin=5V):")
    println("   V(2) = ")
    println(matrix_get(V3, 2, 0))
    println("   V(3) = ")
    println(matrix_get(V3, 3, 0))
    println("")

    # --- 4. Wheatstone Bridge (nodes: 0,1,2,3 → max_nodes=3) ---
    var comps4 = matrix_zeros(max_comps, 12)
    var ctrl4 = circuit_create(3, max_comps)
    ex_wheatstone_bridge(comps4, ctrl4, 1000.0, 2000.0, 2000.0, 1000.0, 5000.0, 10.0)
    var V4 = dc_solve(comps4, ctrl4, 100, 1e-9)

    println("4. Wheatstone Bridge (R1=1k, R2=2k, R3=2k, R4=1k, R5=5k, Vin=10V):")
    println("   V(1) = ")
    println(matrix_get(V4, 1, 0))
    println("   V(2) = ")
    println(matrix_get(V4, 2, 0))
    println("   V(3) = ")
    println(matrix_get(V4, 3, 0))
    println("   Vdiff = ")
    println(matrix_get(V4, 2, 0) - matrix_get(V4, 3, 0))
    println("")

    # --- 5. Op-Amp Inverting (nodes: 0,1,2,3 → max_nodes=3) ---
    var comps5 = matrix_zeros(max_comps, 12)
    var ctrl5 = circuit_create(3, max_comps)
    ex_opamp_inverting(comps5, ctrl5, 1000.0, 10000.0, 1.0)
    var V5 = dc_solve(comps5, ctrl5, 100, 1e-9)

    println("5. Op-Amp Inverting (R1=1k, Rf=10k, Vin=1V):")
    println("   V(3) = ")
    println(matrix_get(V5, 3, 0))
    println("   (expected ~ -10.0)")
    println("")

    # --- 6. VCVS Amplifier (nodes: 0,1,2,3 → max_nodes=3) ---
    var comps6 = matrix_zeros(max_comps, 12)
    var ctrl6 = circuit_create(3, max_comps)
    ex_vcvs_amplifier(comps6, ctrl6, 100.0, 1000.0, 100.0, 10000.0, 1.0)
    var V6 = dc_solve(comps6, ctrl6, 100, 1e-9)

    println("6. VCVS Amplifier (gain=100, Rin=1k, Rout=100, Rload=10k, Vin=1V):")
    println("   V(1) = ")
    println(matrix_get(V6, 1, 0))
    println("   V(2) = ")
    println(matrix_get(V6, 2, 0))
    println("   V(3) = ")
    println(matrix_get(V6, 3, 0))
    println("   (expected: V(3) ~ 100 * 1 * 10k/10.1k ~ 99)")
    println("")

    println("All tests complete.")
}

main()
