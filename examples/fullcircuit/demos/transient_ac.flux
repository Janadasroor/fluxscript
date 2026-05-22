# FullCircuit — Complete Engine Demo (DC, Transient, AC, Sensitivity, Noise)
import circuit
import mna
import dcsolve
import trsim
import acswp
import analysis
import examples

# -------------------------------------------------------
# Transient Analysis: RC Low-Pass with Pulse Input
#   V1=5V pulse, R=1k, C=1uF
#   Pulse: 0→5V, t_rise=0.1ms, pw=2ms, per=5ms
#   Expected: V(2) charges exponentially toward 5V with τ=1ms
# -------------------------------------------------------
def demo_transient() {
    var max_comps = 100
    var comps = matrix_zeros(max_comps, 12)
    var ctrl = circuit_create(2, max_comps)

    # Resistor R=1k between node 1 and 2
    circuit_add_resistor(comps, ctrl, 1, 2, 1000.0)
    # Capacitor C=1uF between node 2 and 0
    circuit_add_capacitor(comps, ctrl, 2, 0, 1e-6, 0.0)
    # DC voltage source 5V between node 1 and 0
    var src_idx = circuit_add_vdc(comps, ctrl, 1, 0, 5.0)
    # Configure it as a pulse source (col[11]=1)
    #   col[5]=t_rise=0.1ms, col[6]=t_fall=0.1ms,
    #   col[7]=pw=2ms, col[8]=v1=0, col[9]=v2=5, col[10]=td=0
    circuit_set_param(comps, src_idx, 5, 0.0001)
    circuit_set_param(comps, src_idx, 6, 0.0001)
    circuit_set_param(comps, src_idx, 7, 0.002)
    circuit_set_param(comps, src_idx, 8, 0.0)
    circuit_set_param(comps, src_idx, 9, 5.0)
    circuit_set_param(comps, src_idx, 10, 0.0)
    circuit_set_param(comps, src_idx, 11, 1.0)

    var t_start = 0.0
    var t_stop = 0.005
    var h = 0.000025
    var results = tr_solve(comps, ctrl, t_start, t_stop, h)

    println("Transient: RC Low-Pass (R=1k, C=1uF, tau=1ms)")
    println("  Pulse: 0->5V, t_rise=0.1ms, pw=2ms, per=5ms")
    println("")
    println("  t(ms) Vsrc(V) Vcap(V)")
    var ri = 0.0
    var ns = (t_stop - t_start) / h
    var step = floor(ns / 5.0)
    if (step < 1.0) { step = 1.0 }
    while (ri <= ns) {
        var t = matrix_get(results, ri, 0) * 1000.0
        var vsrc = matrix_get(results, ri, 2)
        var vcap = matrix_get(results, ri, 3)
        print("  "); print(t); print("  "); print(vsrc); print("  "); println(vcap)
        ri = ri + step
    }
    println("")
    # At t=1ms (tau=1ms), V(cap) should be approx 5*(1-1/e) approx 3.16V
    var ti = 0.001 / h
    if (ti >= 0.0 && ti <= ns) {
        var v_at_tau = matrix_get(results, ti, 3)
        print("  At t=tau=1ms: V(cap) = "); print(v_at_tau); println(" V (expected ~ 3.16 V)")
    }
    println("")
}

# -------------------------------------------------------
# AC Analysis: RC Low-Pass Frequency Sweep
#   R=1k, C=1uF  → fc = 1/(2πRC) ≈ 159 Hz
#   Sweep 10 Hz – 100 kHz
# -------------------------------------------------------
def demo_ac() {
    var max_comps = 100
    var comps = matrix_zeros(max_comps, 12)
    var ctrl = circuit_create(2, max_comps)

    circuit_add_resistor(comps, ctrl, 1, 2, 1000.0)
    circuit_add_capacitor(comps, ctrl, 2, 0, 1e-6, 0.0)
    circuit_add_vac(comps, ctrl, 1, 0, 0.0, 1.0, 1000.0, 0.0)

    # DC operating point (V is needed by ac_sweep for nonlinear devices)
    var V = dc_solve(comps, ctrl, 100, 1e-9)

    var f_start = 10.0
    var f_stop = 100000.0
    var npts = 30.0
    var results = ac_sweep(comps, ctrl, f_start, f_stop, npts, V)

    println("AC Sweep: RC Low-Pass (R=1k, C=1uF, fc ~ 159 Hz)")
    println("")
    println("  f(Hz)     |V(2)|(V)  gain(dB)")
    var log_fs = log(f_start)
    var log_fe = log(f_stop)
    var step = 0.0
    if (npts > 1.0) { step = (log_fe - log_fs) / npts }
    var ri = 0.0
    while (ri <= npts) {
        var freq = exp(log_fs + ri * step)
        var sol = ac_build(comps, ctrl, freq, V)
        var dim = matrix_get(sol, 0, 0)
        var v2r = matrix_get(sol, 2.0 + mna_ndx(2), 0)
        var v2i = matrix_get(sol, 2.0 + dim + mna_ndx(2), 0)
        var mag = sqrt(v2r * v2r + v2i * v2i)
        var db = 0.0
        if (mag > 1e-15) { db = 20.0 * log(mag) / log(10.0) }
        print("  "); print(freq); print("  "); print(mag); print("  "); println(db)
        ri = ri + 1.0
    }
    println("")
}

