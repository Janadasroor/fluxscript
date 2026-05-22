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

def nl_parse_D(comps : matrix, ctrl : matrix, line, state : matrix) {
    var p = nl_token_end(line, 0.0)
    if (p >= 0.0) {
        var t1 = nl_token(line, p); var n1 = nl_node(t1, state)
        p = nl_token_end(line, p)
        if (p >= 0.0) {
            var t2 = nl_token(line, p); var n2 = nl_node(t2, state)
            p = nl_token_end(line, p)
            if (p >= 0.0) {
                # t3 is model name (ignored, use defaults)
                var t3 = nl_token(line, p)
                circuit_add_diode(comps, ctrl, n1, n2, 1e-14, 1.0, 0.02585)
            }
        }
    }
}

# --- main entry ---

def netlist_parse(filepath, comps : matrix, ctrl : matrix) {
    var state = matrix_zeros(2, 1)

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
                if (first != 42.0 && first != 59.0 && first != 46.0) {
                    if (first == 82.0) { nl_parse_R(comps, ctrl, line, state) }
                    else if (first == 67.0) { nl_parse_C(comps, ctrl, line, state) }
                    else if (first == 76.0) { nl_parse_L(comps, ctrl, line, state) }
                    else if (first == 86.0) { nl_parse_V(comps, ctrl, line, state) }
                    else if (first == 73.0) { nl_parse_I(comps, ctrl, line, state) }
                    else if (first == 68.0) { nl_parse_D(comps, ctrl, line, state) }
                }
            }
        }
        flux_fclose(fh)
    }

    var actual_N = matrix_get(state, 0, 0)
    matrix_set(ctrl, 0, 0, actual_N)
    actual_N
}
