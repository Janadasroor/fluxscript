# FullCircuit — Built-in Test Circuits and Demos
import circuit

def ex_voltage_divider(comps : matrix, ctrl : matrix, r1, r2, vin) {
    circuit_add_resistor(comps, ctrl, 1, 2, r1)
    circuit_add_resistor(comps, ctrl, 2, 0, r2)
    circuit_add_vdc(comps, ctrl, 1, 0, vin)
}

def ex_rc_lowpass(comps : matrix, ctrl : matrix, r, c, vin) {
    circuit_add_resistor(comps, ctrl, 1, 2, r)
    circuit_add_capacitor(comps, ctrl, 2, 0, c, 0.0)
    circuit_add_vdc(comps, ctrl, 1, 0, vin)
}

def ex_rc_highpass(comps : matrix, ctrl : matrix, r, c, vin) {
    circuit_add_capacitor(comps, ctrl, 1, 2, c, 0.0)
    circuit_add_resistor(comps, ctrl, 2, 0, r)
    circuit_add_vdc(comps, ctrl, 1, 0, vin)
}

def ex_rlc_series(comps : matrix, ctrl : matrix, r, l, c, vin) {
    circuit_add_resistor(comps, ctrl, 1, 2, r)
    circuit_add_inductor(comps, ctrl, 2, 3, l, 0.0)
    circuit_add_capacitor(comps, ctrl, 3, 0, c, 0.0)
    circuit_add_vdc(comps, ctrl, 1, 0, vin)
}

def ex_rlc_parallel(comps : matrix, ctrl : matrix, r, l, c, iin) {
    circuit_add_resistor(comps, ctrl, 1, 0, r)
    circuit_add_inductor(comps, ctrl, 1, 0, l, 0.0)
    circuit_add_capacitor(comps, ctrl, 1, 0, c, 0.0)
    circuit_add_idc(comps, ctrl, 1, 0, iin)
}

def ex_vcvs_amplifier(comps : matrix, ctrl : matrix, gain, rin, rout, rload, vin) {
    circuit_add_resistor(comps, ctrl, 1, 0, rin)
    circuit_add_vdc(comps, ctrl, 1, 0, vin)
    circuit_add_vcvs(comps, ctrl, 3, 0, 1, 0, gain)
    circuit_add_resistor(comps, ctrl, 3, 2, rout)
    circuit_add_resistor(comps, ctrl, 2, 0, rload)
}

def ex_vccs_amplifier(comps : matrix, ctrl : matrix, gm, rload, iin) {
    circuit_add_idc(comps, ctrl, 1, 0, iin)
    circuit_add_resistor(comps, ctrl, 1, 0, 1e6)
    circuit_add_vccs(comps, ctrl, 3, 0, 1, 0, gm)
    circuit_add_resistor(comps, ctrl, 3, 0, rload)
}

def ex_wheatstone_bridge(comps : matrix, ctrl : matrix, r1, r2, r3, r4, r5, vin) {
    circuit_add_resistor(comps, ctrl, 1, 2, r1)
    circuit_add_resistor(comps, ctrl, 2, 0, r2)
    circuit_add_resistor(comps, ctrl, 1, 3, r3)
    circuit_add_resistor(comps, ctrl, 3, 0, r4)
    circuit_add_resistor(comps, ctrl, 2, 3, r5)
    circuit_add_vdc(comps, ctrl, 1, 0, vin)
}

def ex_current_mirror(comps : matrix, ctrl : matrix, rref, iref, rload) {
    circuit_add_vdc(comps, ctrl, 1, 0, 5.0)
    circuit_add_resistor(comps, ctrl, 1, 2, rref)
    circuit_add_vccs(comps, ctrl, 3, 0, 2, 0, 1.0)
    circuit_add_vccs(comps, ctrl, 4, 0, 2, 0, 1.0)
    circuit_add_resistor(comps, ctrl, 4, 0, rload)
    circuit_add_idc(comps, ctrl, 2, 0, iref)
}

