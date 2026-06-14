import math
def main() -> Double {
    assert(wrap(7.0,0.0,5.0)==2.0, "wrap1");
    assert(wrap(2.0,0.0,5.0)==2.0, "wrap_in");
    assert(wrap(5.0,0.0,5.0)==0.0, "wrap_at_hi");
    assert(pingpong(0.0,0.0,5.0)==0.0, "ping0");
    assert(pingpong(5.0,0.0,5.0)==5.0, "ping5");
    assert(pingpong(10.0,0.0,5.0)==0.0, "ping10");
    assert(abs(sigmoid(0.0)-0.5)<1e-12, "sig0");
    assert(abs(sigmoid(-10.0))<1e-4, "sig_neg_big");
    assert(abs(sigmoid(10.0)-1.0)<1e-4, "sig_pos_big");
    assert(abs(softplus(10.0)-10.0)<1e-4, "sp_pos");
    assert(abs(inverse_lerp(5.0,0.0,10.0)-0.5)<1e-12, "ilerp");
    assert(abs(normalize_angle(0.0))<1e-12, "nang0");
    assert(normalize_angle(7.0)>0.0, "nang_pos");
    assert(abs(clamp01(0.5)-0.5)<1e-12, "c01");
    assert(clamp01(-0.5)==0.0, "c01_lo");
    assert(clamp01(1.5)==1.0, "c01_hi");
    assert(abs(reflect(1.0,1.0)+1.0)<1e-12, "reflect");
    assert(abs(reflect(1.0,-1.0)-3.0)<1e-12, "reflect2");
    1.0
}
main()
