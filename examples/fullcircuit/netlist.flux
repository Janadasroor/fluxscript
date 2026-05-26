# FullCircuit — SPICE Netlist Parser
#
# Reads .sp / .cir netlist files into component matrix + control vector.
#
# Usage:
#   var comps = matrix_zeros(500, 12)
#   var ctrl  = circuit_create(50, 500)
#   var N     = netlist_parse("circuit.sp", comps, ctrl)
#   var sol   = mna_dc_solve(comps, ctrl)
#
# Supported: R, C, L, V(DC/AC), I(DC), D
# Nodes: numeric only (0 = ground)
# SI suffixes: k M m u n p f
#
# IMPORTANT: Each token string must be consumed (converted to double via
# flux_parse_number) BEFORE the next flux_string_slice or flux_fgets call,
# because the internal string pool may reallocate and invalidate pointers.
#
import circuit
import mna

# --- character helpers ---

def nl_is_ws(c)    { c == 32.0 || c == 9.0 }
def nl_to_upper(c) { if (c >= 97.0 && c <= 122.0) { c - 32.0 } else { c } }

def nl_skip_ws(line, pos) {
    while (pos < flux_strlen(line) && nl_is_ws(flux_string_at(line, pos))) { pos = pos + 1.0 }
    pos
}

def nl_find_ws(line, pos) {
    while (pos < flux_strlen(line) && !nl_is_ws(flux_string_at(line, pos))) { pos = pos + 1.0 }
    pos
}

def nl_strip_comment(line) {
    var n = flux_strlen(line)
    var i = 0.0; var found = -1.0
    while (i < n && found < 0.0) {
        var c = flux_string_at(line, i)
        if (c == 59.0 || c == 35.0) { found = i }
        i = i + 1.0
    }
    if (found >= 0.0) { flux_string_slice(line, 0.0, found) }
    else { line }
}

def nl_token(line, start) {
    var s = nl_skip_ws(line, start)
    if (s >= flux_strlen(line)) { 0.0 }
    else { flux_string_slice(line, s, nl_find_ws(line, s)) }
}

def nl_token_end(line, start) {
    var s = nl_skip_ws(line, start)
    if (s >= flux_strlen(line)) { -1.0 }
    else { nl_find_ws(line, s) }
}

def nl_val(s) { flux_parse_number(s) }

def nl_node(s, state : matrix) {
    var nd = flux_parse_number(s)
    if (nd > matrix_get(state, 0, 0)) { matrix_set(state, 0, 0, nd) }
    nd
}

def nl_is_number(tok) {
    var c = flux_string_at(tok, 0.0)
    (c >= 48.0 && c <= 57.0) || c == 43.0 || c == 45.0 || c == 46.0
}

def nl_hash(s) {
    var h = 0.0
    var n = flux_strlen(s)
    var i = 0.0
    while (i < n) {
        h = h * 31.0 + nl_to_upper(flux_string_at(s, i))
        i = i + 1.0
    }
    h
}

def nl_model_find(state : matrix, name_hash, max_models, model_cols) {
    var nm = matrix_get(state, 1, 0)
    var idx = -1.0
    var i = 0.0
    while (i < nm && idx < 0.0) {
        var off = 2.0 + i * model_cols
        if (matrix_get(state, off, 0) == name_hash) { idx = i }
        i = i + 1.0
    }
    idx
}

def nl_model_add(state : matrix, name_hash, model_type, max_models, model_cols) {
    var nm = matrix_get(state, 1, 0)
    if (nm < max_models) {
        var off = 2.0 + nm * model_cols
        matrix_set(state, off, 0, name_hash)
        matrix_set(state, off + 1.0, 0, model_type)
        matrix_set(state, 1, 0, nm + 1.0)
    }
}

def nl_model_set(state : matrix, model_idx, col_idx, val, model_cols) {
    var off = 2.0 + model_idx * model_cols + col_idx
    matrix_set(state, off, 0, val)
}

