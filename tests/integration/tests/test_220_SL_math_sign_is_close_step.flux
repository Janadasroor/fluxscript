import math
def main() -> Double {
    assert(sign(-5.0)==-1.0, "sign_neg");
    assert(sign(0.0)==0.0, "sign_zero");
    assert(sign(5.0)==1.0, "sign_pos");
    assert(is_close(1.0,1.0)!=0.0, "close_same");
    assert(is_close(1.0,1.0+1e-10)!=0.0, "close_near");
    assert(is_close(1.0,1.000001)==0.0, "close_far");
    assert(step(5.0)==1.0, "step_pos");
    assert(step(-1.0)==0.0, "step_neg");
    assert(step(0.0)==1.0, "step_zero");
    assert(abs(rad2deg(deg2rad(180.0))-180.0)<1e-9, "rad2deg_rt");
    assert(abs(deg2rad(rad2deg(1.0))-1.0)<1e-9, "deg2rad_rt");
    1.0
}
main()
