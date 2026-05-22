# FluxScript Standard Library Tests
# Run: flux --entry=<test_name> --cache=false tests/flux/test_stdlib.flux
# Tests require stdlib/*.flux files to be available via import path

# ============================================================
# math.flux tests
# ============================================================

def test_math_clamp() {
    clamp(5.0, 0.0, 10.0) == 5.0 &&
    clamp(-1.0, 0.0, 10.0) == 0.0 &&
    clamp(15.0, 0.0, 10.0) == 10.0
}

def test_math_lerp() {
    lerp(0.0, 10.0, 0.5) == 5.0 &&
    lerp(0.0, 10.0, 0.0) == 0.0 &&
    lerp(0.0, 10.0, 1.0) == 10.0
}

def test_math_rad2deg_deg2rad() {
    rad2deg(0.0) == 0.0 &&
    deg2rad(0.0) == 0.0 &&
    rad2deg(3.141592653589793) > 179.9 && rad2deg(3.141592653589793) < 180.1
}

def test_math_sign() {
    sign(5.0) == 1.0 && sign(-3.0) == -1.0 && sign(0.0) == 0.0
}

def test_math_is_close() {
    is_close(1.0, 1.0) == 1.0 &&
    is_close(1.0, 1.000000001) == 0.0 &&
    is_close(1.0, 1.1) == 0.0
}

def test_math_step() {
    step(-1.0) == 0.0 && step(0.0) == 1.0 && step(5.0) == 1.0
}

def test_math_factorial() {
    factorial(0.0) == 1.0 &&
    factorial(1.0) == 1.0 &&
    factorial(5.0) == 120.0
}

def test_math_gcd_lcm() {
    gcd(12.0, 8.0) == 4.0 &&
    lcm(12.0, 8.0) == 24.0 &&
    gcd(7.0, 13.0) == 1.0
}

def test_math_map_range() {
    map_range(0.5, 0.0, 1.0, 0.0, 100.0) == 50.0 &&
    map_range(0.0, 0.0, 1.0, -10.0, 10.0) == -10.0
}

def test_math_smoothstep() {
    smoothstep(0.0) == 0.0 &&
    smoothstep(1.0) == 1.0 &&
    smoothstep(0.5) == 0.5
}

def test_math_round_to() {
    round_to(3.14159, 2.0) == 3.14 &&
    round_to(3.14159, 1.0) == 3.1
}

def test_math_frac() {
    frac(3.5) == 0.5 &&
    frac(-1.2) > 0.79 && frac(-1.2) < 0.81
}

def test_math_wrap() {
    wrap(5.0, 0.0, 10.0) == 5.0 &&
    wrap(12.0, 0.0, 10.0) == 2.0 &&
    wrap(-3.0, 0.0, 10.0) == 7.0
}

def test_math_pingpong() {
    pingpong(0.0, 0.0, 10.0) == 0.0 &&
    pingpong(5.0, 0.0, 10.0) == 5.0 &&
    pingpong(10.0, 0.0, 10.0) == 10.0
}

def test_math_inverse_lerp() {
    inverse_lerp(5.0, 0.0, 10.0) == 0.5 &&
    inverse_lerp(0.0, 0.0, 10.0) == 0.0 &&
    inverse_lerp(10.0, 0.0, 10.0) == 1.0
}

def test_math_sigmoid() {
    sigmoid(0.0) == 0.5 &&
    sigmoid(-100.0) < 0.001 &&
    sigmoid(100.0) > 0.999
}

def test_math_normalize_angle() {
    normalize_angle(0.0) == 0.0 &&
    normalize_angle(6.283185307179586) < 1e-9 &&
    normalize_angle(-1.0) > 5.0
}

def test_math_normalize_angle_sym() {
    normalize_angle_sym(0.0) == 0.0 &&
    abs(normalize_angle_sym(pi())) > 3.0 &&
    abs(normalize_angle_sym(pi())) < 3.2 &&
    is_close(abs(normalize_angle_sym(pi())), pi())
}

