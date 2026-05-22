# max: return the larger of two numbers
# Args: a, b - numeric values
# Returns: a if a > b else b
def max(a, b) if a > b then a else b

# min: return the smaller of two numbers
# Args: a, b - numeric values
# Returns: a if a < b else b
def min(a, b) if a < b then a else b

# clamp: constrain x to [lo, hi] interval
# Args: x - value to clamp, lo - lower bound, hi - upper bound
# Throws: if lo > hi
# Returns: lo if x < lo, hi if x > hi, x otherwise
def clamp(x, lo, hi) {
    assert(lo <= hi, "clamp: lo must be <= hi")
    if x < lo then lo
    else if x > hi then hi
    else x
}

# lerp: linear interpolation between a and b by factor t
# Args: a - start value, b - end value, t - interpolation factor (0..1 yields a..b)
# Returns: a + (b - a) * t
def lerp(a, b, t) a + (b - a) * t

# rad2deg: convert radians to degrees
# Returns: r * 180/pi
def rad2deg(r) r * 57.29577951308232

# deg2rad: convert degrees to radians
# Returns: d * pi/180
def deg2rad(d) d * 0.017453292519943295

# sign: return -1, 0, or +1 indicating sign of x
# Returns: -1 if x < 0, +1 if x > 0, 0 if x == 0
def sign(x) {
    if x < 0.0 then -1.0
    else if x > 0.0 then 1.0
    else 0.0
}

# is_close: test if a and b are approximately equal
# Args: a, b - values to compare
# Returns: abs(a - b) < 1e-9
def is_close(a, b) abs(a - b) < 1e-9

# step: unit step function
# Returns: 0 if x < 0, 1 if x >= 0
def step(x) if x < 0.0 then 0.0 else 1.0

# factorial: compute n!
# Args: n - non-negative integer (floating point)
# Throws: if n < 0
# Returns: n * (n-1) * ... * 1, or 1 if n <= 1
def factorial(n) {
    assert(n >= 0.0, "factorial: n must be >= 0")
    if n <= 1.0 then 1.0 else n * factorial(n - 1.0)
}

# gcd: greatest common divisor using Euclidean algorithm
# Args: a, b - numeric values
# Returns: largest positive number dividing both a and b
def gcd(a, b) {
    var x = abs(a); var y = abs(b)
    while (y > 1e-12) {
        var t = y
        y = x - floor(x / y) * y
        x = t
    }
    x
}

# lcm: least common multiple
# Args: a, b - numeric values
# Returns: smallest positive number divisible by both a and b
def lcm(a, b) {
    if (a == 0.0 || b == 0.0) { 0.0 }
    else { abs(a * b) / gcd(a, b) }
}

# map_range: remap x from [in_lo, in_hi] to [out_lo, out_hi]
# Throws: if in_lo == in_hi (would divide by zero)
# Returns: linearly mapped value
def map_range(x, in_lo, in_hi, out_lo, out_hi) {
    assert(in_lo != in_hi, "map_range: in_lo must differ from in_hi")
    out_lo + (x - in_lo) * (out_hi - out_lo) / (in_hi - in_lo)
}

# smoothstep: cubic Hermite interpolation 3t^2 - 2t^3
# Args: x - input, automatically clamped to [0, 1]
# Returns: smooth S-curve from 0 to 1
def smoothstep(x) {
    var t = clamp(x, 0.0, 1.0)
    t * t * (3.0 - 2.0 * t)
}

# round_to: round x to n decimal places
# Args: x - value to round, n - number of decimal places
# Returns: rounded value
def round_to(x, n) {
    var scale = pow(10.0, n)
    round(x * scale) / scale
}

# frac: fractional part of x
# Returns: x - floor(x)
def frac(x) x - floor(x)

# wrap: wrap x into [lo, hi) range
# Args: x - value to wrap, lo - lower bound, hi - upper bound
# Throws: if lo >= hi
# Returns: x modulo (hi - lo), shifted to [lo, hi)
def wrap(x, lo, hi) {
    assert(lo < hi, "wrap: lo must be < hi")
    var r = hi - lo
    var v = x - lo
    lo + (v - floor(v / r) * r)
}

