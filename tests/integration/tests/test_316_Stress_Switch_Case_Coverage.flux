def classify_char(c: Double) -> Double {
    switch (c) {
        0.0 => 1.0,
        1.0 => 2.0,
        2.0 => 3.0,
        3.0 => 4.0,
        4.0 => 5.0,
        5.0 => 6.0,
        6.0 => 7.0,
        7.0 => 8.0,
        8.0 => 9.0,
        9.0 => 10.0,
        ~ => -1.0
    }
}
def day_of_week(d: Double) -> Double {
    switch (d) {
        0.0 => 0.0,
        1.0 => 1.0,
        2.0 => 2.0,
        3.0 => 3.0,
        4.0 => 4.0,
        5.0 => 5.0,
        6.0 => 6.0,
        ~ => -1.0
    }
}
def main() -> Double {
    assert(classify_char(0.0) == 1.0, "char 0")
    assert(classify_char(5.0) == 6.0, "char 5")
    assert(classify_char(9.0) == 10.0, "char 9")
    assert(classify_char(10.0) == -1.0, "char default")
    assert(classify_char(-1.0) == -1.0, "char neg default")
    let s1 = switch (0.0) { 0.0 => 100.0, ~ => 0.0 }
    assert(s1 == 100.0, "switch expr 0")
    let s2 = switch (5.0) { 0.0 => 100.0, ~ => 0.0 }
    assert(s2 == 0.0, "switch expr default")
    var sum = 0.0
    var i = 0.0
    while i < 10.0 do {
        sum = sum + classify_char(i)
        i = i + 1.0
    }
    assert(sum == 55.0, "switch loop sum")
    var day_sum = 0.0
    var d = 0.0
    while d < 7.0 do {
        day_sum = day_sum + day_of_week(d)
        d = d + 1.0
    }
    assert(day_sum == 21.0, "day sum wrong")
    1.0
}
main()
