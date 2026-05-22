# FullCircuit — AC Small-Signal Frequency Sweep Analysis
import circuit
import mna

def ac_build(comps : matrix, ctrl : matrix, freq, V : matrix) {
    var N = circuit_num_nodes(ctrl)
    var nc = circuit_count(ctrl)
    var M = mna_count_branches(comps, ctrl)
    var dim = N + M
    var dim2 = 2.0 * dim
    var A = matrix_zeros(dim2, dim2)
    var b = matrix_zeros(dim2, 1)
    var w = 2.0 * pi() * freq
    var bi = 0.0
    var ci = 0.0
    while (ci < nc) {
        var t = circuit_component_type(comps, ci)
        var np = circuit_node_p(comps, ci)
        var nm = circuit_node_n(comps, ci)
        if (t == 0.0) {
            var rv = circuit_param(comps, ci, 4.0)
            if (rv > 1e-15) { ac_stamp_g(A, np, nm, 1.0 / rv, N, dim) }
        } else if (t == 1.0) {
            var cv = circuit_param(comps, ci, 4.0)
            if (cv > 1e-20) { ac_stamp_sus(A, np, nm, w * cv, N, dim) }
        } else if (t == 2.0) {
            var lv = circuit_param(comps, ci, 4.0)
            if (lv > 1e-20 && w > 1e-20) { ac_stamp_sus(A, np, nm, -1.0 / (w * lv), N, dim) }
            bi = bi + 1.0
        } else if (t == 3.0) {
            ac_stamp_vsrc_re(A, b, np, nm, 0.0, N, dim, bi)
            bi = bi + 1.0
        } else if (t == 5.0) {
            var amag = circuit_param(comps, ci, 8.0)
            var aphase = circuit_param(comps, ci, 10.0)
            var pr = aphase * pi() / 180.0
            ac_stamp_vsrc_cx(A, b, np, nm, amag * cos(pr), amag * sin(pr), N, dim, bi)
            bi = bi + 1.0
        } else if (t == 6.0) {
            var amag = circuit_param(comps, ci, 8.0)
            var aphase = circuit_param(comps, ci, 10.0)
            var pr = aphase * pi() / 180.0
            ac_stamp_isrc_cx(b, np, nm, amag * cos(pr), amag * sin(pr), N, dim)
        } else if (t == 7.0) {
            var n3 = circuit_node_3(comps, ci)
            var n4 = circuit_param(comps, ci, 4.0)
            var gain = circuit_param(comps, ci, 5.0)
            ac_stamp_vcvs(A, np, nm, n3, n4, gain, N, dim, bi)
            bi = bi + 1.0
        } else if (t == 8.0) {
            var n3 = circuit_node_3(comps, ci)
            var n4 = circuit_param(comps, ci, 4.0)
            ac_stamp_gm(A, np, nm, n3, n4, circuit_param(comps, ci, 5.0), N, dim)
        } else if (t == 9.0) {
            ac_stamp_diode_ss(A, comps, ci, np, nm, V, N, dim)
        } else if (t == 10.0 || t == 11.0) {
            ac_stamp_bjt_ss(A, comps, ci, np, nm, V, N, dim)
        }
        ci = ci + 1.0
    }
    var x = matrix_solve(A, b)
    var sol = matrix_zeros(dim2 + 2.0, 1)
    matrix_set(sol, 0, 0, dim)
    matrix_set(sol, 1, 0, M)
    var ri = 0.0
    while (ri < dim2) {
        matrix_set(sol, ri + 2.0, 0, matrix_get(x, ri, 0))
        ri = ri + 1.0
    }
    sol
}

def ac_stamp_g(A : matrix, np, nm, g, N, dim) {
    if (np > 0.0 && np <= N) {
        var r = mna_ndx(np)
        matrix_set(A, r, r, matrix_get(A, r, r) + g)
        matrix_set(A, r + dim, r + dim, matrix_get(A, r + dim, r + dim) + g)
    }
    if (nm > 0.0 && nm <= N) {
        var r = mna_ndx(nm)
        matrix_set(A, r, r, matrix_get(A, r, r) + g)
        matrix_set(A, r + dim, r + dim, matrix_get(A, r + dim, r + dim) + g)
    }
    if (np > 0.0 && nm > 0.0 && np <= N && nm <= N) {
        var rp = mna_ndx(np); var rm = mna_ndx(nm)
        matrix_set(A, rp, rm, matrix_get(A, rp, rm) - g)
        matrix_set(A, rm, rp, matrix_get(A, rm, rp) - g)
        matrix_set(A, rp + dim, rm + dim, matrix_get(A, rp + dim, rm + dim) - g)
        matrix_set(A, rm + dim, rp + dim, matrix_get(A, rm + dim, rp + dim) - g)
    }
}

