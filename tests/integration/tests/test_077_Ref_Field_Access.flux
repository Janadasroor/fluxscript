// Test: Ref field access through MemberExprAST propagates borrow
struct Wrapper { r: &Double }

def main() -> Double {
    let x = 42.0
    let w = Wrapper { r: &x }
    let r = w.r
    x = 10.0
    x
}
main()
