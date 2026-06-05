def worker(x: Double) -> Double {
    x * 2.0
}

def main() -> Double {
    let t = spawn worker(21.0);
    assert(join(t) == 42.0, "spawn/join failed");
    1.0
}

main()
