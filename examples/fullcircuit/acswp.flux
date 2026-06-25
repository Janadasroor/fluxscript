# FullCircuit — AC Small-Signal Frequency Sweep Analysis
import mna
import dcsolve

def ac_build(comps: matrix, ctrl: matrix, freq: Double, V: matrix) -> matrix {
    var N = matrix_get(ctrl, 0.0, 0.0)
    var nc = matrix_get(ctrl, 1.0, 0.0)
    var M = mna_count_branches(comps, ctrl)
    var dim = N + M
    var dim2 = 2.0 * dim
    var A = matrix_zeros(dim2, dim2)
    var b = matrix_zeros(dim2, 1)
    var w = 2.0 * pi() * freq
    var bi = 0.0
    var ci = 0.0
    while (ci < nc) {
        var t = matrix_get(comps, ci, 0.0)
        var np = matrix_get(comps, ci, 1.0)
        var nm = matrix_get(comps, ci, 2.0)
        if (t == 0.0) {
            var rv = matrix_get(comps, ci, 4.0)
            if (rv > 1e-15) { ac_stamp_g(A, np, nm, 1.0 / rv, N, dim) }
        } else if (t == 1.0) {
            var cv = matrix_get(comps, ci, 4.0)
            if (cv > 1e-20) { ac_stamp_sus(A, np, nm, w * cv, N, dim) }
        } else if (t == 2.0) {
            var lv = matrix_get(comps, ci, 4.0)
            if (lv > 1e-20 && w > 1e-20) { ac_stamp_sus(A, np, nm, -1.0 / (w * lv), N, dim) }
            bi = bi + 1.0
        } else if (t == 3.0) {
            ac_stamp_vsrc_re(A, b, np, nm, 0.0, N, dim, bi)
            bi = bi + 1.0
        } else if (t == 5.0) {
            var amag = matrix_get(comps, ci, 8.0)
            var aphase = matrix_get(comps, ci, 10.0)
            var pr = aphase * pi() / 180.0
            ac_stamp_vsrc_cx(A, b, np, nm, amag * cos(pr), amag * sin(pr), N, dim, bi)
            bi = bi + 1.0
        } else if (t == 6.0) {
            var amag = matrix_get(comps, ci, 8.0)
            var aphase = matrix_get(comps, ci, 10.0)
            var pr = aphase * pi() / 180.0
            ac_stamp_isrc_cx(b, np, nm, amag * cos(pr), amag * sin(pr), N, dim)
        } else if (t == 7.0) {
            var n3 = matrix_get(comps, ci, 3.0)
            var n4 = matrix_get(comps, ci, 4.0)
            var gain = matrix_get(comps, ci, 5.0)
            ac_stamp_vcvs(A, np, nm, n3, n4, gain, N, dim, bi)
            bi = bi + 1.0
        } else if (t == 8.0) {
            var n3 = matrix_get(comps, ci, 3.0)
            var n4 = matrix_get(comps, ci, 4.0)
            ac_stamp_gm(A, np, nm, n3, n4, matrix_get(comps, ci, 5.0), N, dim)
        } else if (t == 9.0) {
            ac_stamp_diode_ss(A, comps, ci, np, nm, V, N, dim)
        } else if (t == 10.0 || t == 11.0) {
            ac_stamp_bjt_ss(A, comps, ci, np, nm, V, N, dim)
        } else if (t == 12.0 || t == 13.0) {
            ac_stamp_mos_ss(A, comps, ci, np, nm, V, N, dim)
            
        } else if (t == 14.0 || t == 15.0) {
            ac_stamp_jfet_ss(A, comps, ci, np, nm, V, N, dim)
        } else if (t == 16.0) {
            ac_stamp_vcsw_ss(A, comps, ci, np, nm, V, N, dim)
        } else if (t == 17.0) {
            ac_stamp_ccsw_ss(A, comps, ctrl, ci, np, nm, V, N, dim)
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

def ac_stamp_g(A: matrix, np: Double, nm: Double, g: Double, N: Double, dim: Double) {
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

def ac_stamp_sus(A: matrix, np: Double, nm: Double, bval: Double, N: Double, dim: Double) {
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

def ac_stamp_gm(A: matrix, np: Double, nm: Double, ncp: Double, ncn: Double, gm_val: Double, N: Double, dim: Double) {
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

def ac_stamp_vsrc_re(A: matrix, b: matrix, np: Double, nm: Double, vval: Double, N: Double, dim: Double, bi: Double) {
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

def ac_stamp_vsrc_cx(A: matrix, b: matrix, np: Double, nm: Double, vr: Double, vi: Double, N: Double, dim: Double, bi: Double) {
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

def ac_stamp_isrc_cx(b: matrix, np: Double, nm: Double, ir: Double, ii: Double, N: Double, dim: Double) {
    if (np > 0.0 && np <= N) {
        matrix_set(b, mna_ndx(np), 0, matrix_get(b, mna_ndx(np), 0) - ir)
        matrix_set(b, mna_ndx(np) + dim, 0, matrix_get(b, mna_ndx(np) + dim, 0) - ii)
    }
    if (nm > 0.0 && nm <= N) {
        matrix_set(b, mna_ndx(nm), 0, matrix_get(b, mna_ndx(nm), 0) + ir)
        matrix_set(b, mna_ndx(nm) + dim, 0, matrix_get(b, mna_ndx(nm) + dim, 0) + ii)
    }
}

def ac_stamp_vcvs(A: matrix, np: Double, nm: Double, ncp: Double, ncn: Double, gain: Double, N: Double, dim: Double, bi: Double) {
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

def ac_stamp_diode_ss(A: matrix, comps: matrix, ci: Double, np: Double, nm: Double, V: matrix, N: Double, dim: Double) {
    var isat = matrix_get(comps, ci, 4.0)
    var nf = matrix_get(comps, ci, 5.0)
    var vt = matrix_get(comps, ci, 7.0)
    var vnp = 0.0; var vnm = 0.0
    if (np > 0.0 && np <= N) { vnp = matrix_get(V, np, 0) }
    if (nm > 0.0 && nm <= N) { vnm = matrix_get(V, nm, 0) }
    var vd = vnp - vnm
    var djr = diode_junction(vd, isat, nf, vt)
    var geq = matrix_get(djr, 1, 0)
    ac_stamp_g(A, np, nm, geq, N, dim)
}

def ac_stamp_bjt_ss(A: matrix, comps: matrix, ci: Double, nc_node: Double, nb_node: Double, V: matrix, N: Double, dim: Double) {
    var ne_node = matrix_get(comps, ci, 3.0)
    var bf = matrix_get(comps, ci, 4.0)
    var isat = matrix_get(comps, ci, 5.0)
    var vaf = matrix_get(comps, ci, 6.0)
    var vt = matrix_get(comps, ci, 7.0)
    var br = matrix_get(comps, ci, 8.0)
    if (br <= 0.0) { br = 2.0 }
    var vc_v = 0.0; var vb_v = 0.0; var ve_v = 0.0
    if (nc_node > 0.0 && nc_node <= N) { vc_v = matrix_get(V, nc_node, 0) }
    if (nb_node > 0.0 && nb_node <= N) { vb_v = matrix_get(V, nb_node, 0) }
    if (ne_node > 0.0 && ne_node <= N) { ve_v = matrix_get(V, ne_node, 0) }
    var vbe = vb_v - ve_v
    var vbc = vb_v - vc_v
    var vce = vc_v - ve_v
    var be_jn = diode_junction(vbe, isat, 1.0, vt)
    var icc = matrix_get(be_jn, 0, 0)
    var gbe = matrix_get(be_jn, 1, 0)
    var bc_jn = diode_junction(vbc, isat, 1.0, vt)
    var iec = matrix_get(bc_jn, 0, 0)
    var gbc = matrix_get(bc_jn, 1, 0)
    var af = bf / (bf + 1.0)
    var go = 0.0
    if (vaf > 0.0 && vce > -vaf * 0.5) {
        var vce_vaf = vce
        if (vce_vaf < 0.0) { vce_vaf = 0.0 }
        icc = icc * (1.0 + vce_vaf / vaf)
        go = icc / vaf
    }
    var gpi = gbe / bf + gbc / br
    var gm_cb = af * gbe - gbc
    if (nc_node > 0.0 && nc_node <= N) {
        matrix_set(A, mna_ndx(nc_node), mna_ndx(nc_node), matrix_get(A, mna_ndx(nc_node), mna_ndx(nc_node)) + go + gbc)
        matrix_set(A, mna_ndx(nc_node) + dim, mna_ndx(nc_node) + dim, matrix_get(A, mna_ndx(nc_node) + dim, mna_ndx(nc_node) + dim) + go + gbc)
    }
    if (nc_node > 0.0 && nc_node <= N && nb_node > 0.0 && nb_node <= N) {
        matrix_set(A, mna_ndx(nc_node), mna_ndx(nb_node), matrix_get(A, mna_ndx(nc_node), mna_ndx(nb_node)) + gm_cb)
        matrix_set(A, mna_ndx(nc_node) + dim, mna_ndx(nb_node) + dim, matrix_get(A, mna_ndx(nc_node) + dim, mna_ndx(nb_node) + dim) + gm_cb)
    }
    if (nc_node > 0.0 && nc_node <= N && ne_node > 0.0 && ne_node <= N) {
        matrix_set(A, mna_ndx(nc_node), mna_ndx(ne_node), matrix_get(A, mna_ndx(nc_node), mna_ndx(ne_node)) - (af * gbe + go))
        matrix_set(A, mna_ndx(nc_node) + dim, mna_ndx(ne_node) + dim, matrix_get(A, mna_ndx(nc_node) + dim, mna_ndx(ne_node) + dim) - (af * gbe + go))
    }
    if (nb_node > 0.0 && nb_node <= N) {
        matrix_set(A, mna_ndx(nb_node), mna_ndx(nb_node), matrix_get(A, mna_ndx(nb_node), mna_ndx(nb_node)) + gpi)
        matrix_set(A, mna_ndx(nb_node) + dim, mna_ndx(nb_node) + dim, matrix_get(A, mna_ndx(nb_node) + dim, mna_ndx(nb_node) + dim) + gpi)
    }
    if (nb_node > 0.0 && nb_node <= N && nc_node > 0.0 && nc_node <= N) {
        matrix_set(A, mna_ndx(nb_node), mna_ndx(nc_node), matrix_get(A, mna_ndx(nb_node), mna_ndx(nc_node)) - gbc / br)
        matrix_set(A, mna_ndx(nb_node) + dim, mna_ndx(nc_node) + dim, matrix_get(A, mna_ndx(nb_node) + dim, mna_ndx(nc_node) + dim) - gbc / br)
    }
    if (nb_node > 0.0 && nb_node <= N && ne_node > 0.0 && ne_node <= N) {
        matrix_set(A, mna_ndx(nb_node), mna_ndx(ne_node), matrix_get(A, mna_ndx(nb_node), mna_ndx(ne_node)) - gbe / bf)
        matrix_set(A, mna_ndx(nb_node) + dim, mna_ndx(ne_node) + dim, matrix_get(A, mna_ndx(nb_node) + dim, mna_ndx(ne_node) + dim) - gbe / bf)
    }
    if (ne_node > 0.0 && ne_node <= N && nb_node > 0.0 && nb_node <= N) {
        matrix_set(A, mna_ndx(ne_node), mna_ndx(nb_node), matrix_get(A, mna_ndx(ne_node), mna_ndx(nb_node)) - (af * gbe + gbe / bf) + gbc * (1.0 - 1.0 / br))
        matrix_set(A, mna_ndx(ne_node) + dim, mna_ndx(nb_node) + dim, matrix_get(A, mna_ndx(ne_node) + dim, mna_ndx(nb_node) + dim) - (af * gbe + gbe / bf) + gbc * (1.0 - 1.0 / br))
    }
    if (ne_node > 0.0 && ne_node <= N && nc_node > 0.0 && nc_node <= N) {
        matrix_set(A, mna_ndx(ne_node), mna_ndx(nc_node), matrix_get(A, mna_ndx(ne_node), mna_ndx(nc_node)) - go - gbc * (1.0 - 1.0 / br))
        matrix_set(A, mna_ndx(ne_node) + dim, mna_ndx(nc_node) + dim, matrix_get(A, mna_ndx(ne_node) + dim, mna_ndx(nc_node) + dim) - go - gbc * (1.0 - 1.0 / br))
    }
    if (ne_node > 0.0 && ne_node <= N) {
        matrix_set(A, mna_ndx(ne_node), mna_ndx(ne_node), matrix_get(A, mna_ndx(ne_node), mna_ndx(ne_node)) + af * gbe + gbe / bf + go)
        matrix_set(A, mna_ndx(ne_node) + dim, mna_ndx(ne_node) + dim, matrix_get(A, mna_ndx(ne_node) + dim, mna_ndx(ne_node) + dim) + af * gbe + gbe / bf + go)
    }
}

def ac_stamp_mos_ss(A: matrix, comps: matrix, ci: Double, nd_node: Double, ng_node: Double, V: matrix, N: Double, dim: Double) {
    var ns_node = matrix_get(comps, ci, 3.0)
    var t = matrix_get(comps, ci, 0.0)
    var kp_val = matrix_get(comps, ci, 5.0)
    var vto_val = matrix_get(comps, ci, 6.0)
    var lambda_val = matrix_get(comps, ci, 7.0)
    var vd_v = 0.0; var vg_v = 0.0; var vs_v = 0.0
    if (nd_node > 0.0 && nd_node <= N) { vd_v = matrix_get(V, nd_node, 0) }
    if (ng_node > 0.0 && ng_node <= N) { vg_v = matrix_get(V, ng_node, 0) }
    if (ns_node > 0.0 && ns_node <= N) { vs_v = matrix_get(V, ns_node, 0) }
    var vgs = vg_v - vs_v
    var vds = vd_v - vs_v
    var gm = 0.0; var gds = 0.0
    var vov = vgs - vto_val
    if (t == 13.0) { vgs = -vgs; vds = -vds; vov = -vov }
    if (vov > 0.0) {
        if (vds < vov) {
            var id = kp_val * (vov * vds - 0.5 * vds * vds)
            gm = kp_val * vds
            gds = kp_val * (vov - vds)
            if (lambda_val > 0.0) {
                id = id * (1.0 + lambda_val * vds)
                gm = gm * (1.0 + lambda_val * vds)
                gds = gds * (1.0 + lambda_val * vds) + lambda_val * id / (1.0 + lambda_val * vds)
            }
        } else {
            var id = 0.5 * kp_val * vov * vov
            gm = kp_val * vov
            gds = 0.0
            if (lambda_val > 0.0) {
                id = id * (1.0 + lambda_val * vds)
                gm = gm * (1.0 + lambda_val * vds)
                gds = lambda_val * id / (1.0 + lambda_val * vds)
            }
        }
    }
    var gds_plus_gm = gds + gm
    if (nd_node > 0.0 && nd_node <= N) {
        matrix_set(A, mna_ndx(nd_node), mna_ndx(nd_node), matrix_get(A, mna_ndx(nd_node), mna_ndx(nd_node)) + gds)
        matrix_set(A, mna_ndx(nd_node) + dim, mna_ndx(nd_node) + dim, matrix_get(A, mna_ndx(nd_node) + dim, mna_ndx(nd_node) + dim) + gds)
    }
    if (nd_node > 0.0 && nd_node <= N && ng_node > 0.0 && ng_node <= N) {
        matrix_set(A, mna_ndx(nd_node), mna_ndx(ng_node), matrix_get(A, mna_ndx(nd_node), mna_ndx(ng_node)) + gm)
        matrix_set(A, mna_ndx(nd_node) + dim, mna_ndx(ng_node) + dim, matrix_get(A, mna_ndx(nd_node) + dim, mna_ndx(ng_node) + dim) + gm)
    }
    if (nd_node > 0.0 && nd_node <= N && ns_node > 0.0 && ns_node <= N) {
        matrix_set(A, mna_ndx(nd_node), mna_ndx(ns_node), matrix_get(A, mna_ndx(nd_node), mna_ndx(ns_node)) - gds_plus_gm)
        matrix_set(A, mna_ndx(nd_node) + dim, mna_ndx(ns_node) + dim, matrix_get(A, mna_ndx(nd_node) + dim, mna_ndx(ns_node) + dim) - gds_plus_gm)
    }
    if (ns_node > 0.0 && ns_node <= N && nd_node > 0.0 && nd_node <= N) {
        matrix_set(A, mna_ndx(ns_node), mna_ndx(nd_node), matrix_get(A, mna_ndx(ns_node), mna_ndx(nd_node)) - gds)
        matrix_set(A, mna_ndx(ns_node) + dim, mna_ndx(nd_node) + dim, matrix_get(A, mna_ndx(ns_node) + dim, mna_ndx(nd_node) + dim) - gds)
    }
    if (ns_node > 0.0 && ns_node <= N && ng_node > 0.0 && ng_node <= N) {
        matrix_set(A, mna_ndx(ns_node), mna_ndx(ng_node), matrix_get(A, mna_ndx(ns_node), mna_ndx(ng_node)) - gm)
        matrix_set(A, mna_ndx(ns_node) + dim, mna_ndx(ng_node) + dim, matrix_get(A, mna_ndx(ns_node) + dim, mna_ndx(ng_node) + dim) - gm)
    }
    if (ns_node > 0.0 && ns_node <= N) {
        matrix_set(A, mna_ndx(ns_node), mna_ndx(ns_node), matrix_get(A, mna_ndx(ns_node), mna_ndx(ns_node)) + gds_plus_gm)
        matrix_set(A, mna_ndx(ns_node) + dim, mna_ndx(ns_node) + dim, matrix_get(A, mna_ndx(ns_node) + dim, mna_ndx(ns_node) + dim) + gds_plus_gm)
    }
}

def ac_stamp_jfet_ss(A: matrix, comps: matrix, ci: Double, nd_node: Double, ng_node: Double, V: matrix, N: Double, dim: Double) {
    var ns_node = matrix_get(comps, ci, 3.0)
    var t = matrix_get(comps, ci, 0.0)
    var beta_val = matrix_get(comps, ci, 5.0)
    var vto_val = matrix_get(comps, ci, 6.0)
    var lambda_val = matrix_get(comps, ci, 7.0)
    var vd_v = 0.0; var vg_v = 0.0; var vs_v = 0.0
    if (nd_node > 0.0 && nd_node <= N) { vd_v = matrix_get(V, nd_node, 0) }
    if (ng_node > 0.0 && ng_node <= N) { vg_v = matrix_get(V, ng_node, 0) }
    if (ns_node > 0.0 && ns_node <= N) { vs_v = matrix_get(V, ns_node, 0) }
    var vgs = vg_v - vs_v
    var vds = vd_v - vs_v
    var gm = 0.0; var gds = 0.0
    var vov = vgs - vto_val
    if (t == 15.0) { vgs = -vgs; vds = -vds; vov = -vov }
    if (vov > 0.0) {
        if (vds < vov) {
            var id = beta_val * (vov * vds - 0.5 * vds * vds)
            gm = beta_val * vds
            gds = beta_val * (vov - vds)
            if (lambda_val > 0.0) {
                id = id * (1.0 + lambda_val * vds)
                gm = gm * (1.0 + lambda_val * vds)
                gds = gds * (1.0 + lambda_val * vds) + lambda_val * id / (1.0 + lambda_val * vds)
            }
        } else {
            var id = 0.5 * beta_val * vov * vov
            gm = beta_val * vov
            gds = 0.0
            if (lambda_val > 0.0) {
                id = id * (1.0 + lambda_val * vds)
                gm = gm * (1.0 + lambda_val * vds)
                gds = lambda_val * id / (1.0 + lambda_val * vds)
            }
        }
    }
    var gds_plus_gm = gds + gm
    if (nd_node > 0.0 && nd_node <= N) {
        matrix_set(A, mna_ndx(nd_node), mna_ndx(nd_node), matrix_get(A, mna_ndx(nd_node), mna_ndx(nd_node)) + gds)
        matrix_set(A, mna_ndx(nd_node) + dim, mna_ndx(nd_node) + dim, matrix_get(A, mna_ndx(nd_node) + dim, mna_ndx(nd_node) + dim) + gds)
    }
    if (nd_node > 0.0 && nd_node <= N && ng_node > 0.0 && ng_node <= N) {
        matrix_set(A, mna_ndx(nd_node), mna_ndx(ng_node), matrix_get(A, mna_ndx(nd_node), mna_ndx(ng_node)) + gm)
        matrix_set(A, mna_ndx(nd_node) + dim, mna_ndx(ng_node) + dim, matrix_get(A, mna_ndx(nd_node) + dim, mna_ndx(ng_node) + dim) + gm)
    }
    if (nd_node > 0.0 && nd_node <= N && ns_node > 0.0 && ns_node <= N) {
        matrix_set(A, mna_ndx(nd_node), mna_ndx(ns_node), matrix_get(A, mna_ndx(nd_node), mna_ndx(ns_node)) - gds_plus_gm)
        matrix_set(A, mna_ndx(nd_node) + dim, mna_ndx(ns_node) + dim, matrix_get(A, mna_ndx(nd_node) + dim, mna_ndx(ns_node) + dim) - gds_plus_gm)
    }
    if (ns_node > 0.0 && ns_node <= N && nd_node > 0.0 && nd_node <= N) {
        matrix_set(A, mna_ndx(ns_node), mna_ndx(nd_node), matrix_get(A, mna_ndx(ns_node), mna_ndx(nd_node)) - gds)
        matrix_set(A, mna_ndx(ns_node) + dim, mna_ndx(nd_node) + dim, matrix_get(A, mna_ndx(ns_node) + dim, mna_ndx(nd_node) + dim) - gds)
    }
    if (ns_node > 0.0 && ns_node <= N && ng_node > 0.0 && ng_node <= N) {
        matrix_set(A, mna_ndx(ns_node), mna_ndx(ng_node), matrix_get(A, mna_ndx(ns_node), mna_ndx(ng_node)) - gm)
        matrix_set(A, mna_ndx(ns_node) + dim, mna_ndx(ng_node) + dim, matrix_get(A, mna_ndx(ns_node) + dim, mna_ndx(ng_node) + dim) - gm)
    }
    if (ns_node > 0.0 && ns_node <= N) {
        matrix_set(A, mna_ndx(ns_node), mna_ndx(ns_node), matrix_get(A, mna_ndx(ns_node), mna_ndx(ns_node)) + gds_plus_gm)
        matrix_set(A, mna_ndx(ns_node) + dim, mna_ndx(ns_node) + dim, matrix_get(A, mna_ndx(ns_node) + dim, mna_ndx(ns_node) + dim) + gds_plus_gm)
    }
}

def ac_mag_db(vr: Double, vi: Double) -> Double {
    sqrt(vr * vr + vi * vi)
}

def ac_node_voltage(sol: matrix, nd: Double, dim: Double) -> matrix {
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

def ac_sweep(comps: matrix, ctrl: matrix, f_start: Double, f_stop: Double, npoints: Double, V: matrix) -> matrix {
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

def ac_stamp_vcsw_ss(A: matrix, comps: matrix, ci: Double, np_node: Double, nm_node: Double, V: matrix, N: Double, dim: Double) {
    var ncp_node = matrix_get(comps, ci, 3.0)
    var ncm_node = matrix_get(comps, ci, 4.0)
    var vnp = 0.0; var vnm = 0.0; var vcp = 0.0; var vcm = 0.0
    if (np_node > 0.0 && np_node <= N) { vnp = matrix_get(V, np_node, 0) }
    if (nm_node > 0.0 && nm_node <= N) { vnm = matrix_get(V, nm_node, 0) }
    if (ncp_node > 0.0 && ncp_node <= N) { vcp = matrix_get(V, ncp_node, 0) }
    if (ncm_node > 0.0 && ncm_node <= N) { vcm = matrix_get(V, ncm_node, 0) }
    var Vctrl = vcp - vcm
    var Vdiff = vnp - vnm
    var sw_info = vcsw_conductance(comps, ci, Vctrl)
    var g = matrix_get(sw_info, 0, 0)
    var dg = matrix_get(sw_info, 1, 0)
    var gm_val = dg * Vdiff
    ac_stamp_g(A, np_node, nm_node, g, N, dim)
    if (gm_val != 0.0) {
        ac_stamp_gm(A, np_node, nm_node, ncp_node, ncm_node, gm_val, N, dim)
    }
}

def ac_stamp_ccsw_ss(A: matrix, comps: matrix, ctrl: matrix, ci: Double, np_node: Double, nm_node: Double, V: matrix, N: Double, dim: Double) {
    var vsrc_idx = matrix_get(comps, ci, 3.0)
    var Ictrl = matrix_get(comps, ci, 9.0)
    var vnp = 0.0; var vnm = 0.0
    if (np_node > 0.0 && np_node <= N) { vnp = matrix_get(V, np_node, 0) }
    if (nm_node > 0.0 && nm_node <= N) { vnm = matrix_get(V, nm_node, 0) }
    var Vdiff = vnp - vnm
    var sw_info = ccsw_conductance(comps, ci, Ictrl)
    var g = matrix_get(sw_info, 0, 0)
    var dg = matrix_get(sw_info, 1, 0)
    var gm_val = dg * Vdiff
    ac_stamp_g(A, np_node, nm_node, g, N, dim)
    if (gm_val != 0.0 && vsrc_idx >= 0.0) {
        var vsrc_bi = mna_branch_index(comps, ctrl, vsrc_idx)
        var br = N + vsrc_bi
        if (np_node > 0.0 && np_node <= N && br < N + dim) {
            matrix_set(A, mna_ndx(np_node), br, matrix_get(A, mna_ndx(np_node), br) + gm_val)
            matrix_set(A, mna_ndx(np_node) + dim, br + dim, matrix_get(A, mna_ndx(np_node) + dim, br + dim) + gm_val)
        }
        if (nm_node > 0.0 && nm_node <= N && br < N + dim) {
            matrix_set(A, mna_ndx(nm_node), br, matrix_get(A, mna_ndx(nm_node), br) - gm_val)
            matrix_set(A, mna_ndx(nm_node) + dim, br + dim, matrix_get(A, mna_ndx(nm_node) + dim, br + dim) - gm_val)
        }
    }
}