def nl_model_get(state : matrix, model_idx, col_idx, model_cols) {
    var off = 2.0 + model_idx * model_cols + col_idx
    matrix_get(state, off, 0)
}

def nl_parse_dot_model(comps : matrix, ctrl : matrix, line, state : matrix, max_models, model_cols) {
    var p = nl_token_end(line, 0.0)
    if (p >= 0.0) {
        var t1 = nl_token(line, p); var name_hash = nl_hash(t1)
        p = nl_token_end(line, p)
        if (p >= 0.0) {
            var t2 = nl_token(line, p)
            var tn2 = flux_strlen(t2)
            var has_paren = 0.0
            if (flux_string_at(t2, 0.0) == 40.0) {
                has_paren = 1.0
                t2 = flux_string_slice(t2, 1.0, tn2)
            }
            var model_type = 0.0
            if (flux_strlen(t2) > 0.0) {
                var c0 = nl_to_upper(flux_string_at(t2, 0.0))
                var c1 = 0.0
                if (flux_strlen(t2) > 1.0) { c1 = nl_to_upper(flux_string_at(t2, 1.0)) }
                if (c0 == 78.0 && c1 == 80.0) { model_type = 10.0 }
                else if (c0 == 80.0 && c1 == 78.0) { model_type = 11.0 }
                else if (c0 == 78.0 && c1 == 77.0) { model_type = 12.0 }
                else if (c0 == 80.0 && c1 == 77.0) { model_type = 13.0 }
                else if (c0 == 78.0 && c1 == 74.0) { model_type = 14.0 }
                else if (c0 == 80.0 && c1 == 74.0) { model_type = 15.0 }
                else if (c0 == 86.0 && c1 == 83.0) { model_type = 16.0 }
                else if (c0 == 73.0 && c1 == 83.0) { model_type = 17.0 }
                else if (c0 == 68.0) { model_type = 9.0 }
            }
            if (model_type > 0.0) {
                nl_model_add(state, name_hash, model_type, max_models, model_cols)
                var mi = nl_model_find(state, name_hash, max_models, model_cols)
                if (mi >= 0.0) {
                    if (model_type == 9.0) {
                        nl_model_set(state, mi, 2.0, 1e-14, model_cols)
                        nl_model_set(state, mi, 3.0, 1.0, model_cols)
                        nl_model_set(state, mi, 4.0, 0.0, model_cols)
                        nl_model_set(state, mi, 5.0, 0.02585, model_cols)
                        nl_model_set(state, mi, 6.0, 0.0, model_cols)
                    } else if (model_type == 12.0 || model_type == 13.0) {
                        nl_model_set(state, mi, 2.0, 1e-14, model_cols)
                        nl_model_set(state, mi, 3.0, 1e-4, model_cols)
                        nl_model_set(state, mi, 4.0, 1.0, model_cols)
                        nl_model_set(state, mi, 5.0, 0.0, model_cols)
                        nl_model_set(state, mi, 6.0, 0.0, model_cols)
                    } else if (model_type == 14.0) {
                        nl_model_set(state, mi, 2.0, 1e-14, model_cols)
                        nl_model_set(state, mi, 3.0, 1e-3, model_cols)
                        nl_model_set(state, mi, 4.0, -2.0, model_cols)
                        nl_model_set(state, mi, 5.0, 0.0, model_cols)
                        nl_model_set(state, mi, 6.0, 0.0, model_cols)
                    } else if (model_type == 15.0) {
                        nl_model_set(state, mi, 2.0, 1e-14, model_cols)
                        nl_model_set(state, mi, 3.0, 1e-3, model_cols)
                        nl_model_set(state, mi, 4.0, 2.0, model_cols)
                        nl_model_set(state, mi, 5.0, 0.0, model_cols)
                        nl_model_set(state, mi, 6.0, 0.0, model_cols)
                    } else if (model_type == 16.0) {
                        nl_model_set(state, mi, 2.0, 0.0, model_cols)
                        nl_model_set(state, mi, 3.0, 1e9, model_cols)
                        nl_model_set(state, mi, 4.0, 1.0, model_cols)
                        nl_model_set(state, mi, 5.0, 0.0, model_cols)
                        nl_model_set(state, mi, 6.0, 1.0, model_cols)
                    } else if (model_type == 17.0) {
                        nl_model_set(state, mi, 2.0, 0.0, model_cols)
                        nl_model_set(state, mi, 3.0, 1e9, model_cols)
                        nl_model_set(state, mi, 4.0, 0.01, model_cols)
                        nl_model_set(state, mi, 5.0, 0.0, model_cols)
                        nl_model_set(state, mi, 6.0, 1.0, model_cols)
                    } else {
                        nl_model_set(state, mi, 2.0, 1e-14, model_cols)
                        nl_model_set(state, mi, 3.0, 100.0, model_cols)
                        nl_model_set(state, mi, 4.0, 50.0, model_cols)
                        nl_model_set(state, mi, 5.0, 0.02585, model_cols)
                        nl_model_set(state, mi, 6.0, 2.0, model_cols)
                    }
                    p = nl_token_end(line, p)
                    while (p >= 0.0) {
                        var t3 = nl_token(line, p)
                        var tn = flux_strlen(t3)
                        if (tn > 0.0 && flux_string_at(t3, 0.0) == 40.0) {
                            t3 = flux_string_slice(t3, 1.0, tn)
                            tn = flux_strlen(t3)
                        }
                        if (tn > 0.0) {
                            var last_c = flux_string_at(t3, tn - 1.0)
                            if (last_c == 41.0) { t3 = flux_string_slice(t3, 0.0, tn - 1.0) }
                        }
                        tn = flux_strlen(t3)
                        if (tn > 0.0 && flux_string_at(t3, 0.0) != 41.0) {
                            var eq_pos = 0.0
                            while (eq_pos < tn && flux_string_at(t3, eq_pos) != 61.0) { eq_pos = eq_pos + 1.0 }
                            if (eq_pos < tn) {
                                var pname = nl_to_upper(flux_string_at(t3, 0.0))
                                var pval = nl_val(flux_string_slice(t3, eq_pos + 1.0, tn))
                                if (pname == 73.0) { nl_model_set(state, mi, 2.0, pval, model_cols) }
                                else if (pname == 66.0) { nl_model_set(state, mi, 3.0, pval, model_cols) }
                                else if (pname == 86.0) { nl_model_set(state, mi, 4.0, pval, model_cols) }
                                else if (pname == 75.0) { nl_model_set(state, mi, 3.0, pval, model_cols) }
                                else if (pname == 76.0) { nl_model_set(state, mi, 5.0, pval, model_cols) }
                                else if (pname == 78.0) { nl_model_set(state, mi, 3.0, pval, model_cols) }
                                else if (pname == 82.0) { nl_model_set(state, mi, 6.0, pval, model_cols) }
                                else if (pname == 70.0) { nl_model_set(state, mi, 3.0, pval, model_cols) }
                                else if (pname == 79.0) { nl_model_set(state, mi, 5.0, pval, model_cols) }
                            }
                        }
                        p = nl_token_end(line, p)
                    }
                }
            }
        }
    }
}

