import array
def main() -> Double {
    var l = linspace(0.0,1.0,5.0);
    assert(abs(matrix_get(l,0,0))<1e-12, "lin0");
    assert(abs(matrix_get(l,4,0)-1.0)<1e-12, "lin_end");
    assert(abs(matrix_get(l,2,0)-0.5)<1e-12, "lin_mid");
    var lg = logspace(0.0,2.0,3.0);
    assert(abs(matrix_get(lg,0,0)-1.0)<1e-9, "log0");
    assert(abs(matrix_get(lg,2,0)-100.0)<1e-9, "log_end");
    var a = arange(0.0,5.0,1.0);
    assert(abs(matrix_get(a,0,0))<1e-12, "arange0");
    assert(abs(matrix_get(a,4,0)-4.0)<1e-12, "arange4");
    assert(matrix_rows(a)==5.0, "arange_rows");
    1.0
}
main()
