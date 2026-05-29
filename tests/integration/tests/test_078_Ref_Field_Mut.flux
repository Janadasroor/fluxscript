// Test: Mutable ref field access through MemberExprAST propagates borrow
struct Wrapper { r: &mut Double }

def main() -> Double {
    let x = 42.0
    let w = Wrapper { r: &mut x }
    let rm = w.r
    x = 10.0
    x
}
main()
