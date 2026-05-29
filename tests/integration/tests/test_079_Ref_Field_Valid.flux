// Test: Valid ref field access — borrow released when struct goes out of scope
struct Wrapper { r: &Double }

def main() -> Double {
    let x = 42.0
    {
        let w = Wrapper { r: &x }
        let r = w.r
    }
    x
}
main()
