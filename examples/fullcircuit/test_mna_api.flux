import circuit
import mna

def test() {
    var c = circuit_create(2.0, 5.0)
    c.add(Component.R { n_plus: 1.0, n_minus: 0.0, r_val: 1000.0 })
    var info = mna_build_A_b(c)
    var sol = mna_dc_solve(c)
    var v = mna_get_node_voltage(sol, 1.0)
    v
}
test()