# pingpong: bounce x back and forth between lo and hi
# Args: x - value to bounce, lo - lower bound, hi - upper bound
# Throws: if lo >= hi
# Returns: triangular wave in [lo, hi]
def pingpong(x, lo, hi) {
    assert(lo < hi, "pingpong: lo must be < hi")
    var r = hi - lo
    var d = abs(x - lo)
    var t = d - floor(d / r) * r
    var phase = floor(d / r)
    if (phase - floor(phase / 2.0) * 2.0 == 0.0) { lo + t }
    else { hi - t }
}

# inverse_lerp: where does x lie between a and b?
# Args: x - value to locate, a - start of range, b - end of range
# Returns: 0 if x == a, 1 if x == b, extrapolates beyond
def inverse_lerp(x, a, b) {
    if (abs(b - a) < 1e-15) { 0.0 }
    else { (x - a) / (b - a) }
}

# sigmoid: logistic sigmoid 1 / (1 + e^-x)
# Returns: value in (0, 1)
def sigmoid(x) 1.0 / (1.0 + exp(-x))

# normalize_angle: wrap angle to [0, 2*pi)
# Returns: angle in [0, 2*pi)
def normalize_angle(x) {
    var two_pi = 6.283185307179586
    var v = x - floor(x / two_pi) * two_pi
    if (v < 0.0) { v + two_pi }
    else { v }
}

# normalize_angle_sym: wrap angle to [-pi, pi]
# Returns: angle in [-pi, pi]
def normalize_angle_sym(x) {
    var two_pi = 6.283185307179586
    var v = x - floor((x + pi()) / two_pi) * two_pi
    v
}

# softplus: smooth ReLU log(1 + e^x)
# Returns: log(1 + exp(x))
def softplus(x) log(1.0 + exp(-abs(x))) + max(x, 0.0)

# clamp01: clamp x to [0, 1]
# Returns: 0 if x < 0, 1 if x > 1, x otherwise
def clamp01(x) {
    if x < 0.0 then 0.0
    else if x > 1.0 then 1.0
    else x
}

# relu: rectified linear unit
# Returns: 0 if x < 0, x otherwise
def relu(x) if x < 0.0 then 0.0 else x

# logit: log-odds function, inverse of sigmoid
# Throws: if x <= 0 or x >= 1
# Returns: log(x / (1 - x))
def logit(x) {
    assert(x > 0.0, "logit: x must be > 0")
    assert(x < 1.0, "logit: x must be < 1")
    log(x / (1.0 - x))
}

# avg: arithmetic mean of two numbers
# Returns: (a + b) / 2
def avg(a, b) (a + b) / 2.0

# comb: binomial coefficient n choose k
# Args: n - total items, k - items to choose
# Throws: if n < 0 or k < 0
# Returns: n! / (k! * (n-k)!)
def comb(n, k) {
    assert(n >= 0.0, "comb: n must be >= 0")
    assert(k >= 0.0, "comb: k must be >= 0")
    if (k > n) { 0.0 }
    else if (k == 0.0 || k == n) { 1.0 }
    else {
        var r = 1.0
        var i = 1.0
        while(i <= k) {
            r = r * (n - k + i) / i
            i = i + 1.0
        }
        r
    }
}

# perm: permutations P(n, k) = n! / (n-k)!
# Args: n - total items, k - items to arrange
# Throws: if n < 0 or k < 0
# Returns: n * (n-1) * ... * (n-k+1)
def perm(n, k) {
    assert(n >= 0.0, "perm: n must be >= 0")
    assert(k >= 0.0, "perm: k must be >= 0")
    if (k > n) { 0.0 }
    else {
        var r = 1.0
        var i = n - k + 1.0
        while(i <= n) {
            r = r * i
            i = i + 1.0
        }
        r
    }
}

# reflect: reflect vector v about normal n
# Args: v - incident vector, n - unit normal
# Returns: v - 2 * dot(v, n) * n
def reflect(v, n) v - 2.0 * v * n