def test_math_softplus() {
    softplus(-100.0) < 0.001 &&
    softplus(0.0) > 0.69 && softplus(0.0) < 0.70 &&
    softplus(100.0) > 99.9
}

def test_math_clamp01() {
    clamp01(0.5) == 0.5 && clamp01(-0.5) == 0.0 && clamp01(1.5) == 1.0
}

def test_math_relu() {
    relu(-5.0) == 0.0 && relu(0.0) == 0.0 && relu(5.0) == 5.0
}

def test_math_logit() {
    logit(0.5) == 0.0 && logit(0.73105857863) > 0.99 && logit(0.73105857863) < 1.01
}

def test_math_avg() {
    avg(3.0, 5.0) == 4.0 && avg(-2.0, 2.0) == 0.0
}

def test_math_comb() {
    comb(5.0, 0.0) == 1.0 && comb(5.0, 2.0) == 10.0 && comb(5.0, 5.0) == 1.0
}

def test_math_perm() {
    perm(5.0, 0.0) == 1.0 && perm(5.0, 2.0) == 20.0 && perm(5.0, 5.0) == 120.0
}

def test_math_reflect() {
    reflect(1.0, 0.0) == 1.0 && reflect(1.0, 1.0) == -1.0
}

# ============================================================
# trig.flux tests
# ============================================================

def test_trig_sec() {
    sec(0.0) == 1.0 &&
    abs(sec(45.0)) - abs(1.0 / cos(45.0)) < 1e-12
}

def test_trig_csc() {
    csc(pi() / 2.0) == 1.0 &&
    abs(csc(pi() / 2.0) - 1.0 / sin(pi() / 2.0)) < 1e-12
}

def test_trig_cot() {
    cot(pi() / 4.0) > 0.99 && cot(pi() / 4.0) < 1.01
}

def test_trig_sinc() {
    sinc(0.0) == 1.0 && sinc(1.0) > 0.84 && sinc(1.0) < 0.85
}

def test_trig_asec() {
    asec(1.0) == 0.0 && asec(2.0) > 1.04 && asec(2.0) < 1.05
}

def test_trig_acsc() {
    acsc(1.0) == pi() / 2.0 && acsc(2.0) > 0.52 && acsc(2.0) < 0.53
}

def test_trig_acot() {
    acot(1.0) > 0.78 && acot(1.0) < 0.79 && acot(0.0) > 1.57 && acot(0.0) < 1.58
}

def test_trig_sech() {
    sech(0.0) == 1.0 && sech(10.0) < 0.001
}

def test_trig_csch() {
    csch(0.5) > 1.91 && csch(0.5) < 1.92 && csch(10.0) < 0.001
}

def test_trig_coth() {
    coth(1.0) > 1.31 && coth(1.0) < 1.32 && coth(10.0) > 0.99 && coth(10.0) < 1.01
}

def test_trig_degrees_radians() {
    degrees(pi()) == 180.0 && radians(180.0) > 3.141 && radians(180.0) < 3.142
}

def test_trig_haversine_vercosine() {
    haversine(0.0) == 0.0 && haversine(pi()) == 1.0 &&
    vercosine(0.0) == 1.0 && vercosine(pi()) == 0.0
}

# ============================================================
# array.flux tests
# ============================================================

def test_array_linspace() {
    var v = linspace(0.0, 10.0, 5.0)
    matrix_rows(v) == 5.0 &&
    matrix_get(v, 0, 0) == 0.0 &&
    matrix_get(v, 4, 0) == 10.0 &&
    matrix_get(v, 2, 0) == 5.0
}

def test_array_logspace() {
    var v = logspace(0.0, 2.0, 3.0)
    matrix_rows(v) == 3.0 &&
    matrix_get(v, 0, 0) == 1.0 &&
    matrix_get(v, 2, 0) == 100.0
}

def test_array_arange() {
    var v = arange(0.0, 5.0, 1.0)
    matrix_rows(v) == 5.0 &&
    matrix_get(v, 0, 0) == 0.0 &&
    matrix_get(v, 4, 0) == 4.0
}

