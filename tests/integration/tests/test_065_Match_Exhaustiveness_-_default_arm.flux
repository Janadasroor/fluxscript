enum Color { Red, Green, Blue }
def run_check() -> Double {
    let c = Color.Red;
    match c {
        Color.Red -> 1.0,
        default -> 0.0
    }
}
run_check()