def nl_is_DC(tok) {
    flux_strlen(tok) == 2.0 &&
    nl_to_upper(flux_string_at(tok, 0.0)) == 68.0 &&
    nl_to_upper(flux_string_at(tok, 1.0)) == 67.0
}

def nl_is_AC(tok) {
    flux_strlen(tok) == 2.0 &&
    nl_to_upper(flux_string_at(tok, 0.0)) == 65.0 &&
    nl_to_upper(flux_string_at(tok, 1.0)) == 67.0
}

def nl_is_IC(tok) {
    flux_strlen(tok) >= 3.0 &&
    nl_to_upper(flux_string_at(tok, 0.0)) == 73.0 &&
    nl_to_upper(flux_string_at(tok, 1.0)) == 67.0 &&
    flux_string_at(tok, 2.0) == 61.0
}

# --- element parsers ---
# CRITICAL: each token is consumed (converted to double) immediately after
# extraction, before the next token is extracted (avoids dangling pointers
# from the string-pool vector reallocation).

def nl_parse_R(comps : matrix, ctrl : matrix, line, state : matrix) {
    var p = nl_token_end(line, 0.0)
    if (p >= 0.0) {
        var t1 = nl_token(line, p); var n1 = nl_node(t1, state)
        p = nl_token_end(line, p)
        if (p >= 0.0) {
            var t2 = nl_token(line, p); var n2 = nl_node(t2, state)
            p = nl_token_end(line, p)
            if (p >= 0.0) {
                var t3 = nl_token(line, p); var v = nl_val(t3)
                if (v > 1e-18) { circuit_add_resistor(comps, ctrl, n1, n2, v) }
            }
        }
    }
}

