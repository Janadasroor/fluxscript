# FluxScript Dimensional Analysis Tests
# Run: flux --entry=<function> tests/flux/test_units.flux

def test_voltage_literal() {
    # 5V is valid with Voltage dimensions
    var v = 5V
    v == 5.0
}

def test_current_literal() {
    var i = 3A
    i == 3.0
}

def test_resistor_voltage_divider() {
    var vin = 10V
    var r1 = 1k
    var r2 = 2k
    var vout = vin * r2 / (r1 + r2)
    vout == 6.666666666666667
}

def test_power_from_voltage_current() {
    var v = 5V
    var i = 2A
    var p = v * i
    p == 10.0
}

def test_resistance_from_voltage_current() {
    var v = 10V
    var i = 2A
    var r = v / i
    r == 5.0
}

def test_frequency_from_period() {
    var t = 1ms
    var f = 1.0 / t
    f == 1000.0
}

def test_capacitor_charge() {
    var c = 10uF
    var v = 5V
    var q = c * v
    q == 50e-6
}

def test_milli_conversion() {
    var a = 1000mV
    a == 1.0
}

def test_kilo_conversion() {
    var r = 1k
    r == 1000.0
}
