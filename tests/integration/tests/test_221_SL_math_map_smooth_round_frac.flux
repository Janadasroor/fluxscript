import math
def main() -> Double {
    assert(map_range(5.0,0.0,10.0,0.0,100.0)==50.0, "map_mid");
    assert(map_range(5.0,0.0,10.0,100.0,0.0)==50.0, "map_rev");
    assert(smoothstep(0.0)==0.0, "smooth0");
    assert(smoothstep(1.0)==1.0, "smooth1");
    assert(smoothstep(-1.0)==0.0, "smooth_clamp_lo");
    assert(smoothstep(2.0)==1.0, "smooth_clamp_hi");
    assert(abs(round_to(3.14159,2.0)-3.14)<1e-9, "round_2");
    assert(frac(3.0)==0.0, "frac_int");
    assert(abs(frac(3.5)-0.5)<1e-12, "frac_pos");
    assert(abs(frac(-3.5)-0.5)<1e-12, "frac_neg");
    assert(relu(5.0)==5.0, "relu_pos");
    assert(relu(-3.0)==0.0, "relu_neg");
    assert(avg(3.0,5.0)==4.0, "avg");
    1.0
}
main()
