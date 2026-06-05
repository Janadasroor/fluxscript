# FullCircuit — Circuit Data Structures & Netlist Builder
#
# Component storage format (matrix of size max_comps x 12):
#   col[0]:  type code
#   col[1]:  node+ (or collector/drain for 3-terminal devices)
#   col[2]:  node- (or base/gate)
#   col[3]:  node  (or source/emitter + )
#   col[4]:  param1 (value, saturation current, etc.)
#   col[5]:  param2 (ideal factor, beta_F, kp, etc.)
#   col[6]:  param3 (Early voltage, VTO, etc.)
#   col[7]:  param4 (VT, lambda, etc.)
#   col[8]:  param5 (AC magnitude / v1 / voff)
#   col[9]:  param6 (AC frequency / v2 / vamp)
#   col[10]: param7 (AC phase / initial condition / td / freq)
#   col[11]: (reserved)
#
# Control vector (3 x 1):
#   [0] = num_non_gnd_nodes
#   [1] = component_count  (next free row in comps)
#   [2] = max_components

enum Component {
    R { n_plus: Double, n_minus: Double, r_val: Double }
    C { n_plus: Double, n_minus: Double, c_val: Double, init_v: Double }
    L { n_plus: Double, n_minus: Double, l_val: Double, init_i: Double }
    Vdc { n_plus: Double, n_minus: Double, v_val: Double }
    Idc { n_plus: Double, n_minus: Double, i_val: Double }
    Vac { n_plus: Double, n_minus: Double, dc_val: Double, ac_mag: Double, ac_freq: Double, ac_phase: Double }
    Iac { n_plus: Double, n_minus: Double, dc_val: Double, ac_mag: Double, ac_freq: Double, ac_phase: Double }
    Vpulse { n_plus: Double, n_minus: Double, per: Double, v1: Double, v2: Double, td: Double, tr: Double, tfall: Double, pw: Double }
    Vsin { n_plus: Double, n_minus: Double, voff: Double, vamp: Double, freq: Double, phase: Double }
    E { n_op: Double, n_on: Double, n_ip: Double, n_in: Double, gain: Double }
    G { n_op: Double, n_on: Double, n_ip: Double, n_in: Double, gm: Double }
    D { n_plus: Double, n_minus: Double, isat: Double, n_factor: Double, vt: Double }
    Qnpn { n_c: Double, n_b: Double, n_e: Double, bf: Double, isat: Double, vaf: Double, vt: Double }
    Qpnp { n_c: Double, n_b: Double, n_e: Double, bf: Double, isat: Double, vaf: Double, vt: Double }
    Mnmos { n_d: Double, n_g: Double, n_s: Double, n_bulk: Double, kp: Double, vto: Double, lambda_val: Double }
    Mpmos { n_d: Double, n_g: Double, n_s: Double, n_bulk: Double, kp: Double, vto: Double, lambda_val: Double }
    Njf { n_d: Double, n_g: Double, n_s: Double, beta_val: Double, vto: Double, lambda_val: Double }
    Pjf { n_d: Double, n_g: Double, n_s: Double, beta_val: Double, vto: Double, lambda_val: Double }
    S { n_plus: Double, n_minus: Double, n_cp: Double, n_cm: Double, ron: Double, roff: Double, von: Double, voff: Double }
    W { n_plus: Double, n_minus: Double, vsrc_idx: Double, ron: Double, roff: Double, ion: Double, ioff: Double }
}

class Circuit {
    comps: matrix
    ctrl: matrix

    def get_comps(self) -> matrix { self.comps }
    def get_ctrl(self) -> matrix { self.ctrl }