def nl_parse_C(comps : matrix, ctrl : matrix, line, state : matrix) {
    var p = nl_token_end(line, 0.0)
    if (p >= 0.0) {
        var t1 = nl_token(line, p); var n1 = nl_node(t1, state)
        p = nl_token_end(line, p)
        if (p >= 0.0) {
            var t2 = nl_token(line, p); var n2 = nl_node(t2, state)
            p = nl_token_end(line, p)
            if (p >= 0.0) {
                var t3 = nl_token(line, p); var v = nl_val(t3)
                p = nl_token_end(line, p)
                var ic = 0.0
                if (p >= 0.0) {
                    var t4 = nl_token(line, p)
                    if (nl_is_IC(t4)) { ic = nl_val(flux_string_slice(t4, 3.0, flux_strlen(t4))) }
                }
                circuit_add_capacitor(comps, ctrl, n1, n2, v, ic)
            }
        }
    }
}

def nl_parse_L(comps : matrix, ctrl : matrix, line, state : matrix) {
    var p = nl_token_end(line, 0.0)
    if (p >= 0.0) {
        var t1 = nl_token(line, p); var n1 = nl_node(t1, state)
        p = nl_token_end(line, p)
        if (p >= 0.0) {
            var t2 = nl_token(line, p); var n2 = nl_node(t2, state)
            p = nl_token_end(line, p)
            if (p >= 0.0) {
                var t3 = nl_token(line, p); var v = nl_val(t3)
                p = nl_token_end(line, p)
                var ic = 0.0
                if (p >= 0.0) {
                    var t4 = nl_token(line, p)
                    if (nl_is_IC(t4)) { ic = nl_val(flux_string_slice(t4, 3.0, flux_strlen(t4))) }
                }
                circuit_add_inductor(comps, ctrl, n1, n2, v, ic)
            }
        }
    }
}