def test_array_flatten() {
    var m = matrix_zeros(2, 2)
    m[0,0] = 1.0; m[0,1] = 2.0
    m[1,0] = 3.0; m[1,1] = 4.0
    var f = flatten(m)
    matrix_rows(f) == 4.0 &&
    matrix_get(f, 0, 0) == 1.0 &&
    matrix_get(f, 3, 0) == 4.0
}

def test_array_reshape() {
    var v = arange(0.0, 6.0, 1.0)
    var m = reshape(v, 3.0, 2.0)
    matrix_rows(m) == 3.0 && matrix_cols(m) == 2.0 &&
    matrix_get(m, 2, 1) == 5.0
}

def test_array_diff() {
    var v = arange(0.0, 5.0, 1.0)
    var d = diff(v)
    matrix_get(d, 0, 0) == 1.0 && matrix_get(d, 1, 0) == 1.0
}

def test_array_cumsum() {
    var v = matrix_zeros(3, 1)
    v[0,0]=1.0; v[1,0]=2.0; v[2,0]=3.0
    var c = cumsum(v)
    matrix_get(c, 0, 0) == 1.0 &&
    matrix_get(c, 1, 0) == 3.0 &&
    matrix_get(c, 2, 0) == 6.0
}

def test_array_cumprod() {
    var v = matrix_zeros(3, 1)
    v[0,0]=2.0; v[1,0]=3.0; v[2,0]=4.0
    var c = cumprod(v)
    matrix_get(c, 0, 0) == 2.0 &&
    matrix_get(c, 1, 0) == 6.0 &&
    matrix_get(c, 2, 0) == 24.0
}

def test_array_argsort() {
    var v = matrix_zeros(3, 1)
    v[0,0]=3.0; v[1,0]=1.0; v[2,0]=2.0
    var idx = argsort(v)
    matrix_get(idx, 0, 0) == 1.0 &&
    matrix_get(idx, 1, 0) == 2.0 &&
    matrix_get(idx, 2, 0) == 0.0
}

def test_array_unique() {
    var v = matrix_zeros(4, 1)
    v[0,0]=1.0; v[1,0]=2.0; v[2,0]=1.0; v[3,0]=3.0
    var u = unique(v)
    matrix_get(u, 0, 0) == 1.0 &&
    matrix_get(u, 1, 0) == 2.0 &&
    matrix_get(u, 2, 0) == 3.0
}

def test_array_pad_zeros() {
    var m = matrix_ones(2, 2)
    var p = pad_zeros(m, 1.0, 1.0)
    matrix_rows(p) == 4.0 && matrix_cols(p) == 4.0 &&
    matrix_get(p, 0, 0) == 0.0 &&
    matrix_get(p, 1, 1) == 1.0
}

def test_array_flipud() {
    var m = matrix_zeros(2, 1)
    m[0,0]=1.0; m[1,0]=2.0
    var f = flipud(m)
    matrix_get(f, 0, 0) == 2.0 && matrix_get(f, 1, 0) == 1.0
}

def test_array_fliplr() {
    var m = matrix_zeros(1, 2)
    m[0,0]=1.0; m[0,1]=2.0
    var f = fliplr(m)
    matrix_get(f, 0, 0) == 2.0 && matrix_get(f, 0, 1) == 1.0
}

def test_array_rot90() {
    var m = matrix_zeros(2, 1)
    m[0,0]=1.0; m[1,0]=2.0
    var r = rot90(m)
    matrix_rows(r) == 1.0 && matrix_cols(r) == 2.0
}

def test_array_triu_tril() {
    var m = matrix_zeros(2, 2)
    m[0,0]=1.0; m[0,1]=2.0; m[1,0]=3.0; m[1,1]=4.0
    var u = triu(m); var l = tril(m)
    matrix_get(u, 1, 0) == 0.0 &&
    matrix_get(l, 0, 1) == 0.0 &&
    matrix_get(u, 0, 1) == 2.0 &&
    matrix_get(l, 1, 0) == 3.0
}

