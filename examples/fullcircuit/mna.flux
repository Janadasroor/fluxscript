# FullCircuit — Modified Nodal Analysis Matrix Builder

def mna_ndx(n: Double) -> Double {
    n - 1.0
}

def mna_stamp_g(A: matrix, np: Double, nm: Double, g: Double, N: Double) {
    if (np > 0.0 && np <= N) {
        var r = mna_ndx(np)
        var v = matrix_get(A, r, r) + g
        matrix_set(A, r, r, v)
    }
    if (nm > 0.0 && nm <= N) {
        var r = mna_ndx(nm)
        var v = matrix_get(A, r, r) + g
        matrix_set(A, r, r, v)
    }
    if (np > 0.0 && nm > 0.0 && np <= N && nm <= N) {
        var rp = mna_ndx(np)
        var rm = mna_ndx(nm)
        var v1 = matrix_get(A, rp, rm) - g
        var v2 = matrix_get(A, rm, rp) - g
        matrix_set(A, rp, rm, v1)
        matrix_set(A, rm, rp, v2)
    }
}

def mna_stamp_vsource(A: matrix, b: matrix, np: Double, nm: Double, v_val: Double, N: Double, bi: Double) {
    var br = N + bi
    if (np > 0.0 && np <= N) {
        matrix_set(A, mna_ndx(np), br, 1.0)
        matrix_set(A, br, mna_ndx(np), 1.0)
    }
    if (nm > 0.0 && nm <= N) {
        matrix_set(A, mna_ndx(nm), br, -1.0)
        matrix_set(A, br, mna_ndx(nm), -1.0)
    }
    matrix_set(b, br, 0.0, v_val)
}

def mna_stamp_isource(b: matrix, np: Double, nm: Double, i_val: Double, N: Double) {
    if (np > 0.0 && np <= N) {
        var v = matrix_get(b, mna_ndx(np), 0.0) - i_val
        matrix_set(b, mna_ndx(np), 0.0, v)
    }
    if (nm > 0.0 && nm <= N) {
        var v = matrix_get(b, mna_ndx(nm), 0.0) + i_val
        matrix_set(b, mna_ndx(nm), 0.0, v)
    }
}

def mna_stamp_vcvs(A: matrix, np: Double, nm: Double, n_ctrl_p: Double, n_ctrl_n: Double, gain: Double, N: Double, bi: Double) {
    var br = N + bi
    if (np > 0.0 && np <= N) {
        matrix_set(A, mna_ndx(np), br, 1.0)
        matrix_set(A, br, mna_ndx(np), 1.0)
    }
    if (nm > 0.0 && nm <= N) {
        matrix_set(A, mna_ndx(nm), br, -1.0)
        matrix_set(A, br, mna_ndx(nm), -1.0)
    }
    if (n_ctrl_p > 0.0 && n_ctrl_p <= N) {
        var v = matrix_get(A, br, mna_ndx(n_ctrl_p)) - gain
        matrix_set(A, br, mna_ndx(n_ctrl_p), v)
    }
    if (n_ctrl_n > 0.0 && n_ctrl_n <= N) {
        var v = matrix_get(A, br, mna_ndx(n_ctrl_n)) + gain
        matrix_set(A, br, mna_ndx(n_ctrl_n), v)
    }
}

