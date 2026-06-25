
import circuit
import mna

def test() {
    var c = circuit_create(2.0, 5.0)
    c.add(Component.R { n_plus: 1.0, n_minus: 2.0, r_val: 1000.0 })
    c.add(Component.R { n_plus: 2.0, n_minus: 0.0, r_val: 1000.0 })
    c.add(Component.Vdc { n_plus: 1.0, n_minus: 0.0, v_val: 5.0 })
    var sol = mna_dc_solve(c)
    var v1 = mna_get_node_voltage(sol, 1.0)
    var v2 = mna_get_node_voltage(sol, 2.0)
    println("V1=5.0, V2=")
    println(v2)
    v2
}
test()
