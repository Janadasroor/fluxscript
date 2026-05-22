# FullCircuit — DC Analysis (Operating Point + Sweep)
import circuit
import mna

def diode_current(vd, isat, n_factor, vt) {
    if (vd > -5.0 * n_factor * vt) {
        isat * (exp(vd / (n_factor * vt)) - 1.0)
    } else {
        -isat * (1.0 + vd / (n_factor * vt))
    }
}

def diode_conductance(vd, isat, n_factor, vt) {
    if (vd > -5.0 * n_factor * vt) {
        isat / (n_factor * vt) * exp(vd / (n_factor * vt))
    } else {
        -isat / (n_factor * vt)
    }
}

def dc_solve_linear(comps : matrix, ctrl : matrix) {
    mna_dc_solve(comps, ctrl)
}

def dc_nonlinear_solve(comps : matrix, ctrl : matrix, max_iter, tol) {
    var N = circuit_num_nodes(ctrl)
    var nc = circuit_count(ctrl)
    var V = matrix_zeros(N + 1.0, 1)
    var converged = 0.0
    var iter = 0.0
    while (iter < max_iter && converged == 0.0) {
        var M = mna_count_branches(comps, ctrl)
        var dim = N + M
        var A = matrix_zeros(dim, dim)
        var b = matrix_zeros(dim, 1)
        var bi = 0.0
        var ci = 0.0
        while (ci < nc) {
            var t = circuit_component_type(comps, ci)
            var np = circuit_node_p(comps, ci)
            var nm = circuit_node_n(comps, ci)
            if (t == 0.0) {
                var r = circuit_param(comps, ci, 4.0)
                if (r > 1e-15) {
                    mna_stamp_g(A, np, nm, 1.0 / r, N)
                }
            } else if (t == 2.0) {
                mna_stamp_vsource(A, b, np, nm, 0.0, N, bi)
                bi = bi + 1.0
            } else if (t == 3.0 || t == 5.0) {
                var vv = circuit_param(comps, ci, 4.0)
                mna_stamp_vsource(A, b, np, nm, vv, N, bi)
                bi = bi + 1.0
            } else if (t == 4.0 || t == 6.0) {
                var iv = circuit_param(comps, ci, 4.0)
                mna_stamp_isource(b, np, nm, iv, N)
            } else if (t == 7.0) {
                var n3 = circuit_node_3(comps, ci)
                var n4 = circuit_param(comps, ci, 4.0)
                var gain = circuit_param(comps, ci, 5.0)
                mna_stamp_vcvs(A, np, nm, n3, n4, gain, N, bi)
                bi = bi + 1.0
            } else if (t == 8.0) {
                var n3 = circuit_node_3(comps, ci)
                var n4 = circuit_param(comps, ci, 4.0)
                var gm = circuit_param(comps, ci, 5.0)
                mna_stamp_vccs(A, np, nm, n3, n4, gm, N)
            } else if (t == 9.0) {
                var isat = circuit_param(comps, ci, 4.0)
                var nf = circuit_param(comps, ci, 5.0)
                var vt = circuit_param(comps, ci, 7.0)
                var vnp = 0.0
                var vnm = 0.0
                if (np > 0.0 && np <= N) { vnp = matrix_get(V, np, 0) }
                if (nm > 0.0 && nm <= N) { vnm = matrix_get(V, nm, 0) }
                var vd = vnp - vnm
                var id = diode_current(vd, isat, nf, vt)
                var geq = diode_conductance(vd, isat, nf, vt)
                var ieq = id - geq * vd
                mna_stamp_g(A, np, nm, geq, N)
                mna_stamp_isource(b, np, nm, ieq, N)
            } else if (t == 10.0 || t == 11.0) {
                var nc_node = np
                var nb_node = nm
                var ne_node = circuit_node_3(comps, ci)
                var bf = circuit_param(comps, ci, 4.0)
                var isat = circuit_param(comps, ci, 5.0)
                var vaf = circuit_param(comps, ci, 6.0)
                var vt = circuit_param(comps, ci, 7.0)
                var vc_v = 0.0
                var vb_v = 0.0
                var ve_v = 0.0
                if (nc_node > 0.0 && nc_node <= N) { vc_v = matrix_get(V, nc_node, 0) }
                if (nb_node > 0.0 && nb_node <= N) { vb_v = matrix_get(V, nb_node, 0) }
                if (ne_node > 0.0 && ne_node <= N) { ve_v = matrix_get(V, ne_node, 0) }
                var vbe = vb_v - ve_v
                var vce = vc_v - ve_v
var i_c = isat * exp(vbe / vt)
                if (vaf > 0.0 && vce > -vaf * 0.5) {
                    i_c = i_c * (1.0 + vce / vaf)
                }
                var ib = i_c / bf
                var gbe = i_c / vt
                var gm_bjt = gbe
                var go = 0.0
                if (vaf > 0.0) {
                    go = i_c / vaf
                }
                if (t == 11.0) {
                    i_c = -i_c
                    ib = -ib
                }
                var i_co = i_c - gbe * vbe - go * vce
                if (nc_node > 0.0 && nc_node <= N) {
                    var vv = matrix_get(A, mna_ndx(nc_node), mna_ndx(nc_node)) + go
                    matrix_set(A, mna_ndx(nc_node), mna_ndx(nc_node), vv)
                }
                if (nc_node > 0.0 && nc_node <= N && nb_node > 0.0 && nb_node <= N) {
                    var vv = matrix_get(A, mna_ndx(nc_node), mna_ndx(nb_node)) - gm_bjt
                    matrix_set(A, mna_ndx(nc_node), mna_ndx(nb_node), vv)
                }
                if (nc_node > 0.0 && nc_node <= N && ne_node > 0.0 && ne_node <= N) {
                    var vv = matrix_get(A, mna_ndx(nc_node), mna_ndx(ne_node)) + gm_bjt + go
                    matrix_set(A, mna_ndx(nc_node), mna_ndx(ne_node), vv)
                }
                if (nb_node > 0.0 && nb_node <= N) {
                    var vv = matrix_get(A, mna_ndx(nb_node), mna_ndx(nb_node)) + gbe
                    matrix_set(A, mna_ndx(nb_node), mna_ndx(nb_node), vv)
                }
                if (nb_node > 0.0 && nb_node <= N && ne_node > 0.0 && ne_node <= N) {
                    var vv = matrix_get(A, mna_ndx(nb_node), mna_ndx(ne_node)) - gbe
                    matrix_set(A, mna_ndx(nb_node), mna_ndx(ne_node), vv)
                }
                if (ne_node > 0.0 && ne_node <= N) {
                    var vv = matrix_get(A, mna_ndx(ne_node), mna_ndx(ne_node)) + gm_bjt + gbe + go
                    matrix_set(A, mna_ndx(ne_node), mna_ndx(ne_node), vv)
                }
                if (nc_node > 0.0 && nc_node <= N) {
                    var vv = matrix_get(b, mna_ndx(nc_node), 0) - i_co
                    matrix_set(b, mna_ndx(nc_node), 0, vv)
                }
                if (nb_node > 0.0 && nb_node <= N) {
                    var vv = matrix_get(b, mna_ndx(nb_node), 0) - (ib - gbe * vbe)
                    matrix_set(b, mna_ndx(nb_node), 0, vv)
                }
                if (ne_node > 0.0 && ne_node <= N) {
                    var vv = matrix_get(b, mna_ndx(ne_node), 0) + i_co + ib
                    matrix_set(b, mna_ndx(ne_node), 0, vv)
                }
            }
            ci = ci + 1.0
        }
        var x = matrix_solve(A, b)
        var max_dv = 0.0
        var vi = 0.0
        while (vi < N) {
            var old_v = matrix_get(V, vi + 1.0, 0)
            var new_v = matrix_get(x, vi, 0)
            var dv = abs(new_v - old_v)
            if (dv > max_dv) { max_dv = dv }
            matrix_set(V, vi + 1.0, 0, new_v)
            vi = vi + 1.0
        }
        if (max_dv < tol) { converged = 1.0 }
        iter = iter + 1.0
    }
    V
}

