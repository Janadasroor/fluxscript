# matrix creation helpers wrapping extern C functions
def eye(n) matrix_eye(n)
def ones(r, c) matrix_ones(r, c)
def sum(m) matrix_sum(m)
def mean(m) matrix_mean(m)

# linspace: evenly spaced values from start to stop
# Args: start - first value, stop - last value, n - number of points
# Throws: if n < 2
# Returns: column vector with n values
def linspace(start, stop, n) {
    assert(n >= 2.0, "linspace: n must be >= 2")
    var m = matrix_zeros(n, 1)
    var n1 = n - 1.0
    var step = (stop - start) / n1
    var i = 0.0
    while(i < n1) {
        matrix_set(m, i, 0, start + i * step)
        i = i + 1.0
    }
    matrix_set(m, n1, 0, stop)
    m
}

# logspace: logarithmically spaced values from 10^start to 10^stop
# Args: start - exponent start, stop - exponent stop, n - number of points
# Throws: if n < 2
# Returns: column vector with n values
def logspace(start, stop, n) {
    assert(n >= 2.0, "logspace: n must be >= 2")
    var m = matrix_zeros(n, 1)
    var n1 = n - 1.0
    var step = (stop - start) / n1
    var i = 0.0
    while(i < n1) {
        matrix_set(m, i, 0, pow(10.0, start + i * step))
        i = i + 1.0
    }
    matrix_set(m, n1, 0, pow(10.0, stop))
    m
}

# arange: values from start to stop with given step
# Args: start - first value, stop - exclusive upper bound, step - increment
# Throws: if step == 0
# Returns: column vector [start, start+step, ..., stop-step)
def arange(start, stop, step) {
    assert(step != 0.0, "arange: step must not be 0")
    var len = max(0.0, ceil((stop - start) / step))
    if(len < 1.0) { matrix_zeros(1, 1) }
    else {
        var m = matrix_zeros(len, 1)
        var i = 0.0
        while(i < len) {
            matrix_set(m, i, 0, start + i * step)
            i = i + 1.0
        }
        m
    }
}

# flatten: reshape matrix into column vector
# Returns: (r*c x 1) column vector in row-major order
def flatten(m) {
    var r = matrix_rows(m)
    var c = matrix_cols(m)
    var out = matrix_zeros(r * c, 1)
    var ri = 0.0
    while(ri < r) {
        var ci = 0.0
        while(ci < c) {
            matrix_set(out, ri * c + ci, 0, matrix_get(m, ri, ci))
            ci = ci + 1.0
        }
        ri = ri + 1.0
    }
    out
}

# reshape: reshape matrix to new dimensions
# Args: m - input matrix, new_r - target rows, new_c - target columns
# Throws: if new_r * new_c != rows(m) * cols(m)
# Returns: reshaped matrix
def reshape(m, new_r, new_c) {
    var n = matrix_rows(m) * matrix_cols(m)
    assert(new_r * new_c == n, "reshape: total element count must match")
    var flat = flatten(m)
    var out = matrix_zeros(new_r, new_c)
    var i = 0.0
    while(i < n) {
        var r = floor(i / new_c)
        var ci = i - r * new_c
        matrix_set(out, r, ci, matrix_get(flat, i, 0))
        i = i + 1.0
    }
    out
}

# diff: forward differences of matrix elements
# Returns: column vector of (n-1) differences in row-major order
def diff(m) {
    var r = matrix_rows(m)
    var c = matrix_cols(m)
    var n = r * c - 1.0
    if (n <= 0.0) { matrix_zeros(1, 1) }
    else {
        var out = matrix_zeros(n, 1)
        var i = 0.0
        while(i < n) {
            matrix_set(out, i, 0, matrix_get(m, floor((i + 1.0) / c), (i + 1.0) - floor((i + 1.0) / c) * c) - matrix_get(m, floor(i / c), i - floor(i / c) * c))
            i = i + 1.0
        }
        out
    }
}

# cumsum: cumulative sum of matrix elements
# Returns: matrix same shape as input, each element = sum of all previous
def cumsum(m) {
    var r = matrix_rows(m)
    var c = matrix_cols(m)
    var out = matrix_zeros(r, c)
    var total = 0.0
    var ri = 0.0
    while(ri < r) {
        var ci = 0.0
        while(ci < c) {
            total = total + matrix_get(m, ri, ci)
            matrix_set(out, ri, ci, total)
            ci = ci + 1.0
        }
        ri = ri + 1.0
    }
    out
}

