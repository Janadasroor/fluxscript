# FullCircuit — Transient Analysis (Time-Domain)
import circuit
import mna

def tr_src_val(comps : matrix, ci, t) {
    var tc = circuit_param(comps, ci, 11.0)
    var dc_val = circuit_param(comps, ci, 4.0)
    if (tc == 1.0) {
        var v1 = circuit_param(comps, ci, 8.0)
        var v2 = circuit_param(comps, ci, 9.0)
        var td = circuit_param(comps, ci, 10.0)
        var t_rise = circuit_param(comps, ci, 5.0)
        var t_fall = circuit_param(comps, ci, 6.0)
        var pw = circuit_param(comps, ci, 7.0)
        var per = dc_val
        if (per <= 0.0) { per = 1e-3 }
        var tm = t
        while (tm >= per) { tm = tm - per }
        if (tm < td) { v1 }
        else if (tm < td + t_rise) { v1 + (v2 - v1) * (tm - td) / t_rise }
        else if (tm < td + t_rise + pw) { v2 }
        else if (tm < td + t_rise + pw + t_fall) { v2 + (v1 - v2) * (tm - td - t_rise - pw) / t_fall }
        else { v1 }
    } else if (tc == 2.0) {
        var voff = circuit_param(comps, ci, 8.0)
        var vamp = circuit_param(comps, ci, 9.0)
        var freq = circuit_param(comps, ci, 10.0)
        var ph = circuit_param(comps, ci, 5.0)
        if (t < 0.0) { voff }
        else { voff + vamp * sin(2.0 * pi() * freq * t + ph * pi() / 180.0) }
    } else { dc_val }
}

def tr_step(comps : matrix, ctrl : matrix, t, dt, cap_v : matrix, ind_i : matrix) {
    var N = circuit_num_nodes(ctrl)
    var nc = circuit_count(ctrl)
    var M = mna_count_branches(comps, ctrl)
    var dim = N + M
    var A = matrix_zeros(dim, dim)
    var b = matrix_zeros(dim, 1)
    var bi = 0.0
    var ci = 0.0
    while (ci < nc) {
        var tc = circuit_component_type(comps, ci)
        var np = circuit_node_p(comps, ci)
        var nm = circuit_node_n(comps, ci)
        if (tc == 0.0) {
            var rv = circuit_param(comps, ci, 4.0)
            if (rv > 1e-15) { mna_stamp_g(A, np, nm, 1.0 / rv, N) }
        } else if (tc == 1.0) {
            var cv = circuit_param(comps, ci, 4.0)
            var geq = cv / dt
            var vco = matrix_get(cap_v, ci, 0)
            mna_stamp_g(A, np, nm, geq, N)
            mna_stamp_isource(b, np, nm, -geq * vco, N)
        } else if (tc == 2.0) {
            var lv = circuit_param(comps, ci, 4.0)
            var geq = dt / lv
            var ilo = matrix_get(ind_i, ci, 0)
            mna_stamp_g(A, np, nm, geq, N)
            mna_stamp_isource(b, np, nm, ilo, N)
            bi = bi + 1.0
        } else if (tc == 3.0 || tc == 5.0) {
            mna_stamp_vsource(A, b, np, nm, tr_src_val(comps, ci, t), N, bi)
            bi = bi + 1.0
        } else if (tc == 4.0 || tc == 6.0) {
            mna_stamp_isource(b, np, nm, tr_src_val(comps, ci, t), N)
        } else if (tc == 7.0) {
            var n3 = circuit_node_3(comps, ci)
            var n4 = circuit_param(comps, ci, 4.0)
            mna_stamp_vcvs(A, np, nm, n3, n4, circuit_param(comps, ci, 5.0), N, bi)
            bi = bi + 1.0
        } else if (tc == 8.0) {
            var n3 = circuit_node_3(comps, ci)
            var n4 = circuit_param(comps, ci, 4.0)
            mna_stamp_vccs(A, np, nm, n3, n4, circuit_param(comps, ci, 5.0), N)
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

def tr_solve(comps : matrix, ctrl : matrix, t_start, t_stop, dt) {
    var N = circuit_num_nodes(ctrl)
    var nc = circuit_count(ctrl)
    var ns = (t_stop - t_start) / dt
    if (ns < 1.0) { ns = 1.0 }
    var npts = floor(ns) + 1.0
    var results = matrix_zeros(npts, N + 2.0)
    var cap_v = matrix_zeros(nc, 1)
    var ind_i = matrix_zeros(nc, 1)
    var t = t_start
    var si = 0.0
    while (si < npts) {
        var sol = tr_step(comps, ctrl, t, dt, cap_v, ind_i)
        var Nsol = matrix_get(sol, 0, 0)
        var ci = 0.0
        while (ci < nc) {
            var tc = circuit_component_type(comps, ci)
            var np = circuit_node_p(comps, ci)
            var nm = circuit_node_n(comps, ci)
            if (tc == 1.0) {
                var vnp = 0.0; var vnm = 0.0
                if (np > 0.0 && np <= N) { vnp = mna_get_node_voltage(sol, np) }
                if (nm > 0.0 && nm <= N) { vnm = mna_get_node_voltage(sol, nm) }
                matrix_set(cap_v, ci, 0, vnp - vnm)
            } else if (tc == 2.0) {
                var bi = mna_branch_index(comps, ctrl, ci)
                var il = mna_get_branch_current(sol, bi)
                matrix_set(ind_i, ci, 0, il)
            }
            ci = ci + 1.0
        }
        matrix_set(results, si, 0, t)
        var ri = 0.0
        while (ri <= N) {
            matrix_set(results, si, ri + 1.0, mna_get_node_voltage(sol, ri))
            ri = ri + 1.0
        }
        t = t + dt
        si = si + 1.0
    }
    results
}