def ex_opamp_inverting(comps : matrix, ctrl : matrix, r1, rf, vin) {
    circuit_add_resistor(comps, ctrl, 1, 2, r1)
    circuit_add_resistor(comps, ctrl, 2, 3, rf)
    circuit_add_vcvs(comps, ctrl, 3, 0, 2, 0, 1e6)
    circuit_add_vdc(comps, ctrl, 1, 0, vin)
}

def ex_opamp_noninv(comps : matrix, ctrl : matrix, r1, rf, vin) {
    circuit_add_resistor(comps, ctrl, 2, 0, r1)
    circuit_add_resistor(comps, ctrl, 2, 3, rf)
    circuit_add_vcvs(comps, ctrl, 3, 0, 1, 2, 1e6)
    circuit_add_vdc(comps, ctrl, 1, 0, vin)
}

def ex_sallen_key_lpf(comps : matrix, ctrl : matrix, r, c, vin) {
    circuit_add_resistor(comps, ctrl, 1, 2, r)
    circuit_add_resistor(comps, ctrl, 2, 3, r)
    circuit_add_capacitor(comps, ctrl, 2, 0, c, 0.0)
    circuit_add_capacitor(comps, ctrl, 3, 4, c, 0.0)
    circuit_add_vcvs(comps, ctrl, 4, 0, 3, 0, 1.0)
    circuit_add_vdc(comps, ctrl, 1, 0, vin)
}

def ex_rlc_bpf(comps : matrix, ctrl : matrix, r, l, c, vin) {
    circuit_add_resistor(comps, ctrl, 1, 2, r)
    circuit_add_inductor(comps, ctrl, 2, 3, l, 0.0)
    circuit_add_capacitor(comps, ctrl, 3, 0, c, 0.0)
    circuit_add_resistor(comps, ctrl, 3, 0, 1.0)
    circuit_add_vdc(comps, ctrl, 1, 0, vin)
}

def ex_pulse_response(comps : matrix, ctrl : matrix, r, c, v_high, t_rise, t_width) {
    circuit_add_resistor(comps, ctrl, 1, 2, r)
    circuit_add_capacitor(comps, ctrl, 2, 0, c, 0.0)
    var vc = circuit_add_vac(comps, ctrl, 1, 0, 0.0, 1.0, 1.0, 0.0)
    circuit_set_param(comps, vc, 11.0, 1.0)
    circuit_set_param(comps, vc, 8.0, 0.0)
    circuit_set_param(comps, vc, 9.0, v_high)
    circuit_set_param(comps, vc, 10.0, 0.0)
    circuit_set_param(comps, vc, 5.0, t_rise)
    circuit_set_param(comps, vc, 6.0, t_rise)
    circuit_set_param(comps, vc, 7.0, t_width)
    circuit_set_param(comps, vc, 4.0, 1.0)
}

def ex_diode_rectifier(comps : matrix, ctrl : matrix, rload, cfilter, isat, vt) {
    circuit_add_vac(comps, ctrl, 1, 0, 0.0, 10.0, 60.0, 0.0)
    circuit_add_diode(comps, ctrl, 1, 2, isat, 1.0, vt)
    circuit_add_resistor(comps, ctrl, 2, 0, rload)
    circuit_add_capacitor(comps, ctrl, 2, 0, cfilter, 0.0)
}

def ex_bjt_ce_amplifier(comps : matrix, ctrl : matrix, r1, r2, rc, re, rload, bf, isat, vaf, vt, vcc) {
    circuit_add_vdc(comps, ctrl, 6, 0, vcc)
    circuit_add_resistor(comps, ctrl, 6, 1, rc)
    circuit_add_resistor(comps, ctrl, 6, 2, r1)
    circuit_add_resistor(comps, ctrl, 2, 0, r2)
    circuit_add_resistor(comps, ctrl, 3, 0, re)
    circuit_add_capacitor(comps, ctrl, 4, 2, 1e-6, 0.0)
    circuit_add_capacitor(comps, ctrl, 1, 5, 1e-6, 0.0)
    circuit_add_resistor(comps, ctrl, 5, 0, rload)
    circuit_add_npn(comps, ctrl, 1, 4, 3, bf, isat, vaf, vt)
}

