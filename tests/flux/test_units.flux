# FluxScript Dimensional Analysis Tests
# Run: flux --entry=<function> tests/flux/test_units.flux

def test_voltage_literal() {
    var v = 5V; v == 5.0
}

def test_current_literal() {
    var i = 3A; i == 3.0
}

def test_resistor_voltage_divider() {
    var vin = 10V; var r1 = 1k; var r2 = 2k; var vout = vin * r2 / (r1 + r2); vout == 6.666666666666667
}

def test_power_from_voltage_current() {
    var v = 5V; var i = 2A; var p = v * i; p == 10.0
}

def test_resistance_from_voltage_current() {
    var v = 10V; var i = 2A; var r = v / i; r == 5.0
}

def test_frequency_from_period() {
    var t = 1ms; var f = 1.0 / t; f == 1000.0
}

def test_capacitor_charge() {
    var c = 10uF; var v = 5V; var q = c * v; q == 50e-6
}

def test_milli_conversion() {
    var a = 1000mV; a == 1.0
}

def test_kilo_conversion() {
    var r = 1k; r == 1000.0
}

# --- Dimension propagation through math functions ---

def test_abs_preserves_dims() {
    var x = -5V; abs(x) + 3V == 8.0
}

def test_floor_preserves_dims() {
    var x = 5.7V; floor(x) + 0.3V == 5.3
}

def test_ceil_preserves_dims() {
    var x = 5.3V; ceil(x) + 0.7V == 6.0
}

def test_round_preserves_dims() {
    var x = 5.5V; round(x) + 0.5V == 6.0
}

def test_chained_math_preserves_dims() {
    var x = -5.7V; abs(floor(x)) + abs(round(x)) == 11.0
}

def test_sqrt_halves_dims() {
    var v = 5V; sqrt(v * v) + 5V == 10.0
}

def test_cbrt_thirds_dims() {
    var v = 8V; cbrt(v * v * v) + 2V == 10.0
}

def test_trig_is_dimensionless() {
    var x = 5V; sin(x) + cos(x) + tan(x) == sin(5.0) + cos(5.0) + tan(5.0)
}

def test_log_is_dimensionless() {
    var x = 5V; log(x) + ln(x) + log10(x) == log(5.0) + ln(5.0) + log10(5.0)
}

def test_exp_is_dimensionless() {
    var x = 5V; exp(x) + exp2(x) == exp(5.0) + exp2(5.0)
}

def test_min_max_preserve_dims() {
    var x = 5V; var y = 10V; min(x, y) + max(x, y) == 15.0
}

def test_neg_preserves_dims() {
    var x = 5V; -x + x == 0.0
}

def test_typed_param_math() {
    let v = 5V; v + 3V == 8.0
}

def test_typed_param_chained() {
    let v = 5V; abs(v + v + v) + 5V == 20.0
}

# --- Block dimension propagation ---

def test_block_preserves_dims() {
    var v = { var x = 5V; x } + 3V; v == 8.0
}

def test_extern_ret_type() {
    let v = 5V; v + 3V == 8.0
}

# --- Mixed dimension expressions ---

def test_voltage_divider_fixed() {
    let vin = 10V; let r1 = 1k; let r2 = 2k; vin * r2 / (r1 + r2) == 6.666666666666667
}

def test_ohm_law_compound() {
    let v = 5V; let i = 2A; v / i == 2.5
}

# --- Compile-time unit builtins ---

def test_unit_builtin() {
    var v = unit(5, "V"); v + 3V == 8.0
}

def test_convert_builtin() {
    var v = convert(5V, "V", "mV"); v == 5000.0
}

def test_has_unit_typed() {
    has_unit(5V, "V") == 1.0
}

def test_has_unit_no_unit() {
    has_unit(5.0, "") == 0.0
}

# --- Complex nested expressions ---

def test_complex_nested_dims() {
    var r = 10k; var c = 1uF; var tau = r * c; abs(floor(1.0 / tau)) > 0.0
}

def test_sqrt_power_expression() {
    var v = 10V; var r = 1k; sqrt(v * v / r) > 0.0
}

# --- Unary operator propagation ---

def test_unary_minus_dims() {
    var x = 5V; -x + 10V == 5.0
}

# --- pow() dimension inference ---

def test_pow_preserves_dims() {
    var x = 5V; pow(x, 1) + 3V == 8.0
}

def test_pow_zero_dims() {
    var x = 5V; pow(x, 0) == 1.0
}

def test_pow_doubles_dims() {
    var x = 5V; pow(x, 2) / 5V == 5.0
}

def test_pow_negate_dims() {
    var x = 5V; pow(x, -1) * 25.0 == 5.0
}