    def add(self, comp: Component) -> Double {
        var idx = matrix_get(self.ctrl, 1.0, 0.0)
        match comp {
            Component.R(p) -> {
                matrix_set(self.comps, idx, 0.0, 0.0)
                matrix_set(self.comps, idx, 1.0, p.n_plus)
                matrix_set(self.comps, idx, 2.0, p.n_minus)
                matrix_set(self.comps, idx, 4.0, p.r_val)
            }
            Component.C(p) -> {
                matrix_set(self.comps, idx, 0.0, 1.0)
                matrix_set(self.comps, idx, 1.0, p.n_plus)
                matrix_set(self.comps, idx, 2.0, p.n_minus)
                matrix_set(self.comps, idx, 4.0, p.c_val)
                matrix_set(self.comps, idx, 10.0, p.init_v)
            }
            Component.L(p) -> {
                matrix_set(self.comps, idx, 0.0, 2.0)
                matrix_set(self.comps, idx, 1.0, p.n_plus)
                matrix_set(self.comps, idx, 2.0, p.n_minus)
                matrix_set(self.comps, idx, 4.0, p.l_val)
                matrix_set(self.comps, idx, 10.0, p.init_i)
            }
            Component.Vdc(p) -> {
                matrix_set(self.comps, idx, 0.0, 3.0)
                matrix_set(self.comps, idx, 1.0, p.n_plus)
                matrix_set(self.comps, idx, 2.0, p.n_minus)
                matrix_set(self.comps, idx, 4.0, p.v_val)
            }
            Component.Idc(p) -> {
                matrix_set(self.comps, idx, 0.0, 4.0)
                matrix_set(self.comps, idx, 1.0, p.n_plus)
                matrix_set(self.comps, idx, 2.0, p.n_minus)
                matrix_set(self.comps, idx, 4.0, p.i_val)
            }
            Component.Vac(p) -> {
                matrix_set(self.comps, idx, 0.0, 5.0)
                matrix_set(self.comps, idx, 1.0, p.n_plus)
                matrix_set(self.comps, idx, 2.0, p.n_minus)
                matrix_set(self.comps, idx, 4.0, p.dc_val)
                matrix_set(self.comps, idx, 8.0, p.ac_mag)
                matrix_set(self.comps, idx, 9.0, p.ac_freq)
                matrix_set(self.comps, idx, 10.0, p.ac_phase)
            }
            Component.Iac(p) -> {
                matrix_set(self.comps, idx, 0.0, 6.0)
                matrix_set(self.comps, idx, 1.0, p.n_plus)
                matrix_set(self.comps, idx, 2.0, p.n_minus)
                matrix_set(self.comps, idx, 4.0, p.dc_val)
                matrix_set(self.comps, idx, 8.0, p.ac_mag)
                matrix_set(self.comps, idx, 9.0, p.ac_freq)
                matrix_set(self.comps, idx, 10.0, p.ac_phase)
            }
            Component.Vpulse(p) -> {
                matrix_set(self.comps, idx, 0.0, 3.0)
                matrix_set(self.comps, idx, 1.0, p.n_plus)
                matrix_set(self.comps, idx, 2.0, p.n_minus)
                matrix_set(self.comps, idx, 4.0, p.per)
                matrix_set(self.comps, idx, 5.0, p.tr)
                matrix_set(self.comps, idx, 6.0, p.tfall)
                matrix_set(self.comps, idx, 7.0, p.pw)
                matrix_set(self.comps, idx, 8.0, p.v1)
                matrix_set(self.comps, idx, 9.0, p.v2)
                matrix_set(self.comps, idx, 10.0, p.td)
                matrix_set(self.comps, idx, 11.0, 1.0)
            }
            Component.Vsin(p) -> {
                matrix_set(self.comps, idx, 0.0, 3.0)
                matrix_set(self.comps, idx, 1.0, p.n_plus)
                matrix_set(self.comps, idx, 2.0, p.n_minus)
                matrix_set(self.comps, idx, 4.0, 0.0)
                matrix_set(self.comps, idx, 5.0, p.phase)
                matrix_set(self.comps, idx, 8.0, p.voff)
                matrix_set(self.comps, idx, 9.0, p.vamp)
                matrix_set(self.comps, idx, 10.0, p.freq)
                matrix_set(self.comps, idx, 11.0, 2.0)
            }
            Component.E(p) -> {
                matrix_set(self.comps, idx, 0.0, 7.0)
                matrix_set(self.comps, idx, 1.0, p.n_op)
                matrix_set(self.comps, idx, 2.0, p.n_on)
                matrix_set(self.comps, idx, 3.0, p.n_ip)
                matrix_set(self.comps, idx, 4.0, p.n_in)
                matrix_set(self.comps, idx, 5.0, p.gain)
            }
            Component.G(p) -> {
                matrix_set(self.comps, idx, 0.0, 8.0)
                matrix_set(self.comps, idx, 1.0, p.n_op)
                matrix_set(self.comps, idx, 2.0, p.n_on)
                matrix_set(self.comps, idx, 3.0, p.n_ip)
                matrix_set(self.comps, idx, 4.0, p.n_in)
                matrix_set(self.comps, idx, 5.0, p.gm)
            }
            Component.D(p) -> {
                matrix_set(self.comps, idx, 0.0, 9.0)
                matrix_set(self.comps, idx, 1.0, p.n_plus)
                matrix_set(self.comps, idx, 2.0, p.n_minus)
                matrix_set(self.comps, idx, 4.0, p.isat)
                matrix_set(self.comps, idx, 5.0, p.n_factor)
                matrix_set(self.comps, idx, 7.0, p.vt)
            }
            Component.Qnpn(p) -> {
                matrix_set(self.comps, idx, 0.0, 10.0)
                matrix_set(self.comps, idx, 1.0, p.n_c)
                matrix_set(self.comps, idx, 2.0, p.n_b)
                matrix_set(self.comps, idx, 3.0, p.n_e)
                matrix_set(self.comps, idx, 4.0, p.bf)
                matrix_set(self.comps, idx, 5.0, p.isat)
                matrix_set(self.comps, idx, 6.0, p.vaf)
                matrix_set(self.comps, idx, 7.0, p.vt)
            }
            Component.Qpnp(p) -> {
                matrix_set(self.comps, idx, 0.0, 11.0)
                matrix_set(self.comps, idx, 1.0, p.n_c)
                matrix_set(self.comps, idx, 2.0, p.n_b)
                matrix_set(self.comps, idx, 3.0, p.n_e)
                matrix_set(self.comps, idx, 4.0, p.bf)
                matrix_set(self.comps, idx, 5.0, p.isat)
                matrix_set(self.comps, idx, 6.0, p.vaf)
                matrix_set(self.comps, idx, 7.0, p.vt)
            }
            Component.Mnmos(p) -> {
                matrix_set(self.comps, idx, 0.0, 12.0)
                matrix_set(self.comps, idx, 1.0, p.n_d)
                matrix_set(self.comps, idx, 2.0, p.n_g)
                matrix_set(self.comps, idx, 3.0, p.n_s)
                matrix_set(self.comps, idx, 4.0, p.n_bulk)
                matrix_set(self.comps, idx, 5.0, p.kp)
                matrix_set(self.comps, idx, 6.0, p.vto)
                matrix_set(self.comps, idx, 7.0, p.lambda_val)
            }
            Component.Mpmos(p) -> {
                matrix_set(self.comps, idx, 0.0, 13.0)
                matrix_set(self.comps, idx, 1.0, p.n_d)
                matrix_set(self.comps, idx, 2.0, p.n_g)
                matrix_set(self.comps, idx, 3.0, p.n_s)
                matrix_set(self.comps, idx, 4.0, p.n_bulk)
                matrix_set(self.comps, idx, 5.0, p.kp)
                matrix_set(self.comps, idx, 6.0, p.vto)
                matrix_set(self.comps, idx, 7.0, p.lambda_val)
            }
            Component.Njf(p) -> {
                matrix_set(self.comps, idx, 0.0, 14.0)
                matrix_set(self.comps, idx, 1.0, p.n_d)
                matrix_set(self.comps, idx, 2.0, p.n_g)
                matrix_set(self.comps, idx, 3.0, p.n_s)
                matrix_set(self.comps, idx, 4.0, 0.0)
                matrix_set(self.comps, idx, 5.0, p.beta_val)
                matrix_set(self.comps, idx, 6.0, p.vto)
                matrix_set(self.comps, idx, 7.0, p.lambda_val)
            }
            Component.Pjf(p) -> {
                matrix_set(self.comps, idx, 0.0, 15.0)
                matrix_set(self.comps, idx, 1.0, p.n_d)
                matrix_set(self.comps, idx, 2.0, p.n_g)
                matrix_set(self.comps, idx, 3.0, p.n_s)
                matrix_set(self.comps, idx, 4.0, 0.0)
                matrix_set(self.comps, idx, 5.0, p.beta_val)
                matrix_set(self.comps, idx, 6.0, p.vto)
                matrix_set(self.comps, idx, 7.0, p.lambda_val)
            }
            Component.S(p) -> {
                matrix_set(self.comps, idx, 0.0, 16.0)
                matrix_set(self.comps, idx, 1.0, p.n_plus)
                matrix_set(self.comps, idx, 2.0, p.n_minus)
                matrix_set(self.comps, idx, 3.0, p.n_cp)
                matrix_set(self.comps, idx, 4.0, p.n_cm)
                matrix_set(self.comps, idx, 5.0, p.ron)
                matrix_set(self.comps, idx, 6.0, p.roff)
                matrix_set(self.comps, idx, 7.0, p.von)
                matrix_set(self.comps, idx, 8.0, p.voff)
            }
            Component.W(p) -> {
                matrix_set(self.comps, idx, 0.0, 17.0)
                matrix_set(self.comps, idx, 1.0, p.n_plus)
                matrix_set(self.comps, idx, 2.0, p.n_minus)
                matrix_set(self.comps, idx, 3.0, p.vsrc_idx)
                matrix_set(self.comps, idx, 5.0, p.ron)
                matrix_set(self.comps, idx, 6.0, p.roff)
                matrix_set(self.comps, idx, 7.0, p.ion)
                matrix_set(self.comps, idx, 8.0, p.ioff)
            }
        }
        matrix_set(self.ctrl, 1.0, 0.0, idx + 1.0)
        idx
    }

