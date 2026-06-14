def add(a: Double, b: Double, c: Double) -> Double { a + b + c }
def main() -> Double {
    let t = spawn add(10.0, 20.0, 30.0);
    join(t)
}
