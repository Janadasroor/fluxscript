def main() -> Double {
    var sum1 = 0.0
    for i in 0, 100 do {
        sum1 = sum1 + i
    }
    assert(sum1 == 4950.0, "for sum wrong")
    var sum4 = 0.0
    for i in 0, 10 do {
        for j in 0, 10 do {
            sum4 = sum4 + 1.0
        }
    }
    assert(sum4 == 100.0, "nested for sum wrong")
    var nested_if = 0.0
    for i in 0, 20 do {
        nested_if = if i < 5.0 then nested_if + 1.0 else if i < 10.0 then nested_if + 2.0 else if i < 15.0 then nested_if + 3.0 else nested_if + 4.0
    }
    assert(nested_if == 50.0, "nested if-else chain wrong")
    1.0
}
main()
