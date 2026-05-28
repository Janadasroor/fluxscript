trait HasArea {
    def area(self) -> Double
}

trait Describe {
    def describe(self) -> Double
}

struct Circle {
    radius: Double
}

impl HasArea for Circle {
    def area(self) -> Double {
        3.14159 * self.radius * self.radius
    }
}

impl Describe for Circle {
    def describe(self) -> Double {
        self.radius
    }
}

struct Rectangle {
    width: Double,
    height: Double
}

impl HasArea for Rectangle {
    def area(self) -> Double {
        self.width * self.height
    }
}

def print_area[T: HasArea](shape: T) -> Double {
    shape.area()
}

def verify_traits() -> Double {
    let c = Circle { radius: 2.0 };
    let r = Rectangle { width: 3.0, height: 4.0 };

    let circle_area = print_area[Circle](c);
    let rect_area = print_area[Rectangle](r);

    assert(abs(circle_area - 12.56636) < 0.001, "Circle area via trait wrong");
    assert(abs(rect_area - 12.0) < 0.001, "Rectangle area via trait wrong");

    1.0
}

verify_traits()
