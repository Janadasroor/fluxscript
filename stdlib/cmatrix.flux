# cmatrix.flux — Complex Matrix Construction and Utilities
# All functions return complex-valued matrices (<2 x double> elements).

# cmatrix: Create an empty (zero) complex matrix (rows x cols)
def cmatrix(rows, cols) flux_complex_matrix_zeros(rows, cols)

# complex_eye: Identity complex matrix (n x n)
def complex_eye(n) flux_complex_matrix_eye(n)

# complex_ones: Complex matrix of all ones (rows x cols)
def complex_ones(rows, cols) flux_complex_matrix_ones(rows, cols)

# complex_zeros: Complex matrix of all zeros (rows x cols)
def complex_zeros(rows, cols) flux_complex_matrix_zeros(rows, cols)

# ctranspose: Conjugate transpose (Hermitian transpose) of complex matrix
def ctranspose(m) flux_complex_matrix_ctranspose(m)

# conj: Element-wise complex conjugate of matrix
def conj(m) flux_complex_matrix_conj(m)

# complex_transpose: Transpose (without conjugation) of complex matrix
def complex_transpose(m) flux_complex_matrix_transpose(m)

# complex_inv: Inverse of a complex square matrix
def complex_inv(m) flux_complex_matrix_inv(m)

# complex_det: Determinant magnitude of a complex square matrix
def complex_det(m) flux_complex_matrix_det(m)

# complex_trace: Trace (sum of diagonal real parts) of complex matrix
def complex_trace(m) flux_complex_matrix_trace(m)

# cmatrix_get: Get element at (row, col) as a complex number (<2 x double>)
def cmatrix_get(mat, row, col)
    flux_complex_matrix_get_real(mat, row, col) + flux_complex_matrix_get_imag(mat, row, col) * 1j
end

# cmatrix_set_ri: Set element at (row, col) from real and imag parts
def cmatrix_set_ri(mat, row, col, real, imag)
    flux_complex_matrix_set(mat, row, col, real, imag)
end

# rows: Number of rows of a complex matrix
def rows(m) flux_complex_matrix_rows(m)

# cols: Number of columns of a complex matrix
def cols(m) flux_complex_matrix_cols(m)
