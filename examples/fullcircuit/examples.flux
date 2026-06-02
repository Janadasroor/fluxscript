# FullCircuit — Built-in Test Circuits and Demos
import circuit

def ex_voltage_divider(c: Circuit, r1: Double, r2: Double, vin: Double) {
    var comp = Component.R { n_plus: 1.0, n_minus: 2.0, r_val: r1 }
    c.add(comp)
    comp = Component.R { n_plus: 2.0, n_minus: 0.0, r_val: r2 }
    c.add(comp)
    comp = Component.Vdc { n_plus: 1.0, n_minus: 0.0, v_val: vin }
    c.add(comp)
}

def ex_rc_lowpass(c: Circuit, r: Double, cap: Double, vin: Double) {
    var comp = Component.R { n_plus: 1.0, n_minus: 2.0, r_val: r }
    c.add(comp)
    comp = Component.C { n_plus: 2.0, n_minus: 0.0, c_val: cap, init_v: 0.0 }
    c.add(comp)
    comp = Component.Vdc { n_plus: 1.0, n_minus: 0.0, v_val: vin }
    c.add(comp)
}

def ex_rc_highpass(c: Circuit, r: Double, cap: Double, vin: Double) {
    var comp = Component.C { n_plus: 1.0, n_minus: 2.0, c_val: cap, init_v: 0.0 }
    c.add(comp)
    comp = Component.R { n_plus: 2.0, n_minus: 0.0, r_val: r }
    c.add(comp)
    comp = Component.Vdc { n_plus: 1.0, n_minus: 0.0, v_val: vin }
    c.add(comp)
}

def ex_rlc_series(c: Circuit, r: Double, l: Double, cap: Double, vin: Double) {
    var comp = Component.R { n_plus: 1.0, n_minus: 2.0, r_val: r }
    c.add(comp)
    comp = Component.L { n_plus: 2.0, n_minus: 3.0, l_val: l, init_i: 0.0 }
    c.add(comp)
    comp = Component.C { n_plus: 3.0, n_minus: 0.0, c_val: cap, init_v: 0.0 }
    c.add(comp)
    comp = Component.Vdc { n_plus: 1.0, n_minus: 0.0, v_val: vin }
    c.add(comp)
}

def ex_rlc_parallel(c: Circuit, r: Double, l: Double, cap: Double, iin: Double) {
    var comp = Component.R { n_plus: 1.0, n_minus: 0.0, r_val: r }
    c.add(comp)
    comp = Component.L { n_plus: 1.0, n_minus: 0.0, l_val: l, init_i: 0.0 }
    c.add(comp)
    comp = Component.C { n_plus: 1.0, n_minus: 0.0, c_val: cap, init_v: 0.0 }
    c.add(comp)
    comp = Component.Idc { n_plus: 1.0, n_minus: 0.0, i_val: iin }
    c.add(comp)
}

def ex_vcvs_amplifier(c: Circuit, gain: Double, rin: Double, rout: Double, rload: Double, vin: Double) {
    var comp = Component.R { n_plus: 1.0, n_minus: 0.0, r_val: rin }
    c.add(comp)
    comp = Component.Vdc { n_plus: 1.0, n_minus: 0.0, v_val: vin }
    c.add(comp)
    comp = Component.E { n_op: 3.0, n_on: 0.0, n_ip: 1.0, n_in: 0.0, gain: gain }
    c.add(comp)
    comp = Component.R { n_plus: 3.0, n_minus: 2.0, r_val: rout }
    c.add(comp)
    comp = Component.R { n_plus: 2.0, n_minus: 0.0, r_val: rload }
    c.add(comp)
}

def ex_vccs_amplifier(c: Circuit, gm: Double, rload: Double, iin: Double) {
    var comp = Component.Idc { n_plus: 1.0, n_minus: 0.0, i_val: iin }
    c.add(comp)
    comp = Component.R { n_plus: 1.0, n_minus: 0.0, r_val: 1e6 }
    c.add(comp)
    comp = Component.G { n_op: 3.0, n_on: 0.0, n_ip: 1.0, n_in: 0.0, gm: gm }
    c.add(comp)
    comp = Component.R { n_plus: 3.0, n_minus: 0.0, r_val: rload }
    c.add(comp)
}