def test_array_repmat() {
    var m = matrix_ones(1, 1)
    m[0,0] = 5.0
    var r = repmat(m, 2.0, 3.0)
    matrix_rows(r) == 2.0 && matrix_cols(r) == 3.0 &&
    matrix_get(r, 1, 2) == 5.0
}

def test_array_trapz() {
    var x = linspace(0.0, 1.0, 100.0)
    var y = matrix_zeros(100, 1)
    var i = 0.0
    while(i < 100.0) {
        var xi = matrix_get(x, i, 0)
        matrix_set(y, i, 0, xi * xi)
        i = i + 1.0
    }
    var integral = trapz(x, y)
    integral > 0.32 && integral < 0.34
}

def test_array_nonzero() {
    var m = matrix_zeros(2, 2)
    m[0,1] = 1.0
    m[1,0] = 2.0
    var nz = nonzero(m)
    matrix_rows(nz) == 4.0 &&
    matrix_get(nz, 0, 1) == 1.0 &&
    matrix_get(nz, 1, 0) == 1.0 &&
    matrix_get(nz, 1, 1) == 0.0
}

def test_array_clip() {
    var m = matrix_zeros(2, 2)
    m[0,0] = -5.0; m[1,1] = 15.0
    var c = clip(m, 0.0, 10.0)
    matrix_get(c, 0, 0) == 0.0 &&
    matrix_get(c, 1, 1) == 10.0
}

def test_array_squeeze() {
    var m1 = matrix_zeros(1, 1)
    m1[0,0] = 42.0
    var s1 = squeeze(m1)
    var m2 = matrix_zeros(1, 3)
    m2[0,0]=1.0; m2[0,1]=2.0; m2[0,2]=3.0
    var s3 = squeeze(m2)
    var m3 = matrix_zeros(3, 1)
    m3[0,0]=1.0; m3[1,0]=2.0; m3[2,0]=3.0
    var s4 = squeeze(m3)
    var ok = matrix_get(s1, 0, 0) == 42.0 &&
        matrix_rows(s3) == 1.0 && matrix_cols(s3) == 3.0 &&
        matrix_get(s3, 0, 0) == 1.0 && matrix_get(s3, 0, 2) == 3.0 &&
        matrix_rows(s4) == 3.0 && matrix_cols(s4) == 1.0 &&
        matrix_get(s4, 0, 0) == 1.0 && matrix_get(s4, 2, 0) == 3.0
    ok
}

def test_array_sort() {
    var v = matrix_zeros(3, 1)
    v[0,0]=3.0; v[1,0]=1.0; v[2,0]=2.0
    var s = sort(v)
    matrix_get(s, 0, 0) == 1.0 &&
    matrix_get(s, 1, 0) == 2.0 &&
    matrix_get(s, 2, 0) == 3.0
}

def test_array_replace() {
    var v = matrix_ones(2, 1)
    var r = replace(v, 1.0, 99.0)
    matrix_get(r, 0, 0) == 99.0
}

def test_array_count_eq() {
    var v = matrix_zeros(3, 1)
    v[0,0]=1.0; v[1,0]=1.0; v[2,0]=2.0
    count_eq(v, 1.0) == 2.0
}

def test_array_find_eq() {
    var v = matrix_zeros(2, 1)
    v[0,0]=1.0; v[1,0]=2.0
    var f = find_eq(v, 1.0)
    matrix_rows(f) == 2.0 &&
    matrix_get(f, 0, 0) == 0.0 &&
    matrix_get(f, 0, 1) == 0.0
}

def test_stats_variance_std() {
    var v = matrix_zeros(3, 1)
    v[0,0]=1.0; v[1,0]=2.0; v[2,0]=3.0
    variance(v) > 0.66 && variance(v) < 0.67 &&
    std(v) > 0.81 && std(v) < 0.82
}

