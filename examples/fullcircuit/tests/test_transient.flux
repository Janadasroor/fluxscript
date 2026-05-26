# FullCircuit — Transient Nonlinear Tests
import circuit
import mna
import trsim
import dcsolve

def main() {
    println("=== Transient: Diode Half-Wave Rectifier ===")
    var comps1 = matrix_zeros(20, 12)
    var ctrl1 = circuit_create(3, 20)
    circuit_add_vsin(comps1, ctrl1, 1, 0, 0.0, 5.0, 1000.0, 0.0)
    circuit_add_diode(comps1, ctrl1, 1, 2, 1e-14, 1.0, 0.02585)
    circuit_add_resistor(comps1, ctrl1, 2, 3, 1000.0)
    circuit_add_capacitor(comps1, ctrl1, 3, 0, 10e-6, 0.0)
    var r1 = tr_solve(comps1, ctrl1, 0.0, 0.003, 1e-5)
    var nr = floor(0.003 / 1e-5) + 1.0
    print("  Points: "); println(nr)
    var si = nr - 3.0
    var vcap_end = matrix_get(r1, si, 3)
    print("  Vcap at end: "); print(vcap_end); println(" V")
    if (vcap_end > 0.0 && vcap_end < 5.0) {
        println("  PASS: diode charges capacitor")
    } else {
        println("  FAIL: unexpected capacitor voltage")
    }

    println("")
    println("=== Transient: BJT CE Switch ===")
    # BJT CE: pulse -> Rb -> base; Rc from Vcc to collector; emitter to GND
    var comps2 = matrix_zeros(20, 12)
    var ctrl2 = circuit_create(4, 20)
    circuit_add_vdc(comps2, ctrl2, 1, 0, 5.0)
    circuit_add_resistor(comps2, ctrl2, 1, 3, 1000.0)
    circuit_add_npn(comps2, ctrl2, 3, 2, 0, 100.0, 1e-14, 50.0, 0.02585)
    circuit_add_resistor(comps2, ctrl2, 4, 2, 10000.0)
    circuit_add_vpulse(comps2, ctrl2, 4, 0, 0.001, 0.0, 3.0, 0.0, 1e-5, 1e-5, 0.0005)
    var r2 = tr_solve(comps2, ctrl2, 0.0, 0.002, 1e-5)
    var nr2 = floor(0.002 / 1e-5) + 1.0
    print("  Points: "); println(nr2)
    si = 0.0
    var found_switch = 0.0
    while (si < nr2) {
        var tt = matrix_get(r2, si, 0)
        var vc = matrix_get(r2, si, 3)
        var vb = matrix_get(r2, si, 2)
        if (tt >= 0.0004 && tt <= 0.0006) {
            var vce = vc - 0.0
            if (vce < 1.0) { found_switch = 1.0 }
        }
        si = si + 1.0
    }
    if (found_switch > 0.0) {
        println("  PASS: BJT switches on")
    } else {
        println("  FAIL: BJT did not switch")
    }

    println("")
    println("=== Transient: NMOS CS Switch ===")
    # NMOS CS: pulse -> Rg -> gate; Rd from Vcc to drain; source to GND
    var comps3 = matrix_zeros(20, 12)
    var ctrl3 = circuit_create(4, 20)
    circuit_add_vdc(comps3, ctrl3, 1, 0, 5.0)
    circuit_add_resistor(comps3, ctrl3, 1, 3, 1000.0)
    circuit_add_nmos(comps3, ctrl3, 3, 2, 0, 0, 1e-4, 1.0, 0.01)
    circuit_add_resistor(comps3, ctrl3, 4, 2, 10000.0)
    circuit_add_vpulse(comps3, ctrl3, 4, 0, 0.001, 0.0, 3.3, 0.0, 1e-5, 1e-5, 0.0005)
    var r3 = tr_solve(comps3, ctrl3, 0.0, 0.002, 1e-5)
    var nr3 = floor(0.002 / 1e-5) + 1.0
    print("  Points: "); println(nr3)
    si = 0.0
    var found_mos_switch = 0.0
    while (si < nr3) {
        var tt = matrix_get(r3, si, 0)
        var vd = matrix_get(r3, si, 3)
        if (tt >= 0.0004 && tt <= 0.0006) {
            if (vd < 1.0) { found_mos_switch = 1.0 }
        }
        si = si + 1.0
    }
    if (found_mos_switch > 0.0) {
        println("  PASS: NMOS switches on")
    } else {
        println("  FAIL: NMOS did not switch")
    }

    println("")
    println("=== All transient nonlinear tests complete ===")
}
main()
