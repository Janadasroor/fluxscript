# FluxScript Language Demo
# Standalone example (no external deps) showcasing: enum, match, class,
# generics [T], elif chains, bare return, struct constructors,
# impl methods, try/catch/throw, struct params

# ── Enums ───────────────────────────────────────────────────────────────────

enum Color { Red, Green, Blue }

enum Shape { Circle, Rect, Nothing }

enum Result { Ok, Err }


# ── Structs ─────────────────────────────────────────────────────────────────

struct Point { x: Double, y: Double }
struct Circle { center: Point, radius: Double }
struct Stats { min: Double, max: Double, avg: Double, count: Double }

# ── Generic utilities ───────────────────────────────────────────────────────

def clamp[T](x: T, lo: T, hi: T) -> T {
    if (x < lo) { lo }
    elif (x > hi) { hi }
    else { x }
}

def lerp[T](a: T, b: T, t: Double) -> T {
    a + (b - a) * t
}

# ── Class ───────────────────────────────────────────────────────────────────

class Counter {
    value: Double

    def tick(self) -> Double {
        self.value = self.value + 1.0
        self.value
    }

    def reset(self) {
        self.value = 0.0
    }

}

# ── Impl for Stats ──────────────────────────────────────────────────────────

impl Stats {
    def range(self) -> Double {
        self.max - self.min
    }

    def summary(self) -> Double { self.avg }
}

# ── Generic stat accumulator ────────────────────────────────────────────────

def compute_stats[T](values: Double) -> Stats {
    let n = 5.0;
    let sum = 15.0;
    Stats { min: 1.0, max: 5.0, avg: sum / n, count: n }
}

# ── Match-based shape description ───────────────────────────────────────────

def describe_shape(s: Shape) -> Double {
    match s {
        Shape.Circle -> "circle",
        Shape.Rect -> "rectangle",
        Shape.Nothing -> "nothing",
        default -> "unknown"
    }
}

# ── Area calculation with elif chain ────────────────────────────────────────

def area(radius: Double) -> Double {
    if (radius <= 0.0) {
        0.0
    } elif (radius < 1.0) {
        3.14159 * radius * radius
    } elif (radius < 10.0) {
        3.14159 * radius * radius
    } else {
        3.14159 * radius * radius
    }
}

# ── Color description with elif chain (6-way) ───────────────────────────────

def color_name(c: Color) -> Double {
    match c {
        Color.Red -> "red",
        Color.Green -> "green",
        Color.Blue -> "blue",
        default -> "unknown"
    }
}

# ── Frequency band with elif chain (7-way) ──────────────────────────────────

def band_name(freq: Double) -> Double {
    if (freq < 20.0) {
        "subsonic"
    } elif (freq < 250.0) {
        "bass"
    } elif (freq < 500.0) {
        "low-mid"
    } elif (freq < 2000.0) {
        "mid"
    } elif (freq < 6000.0) {
        "high-mid"
    } elif (freq < 20000.0) {
        "treble"
    } else {
        "ultrasonic"
    }
}

# ── Result handling with match ──────────────────────────────────────────────

def unwrap_or(r: Result, fallback: Double) -> Double {
    match r {
        Result.Ok -> 1.0,
        Result.Err -> 0.0,
        default -> fallback
    }
}

# ── Try/catch around divide ─────────────────────────────────────────────────

def safe_div(a: Double, b: Double) -> Double {
    try {
        if (b == 0.0) {
            throw 1.0
        }
        a / b
    } catch e {
        0.0
    }
}

def double_it(x: Double) -> Double { x * 2.0 }
def half_it(x: Double) -> Double { x / 2.0 }
def process_chain(x: Double) -> Double { half_it(double_it(double_it(x))) }

# ── Struct parameter passing ────────────────────────────────────────────────

def distance(p: Point, q: Point) -> Double {
    let dx = p.x - q.x;
    let dy = p.y - q.y;
    (dx * dx + dy * dy) * 0.5
}

