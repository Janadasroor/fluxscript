# sec: secant, 1 / cos(x)
# Returns: 1/cos(x)
# Domain: x != pi/2 + n*pi
def sec(x) 1.0 / cos(x)

# csc: cosecant, 1 / sin(x)
# Returns: 1/sin(x)
# Domain: x != n*pi
def csc(x) 1.0 / sin(x)

# cot: cotangent, cos(x) / sin(x)
# Returns: cos(x)/sin(x)
# Domain: x != n*pi
def cot(x) cos(x) / sin(x)

# sinc: sin(x)/x, with limit at 0
# Returns: 1 if |x| < 1e-15, sin(x)/x otherwise
# Domain: all real
def sinc(x) if abs(x) < 1e-15 then 1.0 else sin(x) / x

# asec: inverse secant, acos(1/x)
# Returns: acos(1/x)
# Domain: |x| >= 1
def asec(x) acos(1.0 / x)

# acsc: inverse cosecant, asin(1/x)
# Returns: asin(1/x)
# Domain: |x| >= 1
def acsc(x) asin(1.0 / x)

# acot: inverse cotangent, atan2(1, x)
# Returns: atan2(1, x)
# Domain: all real
def acot(x) atan2(1.0, x)

# sech: hyperbolic secant, 2 / (e^x + e^-x)
# Returns: 2/(exp(x) + exp(-x))
# Domain: all real
def sech(x) 2.0 / (exp(x) + exp(-x))

# csch: hyperbolic cosecant, 2 / (e^x - e^-x)
# Returns: 2/(exp(x) - exp(-x))
# Domain: x != 0
def csch(x) 2.0 / (exp(x) - exp(-x))

# coth: hyperbolic cotangent
# Returns: (exp(x) + exp(-x)) / (exp(x) - exp(-x))
# Domain: x != 0
def coth(x) (exp(x) + exp(-x)) / (exp(x) - exp(-x))

# degrees: convert radians to degrees (alias for rad2deg)
# Returns: r * 180/pi
def degrees(r) r * 57.29577951308232

# radians: convert degrees to radians (alias for deg2rad)
# Returns: d * pi/180
def radians(d) d * 0.017453292519943295

# haversine: (1 - cos(x)) / 2
# Returns: sin^2(x/2) = (1 - cos(x))/2
# Domain: all real
def haversine(x) (1.0 - cos(x)) / 2.0

# vercosine: (1 + cos(x)) / 2
# Returns: cos^2(x/2) = (1 + cos(x))/2
# Domain: all real
def vercosine(x) (1.0 + cos(x)) / 2.0
