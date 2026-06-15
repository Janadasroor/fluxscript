def double_it(x) -> Double { x * 2.0 }
def add_one(x) -> Double { x + 1.0 }
def square(x) -> Double { x * x }
def negate(x) -> Double { -x }
def main() -> Double {
    let r1 = 5.0 |> double_it |> add_one
    assert(r1 == 11.0, "pipe simple wrong")
    let r2 = 3.0 |> square |> double_it |> add_one
    assert(r2 == 19.0, "pipe chain3 wrong")
    let r3 = 10.0 |> add_one |> square |> negate
    assert(r3 == -121.0, "pipe with negate wrong")
    let r4 = 5.0 |> double_it |> double_it |> double_it
    assert(r4 == 40.0, "pipe triple double wrong")
    let r5 = 2.0 |> square |> square |> square
    assert(r5 == 256.0, "pipe triple square wrong")
    let r7 = 1.0 |> add_one |> add_one |> add_one |> add_one |> add_one
    assert(r7 == 6.0, "pipe five adds wrong")
    var total = 0.0
    var i = 0.0
    while i < 50.0 do {
        let val = i |> double_it |> add_one |> square
        total = total + val
        i = i + 1.0
    }
    assert(total == 166650.0, "pipe loop wrong")
    1.0
}
main()