def test_stats_geometric_mean() {
    var v = matrix_zeros(2, 1)
    v[0,0]=1.0; v[1,0]=100.0
    var g = geometric_mean(v)
    g > 9.99 && g < 10.01
}

def test_stats_harmonic_mean() {
    var v = matrix_zeros(2, 1)
    v[0,0]=2.0; v[1,0]=3.0
    harmonic_mean(v) > 2.39 && harmonic_mean(v) < 2.41
}

def test_stats_min_max_element() {
    var v = matrix_zeros(3, 1)
    v[0,0]=5.0; v[1,0]=-3.0; v[2,0]=9.0
    min_element(v) == -3.0 && max_element(v) == 9.0
}

def test_stats_median() {
    var v = matrix_zeros(5, 1)
    v[0,0]=3.0; v[1,0]=1.0; v[2,0]=4.0; v[3,0]=1.0; v[4,0]=5.0
    median(v) == 3.0
}

def test_stats_iqr() {
    var v = matrix_zeros(7, 1)
    v[0,0]=1.0; v[1,0]=2.0; v[2,0]=3.0; v[3,0]=4.0; v[4,0]=5.0; v[5,0]=6.0; v[6,0]=7.0
    iqr(v) == 4.0
}

def test_stats_normalize_minmax() {
    var v = matrix_zeros(4, 1)
    v[0,0]=0.0; v[1,0]=5.0; v[2,0]=10.0; v[3,0]=15.0
    var n = normalize_minmax(v)
    matrix_get(n, 0, 0) == 0.0 &&
    is_close(matrix_get(n, 1, 0), 1.0/3.0) &&
    matrix_get(n, 3, 0) == 1.0
}

def test_stats_range_val() {
    var v = matrix_zeros(3, 1)
    v[0,0]=-5.0; v[1,0]=0.0; v[2,0]=10.0
    range_val(v) == 15.0
}

def test_range_simple() {
    var r = 0:4
    matrix_rows(r) == 5.0 &&
    matrix_get(r, 0, 0) == 0.0 &&
    matrix_get(r, 4, 0) == 4.0
}

def test_range_step() {
    var r = 0:2:10
    matrix_rows(r) == 6.0 &&
    matrix_get(r, 0, 0) == 0.0 &&
    matrix_get(r, 5, 0) == 10.0
}

def test_dowhile_basic() {
    var i = 0.0
    var s = 0.0
    do {
        s = s + i
        i = i + 1.0
    } while(i < 5.0)
    s == 10.0
}

def test_dowhile_once() {
    var i = 0.0
    do {
        i = i + 1.0
    } while(false)
    i == 1.0
}

def test_preprocessor_ifdef() {
    var x = 0.0
#ifdef __FLUX__
    x = 1.0
#endif
    x == 1.0
}

def test_preprocessor_ifndef() {
    var x = 0.0
#ifndef __UNDEFINED_MACRO_XYZ__
    x = 1.0
#endif
    x == 1.0
}

def test_preprocessor_else() {
    var x = 0.0
#ifdef __UNDEFINED_MACRO_XYZ__
    x = 1.0
#else
    x = 2.0
#endif
    x == 2.0
}

def test_preprocessor_define() {
#define TEST_MACRO 42.0
    var x = TEST_MACRO
    x == 42.0
}

def test_signal_hann() {
    var w = hann(4)
    matrix_rows(w) == 4.0 &&
    matrix_get(w, 0, 0) == 0.0 &&
    matrix_get(w, 1, 0) > 0.5 &&
    matrix_get(w, 3, 0) == 0.0
}

def test_signal_hamming() {
    var w = hamming(4)
    matrix_rows(w) == 4.0 &&
    matrix_get(w, 0, 0) > 0.0 &&
    matrix_get(w, 1, 0) > 0.5
}

def test_signal_blackman() {
    var w = blackman(4)
    matrix_rows(w) == 4.0 &&
    is_close(matrix_get(w, 0, 0), 0.0) &&
    matrix_get(w, 1, 0) > 0.3
}

