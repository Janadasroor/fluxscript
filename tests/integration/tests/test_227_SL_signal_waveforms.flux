import signal
def main() -> Double {
    assert(abs(square(0.25,0.5)-1.0)<1e-12, "square_on");
    assert(abs(square(0.75,0.5)+1.0)<1e-12, "square_off");
    assert(abs(sawtooth(0.5)-0.5)<1e-12, "saw");
    assert(abs(sawtooth(1.5)-0.5)<1e-12, "saw_mod");
    assert(abs(triangle(0.0))<1e-12, "tri0");
    assert(abs(pulse_train(0.1,0.5)-1.0)<1e-12, "pulse_on");
    assert(abs(pulse_train(0.6,0.5))<1e-12, "pulse_off");
    assert(abs(chirp(0.0,10.0,20.0,1.0))<1e-12, "chirp0");
    var f = fftfreq(4.0,1.0);
    assert(abs(matrix_get(f,0,0))<1e-12, "fftfreq0");
    assert(abs(matrix_get(f,3,0)+0.25)<1e-12, "fftfreq3");
    1.0
}
main()
