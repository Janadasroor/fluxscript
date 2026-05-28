struct Point { x: Double, y: Double }
def get_x(p: Point) -> Double p.x
def call_it() -> Double get_x(Point { x: 5.0, y: 6.0 })
call_it()
