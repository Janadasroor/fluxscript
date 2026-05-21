# FluxScript Feature Verification Tests
# Run: flux --entry=<function> tests/test_all_features.flux

def test_matrix_create() {
    var A = matrix_zeros(3, 2)
    matrix_rows(A) == 3 && matrix_cols(A) == 2
}

def test_matrix_eye() {
    var I = matrix_eye(3)
    I[0,0] == 1.0 && I[0,1] == 0.0 && I[1,1] == 1.0
}

def test_matrix_ones() {
    var O = matrix_ones(2, 2)
    O[0,0] == 1.0 && O[1,1] == 1.0
}

def test_matrix_copy() {
    var A = matrix_zeros(2, 2)
    A[0,0] = 42.0
    var B = matrix_copy(A)
    B[0,0] == 42.0
}

def test_matrix_arith() {
    var A = matrix_eye(2)
    var B = matrix_eye(2)
    var Cm = matrix_mul(A, B)
    var Ca = matrix_add(A, B)
    var Cs = matrix_sub(A, B)
    Cm[0,0] == 1.0 && Ca[0,0] == 2.0 && Cs[1,1] == 0.0
}

def test_matrix_transpose() {
    var A = matrix_zeros(2, 3)
    A[1,0] = 5.0
    var T = matrix_transpose(A)
    T[0,1] == 5.0
}

def test_matrix_inv_det() {
    var A = matrix_zeros(2, 2)
    A[0,0] = 4.0; A[0,1] = 3.0
    A[1,0] = 2.0; A[1,1] = 1.0
    var d = matrix_det(A)
    var I = matrix_mul(A, matrix_inv(A))
    I[0,0] > 0.99 && I[0,0] < 1.01
}

def test_matrix_solve() {
    var A = matrix_eye(2)
    var b = matrix_zeros(2, 1)
    b[0,0] = 5.0
    var x = matrix_solve(A, b)
    x[0,0] == 5.0
}

def test_matrix_trace() {
    var A = matrix_eye(3)
    matrix_trace(A) == 3.0
}

def test_matrix_diag() {
    var A = matrix_zeros(2, 2)
    A[0,0] = 1.0; A[1,1] = 2.0
    var d = matrix_diag(A)
    d[0,0] == 1.0 && d[1,0] == 2.0
}

def test_matrix_hcat() {
    var B = matrix_eye(2)
    var A = matrix_ones(2, 1)
    var C = matrix_hcat(B, A)
    C[0,2] == 1.0 && matrix_cols(C) == 3
}

def test_matrix_slice() {
    var A = matrix_zeros(3, 3)
    A[0,0]=1; A[0,1]=2; A[0,2]=3
    A[1,0]=4; A[1,1]=5; A[1,2]=6
    var B = A[0:2, 0:2]
    B[0,0] == 1.0 && B[1,1] == 5.0
}

def test_matrix_decomp() {
    var A = matrix_zeros(2, 2)
    A[0,0] = 4.0; A[0,1] = 1.0
    A[1,0] = 1.0; A[1,1] = 4.0
    var ev = matrix_eigenvalues(A)
    var R = matrix_rank(A)
    var N = matrix_norm(A, 0)
    R == 2.0 && N > 0.0
}

def test_stats() {
    var x = matrix_zeros(5, 1)
    x[0,0]=1; x[1,0]=2; x[2,0]=3; x[3,0]=4; x[4,0]=5
    matrix_mean(x) == 3.0 && matrix_sum(x) == 15.0
}

def test_fft() {
    var sig = matrix_zeros(8, 1)
    sig[0,0]=1; sig[1,0]=2; sig[2,0]=3; sig[3,0]=4
    sig[4,0]=5; sig[5,0]=6; sig[6,0]=7; sig[7,0]=8
    var S = fft(sig, 1000.0)
    var thd = fft_thd(sig, 1000.0)
    var snr = fft_snr(sig, 1000.0)
    matrix_rows(S) > 0 && matrix_cols(S) == 3 && thd >= 0.0
}

def test_spice_api() {
    register_analysis("tran")
    register_measure("vout", "MAX")
    register_probe("V(out)", "vout")
    register_save("V(out)")
    register_param("R1", 1000.0)
    register_ic("V(cap)", 0.0)
    1.0
}

# --- Compile-time dimensional analysis ---

def test_simple_voltage() {
    5V + 3V == 8.0
}

def test_ohns_law() {
    var v = 10V; var i = 2A; var r = v / i; r == 5.0
}

def test_unit_builtin_compiletime() {
    var v = unit(5, "V"); v + 3V == 8.0
}

def test_convert_builtin() {
    var v = convert(5V, "V", "mV"); v == 5000.0
}

def test_abs_preserves_voltage() {
    var x = -5V; abs(x) + 3V == 8.0
}

def test_sqrt_halves_voltage() {
    var v = 5V; sqrt(v * v) + 5V == 10.0
}

def test_dimensionless_trig() {
    sin(5V) + cos(5V) == sin(5.0) + cos(5.0)
}

def test_typed_param_propagation() {
    var v = 5V; v + 3V == 8.0
}

def test_voltage_divider_typed() {
    var vin = 10V; var r1 = 1k; var r2 = 2k; vin * r2 / (r1 + r2) == 6.666666666666667
}

def test_pow_one() {
    var x = 5V; pow(x, 1) + 3V == 8.0
}

def test_pow_sq() {
    var x = 5V; pow(x, 2) / 5V == 5.0
}

def test_pow_op_sq() {
    var x = 5V; (x ^ 2) / 5V == 5.0
}

def test_hypot_dim() {
    hypot(3V, 4V) == 5.0
}

def test_pow_op_dimensionless() {
    2.0 ^ 3 == 8.0
}
