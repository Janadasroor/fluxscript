// Test: Multi ref param -> ambiguous, no borrow tracking
def first(a: &Double, b: &Double) -> &Double { a }

def main() -> Double {
    let x = 42.0
    let y = 10.0
    let r = first(&x, &y)
    x
}
main()
