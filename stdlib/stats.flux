# variance: population variance of matrix elements
# Returns: sum((x - mean)^2) / N
def variance(m) {
    var r = matrix_rows(m)
    var c = matrix_cols(m)
    assert(r > 0.0 && c > 0.0, "variance: matrix must not be empty")
    var mu = matrix_mean(m)
    var ss = 0.0
    var ri = 0.0
    while(ri < r) {
        var ci = 0.0
        while(ci < c) {
            var d = matrix_get(m, ri, ci) - mu
            ss = ss + d * d
            ci = ci + 1.0
        }
        ri = ri + 1.0
    }
    ss / (r * c)
}

# std: population standard deviation
# Returns: sqrt(variance(m))
def std(m) { sqrt(variance(m)) }

# geometric_mean: geometric mean of matrix elements (product)^(1/N)
# Throws: if any element <= 0 (log domain error)
# Returns: exp(sum(log(x)) / N)
def geometric_mean(m) {
    var r = matrix_rows(m)
    var c = matrix_cols(m)
    assert(r > 0.0 && c > 0.0, "geometric_mean: matrix must not be empty")
    var total = 0.0
    var ri = 0.0
    while(ri < r) {
        var ci = 0.0
        while(ci < c) {
            var v = matrix_get(m, ri, ci)
            assert(v > 0.0, "geometric_mean: all elements must be positive")
            total = total + log(v)
            ci = ci + 1.0
        }
        ri = ri + 1.0
    }
    exp(total / (r * c))
}

# harmonic_mean: harmonic mean of matrix elements N / sum(1/x)
# Throws: if any element == 0 (division by zero)
# Returns: N / sum(1/x)
def harmonic_mean(m) {
    var r = matrix_rows(m)
    var c = matrix_cols(m)
    assert(r > 0.0 && c > 0.0, "harmonic_mean: matrix must not be empty")
    var total = 0.0
    var ri = 0.0
    while(ri < r) {
        var ci = 0.0
        while(ci < c) {
            var v = matrix_get(m, ri, ci)
            assert(v != 0.0, "harmonic_mean: elements must not be zero")
            total = total + 1.0 / v
            ci = ci + 1.0
        }
        ri = ri + 1.0
    }
    (r * c) / total
}

# cv: coefficient of variation std / |mean|
# Throws: if mean == 0 (division by zero)
# Returns: std / |mean|
def cv(m) {
    var mu = matrix_mean(m)
    assert(mu != 0.0, "cv: mean must not be zero")
    std(m) / abs(mu)
}

# min_element: find minimum value in matrix
# Returns: smallest element value
def min_element(m) {
    var r = matrix_rows(m)
    var c = matrix_cols(m)
    var best = matrix_get(m, 0, 0)
    var ri = 0.0
    while(ri < r) {
        var ci = 0.0
        while(ci < c) {
            var v = matrix_get(m, ri, ci)
            if(v < best) { best = v }
            ci = ci + 1.0
        }
        ri = ri + 1.0
    }
    best
}

# max_element: find maximum value in matrix
# Returns: largest element value
def max_element(m) {
    var r = matrix_rows(m)
    var c = matrix_cols(m)
    var best = matrix_get(m, 0, 0)
    var ri = 0.0
    while(ri < r) {
        var ci = 0.0
        while(ci < c) {
            var v = matrix_get(m, ri, ci)
            if(v > best) { best = v }
            ci = ci + 1.0
        }
        ri = ri + 1.0
    }
    best
}

# median: median value of matrix elements
# Depends on: array.flux (flatten, argsort)
# Returns: middle value (or average of two middle values if even count)
def median(m) {
    var s = sort(flatten(m))
    var n = matrix_rows(s) * matrix_cols(s)
    var half = floor(n / 2.0)
    if (n - half * 2.0 == 0.0) {
        (matrix_get(s, half - 1.0, 0) + matrix_get(s, half, 0)) / 2.0
    } else {
        matrix_get(s, half, 0)
    }
}

# zscore: standardize matrix to zero mean, unit variance
# Throws: if std == 0 (all elements identical)
# Depends on: array.flux (matrix_mean call)
# Returns: (x - mean) / std for each element
def zscore(m) {
    var mu = matrix_mean(m)
    var sigma = std(m)
    assert(sigma != 0.0, "zscore: standard deviation must not be zero")
    var r = matrix_rows(m)
    var c = matrix_cols(m)
    var out = matrix_zeros(r, c)
    var ri = 0.0
    while(ri < r) {
        var ci = 0.0
        while(ci < c) {
            matrix_set(out, ri, ci, (matrix_get(m, ri, ci) - mu) / sigma)
            ci = ci + 1.0
        }
        ri = ri + 1.0
    }
    out
}

