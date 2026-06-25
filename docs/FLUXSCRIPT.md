# FluxScript Documentation

A JIT-compiled scripting language for circuit simulation with compile-time SI unit analysis.

## Table of Contents

1. [Overview](#overview)
2. [Installation](#installation)
3. [CLI Usage](#cli-usage)
4. [Language Basics](#language-basics)
5. [Type System](#type-system)
6. [Functions](#functions)
7. [Control Flow](#control-flow)
8. [Operators](#operators)
9. [Structs](#structs)
10. [Enums](#enums)
11. [Classes](#classes)
12. [Traits](#traits)
13. [Generics](#generics)
14. [Error Handling](#error-handling)
15. [Concurrency](#concurrency)
16. [Standard Library](#standard-library)
17. [SPICE Integration](#spice-integration)
18. [Examples](#examples)

---

## Overview

FluxScript compiles to native code via LLVM 21 ORC JIT with tiered compilation and PGO. The type system enforces SI unit correctness at compile time (7 base dimensions: mass, length, time, current, temperature, amount, luminous).

**Key features:**
- JIT compilation with on-disk caching
- Compile-time SI unit analysis
- Structs, enums, classes, traits with vtable dispatch
- Generics with monomorphization
- Async/await with state machines
- SPICE netlist parsing and simulation
- Matrix/complex number operations
- Interactive REPL

## Installation

```bash
# Build from source
git clone https://github.com/Janadasroor/fluxscript.git
cd fluxscript
make build        # Release build
make build-debug  # Debug build
make test         # Run tests
```

**Dependencies:** LLVM 21, CMake, Ninja, mold linker, ccache.

## CLI Usage

```bash
flux <file.flux>          # JIT execute (default)
flux run <file.flux>      # JIT execute (explicit)
flux check <file.flux>    # Parse + type-check
flux parse <file.flux>    # Syntax-only check
flux fmt <file.flux>      # Format code
flux test [dir]           # Run tests
flux repl                 # Interactive REPL
flux lsp                  # Start LSP server
flux docs [keyword]       # Show/search documentation
flux pkg <cmd>            # Package management
```

**Options:**
```bash
--emit=<mode>    tokens | check | parse-only | llvm | bc | jit
--opt=<0-3>      LLVM optimization level (default: 2)
--cache=0        Disable JIT compilation cache
--entry=<name>   Function to invoke in JIT mode
--profile        Benchmark mode
--version, -v    Print version
```

## Language Basics

### Variables

```flux
let x = 42.0          # immutable binding
var y = 10.0          # mutable binding
let name: String = "hello"  # explicit type hint
```

### Comments

```flux
# This is a comment
```

### Print

```flux
print(42.0)           # print value
println("hello")      # print with newline
print("x = ", x)      # multiple values
```

## Type System

| Type | Description | Size |
|------|-------------|------|
| `Double` | 64-bit floating point (default) | 8 bytes |
| `String` | Heap-allocated string | pointer |
| `Bool` | Boolean (true/false) | 1 byte |
| `Int` | 64-bit integer | 8 bytes |
| `Void` | No return value | 0 bytes |
| `Matrix` | 2D matrix (row-major) | pointer |
| `Vector` | 1D array | pointer |
| `Complex` | Complex number (real + imag) | 16 bytes |

All numeric literals are `Double` by default. String literals use `"double quotes"`.

## Functions

```flux
# Simple function
def add(x: Double, y: Double) -> Double {
    x + y
}

# Extern C function
extern def printf(format: String, ...) -> Double

# Async function
async def fetch_data() -> Double {
    let result = await some_async_op()
    result
}

# Default parameter values
def greet(name: String = "world") -> String {
    "hello " + name
}
```

## Control Flow

### If/Else

```flux
if x > 0.0 then
    println("positive")
else
    println("non-positive")
```

### While

```flux
var i = 0.0
while i < 10.0 {
    println(i)
    i = i + 1.0
}
```

### For

```flux
for i in 0, 10 {
    println(i)  # prints 0, 1, 2, ..., 9
}
```

### Match

```flux
enum Color { Red, Green, Blue }
let c = Color.Red

match c {
    Color.Red -> println("red"),
    Color.Green -> println("green"),
    Color.Blue -> println("blue"),
    default -> println("unknown")
}
```

### Switch (with => syntax)

```flux
switch x {
    1.0 => "one",
    2.0 => "two",
    ~ => "other"
}
```

## Operators

### Arithmetic
`+` `-` `*` `/` `%` (modulo)

### Comparison
`==` `!=` `<` `>` `<=` `>=`

### Logical
`&&` `||` `!` (not)

### Pipe
```flux
5.0 |> double_it      # equivalent to double_it(5.0)
5.0 |> add_one(10.0)  # equivalent to add_one(5.0, 10.0)
```

## Structs

```flux
struct Point {
    x: Double
    y: Double
}

let p = Point { x: 1.0, y: 2.0 }
let px = p.x           # field access

# ~Copy annotation (prevents implicit copying)
struct NoCopy ~Copy {
    data: Double
}
```

### Struct Methods

```flux
struct Vec2 {
    x: Double
    y: Double
}

impl Vec2 {
    def length(self) -> Double {
        sqrt(self.x * self.x + self.y * self.y)
    }
}
```

## Enums

```flux
# Simple enum
enum Direction { North, South, East, West }

# Enum with payload
enum Option {
    Some { value: Double },
    None {}
}

# Enum with positional payload
enum Result {
    Ok(value: Double),
    Err(msg: String)
}

# Usage
let opt = Option.Some { value: 42.0 }
match opt {
    Option.Some { value: v } -> v,
    Option.None {} -> 0.0
}
```

## Classes

```flux
class Counter {
    value: Double

    def increment(self) -> Double {
        self.value = self.value + 1.0
        self.value
    }

    def get(self) -> Double {
        self.value
    }
}

let c = Counter { value: 0.0 }
c.increment()  # 1.0
c.get()        # 1.0
```

## Traits

```flux
trait Drawable {
    def draw(self) -> String
}

struct Circle { radius: Double }
impl Drawable for Circle {
    def draw(self) -> String { "circle" }
}

# Dynamic dispatch
let d: Drawable = Circle { radius: 5.0 }
d.draw()  # "circle"
```

### Associated Types

```flux
trait Container {
    type Item
    def get(self, index: Double) -> Item
}
```

## Generics

```flux
struct Pair[T, U] {
    first: T
    second: U
}

let p = Pair[Double, String] { first: 1.0, second: "hello" }

# Generic function
def identity[T](x: T) -> T { x }
```

## Error Handling

```flux
enum Result { Ok(value: Double), Err(msg: String) }

def safe_divide(a: Double, b: Double) -> Result {
    if (b == 0.0) { Result.Err(msg: "division by zero") }
    else { Result.Ok(value: a / b) }
}

# Using ? operator
def compute() -> Double {
    let r = safe_divide(10.0, 0.0)?
    r  # early return if Err
}
```

### Try/Catch

```flux
try {
    risky_operation()
} catch (e) {
    println("Error: ", e)
}
```

## Concurrency

```flux
# Spawn a thread
let handle = spawn(worker_func, args)

# Join (wait for completion)
let result = join(handle)
```

## Standard Library

### math
`sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `sqrt`, `pow`, `abs`, `log`, `exp`, `pi`, `e`, `max`, `min`, `clamp`, `lerp`, `map_range`, `factorial`, `gcd`, `lcm`, `sigmoid`, `relu`

### trig
`sec`, `csc`, `cot`, `sinc`, `degrees`, `radians`, `haversine`, `vercosine`

### array
`eye`, `ones`, `sum`, `mean`, `linspace`, `logspace`, `arange`, `flatten`, `reshape`, `diff`, `cumsum`, `sort`, `unique`, `clip`, `flipud`, `fliplr`, `repmat`, `trapz`, `meshgrid`

### stats
`variance`, `std`, `geometric_mean`, `harmonic_mean`, `median`, `zscore`, `entropy`, `mad`, `rms`, `mode`, `iqr`, `moment`, `normalize_minmax`, `range_val`

### string
`len`, `at`, `cmp`, `slice`, `find`, `regex`

### signal
`hann`, `hamming`, `blackman`, `bartlett`, `square`, `sawtooth`, `triangle`, `pulse_train`, `chirp`, `fftfreq`, `unwrap`

### cmatrix
`cmatrix`, `complex_eye`, `complex_zeros`, `ctranspose`, `conj`, `complex_inv`, `complex_det`, `complex_trace`

## SPICE Integration

```flux
# Parse SPICE netlist
var comps = matrix_zeros(500, 12)
var ctrl = circuit_create(2, 500)
netlist_parse("circuit.sp", comps, ctrl)

# Build and solve
var sol = mna_dc_solve(comps, ctrl)
var v1 = mna_get_node_voltage(sol, 1.0)
```

## Examples

### Voltage Divider

```flux
import circuit
import mna

def main() {
    var c = circuit_create(2, 500)
    c.add(Component.R { n_plus: 1.0, n_minus: 2.0, r_val: 1000.0 })
    c.add(Component.R { n_plus: 2.0, n_minus: 0.0, r_val: 1000.0 })
    c.add(Component.Vdc { n_plus: 1.0, n_minus: 0.0, v_val: 5.0 })
    var sol = mna_dc_solve(c)
    println(mna_get_node_voltage(sol, 2.0))  # 2.5V
}
main()
```

### Matrix Operations

```flux
let A = matrix_zeros(3, 3)
matrix_set(A, 0, 0, 1.0)
matrix_set(A, 1, 1, 2.0)
matrix_set(A, 2, 2, 3.0)
let det_val = matrix_det(A)  # 6.0
```

### Async Simulation

```flux
async def run_simulation(config: String) -> Double {
    let circuit = parse_netlist(config)
    let result = await transient_solve(circuit)
    result
}
```
