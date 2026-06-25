# FullCircuit — BJT & MOSFET Test
import circuit
import mna
import dcsolve

println("=== NMOS Common-Source ===")
var ctrl = circuit_create(3, 10)
circuit_add_vdc(ctrl, 1.0, 0.0, 5.0)
circuit_add_vdc(ctrl, 2.0, 0.0, 3.3)
circuit_add_resistor(ctrl, 1.0, 3.0, 1000.0)
circuit_add_nmos(ctrl, 3.0, 2.0, 0.0, 0.0, 1e-4, 1.0, 0.01)

var Vm = dc_solve(ctrl.get_comps(), ctrl.get_ctrl(), 30.0, 1e-9)
print("Vgs = "); print(matrix_get(Vm, 2, 0)); println(" V  (expected 3.3)")
print("Vd  = "); print(matrix_get(Vm, 3, 0)); println(" V  (expected ~4.72)")
var info = dc_mos_info(ctrl.get_comps(), ctrl.get_ctrl(), 3.0, Vm)
print("Id  = "); print(matrix_get(info, 2, 0)); println(" A")
print("Region = "); print(matrix_get(info, 3, 0)); println(" (2=sat)")

println("")
println("=== PMOS with Rd to GND ===")
var ctrl2 = circuit_create(3, 10)
circuit_add_vdc(ctrl2, 1.0, 0.0, 5.0)
circuit_add_vdc(ctrl2, 2.0, 0.0, 1.7)
circuit_add_resistor(ctrl2, 3.0, 0.0, 1000.0)
circuit_add_pmos(ctrl2, 3.0, 2.0, 1.0, 1.0, 1e-4, -1.0, 0.01)

var Vp = dc_solve(ctrl2.get_comps(), ctrl2.get_ctrl(), 30.0, 1e-9)
print("Vs = "); print(matrix_get(Vp, 1, 0)); println(" V  (expected 5)")
print("Vg = "); print(matrix_get(Vp, 2, 0)); println(" V  (expected 1.7)")
print("Vd = "); print(matrix_get(Vp, 3, 0)); println(" V  (expected ~2.5-3)")

var p_info = dc_mos_info(ctrl2.get_comps(), ctrl2.get_ctrl(), 3.0, Vp)
print("Vsg = "); print(matrix_get(Vp, 1, 0) - matrix_get(Vp, 2, 0)); println(" V")
print("Vds = "); print(matrix_get(Vp, 3, 0) - matrix_get(Vp, 1, 0)); println(" V")
print("Id  = "); print(matrix_get(p_info, 2, 0)); println(" A")

println("")
println("=== Diode (simple bias) ===")
var ctrl3 = circuit_create(2, 10)
circuit_add_vdc(ctrl3, 1.0, 0.0, 5.0)
circuit_add_resistor(ctrl3, 1.0, 2.0, 1000.0)
circuit_add_diode(ctrl3, 2.0, 0.0, 1e-14, 1.0, 0.02585)

var Vd = dc_solve(ctrl3.get_comps(), ctrl3.get_ctrl(), 30.0, 1e-9)
print("Vd = "); print(matrix_get(Vd, 2, 0)); println(" V  (expected ~0.6-0.7)")
print("Vs = "); print(matrix_get(Vd, 1, 0)); println(" V  (expected 5)")

println("")
println("=== All nonlinear tests complete ===")
