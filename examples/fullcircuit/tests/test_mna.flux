# Minimal test of mna_dc_solve from module
import circuit
import mna

def main() {
    var comps = matrix_zeros(500, 12)
    var ctrl = circuit_create(2, 500)
    circuit_add_resistor(comps, ctrl, 1, 2, 1000.0)
    circuit_add_resistor(comps, ctrl, 2, 0, 1000.0)
    circuit_add_vdc(comps, ctrl, 1, 0, 5.0)
    var sol = mna_dc_solve(comps, ctrl)
    println("N=")
    println(matrix_get(sol, 0, 0))
    println("V1=")
    println(mna_get_node_voltage(sol, 1))
    println("V2=")
    println(mna_get_node_voltage(sol, 2))
}
main()