    def count(self) -> Double {
        matrix_get(self.ctrl, 1.0, 0.0)
    }

    def num_nodes(self) -> Double {
        matrix_get(self.ctrl, 0.0, 0.0)
    }

    def type_code(self, idx: Double) -> Double {
        matrix_get(self.comps, idx, 0.0)
    }

    def node_p(self, idx: Double) -> Double {
        matrix_get(self.comps, idx, 1.0)
    }

    def node_n(self, idx: Double) -> Double {
        matrix_get(self.comps, idx, 2.0)
    }

    def node_3(self, idx: Double) -> Double {
        matrix_get(self.comps, idx, 3.0)
    }

    def get_param(self, idx: Double, col: Double) -> Double {
        matrix_get(self.comps, idx, col)
    }

    def set_param(self, idx: Double, col: Double, val: Double) {
        matrix_set(self.comps, idx, col, val)
    }
}

def circuit_create(num_nodes: Double, max_comps: Double) -> Circuit {
    var c = Circuit {
        comps: matrix_zeros(max_comps, 12.0),
        ctrl: matrix_zeros(3.0, 1.0)
    }
    matrix_set(c.ctrl, 0.0, 0.0, num_nodes)
    matrix_set(c.ctrl, 2.0, 0.0, max_comps)
    c
}

