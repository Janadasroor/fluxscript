def risky(n: Double) -> Double {
    if (n < 0.0) { throw -1.0 } else { n * 2.0 }
}
def safe_run(n: Double) -> Double {
    try { risky(n) } catch e { e + 100.0 }
}
def main() -> Double {
    let a = safe_run(5.0);
    let b = safe_run(-3.0);
    a + b
}
main()
