import circuit
import mna
import dcsolve
import netlist

def main() {
    println("=== .model card parser test ===")
    println("")

    println("1. NPN with .model card (BF=200)")
    var ctrl1 = circuit_create(50, 500)
    netlist_parse("circuits/npn_model.sp", ctrl1.get_comps(), ctrl1.get_ctrl())
    var V1 = dc_solve(ctrl1.get_comps(), ctrl1.get_ctrl(), 500, 1e-9)
    print("  Vb = "); print(matrix_get(V1, 2, 0)); println(" V")
    print("  Vc = "); print(matrix_get(V1, 3, 0)); println(" V")

    println("")
    println("2. Diode with .model card")
    var ctrl2 = circuit_create(50, 500)
    netlist_parse("circuits/diode_model.sp", ctrl2.get_comps(), ctrl2.get_ctrl())
    var V2 = dc_solve(ctrl2.get_comps(), ctrl2.get_ctrl(), 500, 1e-9)
    print("  Vd = "); print(matrix_get(V2, 2, 0)); println(" V")
    print("  Id = "); print((5.0 - matrix_get(V2, 2, 0)) / 1000.0); println(" A")

    println("")
    println("3. NMOS with .model card")
    var ctrl3 = circuit_create(50, 500)
    netlist_parse("circuits/nmos_model.sp", ctrl3.get_comps(), ctrl3.get_ctrl())
    var V3 = dc_solve(ctrl3.get_comps(), ctrl3.get_ctrl(), 500, 1e-9)
    print("  Vg = "); print(matrix_get(V3, 2, 0)); println(" V")
    print("  Vd = "); print(matrix_get(V3, 3, 0)); println(" V")

    println("")
    println("4. PNP with .model card")
    var ctrl4 = circuit_create(50, 500)
    netlist_parse("circuits/pnp_model.sp", ctrl4.get_comps(), ctrl4.get_ctrl())
    var V4 = dc_solve(ctrl4.get_comps(), ctrl4.get_ctrl(), 500, 1e-9)
    print("  Ve = "); print(matrix_get(V4, 3, 0)); println(" V")
    print("  Vb = "); print(matrix_get(V4, 2, 0)); println(" V")
    print("  Vc = "); print(matrix_get(V4, 4, 0)); println(" V")

    println("")
    println("=== All model tests complete ===")
}
main()