def ex_wheatstone_bridge(c: Circuit, r1: Double, r2: Double, r3: Double, r4: Double, r5: Double, vin: Double) {
    var comp = Component.R { n_plus: 1.0, n_minus: 2.0, r_val: r1 }
    c.add(comp)
    comp = Component.R { n_plus: 2.0, n_minus: 0.0, r_val: r2 }
    c.add(comp)
    comp = Component.R { n_plus: 1.0, n_minus: 3.0, r_val: r3 }
    c.add(comp)
    comp = Component.R { n_plus: 3.0, n_minus: 0.0, r_val: r4 }
    c.add(comp)
    comp = Component.R { n_plus: 2.0, n_minus: 3.0, r_val: r5 }
    c.add(comp)
    comp = Component.Vdc { n_plus: 1.0, n_minus: 0.0, v_val: vin }
    c.add(comp)
}

def ex_current_mirror(c: Circuit, rref: Double, iref: Double, rload: Double) {
    var comp = Component.Vdc { n_plus: 1.0, n_minus: 0.0, v_val: 5.0 }
    c.add(comp)
    comp = Component.R { n_plus: 1.0, n_minus: 2.0, r_val: rref }
    c.add(comp)
    comp = Component.G { n_op: 3.0, n_on: 0.0, n_ip: 2.0, n_in: 0.0, gm: 1.0 }
    c.add(comp)
    comp = Component.G { n_op: 4.0, n_on: 0.0, n_ip: 2.0, n_in: 0.0, gm: 1.0 }
    c.add(comp)
    comp = Component.R { n_plus: 4.0, n_minus: 0.0, r_val: rload }
    c.add(comp)
    comp = Component.Idc { n_plus: 2.0, n_minus: 0.0, i_val: iref }
    c.add(comp)
}

def ex_opamp_inverting(c: Circuit, r1: Double, rf: Double, vin: Double) {
    var comp = Component.R { n_plus: 1.0, n_minus: 2.0, r_val: r1 }
    c.add(comp)
    comp = Component.R { n_plus: 2.0, n_minus: 3.0, r_val: rf }
    c.add(comp)
    comp = Component.E { n_op: 3.0, n_on: 0.0, n_ip: 2.0, n_in: 0.0, gain: 1e6 }
    c.add(comp)
    comp = Component.Vdc { n_plus: 1.0, n_minus: 0.0, v_val: vin }
    c.add(comp)
}

def ex_opamp_noninv(c: Circuit, r1: Double, rf: Double, vin: Double) {
    var comp = Component.R { n_plus: 2.0, n_minus: 0.0, r_val: r1 }
    c.add(comp)
    comp = Component.R { n_plus: 2.0, n_minus: 3.0, r_val: rf }
    c.add(comp)
    comp = Component.E { n_op: 3.0, n_on: 0.0, n_ip: 1.0, n_in: 2.0, gain: 1e6 }
    c.add(comp)
    comp = Component.Vdc { n_plus: 1.0, n_minus: 0.0, v_val: vin }
    c.add(comp)
}

def ex_sallen_key_lpf(c: Circuit, r: Double, cap: Double, vin: Double) {
    var comp = Component.R { n_plus: 1.0, n_minus: 2.0, r_val: r }
    c.add(comp)
    comp = Component.R { n_plus: 2.0, n_minus: 3.0, r_val: r }
    c.add(comp)
    comp = Component.C { n_plus: 2.0, n_minus: 0.0, c_val: cap, init_v: 0.0 }
    c.add(comp)
    comp = Component.C { n_plus: 3.0, n_minus: 4.0, c_val: cap, init_v: 0.0 }
    c.add(comp)
    comp = Component.E { n_op: 4.0, n_on: 0.0, n_ip: 3.0, n_in: 0.0, gain: 1.0 }
    c.add(comp)
    comp = Component.Vdc { n_plus: 1.0, n_minus: 0.0, v_val: vin }
    c.add(comp)
}

def ex_rlc_bpf(c: Circuit, r: Double, l: Double, cap: Double, vin: Double) {
    var comp = Component.R { n_plus: 1.0, n_minus: 2.0, r_val: r }
    c.add(comp)
    comp = Component.L { n_plus: 2.0, n_minus: 3.0, l_val: l, init_i: 0.0 }
    c.add(comp)
    comp = Component.C { n_plus: 3.0, n_minus: 0.0, c_val: cap, init_v: 0.0 }
    c.add(comp)
    comp = Component.R { n_plus: 3.0, n_minus: 0.0, r_val: 1.0 }
    c.add(comp)
    comp = Component.Vdc { n_plus: 1.0, n_minus: 0.0, v_val: vin }
    c.add(comp)
}

