enum BigPayload {
    Small { value: Double },
    Medium { a: Double, b: Double, c: Double },
    Large { a: Double, b: Double, c: Double, d: Double, e: Double, f: Double, g: Double, h: Double, i: Double, j: Double }
}
def extract(a: BigPayload) -> Double {
    match a {
        BigPayload.Small(v) -> v,
        BigPayload.Medium(x, y, z) -> x + y + z,
        BigPayload.Large(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10) -> a1 + a2 + a3 + a4 + a5 + a6 + a7 + a8 + a9 + a10
    }
}
def main() -> Double {
    let a = BigPayload.Small(1.0);
    let b = BigPayload.Medium(1.0, 2.0, 3.0);
    let c = BigPayload.Large(1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0);
    let r = extract(a) + extract(b) + extract(c);
    assert(r == 1.0 + 6.0 + 55.0, "extract sum wrong");
    1.0
}
main()
