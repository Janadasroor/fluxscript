import array
import math
import stats
def main() -> Double {
    var d = matrix_zeros(3,3);
    matrix_set(d,0,0,1.0); matrix_set(d,0,1,2.0); matrix_set(d,0,2,3.0);
    matrix_set(d,1,0,4.0); matrix_set(d,1,1,5.0); matrix_set(d,1,2,6.0);
    matrix_set(d,2,0,7.0); matrix_set(d,2,1,8.0); matrix_set(d,2,2,9.0);
    var mu = mean(d);
    assert(abs(mu-5.0)<1e-12, "mean");
    var md = median(d);
    assert(abs(md-5.0)<1e-12, "median");
    var v = variance(d);
    assert(abs(v-60.0/9.0)<1e-9, "variance");
    var s = std(d);
    assert(abs(s-sqrt(v))<1e-9, "std");
    assert(abs(range_val(d)-8.0)<1e-12, "range");
    var rmsv = rms(d);
    assert(abs(rmsv-sqrt(285.0/9.0))<1e-9, "rms");
    1.0
}
main()
