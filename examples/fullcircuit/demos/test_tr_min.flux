import circuit
import mna
import trsim

def main() {
    var max_comps = 100
    var comps = matrix_zeros(max_comps, 12)
    var ctrl = circuit_create(2, max_comps)
    circuit_add_resistor(comps, ctrl, 1, 2, 1000.0)
    circuit_add_capacitor(comps, ctrl, 2, 0, 1e-6, 0.0)
    circuit_add_vdc(comps, ctrl, 1, 0, 5.0)
    var results = tr_solve(comps, ctrl, 0.0, 0.0001, 0.00001)
    print("tr_solve done, rows: ")
    println(floor(0.0001 / 0.00001) + 1.0)
}
main()
