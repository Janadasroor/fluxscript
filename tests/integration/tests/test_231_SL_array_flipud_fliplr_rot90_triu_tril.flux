import array
def main() -> Double {
    var m = matrix_zeros(2,2);
    matrix_set(m,0,0,1.0); matrix_set(m,0,1,2.0);
    matrix_set(m,1,0,3.0); matrix_set(m,1,1,4.0);
    var fud = flipud(m);
    assert(abs(matrix_get(fud,0,0)-3.0)<1e-12, "flipud00");
    assert(abs(matrix_get(fud,1,0)-1.0)<1e-12, "flipud10");
    var flr = fliplr(m);
    assert(abs(matrix_get(flr,0,0)-2.0)<1e-12, "fliplr00");
    var r90 = rot90(m);
    assert(matrix_rows(r90)==2.0, "rot90_r");
    assert(matrix_cols(r90)==2.0, "rot90_c");
    var u = triu(m);
    assert(abs(matrix_get(u,1,0))<1e-12, "triu10");
    assert(abs(matrix_get(u,0,1)-2.0)<1e-12, "triu01");
    var l = tril(m);
    assert(abs(matrix_get(l,0,1))<1e-12, "tril01");
    assert(abs(matrix_get(l,1,0)-3.0)<1e-12, "tril10");
    1.0
}
main()
