import circuit
import mna

def local_mna_dc_solve_minimal(c: Circuit) {
    var comps = c.comps
    var ctrl = c.ctrl
    var N = c.num_nodes()
    var nc = c.count()
    var M = mna_count_branches(comps, ctrl)
    var dim = N + M
    var A = matrix_zeros(dim, dim)
    var b = matrix_zeros(dim, 1)
    var bi = 0.0
    var ci = 0.0
    while (ci < nc) {
        var t = c.type_code(ci)
        var np = c.node_p(ci)
        var nm = c.node_n(ci)
        if (t == 0.0) {
            var r = c.get_param(ci, 4.0)
            if (r > 1e-15) {
                mna_stamp_g(A, np, nm, 1.0 / r, N)
            }
        } else if (t == 3.0 || t == 5.0) {
            var v_val = c.get_param(ci, 4.0)
            mna_stamp_vsource(A, b, np, nm, v_val, N, bi)
            bi = bi + 1.0
        }
        ci = ci + 1.0
    }
    var x = matrix_solve(A, b)
    var sol = matrix_zeros(dim + 2.0, 1)
    matrix_set(sol, 0, 0, N)
    matrix_set(sol, 1, 0, M)
    var ri = 0.0
    while (ri < dim) {
        matrix_set(sol, ri + 2.0, 0, matrix_get(x, ri, 0))
        ri = ri + 1.0
    }
    sol
}

def main() {
    var ctrl = circuit_create(2, 500)
    circuit_add_resistor(ctrl, 1, 2, 1000.0)
    circuit_add_resistor(ctrl, 2, 0, 1000.0)
    circuit_add_vdc(ctrl, 1, 0, 5.0)

    println("Testing local minimal version:")
    var sol1 = local_mna_dc_solve_minimal(ctrl)
    var i1 = 0.0
    while (i1 < 5.0) {
        println(matrix_get(sol1, i1, 0.0))
        i1 = i1 + 1.0
    }

    println("Testing module version:")
    var sol2 = mna_dc_solve(ctrl)
    var i2 = 0.0
    while (i2 < 5.0) {
        println(matrix_get(sol2, i2, 0.0))
        i2 = i2 + 1.0
    }
}
main()
