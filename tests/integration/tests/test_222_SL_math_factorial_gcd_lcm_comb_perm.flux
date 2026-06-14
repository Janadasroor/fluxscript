import math
def main() -> Double {
    assert(factorial(0.0)==1.0, "fact0");
    assert(factorial(1.0)==1.0, "fact1");
    assert(factorial(5.0)==120.0, "fact5");
    assert(factorial(10.0)==3628800.0, "fact10");
    assert(gcd(12.0,8.0)==4.0, "gcd");
    assert(gcd(0.0,5.0)==5.0, "gcd_zero");
    assert(lcm(4.0,6.0)==12.0, "lcm");
    assert(lcm(0.0,5.0)==0.0, "lcm_zero");
    assert(comb(5.0,2.0)==10.0, "comb5_2");
    assert(comb(5.0,0.0)==1.0, "comb_k0");
    assert(comb(5.0,5.0)==1.0, "comb_k_eq_n");
    assert(perm(5.0,2.0)==20.0, "perm5_2");
    assert(perm(5.0,0.0)==1.0, "perm_k0");
    1.0
}
main()
