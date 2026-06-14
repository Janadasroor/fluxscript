def main() -> Double {
    var x = 0.0;
    var sum = 0.0;
    while x < 5.0 do {
        x = x + 1.0;
        if (x == 3.0) { continue };
        sum = sum + x
    };
    sum
}
