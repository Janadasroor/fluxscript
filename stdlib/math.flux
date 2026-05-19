def max(a, b) if a > b then a else b

def min(a, b) if a < b then a else b

def clamp(x, lo, hi) {
    if x < lo then lo
    else if x > hi then hi
    else x
}

def lerp(a, b, t) a + (b - a) * t

def rad2deg(r) r * 57.29577951308232

def deg2rad(d) d * 0.017453292519943295

def sign(x) {
    if x < 0.0 then -1.0
    else if x > 0.0 then 1.0
    else 0.0
}

def is_close(a, b) abs(a - b) < 1e-9

def step(x) if x < 0.0 then 0.0 else 1.0

def factorial(n) if n <= 1.0 then 1.0 else n * factorial(n - 1.0)
