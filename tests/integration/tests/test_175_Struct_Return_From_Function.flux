struct Vec3 { x: Double, y: Double, z: Double }
def make_vec3(x: Double, y: Double, z: Double) -> Vec3 {
    Vec3 { x: x, y: y, z: z }
}
def main() -> Double {
    let v = make_vec3(1.0, 2.0, 3.0);
    assert(v.x == 1.0, "struct return x wrong");
    assert(v.y == 2.0, "struct return y wrong");
    assert(v.z == 3.0, "struct return z wrong");
    1.0
}
main()
