import array
def main() -> Double {
    var m = matrix_zeros(1,1);
    matrix_set(m,0,0,5.0);
    var r = repmat(m,2.0,3.0);
    assert(matrix_rows(r)==2.0, "repmat_rows");
    assert(matrix_cols(r)==3.0, "repmat_cols");
    assert(abs(matrix_get(r,1,2)-5.0)<1e-12, "repmat_val");
    var p = pad_zeros(m,1.0,1.0);
    assert(matrix_rows(p)==3.0, "pad_rows");
    assert(matrix_cols(p)==3.0, "pad_cols");
    assert(abs(matrix_get(p,1,1)-5.0)<1e-12, "pad_center");
    assert(abs(matrix_get(p,0,0))<1e-12, "pad_zero");
    var m2 = matrix_zeros(1,3);
    matrix_set(m2,0,0,1.0); matrix_set(m2,0,1,5.0); matrix_set(m2,0,2,10.0);
    var c = clip(m2,2.0,8.0);
    assert(abs(matrix_get(c,0,0)-2.0)<1e-12, "clip_lo");
    assert(abs(matrix_get(c,0,1)-5.0)<1e-12, "clip_mid");
    assert(abs(matrix_get(c,0,2)-8.0)<1e-12, "clip_hi");
    1.0
}
main()
