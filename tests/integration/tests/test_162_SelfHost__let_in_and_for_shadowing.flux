def test_let_shadow(x: Double) -> Double {
    let x = 42.0 in {
        let x = 100.0 in {
            x
        }
    } + x
}

def test_for_shadow() -> Double {
    var sum = 0.0;
    let i = 1000.0;
    for i in 1, 3 do {
        sum = sum + i
    };
    sum + i
}

def main() -> Double {
    let r1 = test_let_shadow(5.0);
    let r2 = test_for_shadow();
    if (r1 == 105.0 && r2 == 1006.0) { 1.0 } else { 0.0 }
}
