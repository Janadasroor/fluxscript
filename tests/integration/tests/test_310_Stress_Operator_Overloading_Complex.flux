struct Vec3 { x: Double, y: Double, z: Double }
def vec3_add(a: Vec3, b: Vec3) -> Vec3 {
    Vec3 { x: a.x + b.x, y: a.y + b.y, z: a.z + b.z }
}
def vec3_dot(a: Vec3, b: Vec3) -> Double {
    a.x * b.x + a.y * b.y + a.z * b.z
}
def main() -> Double {
    let a = Vec3 { x: 1.0, y: 2.0, z: 3.0 }
    let b = Vec3 { x: 4.0, y: 5.0, z: 6.0 }
    let s = vec3_add(a, b)
    let d = vec3_dot(a, b)
    assert(s.x == 5.0, "sum x wrong")
    assert(s.y == 7.0, "sum y wrong")
    assert(s.z == 9.0, "sum z wrong")
    assert(d == 32.0, "dot wrong")
    1.0
}
main()
