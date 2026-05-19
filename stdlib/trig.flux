def sec(x) 1.0 / cos(x)

def csc(x) 1.0 / sin(x)

def cot(x) cos(x) / sin(x)

def sinc(x) if abs(x) < 1e-15 then 1.0 else sin(x) / x

def hypot(a, b) sqrt(a * a + b * b)

def log2(x) log(x) / 0.6931471805599453

def cbrt(x) pow(x, 1.0 / 3.0)
