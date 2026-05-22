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
#   col[8]:  param5 (AC magnitude)
#   col[9]:  param6 (AC frequency)
#   col[10]: param7 (AC phase / initial condition)
#   col[11]: (reserved)
#
# Control vector (3 x 1):
#   [0] = num_non_gnd_nodes
#   [1] = component_count  (next free row in comps)
#   [2] = max_components
#
# Type codes:
#   0: R   (resistor)
#   1: C   (capacitor)
#   2: L   (inductor)
#   3: Vdc (DC voltage source)
#   4: Idc (DC current source)
#   5: Vac (AC voltage source)
#   6: Iac (AC current source)
#   7: E   (VCVS)
#   8: G   (VCCS)
#   9: D   (diode)
#  10: Qnpn (NPN BJT)
#  11: Qpnp (PNP BJT)
#  12: Mnmos (NMOS)
#  13: Mpmos (PMOS)
#  14: X   (subcircuit placeholder)

def circuit_create(num_nodes, max_comps) {
    var comps = matrix_zeros(max_comps, 12)
    var ctrl = matrix_zeros(3, 1)
    matrix_set(ctrl, 0, 0, num_nodes)
    matrix_set(ctrl, 1, 0, 0.0)
    matrix_set(ctrl, 2, 0, max_comps)
    ctrl
}

# --- linear passive ---

def circuit_add_resistor(comps : matrix, ctrl : matrix, n_plus, n_minus, r_val) {
    var i = matrix_get(ctrl, 1, 0)
    matrix_set(comps, i, 0, 0.0)
    matrix_set(comps, i, 1, n_plus)
    matrix_set(comps, i, 2, n_minus)
    matrix_set(comps, i, 4, r_val)
    matrix_set(ctrl, 1, 0, i + 1.0)
    i
}

def circuit_add_capacitor(comps : matrix, ctrl : matrix, n_plus, n_minus, c_val, init_v) {
    var i = matrix_get(ctrl, 1, 0)
    matrix_set(comps, i, 0, 1.0)
    matrix_set(comps, i, 1, n_plus)
    matrix_set(comps, i, 2, n_minus)
    matrix_set(comps, i, 4, c_val)
    matrix_set(comps, i, 10, init_v)
    matrix_set(ctrl, 1, 0, i + 1.0)
    i
}

def circuit_add_inductor(comps : matrix, ctrl : matrix, n_plus, n_minus, l_val, init_i) {
    var i = matrix_get(ctrl, 1, 0)
    matrix_set(comps, i, 0, 2.0)
    matrix_set(comps, i, 1, n_plus)
    matrix_set(comps, i, 2, n_minus)
    matrix_set(comps, i, 4, l_val)
    matrix_set(comps, i, 10, init_i)
    matrix_set(ctrl, 1, 0, i + 1.0)
    i
}

# --- independent sources ---

def circuit_add_vdc(comps : matrix, ctrl : matrix, n_plus, n_minus, v_val) {
    var i = matrix_get(ctrl, 1, 0)
    matrix_set(comps, i, 0, 3.0)
    matrix_set(comps, i, 1, n_plus)
    matrix_set(comps, i, 2, n_minus)
    matrix_set(comps, i, 4, v_val)
    matrix_set(ctrl, 1, 0, i + 1.0)
    i
}

def circuit_add_idc(comps : matrix, ctrl : matrix, n_plus, n_minus, i_val) {
    var i = matrix_get(ctrl, 1, 0)
    matrix_set(comps, i, 0, 4.0)
    matrix_set(comps, i, 1, n_plus)
    matrix_set(comps, i, 2, n_minus)
    matrix_set(comps, i, 4, i_val)
    matrix_set(ctrl, 1, 0, i + 1.0)
    i
}

def circuit_add_vac(comps : matrix, ctrl : matrix, n_plus, n_minus, dc_val, ac_mag, ac_freq, ac_phase) {
    var i = matrix_get(ctrl, 1, 0)
    matrix_set(comps, i, 0, 5.0)
    matrix_set(comps, i, 1, n_plus)
    matrix_set(comps, i, 2, n_minus)
    matrix_set(comps, i, 4, dc_val)
    matrix_set(comps, i, 8, ac_mag)
    matrix_set(comps, i, 9, ac_freq)
    matrix_set(comps, i, 10, ac_phase)
    matrix_set(ctrl, 1, 0, i + 1.0)
    i
}

def circuit_add_iac(comps : matrix, ctrl : matrix, n_plus, n_minus, dc_val, ac_mag, ac_freq, ac_phase) {
    var i = matrix_get(ctrl, 1, 0)
    matrix_set(comps, i, 0, 6.0)
    matrix_set(comps, i, 1, n_plus)
    matrix_set(comps, i, 2, n_minus)
    matrix_set(comps, i, 4, dc_val)
    matrix_set(comps, i, 8, ac_mag)
    matrix_set(comps, i, 9, ac_freq)
    matrix_set(comps, i, 10, ac_phase)
    matrix_set(ctrl, 1, 0, i + 1.0)
    i
}

# --- controlled sources ---

