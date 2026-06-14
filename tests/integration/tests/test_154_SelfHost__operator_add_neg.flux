struct Vec { x: Double }
def Vec_add(a: Double, b: Double) -> Double { 42.0 }
def Vec_neg(a: Double) -> Double { 43.0 }
def main() -> Double {
    let a = Vec { x: 1.0 };
    let b = Vec { x: 2.0 };
    let r1 = a + b;
    let r2 = -a;
    r1 + r2
}
