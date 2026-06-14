import math
def main() -> Double {
    assert(max(5.0,3.0)==5.0, "max1");
    assert(max(3.0,5.0)==5.0, "max2");
    assert(max(4.0,4.0)==4.0, "max_eq");
    assert(max(-1.0,-5.0)==-1.0, "max_neg");
    assert(min(5.0,3.0)==3.0, "min1");
    assert(min(3.0,5.0)==3.0, "min2");
    assert(min(4.0,4.0)==4.0, "min_eq");
    assert(clamp(10.0,0.0,5.0)==5.0, "clamp_hi");
    assert(clamp(-1.0,0.0,5.0)==0.0, "clamp_lo");
    assert(clamp(3.0,0.0,5.0)==3.0, "clamp_mid");
    assert(abs(lerp(0.0,10.0,0.5)-5.0)<1e-12, "lerp_half");
    assert(abs(lerp(10.0,20.0,-0.5)-5.0)<1e-12, "lerp_extrap_lo");
    assert(abs(lerp(10.0,20.0,1.5)-25.0)<1e-12, "lerp_extrap_hi");
    1.0
}
main()
