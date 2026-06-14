import array
def main() -> Double {
    var m = matrix_zeros(2,2);
    matrix_set(m,0,0,1.0); matrix_set(m,0,1,2.0);
    matrix_set(m,1,0,3.0); matrix_set(m,1,1,4.0);
    var f = flatten(m);
    assert(matrix_rows(f)==4.0, "flat_rows");
    assert(abs(matrix_get(f,0,0)-1.0)<1e-12, "flat0");
    assert(abs(matrix_get(f,3,0)-4.0)<1e-12, "flat3");
    var r = reshape(m,4,1);
    assert(matrix_rows(r)==4.0, "reshape_rows");
    var d = diff(m);
    assert(matrix_rows(d)==3.0, "diff_rows");
    var c = cumsum(m);
    assert(abs(matrix_get(c,1,1)-10.0)<1e-12, "cumsum");
    var cp = cumprod(m);
    assert(abs(matrix_get(cp,1,1)-24.0)<1e-12, "cumprod");
    1.0
}
main()
