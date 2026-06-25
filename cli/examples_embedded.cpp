// Auto-generated embedded examples for FluxScript CLI.
// Do not edit manually — regenerate from source examples.

#include <string>
#include <sstream>
#include <algorithm>
#include <vector>

namespace Flux {
namespace Examples {

const char* getExamplesText() {
    return R"MARKDOWN(# FluxScript Examples

Curated examples demonstrating FluxScript features. Each example is self-contained and can be run with `flux run example.flux`.

---

## Example 1: Hello World

The simplest FluxScript program.

```flux
println("Hello, FluxScript!")
```

Run: `flux run hello.flux`

---

## Example 2: Variables and Arithmetic

Variables, type inference, and basic math.

```flux
let x = 42.0          # immutable
var y = 10.0          # mutable
y = y + x             # mutation
println(y)            # 52.0

let name = "FluxScript"
println("Language: " + name)
```

---

## Example 3: Functions

Defining and calling functions with type annotations.

```flux
def add(x: Double, y: Double) -> Double {
    x + y
}

def factorial(n: Double) -> Double {
    if n <= 1.0 then 1.0
    else n * factorial(n - 1.0)
}

println(add(2.0, 3.0))        # 5.0
println(factorial(5.0))        # 120.0
```

---

## Example 4: Control Flow

if/else, while, for loops with the `do` keyword.

```flux
# If/else expression
let x = 5.0
if x > 0.0 then
    println("positive")
else
    println("non-positive")

# While loop
var sum = 0.0
var i = 0.0
while i < 10.0 do {
    sum = sum + i
    i = i + 1.0
}
println("Sum 0..9 = " + str(sum))  # 45.0

# For loop (exclusive end)
var product = 1.0
for i in 1, 5 do {
    product = product * i
}
println("Product 1..4 = " + str(product))  # 24.0

# Break and continue
var count = 0.0
for i in 0, 100 do {
    if i == 10.0 then break
    if i mod 2.0 == 0.0 then continue
    count = count + 1.0
}
println("Odd numbers below 10: " + str(count))  # 5.0
```

---

## Example 5: Pattern Matching

Match expressions with enum patterns, payloads, and default.

```flux
enum Color { Red, Green, Blue }

def describe(c: Color) -> String {
    match c {
        Color.Red -> "Stop",
        Color.Green -> "Go",
        Color.Blue -> "Yield",
        default -> "Unknown"
    }
}

println(describe(Color.Red))    # Stop
println(describe(Color.Green))  # Go
```

---

## Example 6: Enums with Payloads

Enums carrying data, matched with pattern bindings.

```flux
enum Shape {
    Circle { radius: Double },
    Rect { width: Double, height: Double },
    Triangle { base: Double, height: Double }
}

def area(s: Shape) -> Double {
    match s {
        Shape.Circle(payload) -> 3.14159 * payload.radius * payload.radius,
        Shape.Rect(payload) -> payload.width * payload.height,
        Shape.Triangle(payload) -> 0.5 * payload.base * payload.height
    }
}

let c = Shape.Circle { radius: 5.0 }
let r = Shape.Rect { width: 3.0, height: 4.0 }
println("Circle area: " + str(area(c)))   # 78.54
println("Rect area: " + str(area(r)))     # 12.0
```

---

## Example 7: Structs and Methods

Struct definitions, construction, field access, and impl methods.

```flux
struct Vec2 {
    x: Double,
    y: Double
}

impl Vec2 {
    def length(self) -> Double {
        sqrt(self.x * self.x + self.y * self.y)
    }

    def add(self, other: Vec2) -> Vec2 {
        Vec2 { x: self.x + other.x, y: self.y + other.y }
    }
}

let a = Vec2 { x: 3.0, y: 4.0 }
let b = Vec2 { x: 1.0, y: 2.0 }
println("Length of a: " + str(a.length()))  # 5.0
let c = a.add(b)
println("a + b = (" + str(c.x) + ", " + str(c.y) + ")")  # (4.0, 6.0)
```

---

## Example 8: Classes

Classes combine data and methods. Desugared to struct+impl at compile time.

```flux
class Counter {
    value: Double

    def increment(self: Counter) -> Double {
        self.value = self.value + 1.0
        self.value
    }

    def get(self: Counter) -> Double {
        self.value
    }

    def reset(self: Counter) {
        self.value = 0.0
    }
}

let c = Counter { value: 0.0 }
println(c.increment())  # 1.0
println(c.increment())  # 2.0
println(c.get())        # 2.0
c.reset()
println(c.get())        # 0.0
```

---

## Example 9: Traits and Dynamic Dispatch

Trait definitions, implementations, and vtable dispatch.

```flux
trait Drawable {
    def draw(self) -> String
    def area(self) -> Double
}

struct Circle {
    radius: Double
}

impl Drawable for Circle {
    def draw(self) -> String {
        "Circle(r=" + str(self.radius) + ")"
    }
    def area(self) -> Double {
        3.14159 * self.radius * self.radius
    }
}

struct Square {
    side: Double
}

impl Drawable for Square {
    def draw(self) -> String {
        "Square(s=" + str(self.side) + ")"
    }
    def area(self) -> Double {
        self.side * self.side
    }
}

# Dynamic dispatch through trait
let shapes: Drawable[] = [
    Circle { radius: 5.0 },
    Square { side: 3.0 }
]
for s in shapes do {
    println(s.draw() + " area=" + str(s.area()))
}
```

---

## Example 10: Generics

Generic functions and structs with type parameters.

```flux
struct Pair[T, U] {
    first: T,
    second: U
}

def swap[T, U](p: Pair[T, U]) -> Pair[U, T] {
    Pair { first: p.second, second: p.first }
}

def identity[T](x: T) -> T {
    x
}

let p = Pair { first: 1.0, second: "hello" }
let s = swap(p)
println(str(s.first) + " " + str(s.second))  # hello 1.0
println(identity[Double](42.0))                 # 42.0
```

---

## Example 11: Pipe Operator

Function composition with the pipe operator.

```flux
def double(x: Double) -> Double { x * 2.0 }
def add_one(x: Double) -> Double { x + 1.0 }
def negate(x: Double) -> Double { -x }

# Basic piping
println(5.0 |> double |> add_one)    # 11.0

# Partial application
def add(a: Double, b: Double) -> Double { a + b }
println(10.0 |> add(5.0))            # 15.0

# Method piping
struct Box { val: Double }
impl Box {
    def scale(self, f: Double) -> Double { self.val * f }
}
let b = Box { val: 3.0 }
println(5.0 |> b.scale)              # 15.0
```

---

## Example 12: Error Handling

Result enum with ? operator for error propagation.

```flux
enum Result {
    Ok { value: Double },
    Err { msg: String }
}

def divide(a: Double, b: Double) -> Result {
    if b == 0.0 then
        Result.Err { msg: "division by zero" }
    else
        Result.Ok { value: a / b }
}

def safe_compute() -> Double {
    let r1 = divide(10.0, 2.0)?    # returns 5.0 or early returns Err
    let r2 = divide(100.0, r1)?    # returns 20.0
    r1 + r2                         # 25.0
}

println(safe_compute())  # 25.0
```

---

## Example 13: Matrix Operations

Creating and manipulating matrices.

```flux
import array

# Create matrices
let A = matrix_zeros(3, 3)
matrix_set(A, 0, 0, 1.0)
matrix_set(A, 1, 1, 2.0)
matrix_set(A, 2, 2, 3.0)

# Matrix operations
println("Trace: " + str(matrix_trace(A)))      # 6.0
println("Det: " + str(matrix_det(A)))           # 6.0

# Identity matrix
let I = eye(3.0)
println("Eye trace: " + str(matrix_trace(I)))  # 3.0

# Matrix arithmetic
let B = matrix_scale(A, 2.0)
println("2*A[0,0] = " + str(matrix_get(B, 0, 0)))  # 2.0
```

---

## Example 14: Async/Await

Asynchronous functions with state machines.

```flux
async def fetch_value(delay: Double) -> Double {
    await delay
    42.0
}

async def compute() -> Double {
    let a = await fetch_value(0.1)
    let b = await fetch_value(0.1)
    a + b
}

def main() -> Double {
    let result = compute()
    println("Async result: " + str(result))  # 84.0
    0.0
}
main()
```

---

## Example 15: Circuit Simulation — Voltage Divider

Basic circuit simulation with two resistors and a voltage source.

```flux
import circuit
import mna

def main() {
    var c = circuit_create(2, 500)
    # Two 1k resistors in series with 5V source
    c.add(Component.R { n_plus: 1.0, n_minus: 2.0, r_val: 1000.0 })
    c.add(Component.R { n_plus: 2.0, n_minus: 0.0, r_val: 1000.0 })
    c.add(Component.Vdc { n_plus: 1.0, n_minus: 0.0, v_val: 5.0 })

    var sol = mna_dc_solve(c)
    var v1 = mna_get_node_voltage(sol, 1.0)
    var v2 = mna_get_node_voltage(sol, 2.0)
    println("V1 = " + str(v1) + " V")  # 5.0
    println("V2 = " + str(v2) + " V")  # 2.5
}
main()
```

Run: `flux run voltage_divider.flux`

---

## Example 16: Circuit Simulation — RC Low-Pass Filter

Resistor-capacitor circuit with DC analysis.

```flux
import circuit
import mna

def main() {
    var c = circuit_create(2, 500)
    c.add(Component.R { n_plus: 1.0, n_minus: 2.0, r_val: 1000.0 })
    c.add(Component.C { n_plus: 2.0, n_minus: 0.0, c_val: 1e-6, init_v: 0.0 })
    c.add(Component.Vdc { n_plus: 1.0, n_minus: 0.0, v_val: 5.0 })

    var sol = mna_dc_solve(c)
    var v_out = mna_get_node_voltage(sol, 2.0)
    println("V_out = " + str(v_out) + " V")  # 5.0 (capacitor open at DC)
}
main()
```

---

## Example 17: SPICE Netlist Parsing

Parse a SPICE netlist file and solve.

```flux
import circuit
import mna
import netlist

def main() {
    var ctrl = circuit_create(50, 500)
    var N = netlist_parse("circuit.sp", ctrl.get_comps(), ctrl.get_ctrl())
    println("Parsed " + str(N) + " nodes")

    var sol = mna_dc_solve(ctrl)
    var v1 = mna_get_node_voltage(sol, 1.0)
    println("V(1) = " + str(v1) + " V")
}
main()
```

---

## Example 18: Nonlinear DC Solve

Diode circuit with Newton-Raphson iteration.

```flux
import circuit
import mna
import dcsolve

def main() {
    var ctrl = circuit_create(2, 500)
    circuit_add_vdc(ctrl, 1, 0, 5.0)
    circuit_add_resistor(ctrl, 1, 2, 1000.0)
    circuit_add_diode(ctrl, 2, 0, 1e-14, 1.0, 0.02585)

    var sol = dc_solve(ctrl, 30.0, 1e-9)
    var vd = mna_get_node_voltage(sol, 2.0)
    println("Diode voltage: " + str(vd) + " V")  # ~0.6-0.7V
}
main()
```

---

## Example 19: Transient Simulation

Time-domain simulation of an RC circuit.

```flux
import circuit
import trsim

def main() {
    var ctrl = circuit_create(3, 20)
    circuit_add_vsin(ctrl, 1, 0, 0.0, 5.0, 1000.0, 0.0)
    circuit_add_diode(ctrl, 1, 2, 1e-14, 1.0, 0.02585)
    circuit_add_resistor(ctrl, 2, 3, 1000.0)
    circuit_add_capacitor(ctrl, 3, 0, 10e-6, 0.0)

    var results = tr_solve(ctrl, 0.0, 0.003, 1e-5)
    var npts = floor(0.003 / 1e-5) + 1.0
    println("Simulation points: " + str(npts))
    println("Transient simulation complete")
}
main()
```

---

## Example 20: Concurrency

Spawn threads and join for parallel computation.

```flux
def compute_sum(n: Double) -> Double {
    var sum = 0.0
    var i = 0.0
    while i < n do {
        sum = sum + i
        i = i + 1.0
    }
    sum
}

def main() {
    let h1 = spawn(compute_sum, [1000.0])
    let h2 = spawn(compute_sum, [2000.0])
    let r1 = join(h1)
    let r2 = join(h2)
    println("Sum 1: " + str(r1))  # 499500.0
    println("Sum 2: " + str(r2))  # 1999000.0
    println("Total: " + str(r1 + r2))
}
main()
```

---

## Example 21: Standard Library — Math

Using math functions from the standard library.

```flux
import math

def main() {
    println("pi = " + str(pi()))           # 3.14159...
    println("e = " + str(e()))             # 2.71828...
    println("sqrt(2) = " + str(sqrt(2.0))) # 1.41421...
    println("sin(pi/2) = " + str(sin(1.5708)))  # 1.0
    println("log(100) = " + str(log(100.0)))    # 4.60517...
    println("pow(2,10) = " + str(pow(2.0, 10.0)))  # 1024.0
    println("clamp(15, 0, 10) = " + str(clamp(15.0, 0.0, 10.0)))  # 10.0
    println("lerp(0, 10, 0.3) = " + str(lerp(0.0, 10.0, 0.3)))    # 3.0
}
main()
```

---

## Example 22: Standard Library — Arrays

Matrix and array operations.

```flux
import array

def main() {
    # Create and manipulate matrices
    let A = eye(3.0)
    println("Identity 3x3:")
    println(A)

    # Linspace
    let t = linspace(0.0, 1.0, 5.0)
    println("linspace(0,1,5): " + str(t))

    # Matrix operations
    let B = matrix_zeros(2, 2)
    matrix_set(B, 0, 0, 1.0)
    matrix_set(B, 0, 1, 2.0)
    matrix_set(B, 1, 0, 3.0)
    matrix_set(B, 1, 1, 4.0)
    println("Det = " + str(matrix_det(B)))  # -2.0
    println("Sum = " + str(sum(B)))          # 10.0
    println("Mean = " + str(mean(B)))        # 2.5
}
main()
```

---

## Example 23: Standard Library — Statistics

Statistical functions on data.

```flux
import stats

def main() {
    let data = [1.0, 2.0, 3.0, 4.0, 5.0]
    println("Mean: " + str(mean(data)))      # 3.0
    println("Std: " + str(std(data)))        # ~1.414
    println("Median: " + str(median(data)))  # 3.0
    println("RMS: " + str(rms(data)))        # ~3.162
    println("Min: " + str(min_element(data))) # 1.0
    println("Max: " + str(max_element(data))) # 5.0
}
main()
```

---

## Example 24: Standard Library — Trigonometry

Extended trigonometric functions.

```flux
import trig

def main() {
    let angle = 1.0  # radians
    println("sin(1) = " + str(sin(angle)))
    println("cos(1) = " + str(cos(angle)))
    println("tan(1) = " + str(tan(angle)))
    println("sec(1) = " + str(sec(angle)))
    println("csc(1) = " + str(csc(angle)))
    println("cot(1) = " + str(cot(angle)))
    println("degrees(pi) = " + str(degrees(pi())))  # 180.0
    println("radians(90) = " + str(radians(90.0)))  # pi/2
}
main()
```

---

## Example 25: Error Handling with Try/Catch

```flux
def risky_divide(a: Double, b: Double) -> Double {
    if b == 0.0 then
        error("division by zero")
    else
        a / b
}

def main() {
    try {
        println(risky_divide(10.0, 2.0))   # 5.0
        println(risky_divide(10.0, 0.0))   # throws
    } catch (e) {
        println("Caught: " + str(e))
    }
    println("Program continues")
}
main()
```

---

## Example 26: Vector Operations

Working with vector literals and indexing.

```flux
def main() {
    let v = [1.0, 2.0, 3.0, 4.0, 5.0]
    println("Length: " + str(len(v)))   # 5
    println("v[0] = " + str(v[0]))      # 1.0
    println("v[2] = " + str(v[2]))      # 3.0
    println("v[4] = " + str(v[4]))      # 5.0

    # Vector comparison
    let a = [1.0, 2.0]
    let b = [1.0, 2.0]
    println("a == b: " + str(a == b))   # 1.0 (true)
}
main()
```

---

## Example 27: Complex Numbers

Complex number operations.

```flux
def main() {
    let z1 = complex(3.0, 4.0)   # 3 + 4i
    let z2 = complex(1.0, 2.0)   # 1 + 2i

    let sum = z1 + z2
    println("z1 + z2 = " + str(real(sum)) + " + " + str(imag(sum)) + "i")  # 4 + 6i

    let prod = z1 * z2
    println("z1 * z2 = " + str(real(prod)) + " + " + str(imag(prod)) + "i")  # -5 + 10i

    println("|z1| = " + str(abs(z1)))  # 5.0
}
main()
```

---

## Example 28: Complete Circuit Example

A full circuit simulation with multiple components.

```flux
import circuit
import mna

def main() {
    # Build a voltage divider with bypass capacitor
    var c = circuit_create(3, 500)
    c.add(Component.Vdc { n_plus: 1.0, n_minus: 0.0, v_val: 10.0 })
    c.add(Component.R { n_plus: 1.0, n_minus: 2.0, r_val: 1000.0 })
    c.add(Component.R { n_plus: 2.0, n_minus: 0.0, r_val: 2000.0 })
    c.add(Component.C { n_plus: 2.0, n_minus: 0.0, c_val: 1e-6, init_v: 0.0 })

    # DC analysis
    var sol = mna_dc_solve(c)
    var v1 = mna_get_node_voltage(sol, 1.0)
    var v2 = mna_get_node_voltage(sol, 2.0)
    println("=== Voltage Divider with Bypass Cap ===")
    println("V1 (source) = " + str(v1) + " V")
    println("V2 (output) = " + str(v2) + " V")
    println("Expected V2 = " + str(10.0 * 2000.0 / 3000.0) + " V")

    # Verify result
    var expected = 10.0 * 2000.0 / 3000.0
    var error_pct = abs(v2 - expected) / expected * 100.0
    println("Error: " + str(error_pct) + "%")
}
main()
```

---

## Example 29: Interactive REPL Examples

The FluxScript REPL supports expressions and definitions:

```flux
# In the REPL (flux repl):
> 2 + 3
5.0

> let x = 42.0
> x * 2
84.0

> def double(x) { x * 2.0 }
> double(21.0)
42.0

> :load myscript.flux
> :cache off
> :quit
```

---

## Quick Reference

| Feature | Syntax |
|---------|--------|
| Variable | `let x = 1.0` / `var x = 1.0` |
| Function | `def name(args) -> RetType { body }` |
| If/else | `if cond then a else b` |
| While | `while cond do { body }` |
| For | `for i in start, end do { body }` |
| Match | `match expr { Pattern -> result }` |
| Struct | `struct Name { field: Type }` |
| Enum | `enum Name { Variant { field: Type } }` |
| Class | `class Name { field: Type; def method(self) { ... } }` |
| Trait | `trait Name { def method(self) -> Type; }` |
| Generic | `def fn[T](x: T) -> T { x }` |
| Pipe | `value \|> function` |
| Import | `import module_name` |
| Print | `println(value)` |
| Assert | `assert(condition, "message")` |
| Spawn | `let h = spawn(func, args)` |
| Join | `let r = join(h)` |
| Async | `async def name() { await expr }` |
| Try/Catch | `try { ... } catch (e) { ... }` |
| Error prop | `result?` (early return on Err) |
)MARKDOWN";
}

std::string searchExamples(const std::string& keyword) {
    std::string text = getExamplesText();
    std::istringstream stream(text);
    std::string line;
    int lineNum = 0;
    std::vector<std::pair<int, std::string>> matches;
    while (std::getline(stream, line)) {
        lineNum++;
        std::string lowerLine = line;
        std::string lowerKeyword = keyword;
        std::transform(lowerLine.begin(), lowerLine.end(), lowerLine.begin(), ::tolower);
        std::transform(lowerKeyword.begin(), lowerKeyword.end(), lowerKeyword.begin(), ::tolower);
        if (lowerLine.find(lowerKeyword) != std::string::npos) {
            matches.push_back({lineNum, line});
        }
    }
    if (matches.empty()) return "No examples found for: " + keyword + "\n";
    stream.clear(); stream.seekg(0);
    std::vector<std::string> allLines;
    while (std::getline(stream, line)) allLines.push_back(line);
    std::string result;
    for (auto& [num, matchLine] : matches) {
        int start = std::max(0, num - 3);
        int end = std::min((int)allLines.size(), num + 2);
        for (int i = start; i < end; i++) {
            std::string prefix = (i == num - 1) ? ">> " : "   ";
            result += prefix + std::to_string(i + 1) + "  " + allLines[i] + "\n";
        }
        result += "\n";
    }
    result += std::to_string(matches.size()) + " match(es) found.\n";
    return result;
}

} // namespace Examples
} // namespace Flux
