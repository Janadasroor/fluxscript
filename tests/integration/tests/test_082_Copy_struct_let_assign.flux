struct NoCopy ~Copy {
    val: Double
}
let a = NoCopy { val: 42.0 };
let b = a;
b.val
