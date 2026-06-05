struct NoCopy ~Copy {
    val: Double
}
def get_val(x: NoCopy) -> Double { x.val }
let a = NoCopy { val: 42.0 };
get_val(a)