def circuit_new(num_nodes: Double, max_comps: Double) -> Circuit {
    var c = Circuit {
        comps: matrix_zeros(max_comps, 12.0),
        ctrl: matrix_zeros(3.0, 1.0)
    }
    matrix_set(c.ctrl, 0.0, 0.0, num_nodes)
    matrix_set(c.ctrl, 2.0, 0.0, max_comps)
    c
}

# --- backward-compatible free functions ---

def circuit_count(ctrl: matrix) -> Double {
    matrix_get(ctrl, 1.0, 0.0)
}

def circuit_num_nodes(ctrl: matrix) -> Double {
    matrix_get(ctrl, 0.0, 0.0)
}

def circuit_component_type(comps: matrix, idx: Double) -> Double {
    matrix_get(comps, idx, 0.0)
}

def circuit_node_p(comps: matrix, idx: Double) -> Double {
    matrix_get(comps, idx, 1.0)
}

def circuit_node_n(comps: matrix, idx: Double) -> Double {
    matrix_get(comps, idx, 2.0)
}

def circuit_node_3(comps: matrix, idx: Double) -> Double {
    matrix_get(comps, idx, 3.0)
}

def circuit_param(comps: matrix, idx: Double, col: Double) -> Double {
    matrix_get(comps, idx, col)
}

