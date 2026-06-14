def worker(x: Double) -> Double { x * 2.0 }
def main() -> Double {
    let t = spawn worker(21.0);
    join(t)
}
