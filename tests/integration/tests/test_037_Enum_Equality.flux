enum Color
    Red
    Green
    Blue
end
def test() {
    assert(Color.Red == Color.Red, "eq");
    assert(Color.Red != Color.Blue, "ne");
    1.0
}
test()
