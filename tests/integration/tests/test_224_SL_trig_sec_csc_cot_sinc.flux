import trig
def main() -> Double {
    assert(abs(sec(0.0)-1.0)<1e-9, "sec0");
    assert(abs(csc(1.0)-1.0/sin(1.0))<1e-9, "csc");
    assert(abs(cot(1.0)-cos(1.0)/sin(1.0))<1e-9, "cot");
    assert(abs(sinc(0.0)-1.0)<1e-12, "sinc0");
    assert(abs(sinc(1.0)-sin(1.0))<1e-9, "sinc1");
    assert(abs(degrees(pi())-180.0)<1e-9, "degrees");
    assert(abs(radians(180.0)-pi())<1e-9, "radians");
    assert(abs(haversine(0.0))<1e-12, "hav0");
    assert(abs(haversine(pi())-1.0)<1e-9, "hav_pi");
    assert(abs(vercosine(0.0)-1.0)<1e-12, "verc0");
    assert(abs(vercosine(pi()))<1e-9, "verc_pi");
    1.0
}
main()
