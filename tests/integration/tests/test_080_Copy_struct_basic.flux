struct NoCopy ~Copy {
    val: Double
}
let a = NoCopy { val: 42.0 };
a.val