def circuit_set_param(comps: matrix, idx: Double, col: Double, val: Double) {
    matrix_set(comps, idx, col, val)
}

def circuit_add_resistor(comps: matrix, ctrl: matrix, n_plus: Double, n_minus: Double, r_val: Double) -> Double {
    var i = matrix_get(ctrl, 1.0, 0.0)
    matrix_set(comps, i, 0.0, 0.0)
    matrix_set(comps, i, 1.0, n_plus)
    matrix_set(comps, i, 2.0, n_minus)
    matrix_set(comps, i, 4.0, r_val)
    matrix_set(ctrl, 1.0, 0.0, i + 1.0)
    i
}

def circuit_add_capacitor(comps: matrix, ctrl: matrix, n_plus: Double, n_minus: Double, c_val: Double, init_v: Double) -> Double {
    var i = matrix_get(ctrl, 1.0, 0.0)
    matrix_set(comps, i, 0.0, 1.0)
    matrix_set(comps, i, 1.0, n_plus)
    matrix_set(comps, i, 2.0, n_minus)
    matrix_set(comps, i, 4.0, c_val)
    matrix_set(comps, i, 10.0, init_v)
    matrix_set(ctrl, 1.0, 0.0, i + 1.0)
    i
}

def circuit_add_inductor(comps: matrix, ctrl: matrix, n_plus: Double, n_minus: Double, l_val: Double, init_i: Double) -> Double {
    var i = matrix_get(ctrl, 1.0, 0.0)
    matrix_set(comps, i, 0.0, 2.0)
    matrix_set(comps, i, 1.0, n_plus)
    matrix_set(comps, i, 2.0, n_minus)
    matrix_set(comps, i, 4.0, l_val)
    matrix_set(comps, i, 10.0, init_i)
    matrix_set(ctrl, 1.0, 0.0, i + 1.0)
    i
}

def circuit_add_vdc(comps: matrix, ctrl: matrix, n_plus: Double, n_minus: Double, v_val: Double) -> Double {
    var i = matrix_get(ctrl, 1.0, 0.0)
    matrix_set(comps, i, 0.0, 3.0)
    matrix_set(comps, i, 1.0, n_plus)
    matrix_set(comps, i, 2.0, n_minus)
    matrix_set(comps, i, 4.0, v_val)
    matrix_set(ctrl, 1.0, 0.0, i + 1.0)
    i
}