def dc_solve(comps : matrix, ctrl : matrix, max_iter, tol) {
    var nc = circuit_count(ctrl)
    var has_nonlinear = 0.0
    var ci = 0.0
    while (ci < nc) {
        var t = circuit_component_type(comps, ci)
        if (t == 9.0 || t == 10.0 || t == 11.0 || t == 12.0 || t == 13.0) {
            has_nonlinear = 1.0
        }
        ci = ci + 1.0
    }
    if (has_nonlinear == 0.0) {
        var sol = mna_dc_solve(comps, ctrl)
        var Nv = matrix_get(sol, 0, 0)
        var V = matrix_zeros(Nv + 1.0, 1)
        var vi = 1.0
        while (vi <= Nv) {
            matrix_set(V, vi, 0, mna_get_node_voltage(sol, vi))
            vi = vi + 1.0
        }
        V
    } else {
        dc_nonlinear_solve(comps, ctrl, max_iter, tol)
    }
}

def dc_diode_vd(comps : matrix, ctrl : matrix, comp_idx, V : matrix) {
    var np = circuit_node_p(comps, comp_idx)
    var nm = circuit_node_n(comps, comp_idx)
    var N = circuit_num_nodes(ctrl)
    var vnp = 0.0
    var vnm = 0.0
    if (np > 0.0 && np <= N) { vnp = matrix_get(V, np, 0) }
    if (nm > 0.0 && nm <= N) { vnm = matrix_get(V, nm, 0) }
    vnp - vnm
}