def circuit_add_vcvs(comps : matrix, ctrl : matrix, n_op, n_on, n_ip, n_in, gain) {
    var i = matrix_get(ctrl, 1, 0)
    matrix_set(comps, i, 0, 7.0)
    matrix_set(comps, i, 1, n_op)
    matrix_set(comps, i, 2, n_on)
    matrix_set(comps, i, 3, n_ip)
    matrix_set(comps, i, 4, n_in)
    matrix_set(comps, i, 5, gain)
    matrix_set(ctrl, 1, 0, i + 1.0)
    i
}

def circuit_add_vccs(comps : matrix, ctrl : matrix, n_op, n_on, n_ip, n_in, gm) {
    var i = matrix_get(ctrl, 1, 0)
    matrix_set(comps, i, 0, 8.0)
    matrix_set(comps, i, 1, n_op)
    matrix_set(comps, i, 2, n_on)
    matrix_set(comps, i, 3, n_ip)
    matrix_set(comps, i, 4, n_in)
    matrix_set(comps, i, 5, gm)
    matrix_set(ctrl, 1, 0, i + 1.0)
    i
}

# --- nonlinear devices ---

def circuit_add_diode(comps : matrix, ctrl : matrix, n_plus, n_minus, isat, n_factor, vt) {
    var i = matrix_get(ctrl, 1, 0)
    matrix_set(comps, i, 0, 9.0)
    matrix_set(comps, i, 1, n_plus)
    matrix_set(comps, i, 2, n_minus)
    matrix_set(comps, i, 4, isat)
    matrix_set(comps, i, 5, n_factor)
    matrix_set(comps, i, 7, vt)
    matrix_set(ctrl, 1, 0, i + 1.0)
    i
}

def circuit_add_npn(comps : matrix, ctrl : matrix, n_c, n_b, n_e, bf, isat, vaf, vt) {
    var i = matrix_get(ctrl, 1, 0)
    matrix_set(comps, i, 0, 10.0)
    matrix_set(comps, i, 1, n_c)
    matrix_set(comps, i, 2, n_b)
    matrix_set(comps, i, 3, n_e)
    matrix_set(comps, i, 4, bf)
    matrix_set(comps, i, 5, isat)
    matrix_set(comps, i, 6, vaf)
    matrix_set(comps, i, 7, vt)
    matrix_set(ctrl, 1, 0, i + 1.0)
    i
}

def circuit_add_pnp(comps : matrix, ctrl : matrix, n_c, n_b, n_e, bf, isat, vaf, vt) {
    var i = matrix_get(ctrl, 1, 0)
    matrix_set(comps, i, 0, 11.0)
    matrix_set(comps, i, 1, n_c)
    matrix_set(comps, i, 2, n_b)
    matrix_set(comps, i, 3, n_e)
    matrix_set(comps, i, 4, bf)
    matrix_set(comps, i, 5, isat)
    matrix_set(comps, i, 6, vaf)
    matrix_set(comps, i, 7, vt)
    matrix_set(ctrl, 1, 0, i + 1.0)
    i
}

def circuit_add_nmos(comps : matrix, ctrl : matrix, n_d, n_g, n_s, n_b, kp, vto, lambda_val) {
    var i = matrix_get(ctrl, 1, 0)
    matrix_set(comps, i, 0, 12.0)
    matrix_set(comps, i, 1, n_d)
    matrix_set(comps, i, 2, n_g)
    matrix_set(comps, i, 3, n_s)
    matrix_set(comps, i, 4, n_b)
    matrix_set(comps, i, 5, kp)
    matrix_set(comps, i, 6, vto)
    matrix_set(comps, i, 7, lambda_val)
    matrix_set(ctrl, 1, 0, i + 1.0)
    i
}

def circuit_add_pmos(comps : matrix, ctrl : matrix, n_d, n_g, n_s, n_b, kp, vto, lambda_val) {
    var i = matrix_get(ctrl, 1, 0)
    matrix_set(comps, i, 0, 13.0)
    matrix_set(comps, i, 1, n_d)
    matrix_set(comps, i, 2, n_g)
    matrix_set(comps, i, 3, n_s)
    matrix_set(comps, i, 4, n_b)
    matrix_set(comps, i, 5, kp)
    matrix_set(comps, i, 6, vto)
    matrix_set(comps, i, 7, lambda_val)
    matrix_set(ctrl, 1, 0, i + 1.0)
    i
}

# --- iteration helpers ---

def circuit_count(ctrl : matrix) {
    matrix_get(ctrl, 1, 0)
}

def circuit_num_nodes(ctrl : matrix) {
    matrix_get(ctrl, 0, 0)
}

def circuit_component_type(comps : matrix, idx) {
    matrix_get(comps, idx, 0)
}

def circuit_node_p(comps : matrix, idx) {
    matrix_get(comps, idx, 1)
}

def circuit_node_n(comps : matrix, idx) {
    matrix_get(comps, idx, 2)
}

def circuit_node_3(comps : matrix, idx) {
    matrix_get(comps, idx, 3)
}

def circuit_param(comps : matrix, idx, col) {
    matrix_get(comps, idx, col)
}

def circuit_set_param(comps : matrix, idx, col, val) {
    matrix_set(comps, idx, col, val)
}