def ex_pulse_response(c: Circuit, r: Double, cap: Double, v_high: Double, t_rise: Double, t_width: Double) {
    var comp = Component.R { n_plus: 1.0, n_minus: 2.0, r_val: r }
    c.add(comp)
    comp = Component.C { n_plus: 2.0, n_minus: 0.0, c_val: cap, init_v: 0.0 }
    c.add(comp)
    var vc = c.add(Component.Vac { n_plus: 1.0, n_minus: 0.0, dc_val: 0.0, ac_mag: 1.0, ac_freq: 1.0, ac_phase: 0.0 })
    c.set_param(vc, 11.0, 1.0)
    c.set_param(vc, 8.0, 0.0)
    c.set_param(vc, 9.0, v_high)
    c.set_param(vc, 10.0, 0.0)
    c.set_param(vc, 5.0, t_rise)
    c.set_param(vc, 6.0, t_rise)
    c.set_param(vc, 7.0, t_width)
    c.set_param(vc, 4.0, 1.0)
}

def ex_diode_rectifier(c: Circuit, rload: Double, cfilter: Double, isat: Double, vt: Double) {
    var comp = Component.Vac { n_plus: 1.0, n_minus: 0.0, dc_val: 0.0, ac_mag: 10.0, ac_freq: 60.0, ac_phase: 0.0 }
    c.add(comp)
    comp = Component.D { n_plus: 1.0, n_minus: 2.0, isat: isat, n_factor: 1.0, vt: vt }
    c.add(comp)
    comp = Component.R { n_plus: 2.0, n_minus: 0.0, r_val: rload }
    c.add(comp)
    comp = Component.C { n_plus: 2.0, n_minus: 0.0, c_val: cfilter, init_v: 0.0 }
    c.add(comp)
}

def ex_bjt_ce_amplifier(c: Circuit, r1: Double, r2: Double, rc: Double, re: Double, rload: Double, bf: Double, isat: Double, vaf: Double, vt: Double, vcc: Double) {
    var comp = Component.Vdc { n_plus: 6.0, n_minus: 0.0, v_val: vcc }
    c.add(comp)
    comp = Component.R { n_plus: 6.0, n_minus: 1.0, r_val: rc }
    c.add(comp)
    comp = Component.R { n_plus: 6.0, n_minus: 2.0, r_val: r1 }
    c.add(comp)
    comp = Component.R { n_plus: 2.0, n_minus: 0.0, r_val: r2 }
    c.add(comp)
    comp = Component.R { n_plus: 3.0, n_minus: 0.0, r_val: re }
    c.add(comp)
    comp = Component.C { n_plus: 4.0, n_minus: 2.0, c_val: 1e-6, init_v: 0.0 }
    c.add(comp)
    comp = Component.C { n_plus: 1.0, n_minus: 5.0, c_val: 1e-6, init_v: 0.0 }
    c.add(comp)
    comp = Component.R { n_plus: 5.0, n_minus: 0.0, r_val: rload }
    c.add(comp)
    comp = Component.Qnpn { n_c: 1.0, n_b: 4.0, n_e: 3.0, bf: bf, isat: isat, vaf: vaf, vt: vt }
    c.add(comp)
}

def ex_differential_pair(c: Circuit, rc: Double, ree: Double, rload: Double, bf: Double, isat: Double, vaf: Double, vt: Double, vcc: Double, vee: Double) {
    var comp = Component.Vdc { n_plus: 6.0, n_minus: 0.0, v_val: vcc }
    c.add(comp)
    comp = Component.Vdc { n_plus: 7.0, n_minus: 0.0, v_val: vee }
    c.add(comp)
    comp = Component.R { n_plus: 6.0, n_minus: 1.0, r_val: rc }
    c.add(comp)
    comp = Component.R { n_plus: 6.0, n_minus: 2.0, r_val: rc }
    c.add(comp)
    comp = Component.R { n_plus: 3.0, n_minus: 7.0, r_val: ree }
    c.add(comp)
    comp = Component.R { n_plus: 1.0, n_minus: 0.0, r_val: rload }
    c.add(comp)
    comp = Component.Qnpn { n_c: 1.0, n_b: 4.0, n_e: 3.0, bf: bf, isat: isat, vaf: vaf, vt: vt }
    c.add(comp)
    comp = Component.Qnpn { n_c: 2.0, n_b: 5.0, n_e: 3.0, bf: bf, isat: isat, vaf: vaf, vt: vt }
    c.add(comp)
    comp = Component.Vdc { n_plus: 4.0, n_minus: 0.0, v_val: 0.0 }
    c.add(comp)
    comp = Component.Vdc { n_plus: 5.0, n_minus: 0.0, v_val: 0.0 }
    c.add(comp)
}