def mna_stamp_vccs(A: matrix, np: Double, nm: Double, n_ctrl_p: Double, n_ctrl_n: Double, gm: Double, N: Double) {
    if (np > 0.0 && np <= N && n_ctrl_p > 0.0 && n_ctrl_p <= N) {
        var v = matrix_get(A, mna_ndx(np), mna_ndx(n_ctrl_p)) + gm
        matrix_set(A, mna_ndx(np), mna_ndx(n_ctrl_p), v)
    }
    if (np > 0.0 && np <= N && n_ctrl_n > 0.0 && n_ctrl_n <= N) {
        var v = matrix_get(A, mna_ndx(np), mna_ndx(n_ctrl_n)) - gm
        matrix_set(A, mna_ndx(np), mna_ndx(n_ctrl_n), v)
    }
    if (nm > 0.0 && nm <= N && n_ctrl_p > 0.0 && n_ctrl_p <= N) {
        var v = matrix_get(A, mna_ndx(nm), mna_ndx(n_ctrl_p)) - gm
        matrix_set(A, mna_ndx(nm), mna_ndx(n_ctrl_p), v)
    }
    if (nm > 0.0 && nm <= N && n_ctrl_n > 0.0 && n_ctrl_n <= N) {
        var v = matrix_get(A, mna_ndx(nm), mna_ndx(n_ctrl_n)) + gm
        matrix_set(A, mna_ndx(nm), mna_ndx(n_ctrl_n), v)
    }
}

def mna_branch_index(comps: matrix, ctrl: matrix, comp_idx: Double) -> Double {
    var bi = 0.0
    var ci = 0.0
    while (ci < comp_idx) {
        var t = matrix_get(comps, ci, 0.0)
        if (t == 2.0 || t == 3.0 || t == 5.0 || t == 7.0) { bi = bi + 1.0 }
        ci = ci + 1.0
    }
    bi
}

def mna_count_branches(comps: matrix, ctrl: matrix) -> Double {
    var nc = matrix_get(ctrl, 1.0, 0.0)
    var M = 0.0
    var ci = 0.0
    while (ci < nc) {
        var t = matrix_get(comps, ci, 0.0)
        if (t == 2.0) { M = M + 1.0 }
        if (t == 3.0) { M = M + 1.0 }
        if (t == 5.0) { M = M + 1.0 }
        if (t == 7.0) { M = M + 1.0 }
        ci = ci + 1.0
    }
    M
}

def mna_build_A_b(comps: matrix, ctrl: matrix) {
    var N = matrix_get(ctrl, 0.0, 0.0)
    var nc = matrix_get(ctrl, 1.0, 0.0)
    var M = mna_count_branches(comps, ctrl)
    var dim = N + M
    var A = matrix_zeros(dim, dim)
    var b = matrix_zeros(dim, 1.0)
    var bi = 0.0
    var ci = 0.0
    while (ci < nc) {
        var t = matrix_get(comps, ci, 0.0)
        var np = matrix_get(comps, ci, 1.0)
        var nm = matrix_get(comps, ci, 2.0)
        if (t == 0.0) {
            var r = matrix_get(comps, ci, 4.0)
            if (r > 1e-15) {
                mna_stamp_g(A, np, nm, 1.0 / r, N)
            }
        } else if (t == 1.0) {
        } else if (t == 2.0) {
            mna_stamp_vsource(A, b, np, nm, 0.0, N, bi)
            bi = bi + 1.0
        } else if (t == 3.0 || t == 5.0) {
            var v_val = matrix_get(comps, ci, 4.0)
            mna_stamp_vsource(A, b, np, nm, v_val, N, bi)
            bi = bi + 1.0
        } else if (t == 4.0 || t == 6.0) {
            var i_val = matrix_get(comps, ci, 4.0)
            mna_stamp_isource(b, np, nm, i_val, N)
        } else if (t == 7.0) {
            var n_ctrl_p = matrix_get(comps, ci, 3.0)
            var n_ctrl_n = matrix_get(comps, ci, 4.0)
            var gain = matrix_get(comps, ci, 5.0)
            mna_stamp_vcvs(A, np, nm, n_ctrl_p, n_ctrl_n, gain, N, bi)
            bi = bi + 1.0
        } else if (t == 8.0) {
            var n_ctrl_p = matrix_get(comps, ci, 3.0)
            var n_ctrl_n = matrix_get(comps, ci, 4.0)
            var gm = matrix_get(comps, ci, 5.0)
            mna_stamp_vccs(A, np, nm, n_ctrl_p, n_ctrl_n, gm, N)
        }
        ci = ci + 1.0
    }
    var result = matrix_zeros(2.0, 1.0)
    matrix_set(result, 0.0, 0.0, M)
    matrix_set(result, 1.0, 0.0, dim)
    result
}

