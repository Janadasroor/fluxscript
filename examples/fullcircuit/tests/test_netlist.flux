import circuit
import mna
import netlist

def main() {
    var max_comps = 500.0
    var max_nodes = 50.0

    println("=== SPICE Netlist Parser Test ===")
    println("")

    println("1. Voltage Divider")
    var comps1 = matrix_zeros(max_comps, 12.0)
    var ctrl1 = circuit_create(max_nodes, max_comps)
    var N1 = netlist_parse("circuits/voltage_divider.sp", comps1, ctrl1)
    print("  N = "); print(N1); println(" (expected 2)")
    var sol1 = mna_dc_solve(comps1, ctrl1)
    print("  V(1) = "); print(mna_get_node_voltage(sol1, 1)); println(" V (expected 5)")
    print("  V(2) = "); print(mna_get_node_voltage(sol1, 2)); println(" V (expected 3.333)")
    println("")

    println("2. RC Low-Pass DC")
    var comps2 = matrix_zeros(max_comps, 12.0)
    var ctrl2 = circuit_create(max_nodes, max_comps)
    var N2 = netlist_parse("circuits/rc_lowpass.sp", comps2, ctrl2)
    print("  N = "); print(N2); println(" (expected 2)")
    var sol2 = mna_dc_solve(comps2, ctrl2)
    print("  V(1) = "); print(mna_get_node_voltage(sol2, 1)); println(" V (expected 5)")
    print("  V(2) = "); print(mna_get_node_voltage(sol2, 2)); println(" V (expected 5)")
    println("")

    println("3. Series RLC DC")
    var comps3 = matrix_zeros(max_comps, 12.0)
    var ctrl3 = circuit_create(max_nodes, max_comps)
    var N3 = netlist_parse("circuits/rlc_series.sp", comps3, ctrl3)
    print("  N = "); print(N3); println(" (expected 3)")
    var sol3 = mna_dc_solve(comps3, ctrl3)
    print("  V(1) = "); print(mna_get_node_voltage(sol3, 1)); println(" V (expected 5)")
    print("  V(2) = "); print(mna_get_node_voltage(sol3, 2)); println(" V (expected 5)")
    print("  V(3) = "); print(mna_get_node_voltage(sol3, 3)); println(" V (expected 5)")
    println("")

    println("4. Diode Bias (nonlinear)")
    var comps4 = matrix_zeros(max_comps, 12.0)
    var ctrl4 = circuit_create(max_nodes, max_comps)
    var N4 = netlist_parse("circuits/diode_bias.sp", comps4, ctrl4)
    print("  N = "); print(N4); println(" (expected 2)")
    print("  Components: "); print(circuit_count(ctrl4)); println("")
    println("")

    println("=== All netlist tests complete ===")
}
main()
