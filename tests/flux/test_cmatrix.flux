# Test complex matrix construction and operations

def test_cmatrix_create() {
    m = cmatrix(2, 2)
    flux_complex_matrix_rows(m) == 2.0 && flux_complex_matrix_cols(m) == 2.0
}

def test_complex_real_add() {
    c = cmatrix(2, 2)                 # Complex matrix of zeros
    r = matrix_ones(2, 2)             # Real matrix of ones
    s = c + r                         # Mixed add: promotes r to complex, then adds
    flux_complex_matrix_get_real(s, 0, 0) == 1.0 &&
    flux_complex_matrix_get_imag(s, 0, 0) == 0.0
}

def test_complex_real_mul() {
    c = complex_eye(3)                # Complex identity 3x3
    r = matrix_ones(3, 3)             # Real ones matrix
    s = c * r                         # Mixed multiply: promotes r to complex
    flux_complex_matrix_get_real(s, 0, 0) == 1.0 &&
    flux_complex_matrix_get_real(s, 0, 1) == 1.0
}

def test_complex_real_sub() {
    c = cmatrix(2, 2)                 # Complex zeros
    r = matrix_ones(2, 2)             # Real ones
    s = r - c                         # Mixed sub: real - complex promotes real
    flux_complex_matrix_get_real(s, 0, 0) == 1.0
}
