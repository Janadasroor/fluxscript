# SPICE Netlist Parser + MNA Solver
# Usage: flux --entry=main examples/spice/netlist_parser.flux

def main() {
    var f = flux_fopen("examples/spice/voltage_divider.cir", "r")
    var nc = 0.0; var nmax = 0.0; var nv = 0.0
    var C = matrix_zeros(20, 5)

    while flux_feof(f) == 0.0 do {
        var line = flux_fgets(f)
        if flux_strlen(line) > 2.0 && flux_string_at(line, 0.0) != 42.0 then {
            var c0 = flux_string_at(line, 0.0)
            if c0 == 82.0 || c0 == 86.0 then {
                var p = 0.0
                while p < flux_strlen(line) && flux_string_at(line, p) > 32.0 do p = p + 1.0
                while p < flux_strlen(line) && flux_string_at(line, p) <= 32.0 do p = p + 1.0
                var p2 = p; while p2 < flux_strlen(line) && flux_string_at(line, p2) > 32.0 do p2 = p2 + 1.0
                var n1 = flux_parse_number(flux_string_slice(line, p, p2))
                p = p2; while p < flux_strlen(line) && flux_string_at(line, p) <= 32.0 do p = p + 1.0
                p2 = p; while p2 < flux_strlen(line) && flux_string_at(line, p2) > 32.0 do p2 = p2 + 1.0
                var n2 = flux_parse_number(flux_string_slice(line, p, p2))
                p = p2; while p < flux_strlen(line) && flux_string_at(line, p) <= 32.0 do p = p + 1.0
                p2 = p; while p2 < flux_strlen(line) && flux_string_at(line, p2) > 32.0 do p2 = p2 + 1.0
                var val = flux_parse_number(flux_string_slice(line, p, p2))
                if n1 > nmax then nmax = n1 else 0.0
                if n2 > nmax then nmax = n2 else 0.0
                matrix_set(C, nc, 0, 0.0)
                matrix_set(C, nc, 1, n1)
                matrix_set(C, nc, 2, n2)
                matrix_set(C, nc, 3, val)
                if c0 == 86.0 then { matrix_set(C, nc, 0, 1.0); nv = nv + 1.0 } else 0.0
                nc = nc + 1.0
            } else 0.0
        } else 0.0
    }
    flux_fclose(f)

    var nall = nmax + nv
    var G = matrix_zeros(nall, nall)
    var b = matrix_zeros(nall, 1)
    var vi = 0.0; var ci = 0.0

    while ci < nc do {
        var typ = matrix_get(C, ci, 0)
        var n1  = matrix_get(C, ci, 1)
        var n2  = matrix_get(C, ci, 2)
        var val = matrix_get(C, ci, 3)
        var i1 = n1 - 1.0; var i2 = n2 - 1.0
        if typ == 0.0 then {
            var g = 1.0 / val
            if n1 > 0.0 then matrix_set(G, i1, i1, matrix_get(G, i1, i1) + g) else 0.0
            if n2 > 0.0 then matrix_set(G, i2, i2, matrix_get(G, i2, i2) + g) else 0.0
            if n1 > 0.0 && n2 > 0.0 then {
                matrix_set(G, i1, i2, matrix_get(G, i1, i2) - g)
                matrix_set(G, i2, i1, matrix_get(G, i2, i1) - g)
            } else 0.0
        } else {
            var vs = nmax + vi
            if n1 > 0.0 then {
                matrix_set(G, i1, vs, matrix_get(G, i1, vs) + 1.0)
                matrix_set(G, vs, i1, matrix_get(G, vs, i1) + 1.0)
            } else 0.0
            if n2 > 0.0 then {
                matrix_set(G, i2, vs, matrix_get(G, i2, vs) - 1.0)
                matrix_set(G, vs, i2, matrix_get(G, vs, i2) - 1.0)
            } else 0.0
            matrix_set(b, vs, 0, matrix_get(b, vs, 0) + val)
            vi = vi + 1.0
        }
        ci = ci + 1.0
    }

    var v = matrix_solve(G, b)
    matrix_get(v, 1, 0)
}
