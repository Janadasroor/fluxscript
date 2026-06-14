import array
def main() -> Double {
    var x = linspace(0.0,1.0,11.0);
    var y = linspace(0.0,1.0,11.0);
    var t = trapz(x,y);
    assert(abs(t-0.5)<1e-2, "trapz"); // integral x over [0,1] = 0.5
    var m = matrix_zeros(2,2);
    matrix_set(m,0,0,0.0); matrix_set(m,1,1,5.0);
    var nz = nonzero(m);
    assert(matrix_rows(nz)>=1.0, "nonzero_exists");
    var fe = find_eq(m,5.0);
    assert(matrix_rows(fe)>=1.0, "find_eq_exists");
    1.0
}
main()
