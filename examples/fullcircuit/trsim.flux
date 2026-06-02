# FullCircuit — Transient Analysis (Time-Domain)
import mna
import dcsolve

def tr_src_val(comps : matrix, ci : Double, t : Double) : Double {
    var tc = matrix_get(comps, ci, 11.0)
    var dc_val = matrix_get(comps, ci, 4.0)
    if (tc == 1.0) {
        var v1 = matrix_get(comps, ci, 8.0)
        var v2 = matrix_get(comps, ci, 9.0)
        var td = matrix_get(comps, ci, 10.0)
        var t_rise = matrix_get(comps, ci, 5.0)
        var t_fall = matrix_get(comps, ci, 6.0)
        var pw = matrix_get(comps, ci, 7.0)
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
        var voff = matrix_get(comps, ci, 8.0)
        var vamp = matrix_get(comps, ci, 9.0)
        var freq = matrix_get(comps, ci, 10.0)
        var ph = matrix_get(comps, ci, 5.0)
        if (t < 0.0) { voff }
        else { voff + vamp * sin(2.0 * pi() * freq * t + ph * pi() / 180.0) }
    } else { dc_val }
}

def tr_step(comps : matrix, ctrl : matrix, t : Double, dt : Double, cap_v : matrix, ind_i : matrix, V : matrix, max_iter : Double, tol : Double) : matrix {
    var N = matrix_get(ctrl, 0.0, 0.0)
    var nc = matrix_get(ctrl, 1.0, 0.0)
    var M = mna_count_branches(comps, ctrl)
    var dim = N + M

    var has_nonlinear = 0.0
    var ci = 0.0
    while (ci < nc) {
        var tc = matrix_get(comps, ci, 0.0)
        if (tc == 9.0 || tc == 10.0 || tc == 11.0 || tc == 12.0 || tc == 13.0 || tc == 14.0 || tc == 15.0 || tc == 16.0 || tc == 17.0) { has_nonlinear = 1.0 }
        ci = ci + 1.0
    }

    if (has_nonlinear == 0.0) {
        var A = matrix_zeros(dim, dim)
        var b = matrix_zeros(dim, 1)
        var bi = 0.0
        ci = 0.0
        while (ci < nc) {
            var tc = matrix_get(comps, ci, 0.0)
            var np = matrix_get(comps, ci, 1.0)
            var nm = matrix_get(comps, ci, 2.0)
            if (tc == 0.0) {
                var rv = matrix_get(comps, ci, 4.0)
                if (rv > 1e-15) { mna_stamp_g(A, np, nm, 1.0 / rv, N) }
            } else if (tc == 1.0) {
                var cv = matrix_get(comps, ci, 4.0)
                var geq = cv / dt
                var vco = matrix_get(cap_v, ci, 0)
                mna_stamp_g(A, np, nm, geq, N)
                mna_stamp_isource(b, np, nm, -geq * vco, N)
            } else if (tc == 2.0) {
                var lv = matrix_get(comps, ci, 4.0)
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
                var n3 = matrix_get(comps, ci, 3.0)
                var n4 = matrix_get(comps, ci, 4.0)
                mna_stamp_vcvs(A, np, nm, n3, n4, matrix_get(comps, ci, 5.0), N, bi)
                bi = bi + 1.0
            } else if (tc == 8.0) {
                var n3 = matrix_get(comps, ci, 3.0)
                var n4 = matrix_get(comps, ci, 4.0)
                mna_stamp_vccs(A, np, nm, n3, n4, matrix_get(comps, ci, 5.0), N)
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
    } else {
        var converged = 0.0
        var iter = 0.0
        while (iter < max_iter && converged == 0.0) {
            var A = matrix_zeros(dim, dim)
            var b = matrix_zeros(dim, 1)
            var bi = 0.0
            ci = 0.0
            while (ci < nc) {
                var tc = matrix_get(comps, ci, 0.0)
                var np = matrix_get(comps, ci, 1.0)
                var nm = matrix_get(comps, ci, 2.0)
                if (tc == 0.0) {
                    var rv = matrix_get(comps, ci, 4.0)
                    if (rv > 1e-15) { mna_stamp_g(A, np, nm, 1.0 / rv, N) }
                } else if (tc == 1.0) {
                    var cv = matrix_get(comps, ci, 4.0)
                    var geq = cv / dt
                    var vco = matrix_get(cap_v, ci, 0)
                    mna_stamp_g(A, np, nm, geq, N)
                    mna_stamp_isource(b, np, nm, -geq * vco, N)
                } else if (tc == 2.0) {
                    var lv = matrix_get(comps, ci, 4.0)
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
                    var n3 = matrix_get(comps, ci, 3.0)
                    var n4 = matrix_get(comps, ci, 4.0)
                    mna_stamp_vcvs(A, np, nm, n3, n4, matrix_get(comps, ci, 5.0), N, bi)
                    bi = bi + 1.0
                } else if (tc == 8.0) {
                    var n3 = matrix_get(comps, ci, 3.0)
                    var n4 = matrix_get(comps, ci, 4.0)
                    mna_stamp_vccs(A, np, nm, n3, n4, matrix_get(comps, ci, 5.0), N)
                } else if (tc == 9.0) {
                    var isat = matrix_get(comps, ci, 4.0)
                    var nf = matrix_get(comps, ci, 5.0)
                    var vt = matrix_get(comps, ci, 7.0)
                    var vnp = 0.0; var vnm = 0.0
                    if (np > 0.0 && np <= N) { vnp = matrix_get(V, np, 0) }
                    if (nm > 0.0 && nm <= N) { vnm = matrix_get(V, nm, 0) }
                    var vd = vnp - vnm
                    var djr = diode_junction(vd, isat, nf, vt)
                    var id = matrix_get(djr, 0, 0)
                    var geq = matrix_get(djr, 1, 0)
                    var ieq = id - geq * vd
                    mna_stamp_g(A, np, nm, geq, N)
                    mna_stamp_isource(b, np, nm, ieq, N)
                } else if (tc == 10.0 || tc == 11.0) {
                    var nc_node = np
                    var nb_node = nm
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
                    var sign = 1.0
                    if (tc == 11.0) { sign = -1.0 }
                    var be_jn = diode_junction(vbe * sign, isat, 1.0, vt)
                    var icc = matrix_get(be_jn, 0, 0)
                    var gbe = matrix_get(be_jn, 1, 0)
                    var bc_jn = diode_junction(vbc * sign, isat, 1.0, vt)
                    var iec = matrix_get(bc_jn, 0, 0)
                    var gbc = matrix_get(bc_jn, 1, 0)
                    var af = bf / (bf + 1.0)
                    var go = 0.0
                    if (vaf > 0.0 && vce * sign > -vaf * 0.5) {
                        var vce_vaf = vce * sign
                        if (vce_vaf < 0.0) { vce_vaf = 0.0 }
                        icc = icc * (1.0 + vce_vaf / vaf)
                        go = icc / vaf
                    }
                    var i_c = af * icc - iec
                    var ib = icc / bf + iec / br
                    if (tc == 11.0) { i_c = -i_c; ib = -ib }
                    var gpi = gbe / bf + gbc / br
                    var i_co = i_c - (af * gbe - gbc) * vb_v - (go + gbc) * vc_v + (af * gbe + go) * ve_v
                    var ib_co = ib - gpi * vb_v + (gbc / br) * vc_v + (gbe / bf) * ve_v
                    if (nc_node > 0.0 && nc_node <= N) {
                        A[mna_ndx(nc_node), mna_ndx(nc_node)] += go + gbc
                    }
                    if (nc_node > 0.0 && nc_node <= N && nb_node > 0.0 && nb_node <= N) {
                        A[mna_ndx(nc_node), mna_ndx(nb_node)] += af * gbe - gbc
                    }
                    if (nc_node > 0.0 && nc_node <= N && ne_node > 0.0 && ne_node <= N) {
                        A[mna_ndx(nc_node), mna_ndx(ne_node)] -= af * gbe + go
                    }
                    if (nb_node > 0.0 && nb_node <= N) {
                        A[mna_ndx(nb_node), mna_ndx(nb_node)] += gpi
                    }
                    if (nb_node > 0.0 && nb_node <= N && nc_node > 0.0 && nc_node <= N) {
                        A[mna_ndx(nb_node), mna_ndx(nc_node)] -= gbc / br
                    }
                    if (nb_node > 0.0 && nb_node <= N && ne_node > 0.0 && ne_node <= N) {
                        A[mna_ndx(nb_node), mna_ndx(ne_node)] -= gbe / bf
                    }
                    if (ne_node > 0.0 && ne_node <= N && nb_node > 0.0 && nb_node <= N) {
                        A[mna_ndx(ne_node), mna_ndx(nb_node)] -= (af * gbe + gbe / bf) - gbc * (1.0 - 1.0 / br)
                    }
                    if (ne_node > 0.0 && ne_node <= N && nc_node > 0.0 && nc_node <= N) {
                        A[mna_ndx(ne_node), mna_ndx(nc_node)] -= go + gbc * (1.0 - 1.0 / br)
                    }
                    if (ne_node > 0.0 && ne_node <= N) {
                        A[mna_ndx(ne_node), mna_ndx(ne_node)] += af * gbe + gbe / bf + go
                    }
                    if (nc_node > 0.0 && nc_node <= N) {
                        b[mna_ndx(nc_node), 0] -= i_co
                    }
                    if (nb_node > 0.0 && nb_node <= N) {
                        b[mna_ndx(nb_node), 0] -= ib_co
                    }
                    if (ne_node > 0.0 && ne_node <= N) {
                        b[mna_ndx(ne_node), 0] += i_co + ib_co
                    }
                } else if (tc == 12.0 || tc == 13.0) {
                    var nd_node = np
                    var ng_node = nm
                    var ns_node = matrix_get(comps, ci, 3.0)
                    var vd_v = 0.0; var vg_v = 0.0; var vs_v = 0.0
                    if (nd_node > 0.0 && nd_node <= N) { vd_v = matrix_get(V, nd_node, 0) }
                    if (ng_node > 0.0 && ng_node <= N) { vg_v = matrix_get(V, ng_node, 0) }
                    if (ns_node > 0.0 && ns_node <= N) { vs_v = matrix_get(V, ns_node, 0) }
                    var vgs = vg_v - vs_v
                    var vds = vd_v - vs_v
                    var kp_val = matrix_get(comps, ci, 5.0)
                    var vto_val = matrix_get(comps, ci, 6.0)
                    var lambda_val = matrix_get(comps, ci, 7.0)
                    if (t == 13.0) { vgs = -vgs; vds = -vds; vto_val = -vto_val }
                    var vov = vgs - vto_val
                    var id = 0.0; var gm = 0.0; var gds = 0.0
                    if (vov > 0.0) {
                        if (vds < vov) {
                            id = kp_val * (vov * vds - 0.5 * vds * vds)
                            gm = kp_val * vds
                            gds = kp_val * (vov - vds)
                            if (lambda_val > 0.0) {
                                var lf = 1.0 + lambda_val * vds
                                id = id * lf; gm = gm * lf
                                gds = gds * lf + lambda_val * id / lf
                            }
                        } else {
                            id = 0.5 * kp_val * vov * vov
                            gm = kp_val * vov
                            gds = 0.0
                            if (lambda_val > 0.0) {
                                var lf = 1.0 + lambda_val * vds
                                id = id * lf; gm = gm * lf
                                gds = lambda_val * id / lf
                            }
                        }
                    }
                    if (t == 13.0) { id = -id }
                    var id_co = id - gm * vgs - gds * vds
                    if (nd_node > 0.0 && nd_node <= N) {
                        A[mna_ndx(nd_node), mna_ndx(nd_node)] += gds
                    }
                    if (nd_node > 0.0 && nd_node <= N && ng_node > 0.0 && ng_node <= N) {
                        A[mna_ndx(nd_node), mna_ndx(ng_node)] += gm
                    }
                    if (nd_node > 0.0 && nd_node <= N && ns_node > 0.0 && ns_node <= N) {
                        A[mna_ndx(nd_node), mna_ndx(ns_node)] -= gds + gm
                    }
                    if (ns_node > 0.0 && ns_node <= N) {
                        A[mna_ndx(ns_node), mna_ndx(ns_node)] += gds + gm
                    }
                    if (ns_node > 0.0 && ns_node <= N && nd_node > 0.0 && nd_node <= N) {
                        A[mna_ndx(ns_node), mna_ndx(nd_node)] -= gds
                    }
                    if (ns_node > 0.0 && ns_node <= N && ng_node > 0.0 && ng_node <= N) {
                        A[mna_ndx(ns_node), mna_ndx(ng_node)] -= gm
                    }
                    if (nd_node > 0.0 && nd_node <= N) {
                        b[mna_ndx(nd_node), 0] -= id_co
                    }
                    if (ns_node > 0.0 && ns_node <= N) {
                        b[mna_ndx(ns_node), 0] += id_co
                    }
                } else if (tc == 14.0 || tc == 15.0) {
                    var nd_node = np
                    var ng_node = nm
                    var ns_node = matrix_get(comps, ci, 3.0)
                    var vd_v = 0.0; var vg_v = 0.0; var vs_v = 0.0
                    if (nd_node > 0.0 && nd_node <= N) { vd_v = matrix_get(V, nd_node, 0) }
                    if (ng_node > 0.0 && ng_node <= N) { vg_v = matrix_get(V, ng_node, 0) }
                    if (ns_node > 0.0 && ns_node <= N) { vs_v = matrix_get(V, ns_node, 0) }
                    var vgs = vg_v - vs_v
                    var vds = vd_v - vs_v
                    var beta_val = matrix_get(comps, ci, 5.0)
                    var vto_val = matrix_get(comps, ci, 6.0)
                    var lambda_val = matrix_get(comps, ci, 7.0)
                    if (t == 15.0) { vgs = -vgs; vds = -vds; vto_val = -vto_val }
                    var vov = vgs - vto_val
                    var id = 0.0; var gm = 0.0; var gds = 0.0
                    if (vov > 0.0) {
                        if (vds < vov) {
                            id = beta_val * (vov * vds - 0.5 * vds * vds)
                            gm = beta_val * vds
                            gds = beta_val * (vov - vds)
                            if (lambda_val > 0.0) {
                                var lf = 1.0 + lambda_val * vds
                                id = id * lf; gm = gm * lf
                                gds = gds * lf + lambda_val * id / lf
                            }
                        } else {
                            id = 0.5 * beta_val * vov * vov
                            gm = beta_val * vov
                            gds = 0.0
                            if (lambda_val > 0.0) {
                                var lf = 1.0 + lambda_val * vds
                                id = id * lf; gm = gm * lf
                                gds = lambda_val * id / lf
                            }
                        }
                    }
                    if (t == 15.0) { id = -id }
                    var id_co = id - gm * vgs - gds * vds
                    if (nd_node > 0.0 && nd_node <= N) {
                        A[mna_ndx(nd_node), mna_ndx(nd_node)] += gds
                        b[mna_ndx(nd_node), 0] -= id_co
                    }
                    if (nd_node > 0.0 && nd_node <= N && ng_node > 0.0 && ng_node <= N) {
                        A[mna_ndx(nd_node), mna_ndx(ng_node)] += gm
                    }
                    if (nd_node > 0.0 && nd_node <= N && ns_node > 0.0 && ns_node <= N) {
                        A[mna_ndx(nd_node), mna_ndx(ns_node)] -= gds + gm
                    }
                    if (ns_node > 0.0 && ns_node <= N) {
                        A[mna_ndx(ns_node), mna_ndx(ns_node)] += gds + gm
                        b[mna_ndx(ns_node), 0] += id_co
                    }
                    if (ns_node > 0.0 && ns_node <= N && nd_node > 0.0 && nd_node <= N) {
                        A[mna_ndx(ns_node), mna_ndx(nd_node)] -= gds
                    }
                    if (ns_node > 0.0 && ns_node <= N && ng_node > 0.0 && ng_node <= N) {
                        A[mna_ndx(ns_node), mna_ndx(ng_node)] -= gm
                    }
                } else if (tc == 16.0) {
                    var np_node = np
                    var nm_node = nm
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
                    var ieq = g * Vdiff
                    var gm = dg * Vdiff
                    mna_stamp_g(A, np_node, nm_node, g, N)
                    if (gm != 0.0) {
                        mna_stamp_vccs(A, np_node, nm_node, ncp_node, ncm_node, gm, N)
                    }
                    mna_stamp_isource(b, np_node, nm_node, ieq, N)
                } else if (tc == 17.0) {
                    var np_node = np
                    var nm_node = nm
                    var vsrc_idx = matrix_get(comps, ci, 3.0)
                    var Ictrl = matrix_get(comps, ci, 9.0)
                    var sw_info = ccsw_conductance(comps, ci, Ictrl)
                    var g = matrix_get(sw_info, 0, 0)
                    var dg = matrix_get(sw_info, 1, 0)
                    var vnp = 0.0; var vnm = 0.0
                    if (np_node > 0.0 && np_node <= N) { vnp = matrix_get(V, np_node, 0) }
                    if (nm_node > 0.0 && nm_node <= N) { vnm = matrix_get(V, nm_node, 0) }
                    var Vdiff = vnp - vnm
                    var ieq = g * Vdiff
                    var gm_sw = dg * Vdiff
                    mna_stamp_g(A, np_node, nm_node, g, N)
                    if (gm_sw != 0.0 && vsrc_idx >= 0.0) {
                        var vsrc_bi = mna_branch_index(comps, ctrl, vsrc_idx)
                        var br = N + vsrc_bi
                        if (br < N + M) {
                            if (np_node > 0.0 && np_node <= N) {
                                A[mna_ndx(np_node), br] += gm_sw
                            }
                            if (nm_node > 0.0 && nm_node <= N) {
                                A[mna_ndx(nm_node), br] -= gm_sw
                            }
                        }
                    }
                    mna_stamp_isource(b, np_node, nm_node, ieq, N)
                }
                ci = ci + 1.0
            }
            var x = matrix_solve(A, b)
            var ci2 = 0.0
            while (ci2 < nc) {
                var t2 = matrix_get(comps, ci2, 0.0)
                if (t2 == 17.0) {
                    var vsrc_idx = matrix_get(comps, ci2, 3.0)
                    if (vsrc_idx >= 0.0) {
                        var vsrc_bi = mna_branch_index(comps, ctrl, vsrc_idx)
                        if (vsrc_bi >= 0.0 && vsrc_bi < M) {
                            matrix_set(comps, ci2, 9.0, matrix_get(x, N + vsrc_bi, 0))
                        }
                    }
                }
                ci2 = ci2 + 1.0
            }
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
        var sol = matrix_zeros(dim + 2.0, 1)
        matrix_set(sol, 0, 0, N)
        matrix_set(sol, 1, 0, M)
        var ri = 0.0
        while (ri < N) {
            matrix_set(sol, ri + 2.0, 0, matrix_get(V, ri + 1.0, 0))
            ri = ri + 1.0
        }
        sol
    }
}

def tr_solve(comps : matrix, ctrl : matrix, t_start : Double, t_stop : Double, dt : Double) : matrix {
    var N = matrix_get(ctrl, 0.0, 0.0)
    var nc = matrix_get(ctrl, 1.0, 0.0)
    var ns = (t_stop - t_start) / dt
    if (ns < 1.0) { ns = 1.0 }
    var npts = floor(ns) + 1.0
    var results = matrix_zeros(npts, N + 2.0)
    var cap_v = matrix_zeros(nc, 1)
    var ind_i = matrix_zeros(nc, 1)
    var V = matrix_zeros(N + 1.0, 1)
    var t = t_start
    var si = 0.0
    while (si < npts) {
        var sol = tr_step(comps, ctrl, t, dt, cap_v, ind_i, V, 20.0, 1e-9)
        var Nsol = matrix_get(sol, 0, 0)
        var ci = 0.0
        while (ci < nc) {
            var tc = matrix_get(comps, ci, 0.0)
            var np = matrix_get(comps, ci, 1.0)
            var nm = matrix_get(comps, ci, 2.0)
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
            var vv = mna_get_node_voltage(sol, ri)
            matrix_set(results, si, ri + 1.0, vv)
            matrix_set(V, ri, 0, vv)
            ri = ri + 1.0
        }
        t = t + dt
        si = si + 1.0
    }
    results
}
