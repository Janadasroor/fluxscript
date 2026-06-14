enum Wrap { Value { x: Double }, Empty }
def sq(w: Wrap) -> Double {
    match w {
        Wrap.Value(v) -> { v * v },
        default -> 0.0
    }
}
def main() -> Double {
    assert(sq(Wrap.Value(3.0)) == 9.0, "match var multiply wrong");
    assert(sq(Wrap.Value(0.0)) == 0.0, "match var zero wrong");
    assert(sq(Wrap.Empty) == 0.0, "match var default wrong");
    1.0
}
main()