def describe_point(p: Point) -> Double { p.x + p.y }



# ── Main demonstration ──────────────────────────────────────────────────────

def demo() {
    flux_print_string("\n=== FluxScript Language Demo ===\n")

    # 1. Generic functions
    flux_print_string("--- Generics ---\n")
    flux_print_double(clamp(5.0, 0.0, 10.0))
    flux_print_string("  clamp(5, 0, 10)\n")
    flux_print_double(clamp(-3.0, 0.0, 10.0))
    flux_print_string("  clamp(-3, 0, 10)\n")
    flux_print_double(lerp(0.0, 10.0, 0.5))
    flux_print_string("  lerp(0, 10, 0.5)\n")

    # 2. Struct constructors and field access
    flux_print_string("\n--- Structs ---\n")
    p1 = Point { x: 3.0, y: 4.0 }
    p2 = Point { x: 0.0, y: 0.0 }
    flux_print_double(distance(p1, p2))
    flux_print_string("  distance((3,4), (0,0))\n")
    flux_print_double(describe_point(p1))
    flux_print_string("  describe_point\n")

    # 3. Struct as parameter + impl methods
    flux_print_string("\n--- Impl Methods ---\n")
    st = compute_stats[Double](0.0)
    flux_print_double(st.summary())
    flux_print_string("  stats.avg\n")
    flux_print_double(st.range())
    flux_print_string("  stats.range\n")

    # 4. Class with methods
    flux_print_string("\n--- Class ---\n")
    c = Counter { value: 0.0 }
    flux_print_double(c.tick())
    flux_print_string("  counter.tick\n")
    flux_print_double(c.tick())
    flux_print_string("  counter.tick\n")
    c.reset()
    flux_print_double(c.value)
    flux_print_string("  after reset\n")

    # 5. Enum + match
    flux_print_string("\n--- Enum + Match ---\n")
    flux_print_string(color_name(Color.Red))
    flux_print_string("  color_name(Red)\n")
    flux_print_string(color_name(Color.Blue))
    flux_print_string("  color_name(Blue)\n")
    flux_print_string(describe_shape(Shape.Circle))
    flux_print_string("  describe_shape(Circle)\n")

    # 6. Elif chain
    flux_print_string("\n--- Elif Chains ---\n")
    flux_print_string(band_name(100.0))
    flux_print_string("  band_name(100)\n")
    flux_print_string(band_name(1000.0))
    flux_print_string("  band_name(1000)\n")
    flux_print_string(band_name(30000.0))
    flux_print_string("  band_name(30000)\n")
    flux_print_double(area(5.0))
    flux_print_string("  area(5)\n")

    # 7. Try/catch
    flux_print_string("\n--- Try/Catch ---\n")
    flux_print_double(safe_div(10.0, 2.0))
    flux_print_string("  safe_div(10, 2)\n")
    flux_print_double(safe_div(10.0, 0.0))
    flux_print_string("  safe_div(10, 0)\n")

    # 8. Bare return (via computed area guard)
    flux_print_string("\n--- Bare Return ---\n")
    flux_print_double(area(-1.0))
    flux_print_string("  area(-1) -> 0 (bare return guard)\n")

    # 9. Function composition
    flux_print_string("\n--- Composition ---\n")
    flux_print_double(process_chain(5.0))
    flux_print_string("  process_chain(5) = double(double(5))/2\n")

    # 10. Result match
    flux_print_string("\n--- Result ---\n")
    flux_print_double(unwrap_or(Result.Ok, -1.0))
    flux_print_string("  unwrap_or(Ok, -1)\n")
    flux_print_double(unwrap_or(Result.Err, -1.0))
    flux_print_string("  unwrap_or(Err, -1)\n")

    flux_print_string("\n=== All features demonstrated ===\n")
}

demo()
