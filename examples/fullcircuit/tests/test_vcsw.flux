# FullCircuit — VCSW Regression Test
import circuit
import mna
import dcsolve

var ctrl = circuit_create(3, 100)
circuit_add_resistor(ctrl, 1, 2, 1000.0)
circuit_add_vcsw(ctrl, 2, 0, 3, 0, 10.0, 1e6, 2.0, 1.0)
circuit_add_vdc(ctrl, 1, 0, 5.0)
circuit_add_vdc(ctrl, 3, 0, 0.0)

println("=== VCSW Test ===")
var errors = 0.0

circuit_set_param(ctrl, 3.0, 4.0, 0.0)
var V = dc_solve(ctrl.get_comps(), ctrl.get_ctrl(), 20.0, 1e-9)
var vout = matrix_get(V, 2, 0)
print("Vctrl=0: Vout=")
println(vout)
if (vout < 4.5) { errors = errors + 1.0; println("FAIL") }

circuit_set_param(ctrl, 3.0, 4.0, 1.5)
V = dc_solve(ctrl.get_comps(), ctrl.get_ctrl(), 20.0, 1e-9)
vout = matrix_get(V, 2, 0)
print("Vctrl=1.5: Vout=")
println(vout)

circuit_set_param(ctrl, 3.0, 4.0, 3.0)
V = dc_solve(ctrl.get_comps(), ctrl.get_ctrl(), 20.0, 1e-9)
vout = matrix_get(V, 2, 0)
print("Vctrl=3: Vout=")
println(vout)
if (vout > 0.5) { errors = errors + 1.0; println("FAIL") }

if (errors > 0.0) {
    println("FAIL")
} else {
    println("PASS")
}
