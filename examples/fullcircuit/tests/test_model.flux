import circuit
import mna
import dcsolve
import netlist

def main() {
    println("=== .model card parser test ===")
    println("")

    println("1. NPN with .model card (BF=200)")
    var comps1 = matrix_zeros(500, 12)
    var ctrl1 = circuit_create(50, 500)
    netlist_parse("circuits/npn_model.sp", comps1, ctrl1)
    var V1 = dc_solve(comps1, ctrl1, 500, 1e-9)
    print("  Vb = "); print(matrix_get(V1, 2, 0)); println(" V")
    print("  Vc = "); print(matrix_get(V1, 3, 0)); println(" V")

    println("")
    println("2. Diode with .model card")
    var comps2 = matrix_zeros(500, 12)
    var ctrl2 = circuit_create(50, 500)
    netlist_parse("circuits/diode_model.sp", comps2, ctrl2)
    var V2 = dc_solve(comps2, ctrl2, 500, 1e-9)
    print("  Vd = "); print(matrix_get(V2, 2, 0)); println(" V")
    print("  Id = "); print((5.0 - matrix_get(V2, 2, 0)) / 1000.0); println(" A")

    println("")
    println("3. NMOS with .model card")
    var comps3 = matrix_zeros(500, 12)
    var ctrl3 = circuit_create(50, 500)
    netlist_parse("circuits/nmos_model.sp", comps3, ctrl3)
    var V3 = dc_solve(comps3, ctrl3, 500, 1e-9)
    print("  Vg = "); print(matrix_get(V3, 2, 0)); println(" V")
    print("  Vd = "); print(matrix_get(V3, 3, 0)); println(" V")

    println("")
    println("4. PNP with .model card")
    var comps4 = matrix_zeros(500, 12)
    var ctrl4 = circuit_create(50, 500)
    netlist_parse("circuits/pnp_model.sp", comps4, ctrl4)
    var V4 = dc_solve(comps4, ctrl4, 500, 1e-9)
    print("  Ve = "); print(matrix_get(V4, 3, 0)); println(" V")
    print("  Vb = "); print(matrix_get(V4, 2, 0)); println(" V")
    print("  Vc = "); print(matrix_get(V4, 4, 0)); println(" V")

    println("")
    println("=== All model tests complete ===")
}
main()