def circuit_add_idc(comps: matrix, ctrl: matrix, n_plus: Double, n_minus: Double, i_val: Double) -> Double {
    var i = matrix_get(ctrl, 1.0, 0.0)
    matrix_set(comps, i, 0.0, 4.0)
    matrix_set(comps, i, 1.0, n_plus)
    matrix_set(comps, i, 2.0, n_minus)
    matrix_set(comps, i, 4.0, i_val)
    matrix_set(ctrl, 1.0, 0.0, i + 1.0)
    i
}

def circuit_add_vac(comps: matrix, ctrl: matrix, n_plus: Double, n_minus: Double, dc_val: Double, ac_mag: Double, ac_freq: Double, ac_phase: Double) -> Double {
    var i = matrix_get(ctrl, 1.0, 0.0)
    matrix_set(comps, i, 0.0, 5.0)
    matrix_set(comps, i, 1.0, n_plus)
    matrix_set(comps, i, 2.0, n_minus)
    matrix_set(comps, i, 4.0, dc_val)
    matrix_set(comps, i, 8.0, ac_mag)
    matrix_set(comps, i, 9.0, ac_freq)
    matrix_set(comps, i, 10.0, ac_phase)
    matrix_set(ctrl, 1.0, 0.0, i + 1.0)
    i
}

def circuit_add_iac(comps: matrix, ctrl: matrix, n_plus: Double, n_minus: Double, dc_val: Double, ac_mag: Double, ac_freq: Double, ac_phase: Double) -> Double {
    var i = matrix_get(ctrl, 1.0, 0.0)
    matrix_set(comps, i, 0.0, 6.0)
    matrix_set(comps, i, 1.0, n_plus)
    matrix_set(comps, i, 2.0, n_minus)
    matrix_set(comps, i, 4.0, dc_val)
    matrix_set(comps, i, 8.0, ac_mag)
    matrix_set(comps, i, 9.0, ac_freq)
    matrix_set(comps, i, 10.0, ac_phase)
    matrix_set(ctrl, 1.0, 0.0, i + 1.0)
    i
}

def circuit_add_vpulse(comps: matrix, ctrl: matrix, n_plus: Double, n_minus: Double, per: Double, v1: Double, v2: Double, td: Double, tr_val: Double, tfall: Double, pw: Double) -> Double {
    var i = circuit_add_vdc(comps, ctrl, n_plus, n_minus, per)
    circuit_set_param(comps, i, 5.0, tr_val)
    circuit_set_param(comps, i, 6.0, tfall)
    circuit_set_param(comps, i, 7.0, pw)
    circuit_set_param(comps, i, 8.0, v1)
    circuit_set_param(comps, i, 9.0, v2)
    circuit_set_param(comps, i, 10.0, td)
    circuit_set_param(comps, i, 11.0, 1.0)
    i
}

def circuit_add_vsin(comps: matrix, ctrl: matrix, n_plus: Double, n_minus: Double, voff: Double, vamp: Double, freq: Double, phase: Double) -> Double {
    var i = circuit_add_vdc(comps, ctrl, n_plus, n_minus, 0.0)
    circuit_set_param(comps, i, 5.0, phase)
    circuit_set_param(comps, i, 8.0, voff)
    circuit_set_param(comps, i, 9.0, vamp)
    circuit_set_param(comps, i, 10.0, freq)
    circuit_set_param(comps, i, 11.0, 2.0)
    i
}

def circuit_add_vcvs(comps: matrix, ctrl: matrix, n_op: Double, n_on: Double, n_ip: Double, n_in: Double, gain: Double) -> Double {
    var i = matrix_get(ctrl, 1.0, 0.0)
    matrix_set(comps, i, 0.0, 7.0)
    matrix_set(comps, i, 1.0, n_op)
    matrix_set(comps, i, 2.0, n_on)
    matrix_set(comps, i, 3.0, n_ip)
    matrix_set(comps, i, 4.0, n_in)
    matrix_set(comps, i, 5.0, gain)
    matrix_set(ctrl, 1.0, 0.0, i + 1.0)
    i
}