def ac_stamp_sus(A : matrix, np, nm, bval, N, dim) {
    if (np > 0.0 && np <= N) {
        var r = mna_ndx(np)
        matrix_set(A, r, r + dim, matrix_get(A, r, r + dim) - bval)
        matrix_set(A, r + dim, r, matrix_get(A, r + dim, r) + bval)
    }
    if (nm > 0.0 && nm <= N) {
        var r = mna_ndx(nm)
        matrix_set(A, r, r + dim, matrix_get(A, r, r + dim) - bval)
        matrix_set(A, r + dim, r, matrix_get(A, r + dim, r) + bval)
    }
    if (np > 0.0 && nm > 0.0 && np <= N && nm <= N) {
        var rp = mna_ndx(np); var rm = mna_ndx(nm)
        matrix_set(A, rp, rm + dim, matrix_get(A, rp, rm + dim) + bval)
        matrix_set(A, rm, rp + dim, matrix_get(A, rm, rp + dim) + bval)
        matrix_set(A, rp + dim, rm, matrix_get(A, rp + dim, rm) - bval)
        matrix_set(A, rm + dim, rp, matrix_get(A, rm + dim, rp) - bval)
    }
}

def ac_stamp_gm(A : matrix, np, nm, ncp, ncn, gm_val, N, dim) {
    var sgn = 1.0
    if (np > 0.0 && np <= N && ncp > 0.0 && ncp <= N) {
        var rp = mna_ndx(np); var rc = mna_ndx(ncp)
        matrix_set(A, rp, rc, matrix_get(A, rp, rc) + gm_val)
        matrix_set(A, rp + dim, rc + dim, matrix_get(A, rp + dim, rc + dim) + gm_val)
    }
    if (np > 0.0 && np <= N && ncn > 0.0 && ncn <= N) {
        var rp = mna_ndx(np); var rc = mna_ndx(ncn)
        matrix_set(A, rp, rc, matrix_get(A, rp, rc) - gm_val)
        matrix_set(A, rp + dim, rc + dim, matrix_get(A, rp + dim, rc + dim) - gm_val)
    }
    if (nm > 0.0 && nm <= N && ncp > 0.0 && ncp <= N) {
        var rp = mna_ndx(nm); var rc = mna_ndx(ncp)
        matrix_set(A, rp, rc, matrix_get(A, rp, rc) - gm_val)
        matrix_set(A, rp + dim, rc + dim, matrix_get(A, rp + dim, rc + dim) - gm_val)
    }
    if (nm > 0.0 && nm <= N && ncn > 0.0 && ncn <= N) {
        var rp = mna_ndx(nm); var rc = mna_ndx(ncn)
        matrix_set(A, rp, rc, matrix_get(A, rp, rc) + gm_val)
        matrix_set(A, rp + dim, rc + dim, matrix_get(A, rp + dim, rc + dim) + gm_val)
    }
}

def ac_stamp_vsrc_re(A : matrix, b : matrix, np, nm, vval, N, dim, bi) {
    var br = N + bi
    if (np > 0.0 && np <= N) {
        matrix_set(A, br, mna_ndx(np), 1.0); matrix_set(A, mna_ndx(np), br, 1.0)
        matrix_set(A, br + dim, mna_ndx(np) + dim, 1.0); matrix_set(A, mna_ndx(np) + dim, br + dim, 1.0)
    }
    if (nm > 0.0 && nm <= N) {
        matrix_set(A, br, mna_ndx(nm), -1.0); matrix_set(A, mna_ndx(nm), br, -1.0)
        matrix_set(A, br + dim, mna_ndx(nm) + dim, -1.0); matrix_set(A, mna_ndx(nm) + dim, br + dim, -1.0)
    }
    matrix_set(b, br, 0, vval)
}

def ac_stamp_vsrc_cx(A : matrix, b : matrix, np, nm, vr, vi, N, dim, bi) {
    var br = N + bi
    if (np > 0.0 && np <= N) {
        matrix_set(A, br, mna_ndx(np), 1.0); matrix_set(A, mna_ndx(np), br, 1.0)
        matrix_set(A, br + dim, mna_ndx(np) + dim, 1.0); matrix_set(A, mna_ndx(np) + dim, br + dim, 1.0)
    }
    if (nm > 0.0 && nm <= N) {
        matrix_set(A, br, mna_ndx(nm), -1.0); matrix_set(A, mna_ndx(nm), br, -1.0)
        matrix_set(A, br + dim, mna_ndx(nm) + dim, -1.0); matrix_set(A, mna_ndx(nm) + dim, br + dim, -1.0)
    }
    matrix_set(b, br, 0, vr)
    matrix_set(b, br + dim, 0, vi)
}