def ex_differential_pair(comps : matrix, ctrl : matrix, rc, ree, rload, bf, isat, vaf, vt, vcc, vee) {
    circuit_add_vdc(comps, ctrl, 6, 0, vcc)
    circuit_add_vdc(comps, ctrl, 7, 0, vee)
    circuit_add_resistor(comps, ctrl, 6, 1, rc)
    circuit_add_resistor(comps, ctrl, 6, 2, rc)
    circuit_add_resistor(comps, ctrl, 3, 7, ree)
    circuit_add_resistor(comps, ctrl, 1, 0, rload)
    circuit_add_npn(comps, ctrl, 1, 4, 3, bf, isat, vaf, vt)
    circuit_add_npn(comps, ctrl, 2, 5, 3, bf, isat, vaf, vt)
    circuit_add_vdc(comps, ctrl, 4, 0, 0.0)
    circuit_add_vdc(comps, ctrl, 5, 0, 0.0)
}

def ex_cascade_amplifier(comps : matrix, ctrl : matrix, r1, r2, rc1, re1, rc2, re2, rload, bf, isat, vaf, vt, vcc) {
    circuit_add_vdc(comps, ctrl, 10, 0, vcc)
    circuit_add_resistor(comps, ctrl, 10, 1, rc1)
    circuit_add_resistor(comps, ctrl, 10, 2, r1)
    circuit_add_resistor(comps, ctrl, 10, 3, rc2)
    circuit_add_resistor(comps, ctrl, 2, 0, r2)
    circuit_add_resistor(comps, ctrl, 4, 0, re1)
    circuit_add_resistor(comps, ctrl, 5, 0, re2)
    circuit_add_capacitor(comps, ctrl, 6, 2, 1e-6, 0.0)
    circuit_add_capacitor(comps, ctrl, 1, 7, 1e-6, 0.0)
    circuit_add_resistor(comps, ctrl, 7, 0, rload)
    circuit_add_npn(comps, ctrl, 1, 6, 4, bf, isat, vaf, vt)
    circuit_add_npn(comps, ctrl, 3, 8, 5, bf, isat, vaf, vt)
    circuit_add_vdc(comps, ctrl, 8, 1, 0.0)
}

def ex_wein_bridge_osc(comps : matrix, ctrl : matrix, r, c, rf, rg) {
    circuit_add_resistor(comps, ctrl, 1, 2, r)
    circuit_add_capacitor(comps, ctrl, 2, 0, c, 1.0)
    circuit_add_capacitor(comps, ctrl, 1, 3, c, 0.0)
    circuit_add_resistor(comps, ctrl, 3, 0, r)
    circuit_add_resistor(comps, ctrl, 4, 0, rg)
    circuit_add_resistor(comps, ctrl, 4, 5, rf)
    circuit_add_vcvs(comps, ctrl, 5, 0, 3, 4, 1e6)
    circuit_add_vdc(comps, ctrl, 1, 0, 0.0)
}

def ex_mosfet_cs_amplifier(comps : matrix, ctrl : matrix, rd, rg1, rg2, rs, rload, kp, vto, lambda_val, vdd) {
    circuit_add_vdc(comps, ctrl, 10, 0, vdd)
    circuit_add_resistor(comps, ctrl, 10, 1, rd)
    circuit_add_resistor(comps, ctrl, 10, 2, rg1)
    circuit_add_resistor(comps, ctrl, 2, 0, rg2)
    circuit_add_resistor(comps, ctrl, 3, 0, rs)
    circuit_add_capacitor(comps, ctrl, 4, 2, 1e-6, 0.0)
    circuit_add_capacitor(comps, ctrl, 1, 5, 1e-6, 0.0)
    circuit_add_resistor(comps, ctrl, 5, 0, rload)
    circuit_add_nmos(comps, ctrl, 1, 4, 3, 0, kp, vto, lambda_val)
}
