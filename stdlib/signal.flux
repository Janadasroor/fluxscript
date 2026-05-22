# hann: Hann window (raised cosine)
# Args: n - window length (>= 2)
# Returns: column vector of length n
# Domain: n >= 2
def hann(n) {
    assert(n >= 2.0, "hann: n must be >= 2")
    var w = matrix_zeros(n, 1)
    var n1 = n - 1.0
    var i = 0.0
    while(i < n) {
        matrix_set(w, i, 0, 0.5 * (1.0 - cos(2.0 * pi() * i / n1)))
        i = i + 1.0
    }
    w
}

# hamming: Hamming window
# Args: n - window length (>= 2)
# Returns: column vector of length n
# Domain: n >= 2
def hamming(n) {
    assert(n >= 2.0, "hamming: n must be >= 2")
    var w = matrix_zeros(n, 1)
    var n1 = n - 1.0
    var i = 0.0
    while(i < n) {
        matrix_set(w, i, 0, 0.54 - 0.46 * cos(2.0 * pi() * i / n1))
        i = i + 1.0
    }
    w
}

# blackman: Blackman window
# Args: n - window length (>= 2)
# Returns: column vector of length n
# Domain: n >= 2
def blackman(n) {
    assert(n >= 2.0, "blackman: n must be >= 2")
    var w = matrix_zeros(n, 1)
    var n1 = n - 1.0
    var a0 = 0.42
    var a1 = 0.5
    var a2 = 0.08
    var twopi = 2.0 * pi()
    var fourpi = 4.0 * pi()
    var i = 0.0
    while(i < n) {
        var x = i / n1
        matrix_set(w, i, 0, a0 - a1 * cos(twopi * x) + a2 * cos(fourpi * x))
        i = i + 1.0
    }
    w
}

# bartlett: Bartlett (triangle) window
# Args: n - window length (>= 2)
# Returns: column vector of length n
# Domain: n >= 2
def bartlett(n) {
    assert(n >= 2.0, "bartlett: n must be >= 2")
    var w = matrix_zeros(n, 1)
    var n1 = n - 1.0
    var i = 0.0
    while(i < n) {
        var x = i / n1
        matrix_set(w, i, 0, 1.0 - abs(2.0 * x - 1.0))
        i = i + 1.0
    }
    w
}

# square: square wave at normalized time t (period = 1)
# Args: t - normalized time, duty - fraction of period at +1 (0..1)
# Returns: 1.0 when frac(t) < duty else -1.0
def square(t, duty) {
    assert(duty > 0.0 && duty < 1.0, "square: duty must be in (0, 1)")
    var p = t - floor(t)
    if p < duty then 1.0 else -1.0
}

# sawtooth: sawtooth wave at normalized time t (period = 1)
# Args: t - normalized time
# Returns: ramp from 0 to 1 over each period
def sawtooth(t) {
    t - floor(t)
}

# triangle: triangle wave at normalized time t (period = 1)
# Args: t - normalized time
# Returns: asin(sin(2*pi*t)) * 2/pi, ranges -1 to 1
def triangle(t) {
    asin(sin(2.0 * pi() * t)) * 2.0 / pi()
}

# pulse_train: pulse train at normalized time t (period = 1)
# Args: t - normalized time, width - pulse width in (0, 1)
# Returns: 1.0 during pulse else 0.0
def pulse_train(t, width) {
    assert(width > 0.0 && width < 1.0, "pulse_train: width must be in (0, 1)")
    var p = t - floor(t)
    if p < width then 1.0 else 0.0
}

# chirp: linear chirp signal
# Args: t - time in seconds, f0 - start frequency, f1 - end frequency, t1 - chirp duration
# Returns: sin of linearly swept frequency from f0 to f1 over [0, t1]
def chirp(t, f0, f1, t1) {
    assert(t1 > 0.0, "chirp: t1 must be > 0")
    var rate = (f1 - f0) / (2.0 * t1)
    sin(2.0 * pi() * (f0 * t + rate * t * t))
}

# fftfreq: FFT frequency bins (numpy-compatible)
# Args: n - number of FFT points, d - sample spacing (seconds)
# Returns: column vector of n frequency values
def fftfreq(n, d) {
    assert(n > 0.0, "fftfreq: n must be > 0")
    assert(d > 0.0, "fftfreq: d must be > 0")
    var f = matrix_zeros(n, 1)
    var half = floor(n / 2.0)
    var i = 0.0
    while(i < n) {
        if (i <= half) { matrix_set(f, i, 0, i / (n * d)) }
        else { matrix_set(f, i, 0, (i - n) / (n * d)) }
        i = i + 1.0
    }
    f
}

# unwrap: unwrap radian phase vector
# Args: phase - column matrix of phase values in radians
# Returns: column matrix with discontinuities > pi corrected
# Domain: requires matrix_copy for input
def unwrap(phase) {
    var n = matrix_rows(phase)
    assert(n > 0.0, "unwrap: phase must not be empty")
    var out = matrix_copy(phase)
    var offset = 0.0
    var two_pi = 2.0 * pi()
    var i = 1.0
    while(i < n) {
        var prev = matrix_get(out, i - 1.0, 0)
        var curr = matrix_get(out, i, 0) + offset
        var diff = curr - prev
        if (diff > pi()) {
            offset = offset - two_pi
            curr = curr - two_pi
        } else if (diff < -pi()) {
            offset = offset + two_pi
            curr = curr + two_pi
        }
        matrix_set(out, i, 0, curr)
        i = i + 1.0
    }
    out
}