# cumprod: cumulative product of matrix elements
# Returns: matrix same shape as input, each element = product of all previous
def cumprod(m) {
    var r = matrix_rows(m)
    var c = matrix_cols(m)
    var out = matrix_zeros(r, c)
    var total = 1.0
    var ri = 0.0
    while(ri < r) {
        var ci = 0.0
        while(ci < c) {
            total = total * matrix_get(m, ri, ci)
            matrix_set(out, ri, ci, total)
            ci = ci + 1.0
        }
        ri = ri + 1.0
    }
    out
}

# argsort: indices that would sort the matrix in ascending order
# Returns: column vector of indices into the flattened matrix
def argsort(m) {
    var n = matrix_rows(m) * matrix_cols(m)
    var vals = matrix_zeros(n, 1)
    var idx = matrix_zeros(n, 1)
    var i = 0.0
    var ri = 0.0
    while(ri < matrix_rows(m)) {
        var ci = 0.0
        while(ci < matrix_cols(m)) {
            matrix_set(vals, i, 0, matrix_get(m, ri, ci))
            matrix_set(idx, i, 0, i)
            i = i + 1.0
            ci = ci + 1.0
        }
        ri = ri + 1.0
    }
    var sorted = 0.0
    while(sorted == 0.0) {
        sorted = 1.0
        var j = 0.0
        while(j < n - 1.0) {
            if (matrix_get(vals, j, 0) > matrix_get(vals, j + 1.0, 0)) {
                var tv = matrix_get(vals, j, 0)
                var ti = matrix_get(idx, j, 0)
                matrix_set(vals, j, 0, matrix_get(vals, j + 1.0, 0))
                matrix_set(idx, j, 0, matrix_get(idx, j + 1.0, 0))
                matrix_set(vals, j + 1.0, 0, tv)
                matrix_set(idx, j + 1.0, 0, ti)
                sorted = 0.0
            }
            j = j + 1.0
        }
    }
    idx
}

# unique: extract unique values from matrix
# Returns: column vector of unique elements in order of first occurrence
def unique(m) {
    var n = matrix_rows(m) * matrix_cols(m)
    var flat = matrix_zeros(n, 1)
    var i = 0.0
    var ri = 0.0
    while(ri < matrix_rows(m)) {
        var ci = 0.0
        while(ci < matrix_cols(m)) {
            matrix_set(flat, i, 0, matrix_get(m, ri, ci))
            i = i + 1.0
            ci = ci + 1.0
        }
        ri = ri + 1.0
    }
    var result = matrix_zeros(n, 1)
    var k = 0.0
    var j = 0.0
    while(j < n) {
        var v = matrix_get(flat, j, 0)
        var found = 0.0
        var t = 0.0
        while(t < k) {
            if (matrix_get(result, t, 0) == v) { found = 1.0 }
            t = t + 1.0
        }
        if (found == 0.0) {
            matrix_set(result, k, 0, v)
            k = k + 1.0
        }
        j = j + 1.0
    }
    result
}

# pad_zeros: pad matrix with zeros around edges
# Args: m - input matrix, pad_r - rows to pad on each vertical edge, pad_c - columns to pad on each horizontal edge
# Returns: (r + 2*pad_r) x (c + 2*pad_c) matrix
def pad_zeros(m, pad_r, pad_c) {
    var r = matrix_rows(m) + 2.0 * pad_r
    var c = matrix_cols(m) + 2.0 * pad_c
    var out = matrix_zeros(r, c)
    var ri = 0.0
    while(ri < matrix_rows(m)) {
        var ci = 0.0
        while(ci < matrix_cols(m)) {
            matrix_set(out, ri + pad_r, ci + pad_c, matrix_get(m, ri, ci))
            ci = ci + 1.0
        }
        ri = ri + 1.0
    }
    out
}

# meshgrid: 2D coordinate grids from x and y vectors
# Args: x - column vector of x-coordinates, y - column vector of y-coordinates
# Returns: [X | Y] matrix with ny rows and 2*nx columns (X grid followed by Y grid)
def meshgrid(x, y) {
    var nx = matrix_rows(x) * matrix_cols(x)
    var ny = matrix_rows(y) * matrix_cols(y)
    var X = matrix_zeros(ny, nx)
    var Y = matrix_zeros(ny, nx)
    var ri = 0.0
    while(ri < ny) {
        var ci = 0.0
        while(ci < nx) {
            matrix_set(X, ri, ci, matrix_get(x, ci, 0))
            matrix_set(Y, ri, ci, matrix_get(y, ri, 0))
            ci = ci + 1.0
        }
        ri = ri + 1.0
    }
    var out = matrix_zeros(ny, 2.0 * nx)
    ri = 0.0
    while(ri < ny) {
        var ci = 0.0
        while(ci < nx) {
            matrix_set(out, ri, ci, matrix_get(X, ri, ci))
            matrix_set(out, ri, ci + nx, matrix_get(Y, ri, ci))
            ci = ci + 1.0
        }
        ri = ri + 1.0
    }
    out
}

