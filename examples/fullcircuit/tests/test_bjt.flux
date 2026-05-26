import circuit
import mna
import dcsolve
import netlist

println("=== NPN CE (weak base drive, active) ===")
var comps = matrix_zeros(500, 12)
var ctrl = circuit_create(50, 500)
var N = netlist_parse("circuits/npn_bias.sp", comps, ctrl)
print("Nodes: "); println(N)

var V = dc_solve(comps, ctrl, 500.0, 1e-9)
print("Vcc = "); print(matrix_get(V, 1, 0)); println(" V  (expected 5)")
print("Vb  = "); print(matrix_get(V, 2, 0)); println(" V  (expected ~0.69)")
print("Vc  = "); print(matrix_get(V, 3, 0)); println(" V  (expected ~0.5-3)")
print("Vbe = "); print(matrix_get(V, 2, 0) - matrix_get(V, 0, 0)); println(" V")
print("Vce = "); print(matrix_get(V, 3, 0) - matrix_get(V, 0, 0)); println(" V")

println("")
println("=== NPN CE (direct bias, hard saturation) ===")
var comps2 = matrix_zeros(500, 12)
var ctrl2 = circuit_create(50, 500)
var N2 = netlist_parse("circuits/npn_ce.sp", comps2, ctrl2)
print("Nodes: "); println(N2)

var V2 = dc_solve(comps2, ctrl2, 500.0, 1e-9)
print("Vcc = "); print(matrix_get(V2, 1, 0)); println(" V  (expected 5)")
print("Vb  = "); print(matrix_get(V2, 2, 0)); println(" V  (expected ~0.7)")
print("Vc  = "); print(matrix_get(V2, 3, 0)); println(" V  (expected ~0.1-3)")
print("Vbe = "); print(matrix_get(V2, 2, 0) - matrix_get(V2, 0, 0)); println(" V")
print("Vce = "); print(matrix_get(V2, 3, 0) - matrix_get(V2, 0, 0)); println(" V")

println("")
println("=== Done ===")