def nl_parse_V(comps : matrix, ctrl : matrix, line, state : matrix) {
    var p = nl_token_end(line, 0.0)
    if (p >= 0.0) {
        var t1 = nl_token(line, p); var n1 = nl_node(t1, state)
        p = nl_token_end(line, p)
        if (p >= 0.0) {
            var t2 = nl_token(line, p); var n2 = nl_node(t2, state)
            p = nl_token_end(line, p)
            if (p >= 0.0) {
                var t3 = nl_token(line, p)
                p = nl_token_end(line, p)

                if (p < 0.0) {
                    # Vxxx n+ n- value
                    circuit_add_vdc(comps, ctrl, n1, n2, nl_val(t3))
                } else if (nl_is_DC(t3)) {
                    # Vxxx n+ n- DC value [AC mag phase]
                    var t4 = nl_token(line, p); var dc_val = nl_val(t4)
                    p = nl_token_end(line, p)
                    var ac_mag = 0.0; var ac_phase = 0.0
                    if (p >= 0.0) {
                        var t5 = nl_token(line, p)
                        if (nl_is_AC(t5)) {
                            p = nl_token_end(line, p)
                            if (p >= 0.0) {
                                var t6 = nl_token(line, p); ac_mag = nl_val(t6)
                                p = nl_token_end(line, p)
                                if (p >= 0.0) {
                                    var t7 = nl_token(line, p); ac_phase = nl_val(t7)
                                }
                            }
                        }
                    }
                    if (ac_mag > 0.0) {
                        circuit_add_vac(comps, ctrl, n1, n2, dc_val, ac_mag, 1000.0, ac_phase)
                    } else {
                        circuit_add_vdc(comps, ctrl, n1, n2, dc_val)
                    }
                } else if (nl_is_AC(t3)) {
                    # Vxxx n+ n- AC mag [phase]
                    var ac_mag = 1.0; var ac_phase = 0.0
                    if (p >= 0.0) {
                        var t4 = nl_token(line, p); ac_mag = nl_val(t4)
                        p = nl_token_end(line, p)
                        if (p >= 0.0) {
                            var t5 = nl_token(line, p); ac_phase = nl_val(t5)
                        }
                    }
                    circuit_add_vac(comps, ctrl, n1, n2, 0.0, ac_mag, 1000.0, ac_phase)
                } else {
                    # Vxxx n+ n- value (first token is value, not keyword)
                    circuit_add_vdc(comps, ctrl, n1, n2, nl_val(t3))
                }
            }
        }
    }
}

def nl_parse_I(comps : matrix, ctrl : matrix, line, state : matrix) {
    var p = nl_token_end(line, 0.0)
    if (p >= 0.0) {
        var t1 = nl_token(line, p); var n1 = nl_node(t1, state)
        p = nl_token_end(line, p)
        if (p >= 0.0) {
            var t2 = nl_token(line, p); var n2 = nl_node(t2, state)
            p = nl_token_end(line, p)
            if (p >= 0.0) {
                var t3 = nl_token(line, p)
                circuit_add_idc(comps, ctrl, n1, n2, nl_val(t3))
            }
        }
    }
}

def nl_parse_D(comps : matrix, ctrl : matrix, line, state : matrix, max_models, model_cols) {
    var p = nl_token_end(line, 0.0)
    if (p >= 0.0) {
        var t1 = nl_token(line, p); var n1 = nl_node(t1, state)
        p = nl_token_end(line, p)
        if (p >= 0.0) {
            var t2 = nl_token(line, p); var n2 = nl_node(t2, state)
            p = nl_token_end(line, p)
            if (p >= 0.0) {
                var t3 = nl_token(line, p)
                var isat = 1e-14; var nf = 1.0; var vt = 0.02585
                if (!nl_is_number(t3)) {
                    var mi = nl_model_find(state, nl_hash(t3), max_models, model_cols)
                    if (mi >= 0.0) {
                        isat = nl_model_get(state, mi, 2.0, model_cols)
                        nf = nl_model_get(state, mi, 3.0, model_cols)
                    }
                }
                circuit_add_diode(comps, ctrl, n1, n2, isat, nf, vt)
            }
        }
    }
}

