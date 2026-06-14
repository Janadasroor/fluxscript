from "string" import len, at, cmp, slice, find
def main() -> Double {
    assert(len("hello")==5.0, "str_len");
    assert(len("")==0.0, "str_empty");
    assert(at("hello",0.0)==104.0, "str_at0");
    assert(at("hello",1.0)==101.0, "str_at1");
    assert(cmp("abc","abc")==0.0, "str_cmp_eq");
    assert(cmp("abc","abd")<0.0, "str_cmp_lt");
    assert(cmp("abd","abc")>0.0, "str_cmp_gt");
    var s = slice("hello",0.0,2.0);
    var p = find("hello world","world");
    assert(p >= 0.0, "str_find");
    1.0
}
main()
