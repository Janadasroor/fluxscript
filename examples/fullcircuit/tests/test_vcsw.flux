# FullCircuit — VCSW Regression Test
import circuit
import mna
import dcsolve

var max_comps = 100
var comps = matrix_zeros(max_comps, 12)
var ctrl = circuit_create(3, max_comps)
circuit_add_resistor(comps, ctrl, 1, 2, 1000.0)
circuit_add_vcsw(comps, ctrl, 2, 0, 3, 0, 10.0, 1e6, 2.0, 1.0)
circuit_add_vdc(comps, ctrl, 1, 0, 5.0)
circuit_add_vdc(comps, ctrl, 3, 0, 0.0)

println("=== VCSW Test ===")
var errors = 0.0

circuit_set_param(comps, 3.0, 4.0, 0.0)
var V = dc_solve(comps, ctrl, 20.0, 1e-9)
var vout = matrix_get(V, 2, 0)
print("Vctrl=0: Vout=")
println(vout)
if (vout < 4.5) { errors = errors + 1.0; println("FAIL") }

circuit_set_param(comps, 3.0, 4.0, 1.5)
V = dc_solve(comps, ctrl, 20.0, 1e-9)
vout = matrix_get(V, 2, 0)
print("Vctrl=1.5: Vout=")
println(vout)

circuit_set_param(comps, 3.0, 4.0, 3.0)
V = dc_solve(comps, ctrl, 20.0, 1e-9)
vout = matrix_get(V, 2, 0)
print("Vctrl=3: Vout=")
println(vout)
if (vout > 0.5) { errors = errors + 1.0; println("FAIL") }

if (errors > 0.0) {
    println("FAIL")
} else {
    println("PASS")
}
