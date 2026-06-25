import circuit
import mna
import netlist

def main() {
    var max_comps = 500.0
    var max_nodes = 50.0

    println("=== SPICE Netlist Parser Test ===")
    println("")

    println("1. Voltage Divider")
    var ctrl1 = circuit_create(max_nodes, max_comps)
    var N1 = netlist_parse("circuits/voltage_divider.sp", ctrl1.get_comps(), ctrl1.get_ctrl())
    print("  N = "); print(N1); println(" (expected 2)")
    var sol1 = mna_dc_solve(ctrl1)
    print("  V(1) = "); print(mna_get_node_voltage(sol1, 1)); println(" V (expected 5)")
    print("  V(2) = "); print(mna_get_node_voltage(sol1, 2)); println(" V (expected 3.333)")
    println("")

    println("2. RC Low-Pass DC")
    var ctrl2 = circuit_create(max_nodes, max_comps)
    var N2 = netlist_parse("circuits/rc_lowpass.sp", ctrl2.get_comps(), ctrl2.get_ctrl())
    print("  N = "); print(N2); println(" (expected 2)")
    var sol2 = mna_dc_solve(ctrl2)
    print("  V(1) = "); print(mna_get_node_voltage(sol2, 1)); println(" V (expected 5)")
    print("  V(2) = "); print(mna_get_node_voltage(sol2, 2)); println(" V (expected 5)")
    println("")

    println("3. Series RLC DC")
    var ctrl3 = circuit_create(max_nodes, max_comps)
    var N3 = netlist_parse("circuits/rlc_series.sp", ctrl3.get_comps(), ctrl3.get_ctrl())
    print("  N = "); print(N3); println(" (expected 3)")
    var sol3 = mna_dc_solve(ctrl3)
    print("  V(1) = "); print(mna_get_node_voltage(sol3, 1)); println(" V (expected 5)")
    print("  V(2) = "); print(mna_get_node_voltage(sol3, 2)); println(" V (expected 5)")
    print("  V(3) = "); print(mna_get_node_voltage(sol3, 3)); println(" V (expected 5)")
    println("")

    println("4. Diode Bias (nonlinear)")
    var ctrl4 = circuit_create(max_nodes, max_comps)
    var N4 = netlist_parse("circuits/diode_bias.sp", ctrl4.get_comps(), ctrl4.get_ctrl())
    print("  N = "); print(N4); println(" (expected 2)")
    print("  Components: "); print(ctrl4.count()); println("")
    println("")

    println("5. NPN with .model card")
    var ctrl5 = circuit_create(max_nodes, max_comps)
    var N5 = netlist_parse("circuits/npn_model.sp", ctrl5.get_comps(), ctrl5.get_ctrl())
    print("  N = "); print(N5); println(" (expected 3)")
    println("")

    println("6. Diode with .model card")
    var ctrl6 = circuit_create(max_nodes, max_comps)
    var N6 = netlist_parse("circuits/diode_model.sp", ctrl6.get_comps(), ctrl6.get_ctrl())
    print("  N = "); print(N6); println(" (expected 2)")
    println("")

    println("=== All netlist tests complete ===")
}
main()
