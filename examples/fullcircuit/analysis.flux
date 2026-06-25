# FullCircuit — Sensitivity, Monte Carlo, Noise Analysis
import mna
import dcsolve

def an_kB() -> Double { 1.380649e-23 }

def an_sensitivity(comps : matrix, ctrl : matrix, comp_idx: Double, delta: Double, max_iter: Double, tol: Double) -> matrix {
    var N = matrix_get(ctrl, 0.0, 0.0)
    var Vnom = dc_solve(comps, ctrl, max_iter, tol)
    var orig_val = matrix_get(comps, comp_idx, 4.0)
    matrix_set(comps, comp_idx, 4.0, orig_val * (1.0 + delta))
    var Vpert = dc_solve(comps, ctrl, max_iter, tol)
    matrix_set(comps, comp_idx, 4.0, orig_val)
    var S = matrix_zeros(N + 1.0, 1)
    var vi = 0.0
    while (vi <= N) {
        var dv = matrix_get(Vpert, vi, 0) - matrix_get(Vnom, vi, 0)
        matrix_set(S, vi, 0, dv / (orig_val * delta))
        vi = vi + 1.0
    }
    S
}

def an_sensitivity_all(comps : matrix, ctrl : matrix, delta: Double, max_iter: Double, tol: Double) -> matrix {
    var N = matrix_get(ctrl, 0.0, 0.0)
    var nc = matrix_get(ctrl, 1.0, 0.0)
    var Vnom = dc_solve(comps, ctrl, max_iter, tol)
    var S_all = matrix_zeros(nc, N + 1.0)
    var ci = 0.0
    while (ci < nc) {
        var tc = matrix_get(comps, ci, 0.0)
        if (tc == 0.0 || tc == 1.0 || tc == 2.0) {
            var orig = matrix_get(comps, ci, 4.0)
            matrix_set(comps, ci, 4.0, orig * (1.0 + delta))
            var Vp = dc_solve(comps, ctrl, max_iter, tol)
            matrix_set(comps, ci, 4.0, orig)
            var vi = 0.0
            while (vi <= N) {
                var dv = matrix_get(Vp, vi, 0) - matrix_get(Vnom, vi, 0)
                matrix_set(S_all, ci, vi, dv / (orig * delta))
                vi = vi + 1.0
            }
        }
        ci = ci + 1.0
    }
    S_all
}

def an_monte_carlo(comps : matrix, ctrl : matrix, n_runs: Double, sigma: Double, max_iter: Double, tol: Double) -> matrix {
    var N = matrix_get(ctrl, 0.0, 0.0)
    var nc = matrix_get(ctrl, 1.0, 0.0)
    var results = matrix_zeros(n_runs, N + 1.0)
    var orig_vals = matrix_zeros(nc, 1)
    var ci = 0.0
    while (ci < nc) {
        matrix_set(orig_vals, ci, 0, matrix_get(comps, ci, 4.0))
        ci = ci + 1.0
    }
    var run = 0.0
    while (run < n_runs) {
        ci = 0.0
        while (ci < nc) {
            var tc = matrix_get(comps, ci, 0.0)
            var orig = matrix_get(orig_vals, ci, 0)
            if (tc == 0.0 || tc == 1.0 || tc == 2.0) {
                matrix_set(comps, ci, 4.0, orig * (1.0 + sigma * randn()))
            }
            ci = ci + 1.0
        }
        var V = dc_solve(comps, ctrl, max_iter, tol)
        matrix_set(results, run, 0, run)
        var vi = 1.0
        while (vi <= N) {
            matrix_set(results, run, vi, matrix_get(V, vi, 0))
            vi = vi + 1.0
        }
        run = run + 1.0
    }
    ci = 0.0
    while (ci < nc) {
        matrix_set(comps, ci, 4.0, matrix_get(orig_vals, ci, 0))
        ci = ci + 1.0
    }
    results
}

def an_mc_statistics(results : matrix, nd: Double) -> matrix {
    var n_runs = matrix_rows(results)
    var sum_v = 0.0
    var sum_v2 = 0.0
    var ri = 0.0
    while (ri < n_runs) {
        var v = matrix_get(results, ri, nd)
        sum_v = sum_v + v
        sum_v2 = sum_v2 + v * v
        ri = ri + 1.0
    }
    var mean = sum_v / n_runs
    var variance = sum_v2 / n_runs - mean * mean
    if (variance < 0.0) { variance = 0.0 }
    var stddev = sqrt(variance)
    var stats = matrix_zeros(4, 1)
    matrix_set(stats, 0, 0, n_runs)
    matrix_set(stats, 1, 0, mean)
    matrix_set(stats, 2, 0, stddev)
    matrix_set(stats, 3, 0, variance)
    stats
}

def an_resistor_noise_density(r: Double, tp: Double) -> Double {
    sqrt(4.0 * an_kB() * tp * r)
}

def an_noise_bandwidth(rms_v: Double, total_r: Double, tp: Double) -> Double {
    var psd = an_resistor_noise_density(total_r, tp)
    if (psd > 1e-30) { (rms_v / psd) * (rms_v / psd) } else { 0.0 }
}

def an_signal_to_noise(signal_rms: Double, noise_rms: Double) -> Double {
    if (noise_rms > 1e-30) { 20.0 * log(signal_rms / noise_rms) } else { 0.0 }
}

def an_thd_estimate(harmonics : matrix, n_harmonics: Double) -> Double {
    if (n_harmonics < 2.0) { 0.0 }
    var fund = abs(matrix_get(harmonics, 0, 0))
    if (fund < 1e-15) { 0.0 }
    var dist_power = 0.0
    var hi = 1.0
    while (hi < n_harmonics) {
        var h = abs(matrix_get(harmonics, hi, 0))
        dist_power = dist_power + h * h
        hi = hi + 1.0
    }
    sqrt(dist_power) / fund * 100.0
}

def an_power_dissipation(comps : matrix, ctrl : matrix, V : matrix) -> Double {
    var nc = matrix_get(ctrl, 1.0, 0.0)
    var N = matrix_get(ctrl, 0.0, 0.0)
    var total_p = 0.0
    var ci = 0.0
    while (ci < nc) {
        var tc = matrix_get(comps, ci, 0.0)
        var np = matrix_get(comps, ci, 1.0)
        var nm = matrix_get(comps, ci, 2.0)
        var vnp = 0.0; var vnm = 0.0
        if (np > 0.0 && np <= N) { vnp = matrix_get(V, np, 0) }
        if (nm > 0.0 && nm <= N) { vnm = matrix_get(V, nm, 0) }
        var vd = vnp - vnm
        if (tc == 0.0) {
            var rv = matrix_get(comps, ci, 4.0)
            if (rv > 1e-15) { total_p = total_p + vd * vd / rv }
        } else if (tc == 1.0) {
            # Capacitor: no DC power dissipation
        } else if (tc == 2.0) {
            # Inductor: no DC power dissipation (ideal)
        } else if (tc == 3.0 || tc == 5.0) {
            # Voltage source: P = vd * branch_current
            # (needs full MNA sol; omitted here — use mna_dc_solve for accuracy)
        } else if (tc == 4.0 || tc == 6.0) {
            var iv = matrix_get(comps, ci, 4.0)
            total_p = total_p + vd * iv
        } else if (tc == 7.0 || tc == 8.0) {
            # VCVS/VCCS: power computed from controlled source
        }
        ci = ci + 1.0
    }
    total_p
}
