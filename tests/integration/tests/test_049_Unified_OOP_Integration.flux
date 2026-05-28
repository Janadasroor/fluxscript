enum Status {
    Active,
    Inactive
}

struct Point {
    x: Double,
    y: Double
}

class User {
    id: Double
    pos: Point
    status: Status

    def update_position(self, new_pos: Point) -> Point {
        self.pos = new_pos;
        self.pos
    }

    def is_active(self) -> Double {
        match self.status {
            Status.Active -> 1.0,
            default -> 0.0
        }
    }
}

def verify_unified() -> Double {
    let u = User {
        id: 1.0,
        pos: Point { x: 10.0, y: 20.0 },
        status: Status.Active
    };

    assert(u.is_active() == 1.0, "Enum integration inside class failed");

    let p2 = Point { x: 30.0, y: 40.0 };
    let updated_pos = u.update_position(p2);
    assert(updated_pos.x == 30.0, "Nested struct field update failed");
    assert(u.pos.y == 40.0, "Class state representation failed");

    1.0
}

verify_unified()
