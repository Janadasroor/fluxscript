def main() -> Double {
    let ch = flux_chan_create();
    flux_chan_send(ch, 42.0);
    let val = flux_chan_recv(ch);
    flux_chan_destroy(ch);
    assert(val == 42.0, "channel send/recv failed");
    1.0
}

main()
