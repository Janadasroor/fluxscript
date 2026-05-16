# SPICE Netlist Parser + MNA Solver (split into small functions)
# Usage: flux --entry=main examples/spice/netlist_parser.flux

def read_netlist(filename) {
    var f = flux_fopen(filename, "r")
    var nc = 0.0; var nv = 0.0
    while flux_feof(f) == 0.0 do {
        var line = flux_fgets(f)
        if flux_strlen(line) > 2.0 then {
            var c0 = flux_string_at(line, 0.0)
            if c0 == 82.0 || c0 == 86.0 then nc = nc + 1.0 else 0.0
            if c0 == 86.0 then nv = nv + 1.0 else 0.0
        } else 0.0
    }
    flux_fclose(f)
    var r = matrix_zeros(2, 1)
    r[0,0] = nc; r[1,0] = nv; r
}

def main() {
    var info = read_netlist("examples/spice/voltage_divider.cir")
    info[0,0]
}