def nl_parse_Q(comps : matrix, ctrl : matrix, line, state : matrix, max_models, model_cols) {
    var p = nl_token_end(line, 0.0)
    if (p >= 0.0) {
        var t1 = nl_token(line, p); var n_c = nl_node(t1, state)
        p = nl_token_end(line, p)
        if (p >= 0.0) {
            var t2 = nl_token(line, p); var n_b = nl_node(t2, state)
            p = nl_token_end(line, p)
            if (p >= 0.0) {
                var t3 = nl_token(line, p); var n_e = nl_node(t3, state)
                p = nl_token_end(line, p)
                if (p >= 0.0) {
                    var t4 = nl_token(line, p)
                    if (!nl_is_number(t4)) {
                        var mi = nl_model_find(state, nl_hash(t4), max_models, model_cols)
                        var bf = 100.0; var isat = 1e-14; var vaf = 50.0; var mtype = 10.0
                        if (mi >= 0.0) {
                            isat = nl_model_get(state, mi, 2.0, model_cols)
                            bf = nl_model_get(state, mi, 3.0, model_cols)
                            vaf = nl_model_get(state, mi, 4.0, model_cols)
                            mtype = nl_model_get(state, mi, 1.0, model_cols)
                        }
                        if (mtype == 11.0) {
                            circuit_add_pnp(comps, ctrl, n_c, n_b, n_e, bf, isat, vaf, 0.02585)
                        } else {
                            circuit_add_npn(comps, ctrl, n_c, n_b, n_e, bf, isat, vaf, 0.02585)
                        }
                    } else if (nl_val(t4) < 0.0) {
                        p = nl_token_end(line, p)
                        if (p >= 0.0) { var t5 = nl_token(line, p); var isat = nl_val(t5)
                        p = nl_token_end(line, p)
                        if (p >= 0.0) { var t6 = nl_token(line, p); var bf = nl_val(t6)
                        p = nl_token_end(line, p)
                        if (p >= 0.0) { var t7 = nl_token(line, p); var vaf = nl_val(t7)
                            circuit_add_pnp(comps, ctrl, n_c, n_b, n_e, bf, isat, vaf, 0.02585) } } }
                    } else {
                        var isat = nl_val(t4)
                        p = nl_token_end(line, p)
                        if (p >= 0.0) { var t5 = nl_token(line, p); var bf = nl_val(t5)
                        p = nl_token_end(line, p)
                        if (p >= 0.0) { var t6 = nl_token(line, p); var vaf = nl_val(t6)
                            circuit_add_npn(comps, ctrl, n_c, n_b, n_e, bf, isat, vaf, 0.02585) } }
                    }
                }
            }
        }
    }
}

def nl_parse_M(comps : matrix, ctrl : matrix, line, state : matrix, max_models, model_cols) {
    var p = nl_token_end(line, 0.0)
    if (p >= 0.0) {
        var t1 = nl_token(line, p); var n_d = nl_node(t1, state)
        p = nl_token_end(line, p)
        if (p >= 0.0) {
            var t2 = nl_token(line, p); var n_g = nl_node(t2, state)
            p = nl_token_end(line, p)
            if (p >= 0.0) {
                var t3 = nl_token(line, p); var n_s = nl_node(t3, state)
                p = nl_token_end(line, p)
                if (p >= 0.0) {
                    var t4 = nl_token(line, p); var n_b = nl_node(t4, state)
                    p = nl_token_end(line, p)
                    if (p >= 0.0) {
                        var t5 = nl_token(line, p)
                        if (!nl_is_number(t5)) {
                            var mi = nl_model_find(state, nl_hash(t5), max_models, model_cols)
                            var kp = 1e-4; var vto = 1.0; var lambda_val = 0.0; var mtype = 12.0
                            if (mi >= 0.0) {
                                kp = nl_model_get(state, mi, 3.0, model_cols)
                                vto = nl_model_get(state, mi, 4.0, model_cols)
                                lambda_val = nl_model_get(state, mi, 5.0, model_cols)
                                mtype = nl_model_get(state, mi, 1.0, model_cols)
                            }
                            if (mtype == 13.0) {
                                circuit_add_pmos(comps, ctrl, n_d, n_g, n_s, n_b, kp, vto, lambda_val)
                            } else {
                                circuit_add_nmos(comps, ctrl, n_d, n_g, n_s, n_b, kp, vto, lambda_val)
                            }
                        } else if (nl_val(t5) < 0.0) {
                            p = nl_token_end(line, p)
                            if (p >= 0.0) { var t6 = nl_token(line, p); var kp = nl_val(t6)
                            p = nl_token_end(line, p)
                            if (p >= 0.0) { var t7 = nl_token(line, p); var vto = nl_val(t7)
                            p = nl_token_end(line, p)
                            if (p >= 0.0) { var t8 = nl_token(line, p); var lambda_val = nl_val(t8)
                                circuit_add_pmos(comps, ctrl, n_d, n_g, n_s, n_b, kp, vto, lambda_val) } } }
                        } else {
                            var kp = nl_val(t5)
                            p = nl_token_end(line, p)
                            if (p >= 0.0) { var t6 = nl_token(line, p); var vto = nl_val(t6)
                            p = nl_token_end(line, p)
                            if (p >= 0.0) { var t7 = nl_token(line, p); var lambda_val = nl_val(t7)
                                circuit_add_nmos(comps, ctrl, n_d, n_g, n_s, n_b, kp, vto, lambda_val) } }
                        }
                    }
                }
            }
        }
    }
}

