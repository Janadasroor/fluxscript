# FullCircuit — DC Analysis (Operating Point + Sweep)
import circuit
import mna

def diode_junction(vd, isat, n_factor, vt) {
    var cv = n_factor * vt
    var vcrit = cv * log(cv / (0.5 * isat))
    if (vd > vcrit) {
        var id_lim = isat * exp(vcrit / cv)
        var g_lim = id_lim / cv
        var id = id_lim + g_lim * (vd - vcrit)
        var g = g_lim
        var result = matrix_zeros(2, 1)
        matrix_set(result, 0, 0, id)
        matrix_set(result, 1, 0, g)
        result
    } else if (vd < -vcrit) {
        var vn = vd / cv
        var id = -isat * (1.0 + vn + 0.5 * vn * vn)
        var g = -isat / cv * (1.0 + vn)
        var result = matrix_zeros(2, 1)
        matrix_set(result, 0, 0, id)
        matrix_set(result, 1, 0, g)
        result
    } else {
        var id = isat * (exp(vd / cv) - 1.0)
        var g = isat / cv * exp(vd / cv)
        var result = matrix_zeros(2, 1)
        matrix_set(result, 0, 0, id)
        matrix_set(result, 1, 0, g)
        result
    }
}

def bjt_junction(vd, isat, vt) {
    diode_junction(vd, isat, 1.0, vt)
}

def dc_solve_linear(comps : matrix, ctrl : matrix) {
    mna_dc_solve(comps, ctrl)
}

def dc_nonlinear_solve(comps : matrix, ctrl : matrix, max_iter, tol) {
    var N = circuit_num_nodes(ctrl)
    var nc = circuit_count(ctrl)
    var V = matrix_zeros(N + 1.0, 1)
    dc_nonlinear_solve_v0(comps, ctrl, max_iter, tol, V)
}

def dc_nonlinear_solve_v0(comps : matrix, ctrl : matrix, max_iter, tol, V : matrix) {
    dc_nlsolve_gmin(comps, ctrl, max_iter, tol, V, 0.0)
}

