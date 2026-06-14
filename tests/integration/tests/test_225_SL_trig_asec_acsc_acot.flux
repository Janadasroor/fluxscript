import trig
def main() -> Double {
    assert(abs(asec(1.0)-0.0)<1e-9, "asec1");
    assert(abs(asec(2.0)-acos(0.5))<1e-9, "asec2");
    assert(abs(acsc(1.0)-asin(1.0))<1e-9, "acsc1");
    assert(abs(acot(0.0)-atan2(1.0,0.0))<1e-9, "acot0");
    assert(abs(acot(1.0)-atan2(1.0,1.0))<1e-9, "acot1");
    assert(abs(sech(0.0)-1.0)<1e-9, "sech0");
    assert(abs(csch(1.0)-2.0/(exp(1.0)-exp(-1.0)))<1e-9, "csch");
    assert(abs(coth(1.0)-(exp(1.0)+exp(-1.0))/(exp(1.0)-exp(-1.0)))<1e-9, "coth");
    1.0
}
main()