def ex_cascade_amplifier(c: Circuit, r1: Double, r2: Double, rc1: Double, re1: Double, rc2: Double, re2: Double, rload: Double, bf: Double, isat: Double, vaf: Double, vt: Double, vcc: Double) {
    var comp = Component.Vdc { n_plus: 10.0, n_minus: 0.0, v_val: vcc }
    c.add(comp)
    comp = Component.R { n_plus: 10.0, n_minus: 1.0, r_val: rc1 }
    c.add(comp)
    comp = Component.R { n_plus: 10.0, n_minus: 2.0, r_val: r1 }
    c.add(comp)
    comp = Component.R { n_plus: 10.0, n_minus: 3.0, r_val: rc2 }
    c.add(comp)
    comp = Component.R { n_plus: 2.0, n_minus: 0.0, r_val: r2 }
    c.add(comp)
    comp = Component.R { n_plus: 4.0, n_minus: 0.0, r_val: re1 }
    c.add(comp)
    comp = Component.R { n_plus: 5.0, n_minus: 0.0, r_val: re2 }
    c.add(comp)
    comp = Component.C { n_plus: 6.0, n_minus: 2.0, c_val: 1e-6, init_v: 0.0 }
    c.add(comp)
    comp = Component.C { n_plus: 1.0, n_minus: 7.0, c_val: 1e-6, init_v: 0.0 }
    c.add(comp)
    comp = Component.R { n_plus: 7.0, n_minus: 0.0, r_val: rload }
    c.add(comp)
    comp = Component.Qnpn { n_c: 1.0, n_b: 6.0, n_e: 4.0, bf: bf, isat: isat, vaf: vaf, vt: vt }
    c.add(comp)
    comp = Component.Qnpn { n_c: 3.0, n_b: 8.0, n_e: 5.0, bf: bf, isat: isat, vaf: vaf, vt: vt }
    c.add(comp)
    comp = Component.Vdc { n_plus: 8.0, n_minus: 1.0, v_val: 0.0 }
    c.add(comp)
}

def ex_wein_bridge_osc(c: Circuit, r: Double, cap: Double, rf: Double, rg: Double) {
    var comp = Component.R { n_plus: 1.0, n_minus: 2.0, r_val: r }
    c.add(comp)
    comp = Component.C { n_plus: 2.0, n_minus: 0.0, c_val: cap, init_v: 1.0 }
    c.add(comp)
    comp = Component.C { n_plus: 1.0, n_minus: 3.0, c_val: cap, init_v: 0.0 }
    c.add(comp)
    comp = Component.R { n_plus: 3.0, n_minus: 0.0, r_val: r }
    c.add(comp)
    comp = Component.R { n_plus: 4.0, n_minus: 0.0, r_val: rg }
    c.add(comp)
    comp = Component.R { n_plus: 4.0, n_minus: 5.0, r_val: rf }
    c.add(comp)
    comp = Component.E { n_op: 5.0, n_on: 0.0, n_ip: 3.0, n_in: 4.0, gain: 1e6 }
    c.add(comp)
    comp = Component.Vdc { n_plus: 1.0, n_minus: 0.0, v_val: 0.0 }
    c.add(comp)
}

def ex_mosfet_cs_amplifier(c: Circuit, rd: Double, rg1: Double, rg2: Double, rs: Double, rload: Double, kp: Double, vto: Double, lambda_val: Double, vdd: Double) {
    var comp = Component.Vdc { n_plus: 10.0, n_minus: 0.0, v_val: vdd }
    c.add(comp)
    comp = Component.R { n_plus: 10.0, n_minus: 1.0, r_val: rd }
    c.add(comp)
    comp = Component.R { n_plus: 10.0, n_minus: 2.0, r_val: rg1 }
    c.add(comp)
    comp = Component.R { n_plus: 2.0, n_minus: 0.0, r_val: rg2 }
    c.add(comp)
    comp = Component.R { n_plus: 3.0, n_minus: 0.0, r_val: rs }
    c.add(comp)
    comp = Component.C { n_plus: 4.0, n_minus: 2.0, c_val: 1e-6, init_v: 0.0 }
    c.add(comp)
    comp = Component.C { n_plus: 1.0, n_minus: 5.0, c_val: 1e-6, init_v: 0.0 }
    c.add(comp)
    comp = Component.R { n_plus: 5.0, n_minus: 0.0, r_val: rload }
    c.add(comp)
    comp = Component.Mnmos { n_d: 1.0, n_g: 4.0, n_s: 3.0, n_bulk: 0.0, kp: kp, vto: vto, lambda_val: lambda_val }
    c.add(comp)
}
