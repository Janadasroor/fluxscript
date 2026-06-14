def main() -> Double {
    var x = 0.0;
    var found = 0.0;
    while x < 10.0 do {
        x = x + 1.0;
        if (x == 7.0) { found = 1.0; break }
    };
    found
}
