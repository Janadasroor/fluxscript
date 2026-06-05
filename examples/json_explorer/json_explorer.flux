import json_types

def skip_ws(text: String, pos: matrix) {
    var p = matrix_get(pos, 0, 0)
    while (true) {
        var c = flux_string_at(text, p)
        if (c == CH_SPACE() || c == CH_TAB() || c == CH_NL() || c == CH_CR()) {
            p = p + 1.0
        } else {
            break
        }
    }
    matrix_set(pos, 0, 0, p)
}

def print_indent(depth: Double) {
    var i = 0.0
    while (i < depth) {
        flux_print_string("  ")
        i = i + 1.0
    }
}

def parse_string(text: String, pos: matrix, depth: Double) -> Double {
    var p = matrix_get(pos, 0, 0)
    var hash = 0.0
    while (true) {
        var c = flux_string_at(text, p)
        if (c == CH_QUOTE()) {
            p = p + 1.0
            break
        }
        if (c == CH_BACKSL()) {
            var next_c = flux_string_at(text, p + 1.0)
            if (next_c == CH_u()) {
                flux_print_string(flux_string_slice(text, p, p + 6.0))
                p = p + 6.0
            } else {
                flux_print_string(flux_string_slice(text, p, p + 2.0))
                p = p + 2.0
            }
            hash = hash + 1.0
        } else {
            flux_print_string(flux_string_slice(text, p, p + 1.0))
            p = p + 1.0
            hash = hash + 1.0
        }
    }
    matrix_set(pos, 0, 0, p)
    hash
}

def parse_number(text: String, pos: matrix, depth: Double) -> Double {
    var p = matrix_get(pos, 0, 0)
    var start = p
    if (flux_string_at(text, p) == CH_MINUS()) {
        p = p + 1.0
    }
    while (true) {
        var c = flux_string_at(text, p)
        if (c >= CH_0() && c <= CH_9()) {
            p = p + 1.0
        } else {
            break
        }
    }
    if (flux_string_at(text, p) == CH_DOT()) {
        p = p + 1.0
        while (true) {
            var c = flux_string_at(text, p)
            if (c >= CH_0() && c <= CH_9()) {
                p = p + 1.0
            } else {
                break
            }
        }
    }
    var c = flux_string_at(text, p)
    if (c == CH_E_U() || c == CH_E_L()) {
        p = p + 1.0
        c = flux_string_at(text, p)
        if (c == CH_PLUS() || c == CH_MINUS()) {
            p = p + 1.0
        }
        while (true) {
            c = flux_string_at(text, p)
            if (c >= CH_0() && c <= CH_9()) {
                p = p + 1.0
            } else {
                break
            }
        }
    }
    var num_str = flux_string_slice(text, start, p)
    var val = flux_parse_number(num_str)
    flux_print_string(flux_double_to_string(val))
    matrix_set(pos, 0, 0, p)
    val
}

def check_keyword(text: String, pos: matrix, kw: String) {
    var p = matrix_get(pos, 0, 0)
    var klen = flux_strlen(kw)
    var i = 0.0
    while (i < klen) {
        var c1 = flux_string_at(text, p + i)
        var c2 = flux_string_at(kw, i)
        if (c1 != c2) {
            flux_print_string("ERROR: expected keyword ")
            flux_print_string(kw)
            flux_print_string(" at position ")
            flux_print_double(p)
            flux_print_string("\n")
            p = -1.0
            i = klen
        }
        i = i + 1.0
    }
    p = p + klen
    matrix_set(pos, 0, 0, p)
}

def parse_array(text: String, pos: matrix, depth: Double) -> Double {
    var p = matrix_get(pos, 0, 0)
    flux_print_string("[\n")
    p = p + 1.0
    matrix_set(pos, 0, 0, p)
    var hash = 0.0
    var idx = 0.0
    while (true) {
        skip_ws(text, pos)
        p = matrix_get(pos, 0, 0)
        if (flux_string_at(text, p) == CH_RBRACK()) {
            break
        }
        if (idx > 0.0) {
            flux_print_string(",\n")
        }
        print_indent(depth + 1.0)
        var child_hash = parse_value(text, pos, depth + 1.0)
        hash = hash + (idx + 1.0) * child_hash
        idx = idx + 1.0
        skip_ws(text, pos)
        p = matrix_get(pos, 0, 0)
        if (flux_string_at(text, p) == CH_COMMA()) {
            p = p + 1.0
            matrix_set(pos, 0, 0, p)
        }
    }
    flux_print_string("\n")
    print_indent(depth)
    flux_print_string("]")
    p = matrix_get(pos, 0, 0) + 1.0
    matrix_set(pos, 0, 0, p)
    hash
}