# entropy: Shannon entropy of probability distribution
# Args: p - probability distribution (should sum to 1)
# Returns: -sum(p_i * log(p_i)) with 0*log(0) = 0 convention
def entropy(p) {
    var total = 0.0
    var n = matrix_rows(p) * matrix_cols(p)
    var i = 0.0
    while(i < n) {
        var pi = matrix_get(p, i, 0)
        if(pi > 1e-15) { total = total - pi * log(pi) }
        i = i + 1.0
    }
    total
}

# mad: mean absolute deviation from the mean
# Returns: sum(|x - mean|) / N
def mad(m) {
    var r = matrix_rows(m)
    var c = matrix_cols(m)
    assert(r > 0.0 && c > 0.0, "mad: matrix must not be empty")
    var mu = matrix_mean(m)
    var total = 0.0
    var ri = 0.0
    while(ri < r) {
        var ci = 0.0
        while(ci < c) {
            total = total + abs(matrix_get(m, ri, ci) - mu)
            ci = ci + 1.0
        }
        ri = ri + 1.0
    }
    total / (r * c)
}

# rms: root mean square
# Returns: sqrt(sum(x^2) / N)
def rms(m) {
    var r = matrix_rows(m)
    var c = matrix_cols(m)
    assert(r > 0.0 && c > 0.0, "rms: matrix must not be empty")
    var ss = 0.0
    var ri = 0.0
    while(ri < r) {
        var ci = 0.0
        while(ci < c) {
            var v = matrix_get(m, ri, ci)
            ss = ss + v * v
            ci = ci + 1.0
        }
        ri = ri + 1.0
    }
    sqrt(ss / (r * c))
}

# mode: most frequent value in matrix
# Depends on: array.flux (flatten, argsort)
# Returns: most commonly occurring element
def mode(m) {
    var flat = flatten(m)
    var idx = argsort(flat)
    var n = matrix_rows(flat) * matrix_cols(flat)
    var sorted = matrix_zeros(n, 1)
    var i = 0.0
    while(i < n) {
        matrix_set(sorted, i, 0, matrix_get(flat, matrix_get(idx, i, 0), 0))
        i = i + 1.0
    }
    var best_val = matrix_get(sorted, 0, 0)
    var best_cnt = 1.0
    var cur_val = best_val
    var cur_cnt = 1.0
    i = 1.0
    while(i < n) {
        var v = matrix_get(sorted, i, 0)
        if (v == cur_val) { cur_cnt = cur_cnt + 1.0 }
        else {
            if (cur_cnt > best_cnt) {
                best_cnt = cur_cnt
                best_val = cur_val
            }
            cur_val = v
            cur_cnt = 1.0
        }
        i = i + 1.0
    }
    if (cur_cnt > best_cnt) { cur_val }
    else { best_val }
}

# iqr: interquartile range (75th - 25th percentile)
# Returns: difference between Q3 and Q1 of sorted data
def iqr(m) {
    var s = sort(flatten(m))
    var n = matrix_rows(s) * matrix_cols(s)
    var q1 = matrix_get(s, floor(n * 0.25), 0)
    var q3 = matrix_get(s, floor(n * 0.75), 0)
    q3 - q1
}

# moment: k-th central moment
# Args: m - input matrix, k - moment order (1 = 0, 2 = variance, 3 = skewness, 4 = kurtosis)
# Returns: sum((x - mean)^k) / N
def moment(m, k) {
    var r = matrix_rows(m)
    var c = matrix_cols(m)
    assert(r > 0.0 && c > 0.0, "moment: matrix must not be empty")
    var mu = matrix_mean(m)
    var total = 0.0
    var ri = 0.0
    while(ri < r) {
        var ci = 0.0
        while(ci < c) {
            var d = matrix_get(m, ri, ci) - mu
            total = total + pow(d, k)
            ci = ci + 1.0
        }
        ri = ri + 1.0
    }
    total / (r * c)
}

# normalize_minmax: scale matrix to [0, 1] range
# Returns: (x - min) / (max - min) for each element, or zeros if all equal
def normalize_minmax(m) {
    var lo = min_element(m)
    var hi = max_element(m)
    var r = matrix_rows(m)
    var c = matrix_cols(m)
    var out = matrix_zeros(r, c)
    if (hi - lo < 1e-15) { out }
    else {
        var ri = 0.0
        while(ri < r) {
            var ci = 0.0
            while(ci < c) {
                matrix_set(out, ri, ci, (matrix_get(m, ri, ci) - lo) / (hi - lo))
                ci = ci + 1.0
            }
            ri = ri + 1.0
        }
        out
    }
}

# range_val: range (max - min) of matrix elements
# Returns: max_element(m) - min_element(m)
def range_val(m) {
    max_element(m) - min_element(m)
}
