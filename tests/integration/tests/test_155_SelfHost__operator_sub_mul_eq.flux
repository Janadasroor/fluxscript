struct Wrap { val: Double }
def Wrap_sub(a: Double, b: Double) -> Double { 10.0 }
def Wrap_mul(a: Double, b: Double) -> Double { 20.0 }
def Wrap_eq(a: Double, b: Double) -> Double { 30.0 }
def main() -> Double {
    let a: Wrap = Wrap { val: 1.0 };
    let b: Wrap = Wrap { val: 2.0 };
    let r1 = a - b;
    let r2 = a * b;
    let r3 = a == b;
    r1 + r2 + r3
}
