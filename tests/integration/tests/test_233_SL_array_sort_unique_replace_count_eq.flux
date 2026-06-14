import array
def main() -> Double {
    var m = matrix_zeros(1,5);
    matrix_set(m,0,0,3.0); matrix_set(m,0,1,1.0);
    matrix_set(m,0,2,4.0); matrix_set(m,0,3,1.0); matrix_set(m,0,4,5.0);
    var s = sort(m);
    assert(abs(matrix_get(s,0,0)-1.0)<1e-12, "sort0");
    assert(abs(matrix_get(s,4,0)-5.0)<1e-12, "sort4");
    var u = unique(m);
    assert(abs(matrix_get(u,0,0)-3.0)<1e-12, "uniq0");
    assert(abs(matrix_get(u,1,0)-1.0)<1e-12, "uniq1");
    assert(abs(matrix_get(u,2,0)-4.0)<1e-12, "uniq2");
    var r = replace(m,1.0,99.0);
    assert(abs(matrix_get(r,0,3)-99.0)<1e-12, "replace");
    assert(count_eq(m,1.0)==2.0, "count_eq");
    1.0
}
main()
