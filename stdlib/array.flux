def linspace(start, stop, n) {
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

def logspace(start, stop, n) {
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

def arange(start, stop, step) {
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

def sum(m) {
    var r = matrix_rows(m)
    var c = matrix_cols(m)
    var total = 0.0
    var ri = 0.0
    while(ri < r) {
        var ci = 0.0
        while(ci < c) {
            total = total + matrix_get(m, ri, ci)
            ci = ci + 1.0
        }
        ri = ri + 1.0
    }
    total
}

def mean(m) { sum(m) / (matrix_rows(m) * matrix_cols(m)) }

def eye(n) { matrix_eye(n) }

def ones(r, c) { matrix_ones(r, c) }

def zeros(r, c) { matrix_zeros(r, c) }

def copy(m) { matrix_copy(m) }

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

def reshape(m, new_r, new_c) {
    var flat = flatten(m)
    var out = matrix_zeros(new_r, new_c)
    var n = new_r * new_c
    var i = 0.0
    while(i < n) {
        var r = floor(i / new_c)
        var c_val = i - r * new_c
        matrix_set(out, r, c_val, matrix_get(flat, i, 0))
        i = i + 1.0
    }
    out
}