# flipud: flip matrix upside down (reverse row order)
# Returns: matrix with rows in reverse order
def flipud(m) {
    var r = matrix_rows(m)
    var c = matrix_cols(m)
    var out = matrix_zeros(r, c)
    var ri = 0.0
    while(ri < r) {
        var ci = 0.0
        while(ci < c) {
            matrix_set(out, ri, ci, matrix_get(m, r - 1.0 - ri, ci))
            ci = ci + 1.0
        }
        ri = ri + 1.0
    }
    out
}

# fliplr: flip matrix left-right (reverse column order)
# Returns: matrix with columns in reverse order
def fliplr(m) {
    var r = matrix_rows(m)
    var c = matrix_cols(m)
    var out = matrix_zeros(r, c)
    var ri = 0.0
    while(ri < r) {
        var ci = 0.0
        while(ci < c) {
            matrix_set(out, ri, ci, matrix_get(m, ri, c - 1.0 - ci))
            ci = ci + 1.0
        }
        ri = ri + 1.0
    }
    out
}

# rot90: rotate matrix 90 degrees counter-clockwise
# Args: m - input matrix
# Returns: (c x r) rotated matrix
def rot90(m) {
    var r = matrix_rows(m)
    var c = matrix_cols(m)
    var out = matrix_zeros(c, r)
    var ri = 0.0
    while(ri < r) {
        var ci = 0.0
        while(ci < c) {
            matrix_set(out, ci, r - 1.0 - ri, matrix_get(m, ri, ci))
            ci = ci + 1.0
        }
        ri = ri + 1.0
    }
    out
}

# triu: upper triangular part of matrix
# Returns: copy with elements below diagonal set to 0
def triu(m) {
    var r = matrix_rows(m)
    var c = matrix_cols(m)
    var out = matrix_zeros(r, c)
    var ri = 0.0
    while(ri < r) {
        var ci = ri
        while(ci < c) {
            matrix_set(out, ri, ci, matrix_get(m, ri, ci))
            ci = ci + 1.0
        }
        ri = ri + 1.0
    }
    out
}

# tril: lower triangular part of matrix
# Returns: copy with elements above diagonal set to 0
def tril(m) {
    var r = matrix_rows(m)
    var c = matrix_cols(m)
    var out = matrix_zeros(r, c)
    var ri = 0.0
    while(ri < r) {
        var ci = 0.0
        while(ci <= ri && ci < c) {
            matrix_set(out, ri, ci, matrix_get(m, ri, ci))
            ci = ci + 1.0
        }
        ri = ri + 1.0
    }
    out
}

# repmat: replicate/tile matrix into larger matrix
# Args: m - input matrix, rep_r - repetitions along rows, rep_c - repetitions along columns
# Throws: if rep_r < 1 or rep_c < 1
# Returns: (r*rep_r x c*rep_c) tiled matrix
def repmat(m, rep_r, rep_c) {
    assert(rep_r >= 1.0, "repmat: rep_r must be >= 1")
    assert(rep_c >= 1.0, "repmat: rep_c must be >= 1")
    var r = matrix_rows(m)
    var c = matrix_cols(m)
    var out = matrix_zeros(r * rep_r, c * rep_c)
    var ri = 0.0
    while(ri < rep_r) {
        var ci = 0.0
        while(ci < rep_c) {
            var bi = 0.0
            while(bi < r) {
                var bj = 0.0
                while(bj < c) {
                    matrix_set(out, ri * r + bi, ci * c + bj, matrix_get(m, bi, bj))
                    bj = bj + 1.0
                }
                bi = bi + 1.0
            }
            ci = ci + 1.0
        }
        ri = ri + 1.0
    }
    out
}

# trapz: trapezoidal integration
# Args: x - x-coordinates (column vector), y - y-values (column vector)
# Returns: approximate definite integral sum_i (x_i+1 - x_i) * (y_i + y_i+1) / 2
def trapz(x, y) {
    var n = min(matrix_rows(x) * matrix_cols(x), matrix_rows(y) * matrix_cols(y))
    var total = 0.0
    var i = 0.0
    while(i < n - 1.0) {
        var x0 = matrix_get(x, i, 0)
        var x1 = matrix_get(x, i + 1.0, 0)
        var y0 = matrix_get(y, i, 0)
        var y1 = matrix_get(y, i + 1.0, 0)
        total = total + (x1 - x0) * (y0 + y1) / 2.0
        i = i + 1.0
    }
    total
}