def dc_diode_id(comps : matrix, ctrl : matrix, comp_idx, V : matrix) {
    var vd = dc_diode_vd(comps, ctrl, comp_idx, V)
    var isat = circuit_param(comps, comp_idx, 4.0)
    var nf = circuit_param(comps, comp_idx, 5.0)
    var vt = circuit_param(comps, comp_idx, 7.0)
    diode_current(vd, isat, nf, vt)
}

def dc_bjt_info(comps : matrix, ctrl : matrix, comp_idx, V : matrix) {
    var nc = circuit_node_p(comps, comp_idx)
    var nb = circuit_node_n(comps, comp_idx)
    var ne = circuit_node_3(comps, comp_idx)
    var N = circuit_num_nodes(ctrl)
    var vc = 0.0
    var vb = 0.0
    var ve = 0.0
    if (nc > 0.0 && nc <= N) { vc = matrix_get(V, nc, 0) }
    if (nb > 0.0 && nb <= N) { vb = matrix_get(V, nb, 0) }
    if (ne > 0.0 && ne <= N) { ve = matrix_get(V, ne, 0) }
    var vbe = vb - ve
    var vce = vc - ve
    var bf = circuit_param(comps, comp_idx, 4.0)
    var isat = circuit_param(comps, comp_idx, 5.0)
    var vaf = circuit_param(comps, comp_idx, 6.0)
    var vt = circuit_param(comps, comp_idx, 7.0)
    var i_c = isat * exp(vbe / vt)
    if (vaf > 0.0) { i_c = i_c * (1.0 + vce / vaf) }
    var ib = i_c / bf
    var ie = i_c + ib
    var result = matrix_zeros(5, 1)
    matrix_set(result, 0, 0, vbe)
    matrix_set(result, 1, 0, vce)
    matrix_set(result, 2, 0, i_c)
    matrix_set(result, 3, 0, ib)
    matrix_set(result, 4, 0, ie)
    result
}

def dc_mos_info(comps : matrix, ctrl : matrix, comp_idx, V : matrix) {
    var nd = circuit_node_p(comps, comp_idx)
    var ng = circuit_node_n(comps, comp_idx)
    var ns = circuit_node_3(comps, comp_idx)
    var N = circuit_num_nodes(ctrl)
    var vd = 0.0
    var vg = 0.0
    var vs = 0.0
    if (nd > 0.0 && nd <= N) { vd = matrix_get(V, nd, 0) }
    if (ng > 0.0 && ng <= N) { vg = matrix_get(V, ng, 0) }
    if (ns > 0.0 && ns <= N) { vs = matrix_get(V, ns, 0) }
    var vgs = vg - vs
    var vds = vd - vs
    var kp = circuit_param(comps, comp_idx, 5.0)
    var vto = circuit_param(comps, comp_idx, 6.0)
    var lambda = circuit_param(comps, comp_idx, 7.0)
    var vov = vgs - vto
    var id = 0.0
    var region = 0.0
    if (vov <= 0.0) {
        region = 0.0
    } else if (vds < vov) {
        region = 1.0
        id = kp * (vov * vds - 0.5 * vds * vds)
        if (lambda > 0.0) { id = id * (1.0 + lambda * vds) }
    } else {
        region = 2.0
        id = 0.5 * kp * vov * vov
        if (lambda > 0.0) { id = id * (1.0 + lambda * vds) }
    }
    var result = matrix_zeros(4, 1)
    matrix_set(result, 0, 0, vgs)
    matrix_set(result, 1, 0, vds)
    matrix_set(result, 2, 0, id)
    matrix_set(result, 3, 0, region)
    result
}

def dc_sweep(comps : matrix, ctrl : matrix, src_idx, start_val, stop_val, nsteps, max_iter, tol) {
    var N = circuit_num_nodes(ctrl)
    var step = 0.0
    if (nsteps > 0.0) { step = (stop_val - start_val) / nsteps }
    var results = matrix_zeros(nsteps + 1.0, N + 2.0)
    var si = 0.0
    while (si <= nsteps) {
        var v_val = start_val + si * step
        circuit_set_param(comps, src_idx, 4.0, v_val)
        var Vs = dc_solve(comps, ctrl, max_iter, tol)
        matrix_set(results, si, 0, v_val)
        var ri = 0.0
        while (ri <= N) {
            matrix_set(results, si, ri + 1.0, matrix_get(Vs, ri, 0))
            ri = ri + 1.0
        }
        si = si + 1.0
    }
    results
}

