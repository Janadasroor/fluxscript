enum Color { Red, Green, Blue }
def pick(c: Double) -> Double {
    match c {
        Color.Red -> 1.0,
        Color.Green -> 2.0,
        Color.Blue -> 3.0
    }
}
def main() -> Double { pick(Color.Green) }
