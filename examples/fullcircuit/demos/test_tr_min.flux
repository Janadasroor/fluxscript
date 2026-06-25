import circuit
import mna
import trsim

def main() {
    var ctrl = circuit_create(2, 100)
    circuit_add_resistor(ctrl, 1, 2, 1000.0)
    circuit_add_capacitor(ctrl, 2, 0, 1e-6, 0.0)
    circuit_add_vdc(ctrl, 1, 0, 5.0)
    var results = tr_solve(ctrl.get_comps(), ctrl.get_ctrl(), 0.0, 0.0001, 0.00001)
    print("tr_solve done, rows: ")
    println(floor(0.0001 / 0.00001) + 1.0)
}
main()
