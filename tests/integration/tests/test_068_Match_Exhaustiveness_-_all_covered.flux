enum Color { Red, Green, Blue }
def run_check() -> Double {
    let c = Color.Red;
    match c {
        Color.Red -> 1.0,
        Color.Green -> 2.0,
        Color.Blue -> 3.0
    }
}
run_check()
