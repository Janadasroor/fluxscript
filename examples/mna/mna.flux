# MNA: Modified Nodal Analysis Examples
# All examples run via: flux --entry=<fn> --cache=false examples/mna.flux

# ---------------------------------------------------------------------------
# Example 1: Voltage Divider
#    1 ---R1--- 2
#    V1=5V      R2
#    0 ---------0
# R1=1k, R2=2k
# Expected: V2 = 5 * 2k / (1k + 2k) = 3.333V
# ---------------------------------------------------------------------------
def voltage_divider() {
    var G1 = 1.0 / 1000.0
    var G2 = 1.0 / 2000.0
    var A = matrix_zeros(3, 3)
    matrix_set(A, 0, 0, G1);  matrix_set(A, 0, 1, -G1); matrix_set(A, 0, 2, 1.0)
    matrix_set(A, 1, 0, -G1); matrix_set(A, 1, 1, G1+G2)
    matrix_set(A, 2, 0, 1.0)
    var b = matrix_zeros(3, 1)
    matrix_set(b, 2, 0, 5.0)
    matrix_get(matrix_solve(A, b), 1, 0)
}

# ---------------------------------------------------------------------------
# Example 2: RC Circuit Transient (one step)
#    1 ---R--- 2
#    V1=5V     |
#              C=1uF
#              |
#              0
# Expected: V2(t) = 5 * (1 - exp(-t/RC))
# At t=1ms, RC=1k*1uF=1ms, V2 = 5 * (1 - 1/e) = 3.16V
# ---------------------------------------------------------------------------
def rc_transient() {
    var R = 1000.0; var C = 1e-6
    var tau = R * C; var t = 0.001; var dt = 1e-5
    var V2 = 0.0; var n = t / dt
    for i in 0, n do {
        var dV = (5.0 - V2) / tau * dt
        V2 = V2 + dV
    }
    V2
}

# ---------------------------------------------------------------------------
# Example 3: Build MNA matrix with helper functions
# ---------------------------------------------------------------------------
def build_mna_matrix() {
    var n = 3
    var A = matrix_zeros(n, n)
    var G1 = 1.0 / 1000.0
    var G2 = 1.0 / 2000.0
    matrix_set(A, 0, 0, G1);  matrix_set(A, 0, 1, -G1); matrix_set(A, 0, 2, 1.0)
    matrix_set(A, 1, 0, -G1); matrix_set(A, 1, 1, G1+G2)
    matrix_set(A, 2, 0, 1.0)
    matrix_rank(A)
}

# ---------------------------------------------------------------------------
# Example 4: Matrix identity and concatenation
# ---------------------------------------------------------------------------
def matrix_ops_demo() {
    var I = matrix_eye(3)
    var O = matrix_ones(3, 1)
    var C = matrix_hcat(I, O)
    matrix_get(C, 0, 0) + matrix_get(C, 0, 3)
}
