#!/usr/bin/env python3
"""Generate ~1M lines of FluxScript exercising all core features."""

import sys

# Each segment contributes this known value
SEG_VAL = 315.0  # verified: 21*10 + 105 = 315

def preamble():
    return [
        "// FluxScript ~1M-Line Mega Stress Test",
        "// Auto-generated - exercises all core features",
        "",
        "struct Pair[T, U] { first: T, second: U }",
        "struct Triple[A, B, C] { a: A, b: B, c: C }",
        "def id[T](x: T) -> T { x }",
        "enum Shape { Circle(Double), Rect { w: Double, h: Double }, Nothing }",
        "",
    ]

def seg(sid):
    k = 10.0
    # Pre-computed constants
    k1 = k + 1.0   # 11.0
    k2 = k + 2.0   # 12.0
    k4 = k + 4.0   # 14.0
    swv_val = k * 10.0 + 100.0  # 200.0
    return [
        f"    let p{sid}=Pair[Double,Pair[Double,Double]]{{first:{k:.1f},second:Pair[Double,Double]{{first:{k1:.1f},second:{k2:.1f}}}}};",
        f"    let g{sid}=id[Double]({k:.1f});",
        f"    let s{sid}=match Shape.Circle({k:.1f}){{Shape.Circle(r)->r*2.0,default->0.0}};",
        f"    let o{sid}=match Option.Some({k:.1f}){{Option.Some(x)->x,Option.None->0.0}};",
        f"    let w{sid}=switch({k:.1f}){{{k:.1f}=>{swv_val:.1f},~=>0.0}};",
        f"    let v{sid}=[{k:.1f},{k1:.1f},{k2:.1f}];",
        f"    let t{sid}=Triple[Double,Double,Double]{{a:{k:.1f},b:{k1:.1f},c:{k2:.1f}}};",
        f"    let i{sid}=p{sid}.second.first+p{sid}.second.second;",
        f"    let a{sid}=p{sid}.first+g{sid}+s{sid}+o{sid}+w{sid}+v{sid}[0]+v{sid}[1]+t{sid}.a+t{sid}.b+i{sid};",
        f"    total=total+a{sid};",
    ]

def seg_val(k=10.0):
    return 21.0 * k + 105.0  # 315.0

def main():
    k = 10.0
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument('-s', type=int, default=500, help='segments per function')
    ap.add_argument('-f', type=int, default=9, help='number of functions')
    args = ap.parse_args()
    S = args.s
    F = args.f
    # Total = 16 + F*(8 + 11*S)
    segs = F * S
    exp = segs * seg_val(k)
    line_count = 16 + F * (7 + S * 12)  # ≈ 996597

    for l in preamble():
        print(l)

    off = 0
    for fid in range(F):
        print(f"def f{fid}()->Double{{")
        print("    var total=0.0;")
        print()
        for _ in range(S):
            for l in seg(off):
                print(l)
            print()
            off += 1
        print("    total")
        print("}")
        print()

    print("def main()->Double{")
    print("    var t=0.0;")
    print()
    for fid in range(F):
        print(f"    t=t+f{fid}();")
    print()
    print(f"    assert(abs(t-{exp:.1f})<0.001,\"mega sum wrong\");")
    print("    t")
    print("}")
    print()
    print("main()")

    print(f"// {F} funcs, {segs} segs, {line_count} lines, expected={exp:.1f}",
          file=sys.stderr)

if __name__ == "__main__":
    main()
