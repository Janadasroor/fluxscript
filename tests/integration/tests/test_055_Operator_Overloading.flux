trait Add {
    def add(self, rhs: Complex) -> Complex
}

trait Sub {
    def sub(self, rhs: Complex) -> Complex
}

trait Mul {
    def mul(self, rhs: Complex) -> Complex
}

trait Neg {
    def neg(self) -> Complex
}

trait Eq {
    def eq(self, other: Complex) -> Bool
}

struct Complex { re: Double, im: Double }

impl Add for Complex {
    def add(self, rhs: Complex) -> Complex {
        Complex { re: self.re + rhs.re, im: self.im + rhs.im }
    }
}

impl Sub for Complex {
    def sub(self, rhs: Complex) -> Complex {
        Complex { re: self.re - rhs.re, im: self.im - rhs.im }
    }
}

impl Mul for Complex {
    def mul(self, rhs: Complex) -> Complex {
        let r1 = self.re * rhs.re;
        let r2 = self.im * rhs.im;
        let i1 = self.re * rhs.im;
        let i2 = self.im * rhs.re;
        Complex { re: r1 - r2, im: i1 + i2 }
    }
}

impl Neg for Complex {
    def neg(self) -> Complex {
        Complex { re: -self.re, im: -self.im }
    }
}

impl Eq for Complex {
    def eq(self, other: Complex) -> Bool {
        (abs(self.re - other.re) < 0.0001) && (abs(self.im - other.im) < 0.0001)
    }
}

def verify_overloads() -> Double {
    let a = Complex { re: 1.0, im: 2.0 };
    let b = Complex { re: 3.0, im: 4.0 };
    let sum = a + b;
    let diff = a - b;
    let prod = a * b;
    let neg_a = -a;
    let eq_check = a == a;
    let ne_check = a == b;

    assert(abs(sum.re - 4.0) < 0.0001, "sum.re");
    assert(abs(sum.im - 6.0) < 0.0001, "sum.im");
    assert(abs(diff.re + 2.0) < 0.0001, "diff.re");
    assert(abs(diff.im + 2.0) < 0.0001, "diff.im");
    assert(abs(prod.re + 5.0) < 0.0001, "prod.re");
    assert(abs(prod.im - 10.0) < 0.0001, "prod.im");
    assert(abs(neg_a.re + 1.0) < 0.0001, "neg_a.re");
    assert(abs(neg_a.im + 2.0) < 0.0001, "neg_a.im");
    assert(eq_check != 0.0, "eq");
    assert(ne_check == 0.0, "ne");

    1.0
}

verify_overloads()
