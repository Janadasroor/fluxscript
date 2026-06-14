import signal
def main() -> Double {
    assert(abs(hann(2.0))<1e-12, "hann2");
    assert(abs(hamming(2.0))<1e-12, "hamming2");
    assert(abs(blackman(2.0))<1e-12, "blackman2");
    assert(abs(bartlett(2.0))<1e-12, "bartlett2");
    1.0
}
main()