def mna_dc_solve(comps: matrix, ctrl: matrix) -> matrix {
    var N = matrix_get(ctrl, 0.0, 0.0)
    var nc = matrix_get(ctrl, 1.0, 0.0)
    var M = mna_count_branches(comps, ctrl)
    var dim = N + M
    var A = matrix_zeros(dim, dim)
    var b = matrix_zeros(dim, 1.0)
    var bi = 0.0
    var ci = 0.0
    while (ci < nc) {
        var t = matrix_get(comps, ci, 0.0)
        var np = matrix_get(comps, ci, 1.0)
        var nm = matrix_get(comps, ci, 2.0)
        if (t == 0.0) {
            var r = matrix_get(comps, ci, 4.0)
            if (r > 1e-15) {
                mna_stamp_g(A, np, nm, 1.0 / r, N)
            }
        } else if (t == 1.0) {
        } else if (t == 2.0) {
            mna_stamp_vsource(A, b, np, nm, 0.0, N, bi)
            bi = bi + 1.0
        } else if (t == 3.0 || t == 5.0) {
            var v_val = matrix_get(comps, ci, 4.0)
            mna_stamp_vsource(A, b, np, nm, v_val, N, bi)
            bi = bi + 1.0
        } else if (t == 4.0 || t == 6.0) {
            var i_val = matrix_get(comps, ci, 4.0)
            mna_stamp_isource(b, np, nm, i_val, N)
        } else if (t == 7.0) {
            var n3 = matrix_get(comps, ci, 3.0)
            var n4 = matrix_get(comps, ci, 4.0)
            var gain = matrix_get(comps, ci, 5.0)
            mna_stamp_vcvs(A, np, nm, n3, n4, gain, N, bi)
            bi = bi + 1.0
        } else if (t == 8.0) {
            var n3 = matrix_get(comps, ci, 3.0)
            var n4 = matrix_get(comps, ci, 4.0)
            var gm = matrix_get(comps, ci, 5.0)
            mna_stamp_vccs(A, np, nm, n3, n4, gm, N)
        }
        ci = ci + 1.0
    }
    var x = matrix_solve(A, b)
    var sol = matrix_zeros(dim + 2.0, 1.0)
    matrix_set(sol, 0.0, 0.0, N)
    matrix_set(sol, 1.0, 0.0, M)
    var ri = 0.0
    while (ri < dim) {
        matrix_set(sol, ri + 2.0, 0.0, matrix_get(x, ri, 0.0))
        ri = ri + 1.0
    }
    sol
}

def mna_get_node_voltage(sol: matrix, nd: Double) -> Double {
    var N = matrix_get(sol, 0.0, 0.0)
    if (nd == 0.0) { 0.0 }
    else if (nd <= N) { matrix_get(sol, nd + 1.0, 0.0) }
    else { 0.0 }
}

def mna_get_branch_current(sol: matrix, branch_idx: Double) -> Double {
    var N = matrix_get(sol, 0.0, 0.0)
    var M = matrix_get(sol, 1.0, 0.0)
    if (branch_idx >= 0.0 && branch_idx < M) {
        matrix_get(sol, N + 2.0 + branch_idx, 0.0)
    } else { 0.0 }
}

def mna_extract_voltages(sol: matrix) -> matrix {
    var N = matrix_get(sol, 0.0, 0.0)
    var V = matrix_zeros(N + 1.0, 1.0)
    var i = 1.0
    while (i <= N) {
        matrix_set(V, i, 0.0, mna_get_node_voltage(sol, i))
        i = i + 1.0
    }
    V
}
