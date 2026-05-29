// Test: Cannot assign to variable while it is borrowed via ref-returning function
def get_ref(a: &Double) -> &Double { a }

def id(x: Double) -> Double { x }

def main() -> Double {
    let x = 42.0
    let r = get_ref(&x)
    x = 99.0
    id(r)
}
main()