def nl_parse_J(comps : matrix, ctrl : matrix, line, state : matrix, max_models, model_cols) {
    var p = nl_token_end(line, 0.0)
    if (p >= 0.0) {
        var t1 = nl_token(line, p); var n_d = nl_node(t1, state)
        p = nl_token_end(line, p)
        if (p >= 0.0) {
            var t2 = nl_token(line, p); var n_g = nl_node(t2, state)
            p = nl_token_end(line, p)
            if (p >= 0.0) {
                var t3 = nl_token(line, p); var n_s = nl_node(t3, state)
                p = nl_token_end(line, p)
                if (p >= 0.0) {
                    var t4 = nl_token(line, p)
                    if (!nl_is_number(t4)) {
                        var mi = nl_model_find(state, nl_hash(t4), max_models, model_cols)
                        var beta_val = 1e-3; var vto = -2.0; var lambda_val = 0.0; var mtype = 14.0
                        if (mi >= 0.0) {
                            beta_val = nl_model_get(state, mi, 3.0, model_cols)
                            vto = nl_model_get(state, mi, 4.0, model_cols)
                            lambda_val = nl_model_get(state, mi, 5.0, model_cols)
                            mtype = nl_model_get(state, mi, 1.0, model_cols)
                        }
                        if (mtype == 15.0) {
                            circuit_add_pjf(comps, ctrl, n_d, n_g, n_s, beta_val, vto, lambda_val)
                        } else {
                            circuit_add_njf(comps, ctrl, n_d, n_g, n_s, beta_val, vto, lambda_val)
                        }
                    } else if (nl_val(t4) < 0.0) {
                        p = nl_token_end(line, p)
                        if (p >= 0.0) { var t5 = nl_token(line, p); var beta_val = nl_val(t5)
                        p = nl_token_end(line, p)
                        if (p >= 0.0) { var t6 = nl_token(line, p); var vto = nl_val(t6)
                        p = nl_token_end(line, p)
                        if (p >= 0.0) { var t7 = nl_token(line, p); var lambda_val = nl_val(t7)
                            circuit_add_pjf(comps, ctrl, n_d, n_g, n_s, beta_val, vto, lambda_val) } } }
                    } else {
                        var beta_val = nl_val(t4)
                        p = nl_token_end(line, p)
                        if (p >= 0.0) { var t5 = nl_token(line, p); var vto = nl_val(t5)
                        p = nl_token_end(line, p)
                        if (p >= 0.0) { var t6 = nl_token(line, p); var lambda_val = nl_val(t6)
                            circuit_add_njf(comps, ctrl, n_d, n_g, n_s, beta_val, vto, lambda_val) } }
                    }
                }
            }
        }
    }
}

# --- controlled switches ---

