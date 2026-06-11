struct BigData {
    f00: Double, f01: Double, f02: Double, f03: Double, f04: Double,
    f05: Double, f06: Double, f07: Double, f08: Double, f09: Double,
    f10: Double, f11: Double, f12: Double, f13: Double, f14: Double,
    f15: Double, f16: Double, f17: Double, f18: Double, f19: Double,
    f20: Double, f21: Double, f22: Double, f23: Double, f24: Double,
    f25: Double, f26: Double, f27: Double, f28: Double, f29: Double,
    f30: Double, f31: Double, f32: Double, f33: Double, f34: Double,
    f35: Double, f36: Double, f37: Double, f38: Double, f39: Double,
    f40: Double, f41: Double, f42: Double, f43: Double, f44: Double,
    f45: Double, f46: Double, f47: Double, f48: Double, f49: Double,
    f50: Double, f51: Double, f52: Double, f53: Double, f54: Double,
    f55: Double, f56: Double, f57: Double, f58: Double, f59: Double,
    f60: Double, f61: Double, f62: Double, f63: Double, f64: Double,
    f65: Double, f66: Double, f67: Double, f68: Double, f69: Double,
    f70: Double, f71: Double, f72: Double, f73: Double, f74: Double,
    f75: Double, f76: Double, f77: Double, f78: Double, f79: Double,
    f80: Double, f81: Double, f82: Double, f83: Double, f84: Double,
    f85: Double, f86: Double, f87: Double, f88: Double, f89: Double,
    f90: Double, f91: Double, f92: Double, f93: Double, f94: Double,
    f95: Double, f96: Double, f97: Double, f98: Double, f99: Double
}
def make_big(v: Double) -> BigData {
    BigData {
        f00: v, f01: v+1.0, f02: v+2.0, f03: v+3.0, f04: v+4.0,
        f05: v+5.0, f06: v+6.0, f07: v+7.0, f08: v+8.0, f09: v+9.0,
        f10: v+10.0, f11: v+11.0, f12: v+12.0, f13: v+13.0, f14: v+14.0,
        f15: v+15.0, f16: v+16.0, f17: v+17.0, f18: v+18.0, f19: v+19.0,
        f20: v+20.0, f21: v+21.0, f22: v+22.0, f23: v+23.0, f24: v+24.0,
        f25: v+25.0, f26: v+26.0, f27: v+27.0, f28: v+28.0, f29: v+29.0,
        f30: v+30.0, f31: v+31.0, f32: v+32.0, f33: v+33.0, f34: v+34.0,
        f35: v+35.0, f36: v+36.0, f37: v+37.0, f38: v+38.0, f39: v+39.0,
        f40: v+40.0, f41: v+41.0, f42: v+42.0, f43: v+43.0, f44: v+44.0,
        f45: v+45.0, f46: v+46.0, f47: v+47.0, f48: v+48.0, f49: v+49.0,
        f50: v+50.0, f51: v+51.0, f52: v+52.0, f53: v+53.0, f54: v+54.0,
        f55: v+55.0, f56: v+56.0, f57: v+57.0, f58: v+58.0, f59: v+59.0,
        f60: v+60.0, f61: v+61.0, f62: v+62.0, f63: v+63.0, f64: v+64.0,
        f65: v+65.0, f66: v+66.0, f67: v+67.0, f68: v+68.0, f69: v+69.0,
        f70: v+70.0, f71: v+71.0, f72: v+72.0, f73: v+73.0, f74: v+74.0,
        f75: v+75.0, f76: v+76.0, f77: v+77.0, f78: v+78.0, f79: v+79.0,
        f80: v+80.0, f81: v+81.0, f82: v+82.0, f83: v+83.0, f84: v+84.0,
        f85: v+85.0, f86: v+86.0, f87: v+87.0, f88: v+88.0, f89: v+89.0,
        f90: v+90.0, f91: v+91.0, f92: v+92.0, f93: v+93.0, f94: v+94.0,
        f95: v+95.0, f96: v+96.0, f97: v+97.0, f98: v+98.0, f99: v+99.0
    }
}
def sum_big(b: BigData) -> Double {
    b.f00 + b.f01 + b.f02 + b.f03 + b.f04 + b.f05 + b.f06 + b.f07 + b.f08 + b.f09 +
    b.f10 + b.f11 + b.f12 + b.f13 + b.f14 + b.f15 + b.f16 + b.f17 + b.f18 + b.f19 +
    b.f20 + b.f21 + b.f22 + b.f23 + b.f24 + b.f25 + b.f26 + b.f27 + b.f28 + b.f29 +
    b.f30 + b.f31 + b.f32 + b.f33 + b.f34 + b.f35 + b.f36 + b.f37 + b.f38 + b.f39 +
    b.f40 + b.f41 + b.f42 + b.f43 + b.f44 + b.f45 + b.f46 + b.f47 + b.f48 + b.f49 +
    b.f50 + b.f51 + b.f52 + b.f53 + b.f54 + b.f55 + b.f56 + b.f57 + b.f58 + b.f59 +
    b.f60 + b.f61 + b.f62 + b.f63 + b.f64 + b.f65 + b.f66 + b.f67 + b.f68 + b.f69 +
    b.f70 + b.f71 + b.f72 + b.f73 + b.f74 + b.f75 + b.f76 + b.f77 + b.f78 + b.f79 +
    b.f80 + b.f81 + b.f82 + b.f83 + b.f84 + b.f85 + b.f86 + b.f87 + b.f88 + b.f89 +
    b.f90 + b.f91 + b.f92 + b.f93 + b.f94 + b.f95 + b.f96 + b.f97 + b.f98 + b.f99
}
def main() -> Double {
    let b = make_big(10.0);
    let s = sum_big(b);
    assert(s == 100.0*10.0 + 99.0*50.0, "sum wrong");
    1.0
}
main()