def circuit_add_vccs(comps: matrix, ctrl: matrix, n_op: Double, n_on: Double, n_ip: Double, n_in: Double, gm: Double) -> Double {
    var i = matrix_get(ctrl, 1.0, 0.0)
    matrix_set(comps, i, 0.0, 8.0)
    matrix_set(comps, i, 1.0, n_op)
    matrix_set(comps, i, 2.0, n_on)
    matrix_set(comps, i, 3.0, n_ip)
    matrix_set(comps, i, 4.0, n_in)
    matrix_set(comps, i, 5.0, gm)
    matrix_set(ctrl, 1.0, 0.0, i + 1.0)
    i
}

def circuit_add_diode(comps: matrix, ctrl: matrix, n_plus: Double, n_minus: Double, isat: Double, n_factor: Double, vt: Double) -> Double {
    var i = matrix_get(ctrl, 1.0, 0.0)
    matrix_set(comps, i, 0.0, 9.0)
    matrix_set(comps, i, 1.0, n_plus)
    matrix_set(comps, i, 2.0, n_minus)
    matrix_set(comps, i, 4.0, isat)
    matrix_set(comps, i, 5.0, n_factor)
    matrix_set(comps, i, 7.0, vt)
    matrix_set(ctrl, 1.0, 0.0, i + 1.0)
    i
}

def circuit_add_npn(comps: matrix, ctrl: matrix, n_c: Double, n_b: Double, n_e: Double, bf: Double, isat: Double, vaf: Double, vt: Double) -> Double {
    var i = matrix_get(ctrl, 1.0, 0.0)
    matrix_set(comps, i, 0.0, 10.0)
    matrix_set(comps, i, 1.0, n_c)
    matrix_set(comps, i, 2.0, n_b)
    matrix_set(comps, i, 3.0, n_e)
    matrix_set(comps, i, 4.0, bf)
    matrix_set(comps, i, 5.0, isat)
    matrix_set(comps, i, 6.0, vaf)
    matrix_set(comps, i, 7.0, vt)
    matrix_set(ctrl, 1.0, 0.0, i + 1.0)
    i
}

def circuit_add_pnp(comps: matrix, ctrl: matrix, n_c: Double, n_b: Double, n_e: Double, bf: Double, isat: Double, vaf: Double, vt: Double) -> Double {
    var i = matrix_get(ctrl, 1.0, 0.0)
    matrix_set(comps, i, 0.0, 11.0)
    matrix_set(comps, i, 1.0, n_c)
    matrix_set(comps, i, 2.0, n_b)
    matrix_set(comps, i, 3.0, n_e)
    matrix_set(comps, i, 4.0, bf)
    matrix_set(comps, i, 5.0, isat)
    matrix_set(comps, i, 6.0, vaf)
    matrix_set(comps, i, 7.0, vt)
    matrix_set(ctrl, 1.0, 0.0, i + 1.0)
    i
}

def circuit_add_nmos(comps: matrix, ctrl: matrix, n_d: Double, n_g: Double, n_s: Double, n_b: Double, kp: Double, vto: Double, lambda_val: Double) -> Double {
    var i = matrix_get(ctrl, 1.0, 0.0)
    matrix_set(comps, i, 0.0, 12.0)
    matrix_set(comps, i, 1.0, n_d)
    matrix_set(comps, i, 2.0, n_g)
    matrix_set(comps, i, 3.0, n_s)
    matrix_set(comps, i, 4.0, n_b)
    matrix_set(comps, i, 5.0, kp)
    matrix_set(comps, i, 6.0, vto)
    matrix_set(comps, i, 7.0, lambda_val)
    matrix_set(ctrl, 1.0, 0.0, i + 1.0)
    i
}