# -------------------------------------------------------
# Analysis: Sensitivity, Power, Noise
#   Voltage Divider (R1=1k, R2=2k, Vin=5V)
#   V(2) = 5 * R2/(R1+R2) = 3.333V
# -------------------------------------------------------
def demo_analysis() {
    var max_comps = 100
    var comps = matrix_zeros(max_comps, 12)
    var ctrl = circuit_create(2, max_comps)

    circuit_add_resistor(comps, ctrl, 1, 2, 1000.0)
    circuit_add_resistor(comps, ctrl, 2, 0, 2000.0)
    circuit_add_vdc(comps, ctrl, 1, 0, 5.0)

    # DC operating point
    var V = dc_solve(comps, ctrl, 100, 1e-9)
    println("Analysis: Voltage Divider (R1=1k, R2=2k, Vin=5V)")
    println("")

    # Node voltages
    print("  V(1) = "); print(matrix_get(V, 1, 0)); println(" V  (expected 5.0)")
    print("  V(2) = "); print(matrix_get(V, 2, 0)); println(" V  (expected 3.333)")
    println("")

    # Power dissipation (resistors only; source power needs branch currents)
    var Pd = an_power_dissipation(comps, ctrl, V)
    print("  Resistor power dissipation = "); print(Pd); println(" W")
    print("  V(1)-V(2) = ")
    print(matrix_get(V, 1, 0) - matrix_get(V, 2, 0))
    println(" V across R1, I = V(1)/3000 = ")
    var i = matrix_get(V, 1, 0) / 3000.0
    print(i); println(" A")
    print("  P(R1) = I^2*R1 = ")
    print(i * i * 1000.0); println(" W")
    print("  P(R2) = I^2*R2 = ")
    print(i * i * 2000.0); println(" W")
    println("")

    # Sensitivity of V(2) to R1 (comp 0, 1% delta)
    var S1 = an_sensitivity(comps, ctrl, 0.0, 0.01, 100, 1e-9)
    print("  dV(2)/dR1 (1% pert) = "); print(matrix_get(S1, 2, 0)); println(" V/Ω")
    # Analytical: dV2/dR1 = -Vin * R2 / (R1+R2)^2 = -5*2000/3000^2 = -0.001111
    println("")

    # Sensitivity of V(2) to R2 (comp 1, 1% delta)
    var S2 = an_sensitivity(comps, ctrl, 1.0, 0.01, 100, 1e-9)
    print("  dV(2)/dR2 (1% pert) = "); print(matrix_get(S2, 2, 0)); println(" V/Ω")
    # Analytical: dV2/dR2 = Vin * R1 / (R1+R2)^2 = 5*1000/3000^2 = 0.000556
    println("")

    # Resistor thermal noise density at 300K
    var en = an_resistor_noise_density(1000.0, 300.0)
    print("  Thermal noise (R1=1k, T=300K) = ")
    print(en); println(" V/√Hz")
    print("  Expected: sqrt(4*kB*T*R) = ")
    print(sqrt(4.0 * 1.380649e-23 * 300.0 * 1000.0))
    println(" V/√Hz")
    println("")

    # SNR for 5V DC with 1k resistor noise (10 kHz BW)
    var vn_rms = en * sqrt(10000.0)
    var snr = an_signal_to_noise(5.0, vn_rms)
    print("  SNR (5V signal, 10 kHz BW) = "); print(snr); println(" dB")
    println("")
}

# -------------------------------------------------------
# Run all demos
# -------------------------------------------------------
def main() {
    println("========================================")
    println("  FullCircuit — Circuit Simulation Engine")
    println("========================================")
    println("")
    demo_transient()
    println("----------------------------------------")
    println("")
    demo_ac()
    println("----------------------------------------")
    println("")
    demo_analysis()
    println("========================================")
    println("  All analyses complete.")
}

main()
