import circuit
import mna

def test_A_after_stamp(c: Circuit) {
    var comps = c.comps
    var ctrl = c.ctrl
    var N = c.num_nodes()
    var nc = c.count()
    var M = mna_count_branches(comps, ctrl)
    var dim = N + M
    var A = matrix_zeros(dim, dim)
    # Test: set A(0,0) = 42.0
    matrix_set(A, 0, 0, 42.0)
    # Read it back
    var v = matrix_get(A, 0, 0)
    println("After set A(0,0)=42, value=")
    println(v)
    # Now stamp with mna_stamp_g (defined in mna.flux)
    mna_stamp_g(A, 1, 2, 0.001, N)
    # Read A(0,0) again
    var v2 = matrix_get(A, 0, 0)
    println("After stamp_g, A(0,0)=")
    println(v2)
    A
}

def main() {
    var ctrl = circuit_create(2, 500)
    circuit_add_resistor(ctrl, 1, 2, 1000.0)
    circuit_add_resistor(ctrl, 2, 0, 1000.0)
    circuit_add_vdc(ctrl, 1, 0, 5.0)

    println("Testing A matrix after stamp_g calls from module function:")
    var A_res = test_A_after_stamp(ctrl)
    var i = 0.0
    while (i < 3.0) {
        var j = 0.0
        while (j < 3.0) {
            print(matrix_get(A_res, i, j))
            print(" ")
            j = j + 1.0
        }
        println("")
        i = i + 1.0
    }
}
main()
