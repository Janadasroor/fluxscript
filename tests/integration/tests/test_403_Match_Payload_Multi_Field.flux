// Test: multi-field enum payload binds to whole struct for field access
enum Point { Pos { x: Double, y: Double } }

def test() -> Double {
    let p = Point.Pos { x: 3.0, y: 4.0 }
    match p {
        Point.Pos(q) -> q.x + q.y
    }
}

assert(test() == 7.0, "Match payload multi-field failed")
1.0