def ac_stamp_isrc_cx(b : matrix, np, nm, ir, ii, N, dim) {
    if (np > 0.0 && np <= N) {
        matrix_set(b, mna_ndx(np), 0, matrix_get(b, mna_ndx(np), 0) - ir)
        matrix_set(b, mna_ndx(np) + dim, 0, matrix_get(b, mna_ndx(np) + dim, 0) - ii)
    }
    if (nm > 0.0 && nm <= N) {
        matrix_set(b, mna_ndx(nm), 0, matrix_get(b, mna_ndx(nm), 0) + ir)
        matrix_set(b, mna_ndx(nm) + dim, 0, matrix_get(b, mna_ndx(nm) + dim, 0) + ii)
    }
}

def ac_stamp_vcvs(A : matrix, np, nm, ncp, ncn, gain, N, dim, bi) {
    var br = N + bi
    if (np > 0.0 && np <= N) {
        matrix_set(A, br, mna_ndx(np), 1.0); matrix_set(A, mna_ndx(np), br, 1.0)
        matrix_set(A, br + dim, mna_ndx(np) + dim, 1.0); matrix_set(A, mna_ndx(np) + dim, br + dim, 1.0)
    }
    if (nm > 0.0 && nm <= N) {
        matrix_set(A, br, mna_ndx(nm), -1.0); matrix_set(A, mna_ndx(nm), br, -1.0)
        matrix_set(A, br + dim, mna_ndx(nm) + dim, -1.0); matrix_set(A, mna_ndx(nm) + dim, br + dim, -1.0)
    }
    if (ncp > 0.0 && ncp <= N) {
        matrix_set(A, br, mna_ndx(ncp), matrix_get(A, br, mna_ndx(ncp)) - gain)
        matrix_set(A, br + dim, mna_ndx(ncp) + dim, matrix_get(A, br + dim, mna_ndx(ncp) + dim) - gain)
    }
    if (ncn > 0.0 && ncn <= N) {
        matrix_set(A, br, mna_ndx(ncn), matrix_get(A, br, mna_ndx(ncn)) + gain)
        matrix_set(A, br + dim, mna_ndx(ncn) + dim, matrix_get(A, br + dim, mna_ndx(ncn) + dim) + gain)
    }
}

def ac_stamp_diode_ss(A : matrix, comps : matrix, ci, np, nm, V : matrix, N, dim) {
    var isat = circuit_param(comps, ci, 4.0)
    var nf = circuit_param(comps, ci, 5.0)
    var vt = circuit_param(comps, ci, 7.0)
    var vnp = 0.0; var vnm = 0.0
    if (np > 0.0 && np <= N) { vnp = matrix_get(V, np, 0) }
    if (nm > 0.0 && nm <= N) { vnm = matrix_get(V, nm, 0) }
    var vd = vnp - vnm
    var geq = isat / (nf * vt) * exp(vd / (nf * vt))
    if (vd <= -5.0 * nf * vt) { geq = -isat / (nf * vt) }
    ac_stamp_g(A, np, nm, geq, N, dim)
}

