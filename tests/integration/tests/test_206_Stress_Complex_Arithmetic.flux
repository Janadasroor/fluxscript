def trig_stress(x: Double) -> Double {
    sin(x) * cos(x) + tan(x) / (1.0 + exp(-x)) +
    sqrt(abs(x)) * log(abs(x) + 1.0) +
    sinh(x) * cosh(x) + tanh(x) * atan(x) +
    asin(tanh(x)) * acos(tanh(x))
}
def pow_stress(x: Double) -> Double {
    x*x*x*x*x*x*x*x*x*x +
    x*x*x*x*x*x*x*x*x +
    x*x*x*x*x*x*x*x +
    x*x*x*x*x*x*x +
    x*x*x*x*x*x +
    x*x*x*x*x +
    x*x*x*x +
    x*x*x +
    x*x +
    x
}
def vec_stress() -> Double {
    let v = [1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0];
    v[0] + v[1] + v[2] + v[3] + v[4] +
    v[5] + v[6] + v[7] + v[8] + v[9]
}
def main() -> Double {
    assert(abs(trig_stress(0.5) - 2.18553) < 0.001, "trig_stress wrong");
    assert(pow_stress(2.0) == 2046.0, "pow_stress wrong");
    assert(vec_stress() == 55.0, "vec_stress wrong");
    1.0
}
main()
