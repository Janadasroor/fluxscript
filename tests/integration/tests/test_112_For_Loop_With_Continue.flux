def main() -> Double {
    var sum_odd = 0.0;
    for i in 1, 10 do {
        if i % 2.0 == 0.0 then continue;
        sum_odd = sum_odd + i;
    }
    assert(sum_odd == 25.0, "for-continue odd sum wrong");
    1.0
}
main()