def parse_object(text: String, pos: matrix, depth: Double) -> Double {
    var p = matrix_get(pos, 0, 0)
    flux_print_string("{\n")
    p = p + 1.0
    matrix_set(pos, 0, 0, p)
    var hash = 0.0
    var idx = 0.0
    while (true) {
        skip_ws(text, pos)
        p = matrix_get(pos, 0, 0)
        if (flux_string_at(text, p) == CH_RBRACE()) {
            break
        }
        if (idx > 0.0) {
            flux_print_string(",\n")
        }
        print_indent(depth + 1.0)
        if (flux_string_at(text, p) != CH_QUOTE()) {
            flux_print_string("ERROR: expected string key at position ")
            flux_print_double(p)
            flux_print_string("\n")
            matrix_set(pos, 0, 0, p + 1.0)
            return hash
        }
        flux_print_string("\"")
        p = p + 1.0
        matrix_set(pos, 0, 0, p)
        var key_hash = parse_string(text, pos, depth + 1.0)
        flux_print_string("\"")
        skip_ws(text, pos)
        p = matrix_get(pos, 0, 0)
        if (flux_string_at(text, p) != CH_COLON()) {
            flux_print_string("ERROR: expected colon at position ")
            flux_print_double(p)
            flux_print_string("\n")
            return hash
        }
        flux_print_string(": ")
        p = p + 1.0
        matrix_set(pos, 0, 0, p)
        skip_ws(text, pos)
        var child_hash = parse_value(text, pos, depth + 1.0)
        hash = hash + (key_hash + 1.0) * (child_hash + 1.0)
        idx = idx + 1.0
        skip_ws(text, pos)
        p = matrix_get(pos, 0, 0)
        if (flux_string_at(text, p) == CH_COMMA()) {
            p = p + 1.0
            matrix_set(pos, 0, 0, p)
        }
    }
    flux_print_string("\n")
    print_indent(depth)
    flux_print_string("}")
    p = matrix_get(pos, 0, 0) + 1.0
    matrix_set(pos, 0, 0, p)
    hash
}

def parse_value(text: String, pos: matrix, depth: Double) -> Double {
    skip_ws(text, pos)
    var p = matrix_get(pos, 0, 0)
    var c = flux_string_at(text, p)
    if (c == CH_LBRACE()) {
        parse_object(text, pos, depth)
    } else {
        if (c == CH_LBRACK()) {
            parse_array(text, pos, depth)
        } else {
            if (c == CH_QUOTE()) {
                flux_print_string("\"")
                p = p + 1.0
                matrix_set(pos, 0, 0, p)
                var hash = parse_string(text, pos, depth)
                flux_print_string("\"")
                hash
            } else {
                if (c == CH_t()) {
                    check_keyword(text, pos, "true")
                    flux_print_string("true")
                    2.0
                } else {
                    if (c == CH_f()) {
                        check_keyword(text, pos, "false")
                        flux_print_string("false")
                        1.0
                    } else {
                        if (c == CH_n()) {
                            check_keyword(text, pos, "null")
                            flux_print_string("null")
                            0.0
                        } else {
                            if ((c >= CH_0() && c <= CH_9()) || c == CH_MINUS()) {
                                parse_number(text, pos, depth)
                            } else {
                                flux_print_string("ERROR: unexpected character '")
                                flux_print_string(flux_string_slice(text, p, p + 1.0))
                                flux_print_string("' at position ")
                                flux_print_double(p)
                                flux_print_string("\n")
                                p = p + 1.0
                                matrix_set(pos, 0, 0, p)
                                0.0
                            }
                        }
                    }
                }
            }
        }
    }
}

def explore_json(text: String) -> Double {
    var pos = matrix_zeros(1, 1)
    matrix_set(pos, 0, 0, 0.0)
    var hash = parse_value(text, pos, 0.0)
    skip_ws(text, pos)
    var p = matrix_get(pos, 0, 0)
    if (p < flux_strlen(text)) {
        flux_print_string("\nWARNING: trailing content after position ")
        flux_print_double(p)
        flux_print_string("\n")
    }
    flux_print_string("\n")
    hash
}
