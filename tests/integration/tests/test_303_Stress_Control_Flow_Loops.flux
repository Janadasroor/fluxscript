def main() -> Double {
    var sum = 0.0
    for i in 0, 100 do {
        sum = if i < 50.0 then sum + i else sum + i * 2.0
    }
    assert(sum == 8675.0, "if-else chain in loop wrong")
    var sum2 = 0.0
    var j = 0.0
    while j < 100.0 do {
        sum2 = sum2 + 1.0
        j = j + 1.0
    }
    assert(sum2 == 100.0, "while count wrong")
    var nested_if = 0.0
    for i in 0, 20 do {
        nested_if = if i < 5.0 then nested_if + 1.0 else if i < 10.0 then nested_if + 2.0 else if i < 15.0 then nested_if + 3.0 else nested_if + 4.0
    }
    assert(nested_if == 50.0, "nested if-else chain wrong")
    var triple_sum = 0.0
    for i in 0, 10 do {
        for j in 0, 10 do {
            for k in 0, 10 do {
                triple_sum = triple_sum + 1.0
            }
        }
    }
    assert(triple_sum == 1000.0, "triple nested wrong")
    1.0
}
main()