def nl_parse_S(comps : matrix, ctrl : matrix, line, state : matrix, max_models, model_cols) {
    var p = nl_token_end(line, 0.0)
    if (p >= 0.0) {
        var t1 = nl_token(line, p); var n_plus = nl_node(t1, state)
        p = nl_token_end(line, p)
        if (p >= 0.0) {
            var t2 = nl_token(line, p); var n_minus = nl_node(t2, state)
            p = nl_token_end(line, p)
            if (p >= 0.0) {
                var t3 = nl_token(line, p); var n_cp = nl_node(t3, state)
                p = nl_token_end(line, p)
                if (p >= 0.0) {
                    var t4 = nl_token(line, p); var n_cm = nl_node(t4, state)
                    p = nl_token_end(line, p)
                    if (p >= 0.0) {
                        var t5 = nl_token(line, p)
                        var ron_val = 1.0; var roff_val = 1e9; var von_val = 1.0; var voff_val = 0.0
                        if (!nl_is_number(t5)) {
                            var mi = nl_model_find(state, nl_hash(t5), max_models, model_cols)
                            if (mi >= 0.0) {
                                ron_val = nl_model_get(state, mi, 6.0, model_cols)
                                roff_val = nl_model_get(state, mi, 3.0, model_cols)
                                von_val = nl_model_get(state, mi, 4.0, model_cols)
                                voff_val = nl_model_get(state, mi, 5.0, model_cols)
                            }
                        }
                        circuit_add_vcsw(comps, ctrl, n_plus, n_minus, n_cp, n_cm, ron_val, roff_val, von_val, voff_val)
                    }
                }
            }
        }
    }
}

# --- main entry ---

def netlist_parse(filepath, comps : matrix, ctrl : matrix) {
    var max_nodes = 200.0
    var max_models = 100.0
    var model_cols = 7.0
    var state = matrix_zeros(max_nodes + 2.0 + max_models * model_cols, 1.0)

    var fh = flux_fopen(filepath, "r")
    if (fh != 0.0) {
        var line_num = 0.0
        while (flux_feof(fh) == 0.0) {
            var raw = flux_fgets(fh)
            if (raw == 0.0) { break }
            line_num = line_num + 1.0

            var line = nl_strip_comment(raw)
            var p0 = nl_skip_ws(line, 0.0)
            if (p0 < flux_strlen(line)) {
                var first = nl_to_upper(flux_string_at(line, p0))
                if (first == 46.0) {
                    var p1 = nl_skip_ws(line, p0 + 1.0)
                    if (p1 < flux_strlen(line)) {
                        var kw = nl_to_upper(flux_string_at(line, p1))
                        if (kw == 77.0) { nl_parse_dot_model(comps, ctrl, line, state, max_models, model_cols) }
                    }
                } else if (first != 42.0 && first != 59.0) {
                    if (first == 82.0) { nl_parse_R(comps, ctrl, line, state) }
                    else if (first == 67.0) { nl_parse_C(comps, ctrl, line, state) }
                    else if (first == 76.0) { nl_parse_L(comps, ctrl, line, state) }
                    else if (first == 86.0) { nl_parse_V(comps, ctrl, line, state) }
                    else if (first == 73.0) { nl_parse_I(comps, ctrl, line, state) }
                    else if (first == 68.0) { nl_parse_D(comps, ctrl, line, state, max_models, model_cols) }
                    else if (first == 81.0) { nl_parse_Q(comps, ctrl, line, state, max_models, model_cols) }
                    else if (first == 77.0) { nl_parse_M(comps, ctrl, line, state, max_models, model_cols) }
                    else if (first == 74.0) { nl_parse_J(comps, ctrl, line, state, max_models, model_cols) }
                    else if (first == 83.0) { nl_parse_S(comps, ctrl, line, state, max_models, model_cols) }
                }
            }
        }
        flux_fclose(fh)
    }

    var actual_N = matrix_get(state, 0, 0)
    matrix_set(ctrl, 0, 0, actual_N)
    actual_N
}