def ac_stamp_bjt_ss(A : matrix, comps : matrix, ci, nc_node, nb_node, V : matrix, N, dim) {
    var ne_node = circuit_node_3(comps, ci)
    var isat = circuit_param(comps, ci, 5.0)
    var vaf = circuit_param(comps, ci, 6.0)
    var vt = circuit_param(comps, ci, 7.0)
    var vc_v = 0.0; var vb_v = 0.0; var ve_v = 0.0
    if (nc_node > 0.0 && nc_node <= N) { vc_v = matrix_get(V, nc_node, 0) }
    if (nb_node > 0.0 && nb_node <= N) { vb_v = matrix_get(V, nb_node, 0) }
    if (ne_node > 0.0 && ne_node <= N) { ve_v = matrix_get(V, ne_node, 0) }
    var vbe = vb_v - ve_v; var vce = vc_v - ve_v
    var i_c = isat * exp(vbe / vt)
    if (vaf > 0.0 && vce > -vaf * 0.5) { i_c = i_c * (1.0 + vce / vaf) }
    var gbe = i_c / vt; var gm = gbe; var go = 0.0
    if (vaf > 0.0) { go = i_c / vaf }
    if (nc_node > 0.0 && nc_node <= N) {
        matrix_set(A, mna_ndx(nc_node), mna_ndx(nc_node), matrix_get(A, mna_ndx(nc_node), mna_ndx(nc_node)) + go)
        matrix_set(A, mna_ndx(nc_node) + dim, mna_ndx(nc_node) + dim, matrix_get(A, mna_ndx(nc_node) + dim, mna_ndx(nc_node) + dim) + go)
    }
    if (nc_node > 0.0 && nc_node <= N && nb_node > 0.0 && nb_node <= N) {
        matrix_set(A, mna_ndx(nc_node), mna_ndx(nb_node), matrix_get(A, mna_ndx(nc_node), mna_ndx(nb_node)) - gm)
        matrix_set(A, mna_ndx(nc_node) + dim, mna_ndx(nb_node) + dim, matrix_get(A, mna_ndx(nc_node) + dim, mna_ndx(nb_node) + dim) - gm)
    }
    if (nc_node > 0.0 && nc_node <= N && ne_node > 0.0 && ne_node <= N) {
        matrix_set(A, mna_ndx(nc_node), mna_ndx(ne_node), matrix_get(A, mna_ndx(nc_node), mna_ndx(ne_node)) + gm + go)
        matrix_set(A, mna_ndx(nc_node) + dim, mna_ndx(ne_node) + dim, matrix_get(A, mna_ndx(nc_node) + dim, mna_ndx(ne_node) + dim) + gm + go)
    }
    if (nb_node > 0.0 && nb_node <= N) {
        matrix_set(A, mna_ndx(nb_node), mna_ndx(nb_node), matrix_get(A, mna_ndx(nb_node), mna_ndx(nb_node)) + gbe)
        matrix_set(A, mna_ndx(nb_node) + dim, mna_ndx(nb_node) + dim, matrix_get(A, mna_ndx(nb_node) + dim, mna_ndx(nb_node) + dim) + gbe)
    }
    if (nb_node > 0.0 && nb_node <= N && ne_node > 0.0 && ne_node <= N) {
        matrix_set(A, mna_ndx(nb_node), mna_ndx(ne_node), matrix_get(A, mna_ndx(nb_node), mna_ndx(ne_node)) - gbe)
        matrix_set(A, mna_ndx(nb_node) + dim, mna_ndx(ne_node) + dim, matrix_get(A, mna_ndx(nb_node) + dim, mna_ndx(ne_node) + dim) - gbe)
    }
    if (ne_node > 0.0 && ne_node <= N) {
        matrix_set(A, mna_ndx(ne_node), mna_ndx(ne_node), matrix_get(A, mna_ndx(ne_node), mna_ndx(ne_node)) + gm + gbe + go)
        matrix_set(A, mna_ndx(ne_node) + dim, mna_ndx(ne_node) + dim, matrix_get(A, mna_ndx(ne_node) + dim, mna_ndx(ne_node) + dim) + gm + gbe + go)
    }
}

def ac_mag_db(vr, vi) {
    sqrt(vr * vr + vi * vi)
}

def ac_node_voltage(sol : matrix, nd, dim) {
    var vr = matrix_get(sol, 2.0 + nd - 1.0, 0)
    var vi = matrix_get(sol, 2.0 + dim + nd - 1.0, 0)
    var mag = ac_mag_db(vr, vi)
    var result = matrix_zeros(4, 1)
    matrix_set(result, 0, 0, vr)
    matrix_set(result, 1, 0, vi)
    matrix_set(result, 2, 0, mag)
    matrix_set(result, 3, 0, 0.0)
    result
}

def ac_sweep(comps : matrix, ctrl : matrix, f_start, f_stop, npoints, V : matrix) {
    var log_fs = log(f_start)
    var log_fe = log(f_stop)
    var step = 0.0
    if (npoints > 1.0) { step = (log_fe - log_fs) / npoints }
    var results = matrix_zeros(npoints + 1.0, 4.0)
    var si = 0.0
    while (si <= npoints) {
        var freq = exp(log_fs + si * step)
        var sol = ac_build(comps, ctrl, freq, V)
        var dim = matrix_get(sol, 0, 0)
        var vr = matrix_get(sol, 2.0, 0)
        var vi = matrix_get(sol, 2.0 + dim, 0)
        var mag = sqrt(vr * vr + vi * vi)
        var db = 0.0
        if (mag > 1e-15) { db = 20.0 * log(mag) / log(10.0) }
        matrix_set(results, si, 0, freq)
        matrix_set(results, si, 1, db)
        matrix_set(results, si, 2, mag)
        matrix_set(results, si, 3, 0.0)
        si = si + 1.0
    }
    results
}
