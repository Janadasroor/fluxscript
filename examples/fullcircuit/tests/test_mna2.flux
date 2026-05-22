import circuit
import mna

def main() {
    var comps = matrix_zeros(500, 12)
    var ctrl = circuit_create(2, 500)
    circuit_add_resistor(comps, ctrl, 1, 2, 1000.0)
    circuit_add_resistor(comps, ctrl, 2, 0, 1000.0)
    circuit_add_vdc(comps, ctrl, 1, 0, 5.0)
    var sol = mna_dc_solve(comps, ctrl)
    var i = 0.0
    while (i < 5.0) {
        println(matrix_get(sol, i, 0.0))
        i = i + 1.0
    }
}
main()
