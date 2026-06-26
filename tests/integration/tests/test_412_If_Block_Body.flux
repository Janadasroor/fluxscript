def main() -> Double {
    let a = if 1.0 == 1.0 then {
        let x = 10.0;
        x + 32.0
    } else {
        let y = 5.0;
        y * 3.0
    };
    assert(a == 42.0, "if-block-body true failed");

    let b = if 1.0 == 0.0 then {
        7.0
    } else {
        99.0
    };
    assert(b == 99.0, "if-block-body false failed");

    let c = if 1.0 == 1.0 {
        42.0
    } else {
        0.0
    };
    assert(c == 42.0, "if-block no-then failed");
    1.0
}
main()
