def producer(ch: Double) -> Double {
    flux_chan_send(ch, 99.0);
    1.0
}

def main() -> Double {
    let ch = flux_chan_create();
    spawn producer(ch);
    let val = flux_chan_recv(ch);
    flux_chan_destroy(ch);
    assert(val == 99.0, "spawn+channel failed");
    1.0
}

main()