def test_signal_bartlett() {
    var w = bartlett(5)
    matrix_rows(w) == 5.0 &&
    matrix_get(w, 0, 0) == 0.0 &&
    matrix_get(w, 2, 0) == 1.0 &&
    matrix_get(w, 4, 0) == 0.0
}

def test_signal_square() {
    square(0.0, 0.5) == 1.0 &&
    square(0.5, 0.5) == -1.0 &&
    square(1.5, 0.5) == -1.0
}

def test_signal_sawtooth() {
    sawtooth(0.25) == 0.25 &&
    sawtooth(1.75) == 0.75
}

def test_signal_triangle() {
    triangle(0.0) == 0.0 &&
    triangle(0.25) > 0.99 &&
    triangle(0.5) < 0.01 &&
    triangle(0.75) < -0.99
}

def test_signal_pulse_train() {
    pulse_train(0.1, 0.5) == 1.0 &&
    pulse_train(0.6, 0.5) == 0.0
}

def test_signal_chirp() {
    chirp(0.0, 10.0, 20.0, 1.0) == 0.0 &&
    abs(chirp(0.5, 10.0, 20.0, 1.0)) <= 1.0
}

def test_signal_fftfreq() {
    var f = fftfreq(8, 1.0)
    matrix_get(f, 0, 0) == 0.0 &&
    matrix_get(f, 4, 0) == 0.5 &&
    matrix_get(f, 7, 0) < 0.0
}

def test_signal_unwrap() {
    var p = matrix_zeros(3, 1)
    p[0,0]=0.0; p[1,0]=3.0; p[2,0]=6.0
    var u = unwrap(p)
    matrix_get(u, 0, 0) == 0.0 &&
    matrix_get(u, 1, 0) == 3.0 &&
    matrix_get(u, 2, 0) - 2.0 * pi() < 0.1
}

def test_stats_zscore() {
    var v = matrix_zeros(4, 1)
    v[0,0]=1.0; v[1,0]=2.0; v[2,0]=3.0; v[3,0]=4.0
    var z = zscore(v)
    is_close(matrix_mean(z), 0.0) && is_close(std(z), 1.0)
}

def test_stats_entropy() {
    var v = matrix_zeros(4, 1)
    v[0,0]=0.25; v[1,0]=0.25; v[2,0]=0.25; v[3,0]=0.25
    is_close(entropy(v), 1.3862943611198906)
}

def test_stats_mad() {
    var v = matrix_zeros(4, 1)
    v[0,0]=1.0; v[1,0]=2.0; v[2,0]=3.0; v[3,0]=4.0
    var m = mad(v)
    m > 0.0
}

def test_stats_rms() {
    var v = matrix_zeros(3, 1)
    v[0,0]=3.0; v[1,0]=4.0; v[2,0]=0.0
    is_close(rms(v), 5.0 / sqrt(3.0))
}

def test_stats_mode() {
    var v = matrix_zeros(5, 1)
    v[0,0]=1.0; v[1,0]=2.0; v[2,0]=2.0; v[3,0]=3.0; v[4,0]=2.0
    mode(v) == 2.0
}

def test_stats_moment() {
    var v = matrix_zeros(4, 1)
    v[0,0]=1.0; v[1,0]=2.0; v[2,0]=3.0; v[3,0]=4.0
    is_close(moment(v, 2.0), 1.25)
}

from "trig" import hypot, log2

def test_from_import_string_literal() {
    is_close(hypot(3.0, 4.0), 5.0) && is_close(log2(8.0), 3.0)
}

from trig import degrees, radians

def test_from_import_identifier() {
    is_close(degrees(pi()), 180.0) && is_close(radians(180.0), pi())
}

from "array" import flip, sort, repmat

def test_from_import_multi() {
    var v = matrix_zeros(3, 1)
    v[0,0]=3.0; v[1,0]=1.0; v[2,0]=2.0
    var s = sort(v)
    matrix_get(s, 0, 0) == 1.0 && matrix_get(s, 2, 0) == 3.0
}