def circuit_add_pmos(comps: matrix, ctrl: matrix, n_d: Double, n_g: Double, n_s: Double, n_b: Double, kp: Double, vto: Double, lambda_val: Double) -> Double {
    var i = matrix_get(ctrl, 1.0, 0.0)
    matrix_set(comps, i, 0.0, 13.0)
    matrix_set(comps, i, 1.0, n_d)
    matrix_set(comps, i, 2.0, n_g)
    matrix_set(comps, i, 3.0, n_s)
    matrix_set(comps, i, 4.0, n_b)
    matrix_set(comps, i, 5.0, kp)
    matrix_set(comps, i, 6.0, vto)
    matrix_set(comps, i, 7.0, lambda_val)
    matrix_set(ctrl, 1.0, 0.0, i + 1.0)
    i
}

def circuit_add_njf(comps: matrix, ctrl: matrix, n_d: Double, n_g: Double, n_s: Double, beta_val: Double, vto: Double, lambda_val: Double) -> Double {
    var i = matrix_get(ctrl, 1.0, 0.0)
    matrix_set(comps, i, 0.0, 14.0)
    matrix_set(comps, i, 1.0, n_d)
    matrix_set(comps, i, 2.0, n_g)
    matrix_set(comps, i, 3.0, n_s)
    matrix_set(comps, i, 4.0, 0.0)
    matrix_set(comps, i, 5.0, beta_val)
    matrix_set(comps, i, 6.0, vto)
    matrix_set(comps, i, 7.0, lambda_val)
    matrix_set(ctrl, 1.0, 0.0, i + 1.0)
    i
}

def circuit_add_pjf(comps: matrix, ctrl: matrix, n_d: Double, n_g: Double, n_s: Double, beta_val: Double, vto: Double, lambda_val: Double) -> Double {
    var i = matrix_get(ctrl, 1.0, 0.0)
    matrix_set(comps, i, 0.0, 15.0)
    matrix_set(comps, i, 1.0, n_d)
    matrix_set(comps, i, 2.0, n_g)
    matrix_set(comps, i, 3.0, n_s)
    matrix_set(comps, i, 4.0, 0.0)
    matrix_set(comps, i, 5.0, beta_val)
    matrix_set(comps, i, 6.0, vto)
    matrix_set(comps, i, 7.0, lambda_val)
    matrix_set(ctrl, 1.0, 0.0, i + 1.0)
    i
}

def circuit_add_vcsw(comps: matrix, ctrl: matrix, n_plus: Double, n_minus: Double, n_cp: Double, n_cm: Double, ron: Double, roff: Double, von: Double, voff: Double) -> Double {
    var i = matrix_get(ctrl, 1.0, 0.0)
    matrix_set(comps, i, 0.0, 16.0)
    matrix_set(comps, i, 1.0, n_plus)
    matrix_set(comps, i, 2.0, n_minus)
    matrix_set(comps, i, 3.0, n_cp)
    matrix_set(comps, i, 4.0, n_cm)
    matrix_set(comps, i, 5.0, ron)
    matrix_set(comps, i, 6.0, roff)
    matrix_set(comps, i, 7.0, von)
    matrix_set(comps, i, 8.0, voff)
    matrix_set(ctrl, 1.0, 0.0, i + 1.0)
    i
}

def circuit_add_ccsw(comps: matrix, ctrl: matrix, n_plus: Double, n_minus: Double, vsrc_idx: Double, ron: Double, roff: Double, ion: Double, ioff: Double) -> Double {
    var i = matrix_get(ctrl, 1.0, 0.0)
    matrix_set(comps, i, 0.0, 17.0)
    matrix_set(comps, i, 1.0, n_plus)
    matrix_set(comps, i, 2.0, n_minus)
    matrix_set(comps, i, 3.0, vsrc_idx)
    matrix_set(comps, i, 5.0, ron)
    matrix_set(comps, i, 6.0, roff)
    matrix_set(comps, i, 7.0, ion)
    matrix_set(comps, i, 8.0, ioff)
    matrix_set(ctrl, 1.0, 0.0, i + 1.0)
    i
}
