import math
import signal
import array
def main() -> Double {
    var w = hann(16.0);
    assert(matrix_rows(w)==16.0, "hann_rows");
    var f = fftfreq(16.0, 0.01);
    assert(abs(matrix_get(f,0,0))<1e-12, "fftfreq_dc");
    var m = linspace(0.0, 1.0, 10.0);
    var c = clip(m, 0.2, 0.8);
    assert(abs(matrix_get(c,0,0)-0.2)<1e-12, "clip_lo_edge");
    assert(abs(matrix_get(c,9,0)-0.8)<1e-12, "clip_hi_edge");
    var nz = nonzero(c);
    assert(matrix_rows(nz)>0.0, "nonzero_clip");
    var nu = normalize_angle(100.0);
    assert(nu >= 0.0 && nu < 6.2832, "norm_angle");
    1.0
}
main()
