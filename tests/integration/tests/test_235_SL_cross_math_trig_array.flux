import math
import trig
import array
def main() -> Double {
    var d = deg2rad(180.0);
    assert(abs(d-pi())<1e-9, "deg2rad");
    var v = linspace(0.0, pi(), 100.0);
    var s = matrix_get(v, 0, 0);
    var e = matrix_get(v, 99, 0);
    assert(abs(s)<1e-12, "lin0");
    assert(abs(e-pi())<1e-9, "lin_end");
    var idx = argsort(v);
    assert(matrix_rows(idx)==100.0, "argsort");
    assert(abs(sigmoid(clamp01(sin(0.5)))-6.176e-1)<1e-4, "cross_sigmoid");
    assert(abs(wrap(relu(-5.0),0.0,1.0))<1e-12, "cross_wrap_relu");
    1.0
}
main()
