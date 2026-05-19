def variance(m) {
    var r = matrix_rows(m)
    var c = matrix_cols(m)
    var mu = mean(m)
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

def std(m) { sqrt(variance(m)) }

def norm(m) {
    var r = matrix_rows(m)
    var c = matrix_cols(m)
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
    sqrt(ss)
}

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