# nonzero: find non-zero element positions
# Returns: (k x 2) matrix where each row is [row_index, col_index], or empty if none
def nonzero(m) {
    var r = matrix_rows(m)
    var c = matrix_cols(m)
    var n = r * c
    var out = matrix_zeros(n, 2.0)
    var k = 0.0
    var ri = 0.0
    while(ri < r) {
        var ci = 0.0
        while(ci < c) {
            if (matrix_get(m, ri, ci) != 0.0) {
                matrix_set(out, k, 0, ri)
                matrix_set(out, k, 1, ci)
                k = k + 1.0
            }
            ci = ci + 1.0
        }
        ri = ri + 1.0
    }
    out
}

# clip: clamp every element of matrix to [lo, hi]
# Args: m - input matrix, lo - lower bound, hi - upper bound
# Throws: if lo > hi
# Returns: matrix with values clamped to [lo, hi]
def clip(m, lo, hi) {
    assert(lo <= hi, "clip: lo must be <= hi")
    var r = matrix_rows(m)
    var c = matrix_cols(m)
    var out = matrix_zeros(r, c)
    var ri = 0.0
    while(ri < r) {
        var ci = 0.0
        while(ci < c) {
            var v = matrix_get(m, ri, ci)
            if (v < lo) { matrix_set(out, ri, ci, lo) }
            else if (v > hi) { matrix_set(out, ri, ci, hi) }
            else { matrix_set(out, ri, ci, v) }
            ci = ci + 1.0
        }
        ri = ri + 1.0
    }
    out
}

# squeeze: remove singleton dimensions (1×1 → 1×1, 1×N → 1×N, N×1 → N×1)
def squeeze(m) {
    var r = matrix_rows(m)
    var c = matrix_cols(m)
    if (r == 1.0 && c == 1.0) { m }
    else if (r == 1.0) { matrix_slice(m, 0.0, 1.0, 0.0, c) }
    else if (c == 1.0) { matrix_slice(m, 0.0, r, 0.0, 1.0) }
    else { m }
}

# flip: reverse element order (180-degree rotation of flattened data)
# Returns: matrix with elements in reverse row-major order
def flip(m) {
    var r = matrix_rows(m)
    var c = matrix_cols(m)
    var out = matrix_zeros(r, c)
    var n = r * c
    var i = 0.0
    while(i < n) {
        var ri = floor(i / c)
        var ci = i - ri * c
        matrix_set(out, ri, ci, matrix_get(m, r - 1.0 - floor(i / c), c - 1.0 - (i - floor(i / c) * c)))
        i = i + 1.0
    }
    out
}

# sort: sort matrix elements in ascending order
# Returns: column vector of sorted values
def sort(m) {
    var n = matrix_rows(m) * matrix_cols(m)
    var idx = argsort(m)
    var out = matrix_zeros(n, 1)
    var i = 0.0
    while(i < n) {
        var ri = floor(matrix_get(idx, i, 0) / matrix_cols(m))
        var ci = matrix_get(idx, i, 0) - ri * matrix_cols(m)
        matrix_set(out, i, 0, matrix_get(m, ri, ci))
        i = i + 1.0
    }
    out
}

# replace: replace all occurrences of old value with new value
# Args: m - input matrix, old - value to find, new - replacement value
# Returns: copy of m with replacements
def replace(m, old, new) {
    var r = matrix_rows(m)
    var c = matrix_cols(m)
    var out = matrix_copy(m)
    var ri = 0.0
    while(ri < r) {
        var ci = 0.0
        while(ci < c) {
            if (matrix_get(out, ri, ci) == old) { matrix_set(out, ri, ci, new) }
            ci = ci + 1.0
        }
        ri = ri + 1.0
    }
    out
}

# count_eq: count occurrences of a value in matrix
# Returns: number of elements equal to val
def count_eq(m, val) {
    var r = matrix_rows(m)
    var c = matrix_cols(m)
    var total = 0.0
    var ri = 0.0
    while(ri < r) {
        var ci = 0.0
        while(ci < c) {
            if (matrix_get(m, ri, ci) == val) { total = total + 1.0 }
            ci = ci + 1.0
        }
        ri = ri + 1.0
    }
    total
}

# find_eq: find all positions where matrix element equals val
# Args: m - input matrix, val - value to find
# Returns: (k x 2) matrix where each row is [row_index, col_index], or empty if none
def find_eq(m, val) {
    var r = matrix_rows(m)
    var c = matrix_cols(m)
    var n = r * c
    var out = matrix_zeros(n, 2.0)
    var k = 0.0
    var ri = 0.0
    while(ri < r) {
        var ci = 0.0
        while(ci < c) {
            if (matrix_get(m, ri, ci) == val) {
                matrix_set(out, k, 0, ri)
                matrix_set(out, k, 1, ci)
                k = k + 1.0
            }
            ci = ci + 1.0
        }
        ri = ri + 1.0
    }
    out
}