def dc_nlsolve_gmin(comps : matrix, ctrl : matrix, max_iter, tol, V : matrix, gmin) {
    var N = circuit_num_nodes(ctrl)
    var nc = circuit_count(ctrl)
    var iter = 0.0
    var converged = 0.0
    if (max_iter < 0.0) { max_iter = 20.0 }
    if (max_iter == 0.0) { max_iter = 20.0 }
    if (tol <= 0.0) { tol = 1e-9 }
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
                var djr = diode_junction(vd, isat, nf, vt)
                var id = matrix_get(djr, 0, 0)
                var geq = matrix_get(djr, 1, 0)
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
                var vbc = vb_v - vc_v
                var vce = vc_v - ve_v
                var sign = 1.0
                if (t == 11.0) { sign = -1.0 }
                var vbe_eff = vbe * sign
                var vbc_eff = vbc * sign
                var bjt_jr = diode_junction(vbe_eff, isat, 1.0, vt)
                var icc = matrix_get(bjt_jr, 0, 0)
                var gbe = matrix_get(bjt_jr, 1, 0)
                bjt_jr = diode_junction(vbc_eff, isat, 1.0, vt)
                var iec = matrix_get(bjt_jr, 0, 0)
                var gbc = matrix_get(bjt_jr, 1, 0)
                var br = circuit_param(comps, ci, 8.0)
                if (br <= 0.0) { br = 2.0 }
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
                if (t == 11.0) {
                    i_c = -i_c
                    ib = -ib
                }
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
            } else if (t == 12.0 || t == 13.0) {
                var nd_node = np
                var ng_node = nm
                var ns_node = circuit_node_3(comps, ci)
                var vd_v = 0.0; var vg_v = 0.0; var vs_v = 0.0
                if (nd_node > 0.0 && nd_node <= N) { vd_v = matrix_get(V, nd_node, 0) }
                if (ng_node > 0.0 && ng_node <= N) { vg_v = matrix_get(V, ng_node, 0) }
                if (ns_node > 0.0 && ns_node <= N) { vs_v = matrix_get(V, ns_node, 0) }
                var vgs = vg_v - vs_v
                var vds = vd_v - vs_v
                var kp_val = circuit_param(comps, ci, 5.0)
                var vto_val = circuit_param(comps, ci, 6.0)
                var lambda_val = circuit_param(comps, ci, 7.0)
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
                            id = id * lf
                            gm = gm * lf
                            gds = gds * lf + lambda_val * id / lf
                        }
                    } else {
                        id = 0.5 * kp_val * vov * vov
                        gm = kp_val * vov
                        gds = 0.0
                        if (lambda_val > 0.0) {
                            var lf = 1.0 + lambda_val * vds
                            id = id * lf
                            gm = gm * lf
                            gds = lambda_val * id / lf
                        }
                    }
                }
                if (t == 13.0) { id = -id }
                var ieq = id - gm * vgs - gds * vds
                if (t == 13.0) { ieq = id + gm * vgs + gds * vds }
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
                    b[mna_ndx(nd_node), 0] -= ieq
                }
                if (ns_node > 0.0 && ns_node <= N) {
                    b[mna_ndx(ns_node), 0] += ieq
                }
            } else if (t == 14.0 || t == 15.0) {
                var nd_node = np
                var ng_node = nm
                var ns_node = circuit_node_3(comps, ci)
                var vd_v = 0.0; var vg_v = 0.0; var vs_v = 0.0
                if (nd_node > 0.0 && nd_node <= N) { vd_v = matrix_get(V, nd_node, 0) }
                if (ng_node > 0.0 && ng_node <= N) { vg_v = matrix_get(V, ng_node, 0) }
                if (ns_node > 0.0 && ns_node <= N) { vs_v = matrix_get(V, ns_node, 0) }
                var vgs = vg_v - vs_v
                var vds = vd_v - vs_v
                var beta_val = circuit_param(comps, ci, 5.0)
                var vto_val = circuit_param(comps, ci, 6.0)
                var lambda_val = circuit_param(comps, ci, 7.0)
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
                            id = id * lf
                            gm = gm * lf
                            gds = gds * lf + lambda_val * id / lf
                        }
                    } else {
                        id = 0.5 * beta_val * vov * vov
                        gm = beta_val * vov
                        gds = 0.0
                        if (lambda_val > 0.0) {
                            var lf = 1.0 + lambda_val * vds
                            id = id * lf
                            gm = gm * lf
                            gds = lambda_val * id / lf
                        }
                    }
                }
                if (t == 15.0) { id = -id }
                var ieq = id - gm * vgs - gds * vds
                if (t == 15.0) { ieq = id + gm * vgs + gds * vds }
                if (nd_node > 0.0 && nd_node <= N) {
                    A[mna_ndx(nd_node), mna_ndx(nd_node)] += gds
                    b[mna_ndx(nd_node), 0] -= ieq
                }
                if (nd_node > 0.0 && nd_node <= N && ng_node > 0.0 && ng_node <= N) {
                    A[mna_ndx(nd_node), mna_ndx(ng_node)] += gm
                }
                if (nd_node > 0.0 && nd_node <= N && ns_node > 0.0 && ns_node <= N) {
                    A[mna_ndx(nd_node), mna_ndx(ns_node)] -= gds + gm
                }
                if (ns_node > 0.0 && ns_node <= N) {
                    A[mna_ndx(ns_node), mna_ndx(ns_node)] += gds + gm
                    b[mna_ndx(ns_node), 0] += ieq
                }
                if (ns_node > 0.0 && ns_node <= N && nd_node > 0.0 && nd_node <= N) {
                    A[mna_ndx(ns_node), mna_ndx(nd_node)] -= gds
                }
                if (ns_node > 0.0 && ns_node <= N && ng_node > 0.0 && ng_node <= N) {
                    A[mna_ndx(ns_node), mna_ndx(ng_node)] -= gm
                }
            } else if (t == 16.0) {
                var np_node = np
                var nm_node = nm
                var ncp_node = circuit_node_3(comps, ci)
                var ncm_node = circuit_param(comps, ci, 4.0)
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
                var gm_sw = dg * Vdiff
                mna_stamp_g(A, np_node, nm_node, g, N)
                if (gm_sw != 0.0) {
                    mna_stamp_vccs(A, np_node, nm_node, ncp_node, ncm_node, gm_sw, N)
                }
                mna_stamp_isource(b, np_node, nm_node, ieq, N)
            } else if (t == 17.0) {
                var np_node = np
                var nm_node = nm
                var vsrc_idx = circuit_param(comps, ci, 3.0)
                var Ictrl = circuit_param(comps, ci, 9.0)
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
        if (gmin > 0.0) {
            var gi = 0.0
            while (gi < N) {
                A[gi, gi] += gmin
                gi = gi + 1.0
            }
        }
        var damp = 10.0 - iter * 2.0
        if (damp < 2.0) { damp = 2.0 }
        var x = matrix_solve(A, b)
        # Update CCSW branch currents
        var m = 0.0
        while (m < nc) {
            var mt = circuit_component_type(comps, m)
            if (mt == 17.0) {
                var vsrc_idx = circuit_param(comps, m, 3.0)
                if (vsrc_idx >= 0.0) {
                    var vsrc_bi = mna_branch_index(comps, ctrl, vsrc_idx)
                    if (vsrc_bi >= 0.0 && vsrc_bi < M) {
                        circuit_set_param(comps, m, 9.0, matrix_get(x, N + vsrc_bi, 0))
                    }
                }
            }
            m = m + 1.0
        }
        var max_dv = 0.0
        var vi = 0.0
        while (vi < N) {
            var old_v = matrix_get(V, vi + 1.0, 0)
            var new_v = matrix_get(x, vi, 0)
            var dv = new_v - old_v
            if (dv > damp) { dv = damp }
            if (dv < -damp) { dv = -damp }
            new_v = old_v + dv
            if (abs(dv) > max_dv) { max_dv = abs(dv) }
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
        if (t == 9.0 || t == 10.0 || t == 11.0 || t == 12.0 || t == 13.0 || t == 14.0 || t == 15.0 || t == 16.0 || t == 17.0) {
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
        var N = circuit_num_nodes(ctrl)
        var V = matrix_zeros(N + 1.0, 1)
        var ci = 0.0
        var ns = 0.0
        var src_rows = matrix_zeros(nc, 2)
        while (ci < nc) {
            var t = circuit_component_type(comps, ci)
            if (t == 3.0 || t == 4.0 || t == 5.0 || t == 6.0) {
                var orig = circuit_param(comps, ci, 4.0)
                matrix_set(src_rows, ns, 0, ci)
                matrix_set(src_rows, ns, 1, orig)
                ns = ns + 1.0
            }
            ci = ci + 1.0
        }
        var nsrc = ns
        var gmin_vals = matrix_zeros(5, 1)
        matrix_set(gmin_vals, 0, 0, 0.1)
        matrix_set(gmin_vals, 1, 0, 0.02)
        matrix_set(gmin_vals, 2, 0, 0.01)
        matrix_set(gmin_vals, 3, 0, 0.001)
        matrix_set(gmin_vals, 4, 0, 0.0)
        var ngmin = 5.0
        var src_fracs = matrix_zeros(6, 1)
        matrix_set(src_fracs, 0, 0, 0.0)
        matrix_set(src_fracs, 1, 0, 0.1)
        matrix_set(src_fracs, 2, 0, 0.3)
        matrix_set(src_fracs, 3, 0, 0.6)
        matrix_set(src_fracs, 4, 0, 0.9)
        matrix_set(src_fracs, 5, 0, 1.0)
        var nfrac = 6.0
        var sf = 0.0
        while (sf < nfrac) {
            var frac = matrix_get(src_fracs, sf, 0)
            var si = 0.0
            while (si < nsrc) {
                var s_idx = matrix_get(src_rows, si, 0)
                var orig = matrix_get(src_rows, si, 1)
                circuit_set_param(comps, s_idx, 4.0, orig * frac)
                si = si + 1.0
            }
            var gi = 0.0
            while (gi < ngmin) {
                var gv = matrix_get(gmin_vals, gi, 0)
                var iters_per = max_iter / (nfrac * ngmin)
                if (iters_per < 10.0) { iters_per = 10.0 }
                V = dc_nlsolve_gmin(comps, ctrl, iters_per, tol, V, gv)
                gi = gi + 1.0
            }
            sf = sf + 1.0
        }
        V
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
    var djr = diode_junction(vd, isat, nf, vt)
    matrix_get(djr, 0, 0)
}

def dc_bjt_info(comps : matrix, ctrl : matrix, comp_idx, V : matrix) {
    var nc = circuit_node_p(comps, comp_idx)
    var nb = circuit_node_n(comps, comp_idx)
    var ne = circuit_node_3(comps, comp_idx)
    var N = circuit_num_nodes(ctrl)
    var vc = 0.0; var vb = 0.0; var ve = 0.0
    if (nc > 0.0 && nc <= N) { vc = matrix_get(V, nc, 0) }
    if (nb > 0.0 && nb <= N) { vb = matrix_get(V, nb, 0) }
    if (ne > 0.0 && ne <= N) { ve = matrix_get(V, ne, 0) }
    var vbe = vb - ve
    var vbc = vb - vc
    var vce = vc - ve
    var bf = circuit_param(comps, comp_idx, 4.0)
    var isat = circuit_param(comps, comp_idx, 5.0)
    var vaf = circuit_param(comps, comp_idx, 6.0)
    var vt = circuit_param(comps, comp_idx, 7.0)
    var br = circuit_param(comps, comp_idx, 8.0)
    if (br <= 0.0) { br = 2.0 }
    var be_jn = diode_junction(vbe, isat, 1.0, vt)
    var icc = matrix_get(be_jn, 0, 0)
    var bc_jn = diode_junction(vbc, isat, 1.0, vt)
    var iec = matrix_get(bc_jn, 0, 0)
    if (vaf > 0.0 && vce > -vaf * 0.5) {
        var vce_vaf = vce
        if (vce_vaf < 0.0) { vce_vaf = 0.0 }
        icc = icc * (1.0 + vce_vaf / vaf)
    }
    var af = bf / (bf + 1.0)
    var i_c = af * icc - iec
    var ib = icc / bf + iec / br
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
    var t = circuit_component_type(comps, comp_idx)
    var kp = circuit_param(comps, comp_idx, 5.0)
    var vto = circuit_param(comps, comp_idx, 6.0)
    var lambda = circuit_param(comps, comp_idx, 7.0)
    if (t == 13.0) { vgs = -vgs; vds = -vds; vto = -vto }
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
    if (t == 13.0) { id = -id }
    var result = matrix_zeros(4, 1)
    matrix_set(result, 0, 0, vg - vs)
    matrix_set(result, 1, 0, vd - vs)
    matrix_set(result, 2, 0, id)
    matrix_set(result, 3, 0, region)
    result
}

def dc_jfet_info(comps : matrix, ctrl : matrix, comp_idx, V : matrix) {
    var nd = circuit_node_p(comps, comp_idx)
    var ng = circuit_node_n(comps, comp_idx)
    var ns = circuit_node_3(comps, comp_idx)
    var N = circuit_num_nodes(ctrl)
    var vd = 0.0; var vg = 0.0; var vs = 0.0
    if (nd > 0.0 && nd <= N) { vd = matrix_get(V, nd, 0) }
    if (ng > 0.0 && ng <= N) { vg = matrix_get(V, ng, 0) }
    if (ns > 0.0 && ns <= N) { vs = matrix_get(V, ns, 0) }
    var vgs = vg - vs
    var vds = vd - vs
    var t = circuit_component_type(comps, comp_idx)
    var beta_val = circuit_param(comps, comp_idx, 5.0)
    var vto_val = circuit_param(comps, comp_idx, 6.0)
    var lambda_val = circuit_param(comps, comp_idx, 7.0)
    if (t == 15.0) { vgs = -vgs; vds = -vds; vto_val = -vto_val }
    var vov = vgs - vto_val
    var id = 0.0; var region = 0.0
    if (vov <= 0.0) {
        region = 0.0
    } else if (vds < vov) {
        region = 1.0
        id = beta_val * (vov * vds - 0.5 * vds * vds)
        if (lambda_val > 0.0) { id = id * (1.0 + lambda_val * vds) }
    } else {
        region = 2.0
        id = 0.5 * beta_val * vov * vov
        if (lambda_val > 0.0) { id = id * (1.0 + lambda_val * vds) }
    }
    if (t == 15.0) { id = -id }
    var result = matrix_zeros(4, 1)
    matrix_set(result, 0, 0, vg - vs)
    matrix_set(result, 1, 0, vd - vs)
    matrix_set(result, 2, 0, id)
    matrix_set(result, 3, 0, region)
    result
}

# --- switch helpers ---
# Returns matrix [g, dg_dctrl, gmin] for a voltage-controlled switch
# g = conductance, dg_dctrl = dG/dVcontrol
def vcsw_conductance(comps : matrix, ci, Vctrl) {
    var ron = circuit_param(comps, ci, 5.0)
    var roff = circuit_param(comps, ci, 6.0)
    var von = circuit_param(comps, ci, 7.0)
    var voff = circuit_param(comps, ci, 8.0)
    if (ron <= 0.0) { ron = 1.0 }
    if (roff <= 0.0) { roff = 1e9 }
    var gon = 1.0 / ron
    var goff = 1.0 / roff
    var g = 0.0; var dg = 0.0
    if (Vctrl <= voff) {
        g = goff
    } else if (Vctrl >= von) {
        g = gon
    } else if (von > voff) {
        var frac = (Vctrl - voff) / (von - voff)
        g = goff + (gon - goff) * frac
        dg = (gon - goff) / (von - voff)
    }
    var result = matrix_zeros(3, 1)
    matrix_set(result, 0, 0, g)
    matrix_set(result, 1, 0, dg)
    result
}

# Returns matrix [g, dg_dctrl] for a current-controlled switch
# dg_dctrl = dG/dIcontrol
def ccsw_conductance(comps : matrix, ci, Ictrl) {
    var ron = circuit_param(comps, ci, 5.0)
    var roff = circuit_param(comps, ci, 6.0)
    var ion = circuit_param(comps, ci, 7.0)
    var ioff = circuit_param(comps, ci, 8.0)
    if (ron <= 0.0) { ron = 1.0 }
    if (roff <= 0.0) { roff = 1e9 }
    var gon = 1.0 / ron
    var goff = 1.0 / roff
    var g = 0.0; var dg = 0.0
    if (Ictrl <= ioff) {
        g = goff
    } else if (Ictrl >= ion) {
        g = gon
    } else if (ion > ioff) {
        var frac = (Ictrl - ioff) / (ion - ioff)
        g = goff + (gon - goff) * frac
        dg = (gon - goff) / (ion - ioff)
    }
    var result = matrix_zeros(2, 1)
    matrix_set(result, 0, 0, g)
    matrix_set(result, 1, 0, dg)
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

