def double_it(x) -> Double { x * 2.0 }
def add_one(x) -> Double { x + 1.0 }
def square(x) -> Double { x * x }
def negate(x) -> Double { -x }
def main() -> Double {
    assert(double_it(3.0) == 6.0, "double_it")
    assert(add_one(5.0) == 6.0, "add_one")
    assert(square(4.0) == 16.0, "square")
    assert(negate(7.0) == -7.0, "negate")
    assert(double_it(add_one(3.0)) == 8.0, "compose double(add_one)")
    assert(add_one(square(4.0)) == 17.0, "compose add_one(square)")
    assert(square(double_it(3.0)) == 36.0, "compose square(double)")
    var total = 0.0
    var i = 0.0
    while i < 50.0 do {
        total = total + double_it(i) + add_one(i)
        i = i + 1.0
    }
    assert(total == 3725.0, "call loop wrong")
    1.0
}
main()
