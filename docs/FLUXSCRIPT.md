# FluxScript Documentation

A JIT-compiled scripting language for circuit simulation with compile-time SI unit analysis.

## Table of Contents

1. Types, Structs, and Enums
2. Classes and Traits
3. Control Flow and Error Handling
4. Advanced Features
5. Standard Library and SPICE Integration
6. Examples

---

# Types, Structs, and Enums

## Primitive Types

FluxScript has the following built-in types. The default type for numeric literals is `Double`.

| Type | Kind | LLVM Representation | Size | Description |
|------|------|-------------------|------|-------------|
| `Double` | `Double` | `double` | 8 bytes | 64-bit IEEE 754 floating point. Default type for all numeric literals. |
| `Float` | `Float` | `float` | 4 bytes | Single-precision floating point. |
| `Int` | `Int` | `i32` | 4 bytes | 32-bit signed integer. |
| `Bool` | `Bool` | `i1` | 1 byte | Boolean — `true` or `false`. |
| `Void` | `Void` | `void` | 0 bytes | No return value. Used for side-effect-only functions. |
| `String` | `String` | opaque handle | pointer | Heap-allocated string. Internally an opaque handle (`double` / `i8*`). |
| `Complex` | `Complex` | `<2 x double>` | 16 bytes | Complex number. Two doubles: real and imaginary parts (SSE2-native vector). |
| `Matrix` | `Matrix` | `{ void*, i32, i32 }` | pointer | 2D row-major matrix. Heap-allocated with dimensions. |
| `Vector` | `Vector` | `{ double*, i32 }` | pointer | 1D array. Heap-allocated with length. |
| `Symbolic` | `Symbolic` | `double` / `uintptr_t` | — | Symbolic expression handle. |
| `Fixed` | `Fixed` | `i<N>` | N bits | Fixed-point integer in Q format. Configurable total bits and fractional bits. |

### Unit-Stamped Types

All numeric types carry optional SI unit dimensions (7 base dimensions: mass, length, time, current, temperature, amount, luminous). Unit dimension names like `Voltage`, `Current`, `Resistance`, `Power`, `Force`, `Pressure`, `Frequency`, `Energy`, `Time`, `Length`, `Mass`, `Temperature`, `Charge`, `Conductance`, `Capacitance`, `Inductance` resolve to `Double` with the correct SI base dimensions. The type checker verifies dimensional consistency across operations.

```flux
let v: Voltage = 5.0
let i: Current = 0.5
let p = v * i   # type-checked: Voltage * Current = Power
```

---

## Structs

Structs are named product types with labeled fields.

### Definition

Fields are separated by commas or newlines:

```flux
struct Point {
    x: Double, y: Double
}

struct Inner { val: Double }
struct Outer { inner: Inner, scale: Double }
```

### Construction (Braced Constructor)

Struct instances are created using braced field initialization. The struct name is followed by `{ field: value, ... }`:

```flux
let p = Point { x: 1.0, y: 2.0 }
let inner = Inner { val: 5.0 }
let outer = Outer { inner: inner, scale: 2.0 }
```

An explicit type annotation on the variable is optional but supported:

```flux
let p: Pair = Pair { x: 3.0, y: 4.0 }
```

### Field Access

Fields are accessed with dot notation:

```flux
let p = Point { x: 1.0, y: 2.0 }
p.x     # 1.0
p.y     # 2.0

# Nested field access
outer.inner.val     # 5.0
outer.scale         # 2.0
```

### Mutation

Struct fields are mutable only when the struct binding is declared with `var`. Immutable bindings (`let`) cannot be mutated.

```flux
var p = Point { x: 1.0, y: 2.0 }
p.x = 10.0
p.y = p.x * 2.0    # p.y is now 20.0
```

```flux
let p = Point { x: 1.0, y: 2.0 }
p.x = 5.0          # ERROR: cannot mutate let binding
```

### Struct as Function Parameter

Structs are passed to functions by value (or by reference for large structs >16 bytes):

```flux
struct Pair { a: Double, b: Double }

def sum_pair(p: Pair) -> Double { p.a + p.b }

let p = Pair { a: 10.0, b: 20.0 }
sum_pair(p)     # 30.0
```

### Struct Return from Functions

Functions can return struct values. The return type is inferred from `StructConstructExprAST` bodies:

```flux
struct Vec3 { x: Double, y: Double, z: Double }

def make_vec3(x: Double, y: Double, z: Double) -> Vec3 {
    Vec3 { x: x, y: y, z: z }
}

let v = make_vec3(1.0, 2.0, 3.0)
v.x     # 1.0
v.y     # 2.0
v.z     # 3.0
```

### Struct Methods (`impl`)

Methods are attached to types with `impl` blocks. The first parameter must be `self` with an explicit type annotation:

```flux
struct Box { val: Double }

impl Box {
    def scale(self: Box, f: Double) -> Double { self.val * f }
}

let b: Box = Box { val: 3.0 }
b.scale(2.0)    # 6.0
```

Methods are called with dot syntax: `receiver.method(args)`.

### `~Copy` Annotation

By default, structs are **Copy** — they can be implicitly copied when assigned to a new variable or passed to a function. The `~Copy` annotation disables this, making the type **move-only**:

```flux
struct NoCopy ~Copy {
    val: Double
}

let a = NoCopy { val: 42.0 }
let b = a         # move — a is no longer accessible
b.val             # 42.0
```

When `~Copy` is present, the type cannot be implicitly duplicated. Passing a `~Copy` value to a function transfers ownership:

```flux
struct NoCopy ~Copy {
    val: Double
}

def get_val(x: NoCopy) -> Double { x.val }

let a = NoCopy { val: 42.0 }
get_val(a)      # 42.0 — ownership transferred to get_val
```

Without `~Copy`, the same pattern copies the value, and both `a` and the function parameter remain valid:

```flux
struct Copyable {
    val: Double
}

let a = Copyable { val: 42.0 }
let b = a         # copy — both a and b are valid
b.val             # 42.0
```

### Reference Fields

Structs can hold references to other values:

```flux
struct Wrapper { r: &Double }

def main() -> Double {
    let x = 42.0
    let w = Wrapper { r: &x }
    w.r     # 42.0
}
```

---

## Enums

Enums are tagged unions — a type with a fixed set of named variants, each optionally carrying payload data.

### Simple Variants (No Payload)

Variants with no data:

```flux
enum Color { Red, Green, Blue }

let c = Color.Green
```

### Positional Payload Variants

Variants with a single unnamed payload field use parenthesized syntax in the definition:

```flux
enum Option { Some(value: Double), None }

let x = Option.Some(42.0)
```

### Braced Payload Variants

Variants with named fields use braced syntax in the definition:

```flux
enum Option { Some { value: Double }, None }

let x = Option.Some { value: 42.0 }
```

### Multi-Field Payloads

Variants can carry multiple fields. Both positional and braced styles are supported:

```flux
# Braced multi-field
enum Person { Named { age: Double, score: Double }, Empty }

let p = Person.Named { age: 25.0, score: 98.0 }
```

```flux
# Mixed positional and braced in the same enum
enum Shape { Circle(r: Double), Rect(w: Double, h: Double), Empty }

let c = Shape.Circle(2.0)
let r = Shape.Rect { w: 3.0, h: 4.0 }
```

Multi-field positional payloads are also supported:

```flux
enum Shape { Circle { radius: Double }, Rect { w: Double, h: Double }, Nothing }

let c = Shape.Circle(2.0)
let r = Shape.Rect(3.0, 4.0)
```

### `~Copy` Enums

Enums can also be annotated with `~Copy` to make them move-only:

```flux
enum Opt ~Copy { Some { val: Double }, None }

let x = Opt.Some { val: 42.0 }
match x {
    Opt.Some(v) -> v,
    Opt.None -> 0.0
}
```

### Enum as Function Parameter

Enums are passed to functions by value:

```flux
enum Opt { Some { value: Double }, None }

def unwrap_or_default(x: Opt, def_val: Double) -> Double {
    match x {
        Opt.Some(v) -> v,
        default -> def_val
    }
}

let a = Opt.Some(42.0)
unwrap_or_default(a, 0.0)     # 42.0
```

### Enum Nested in Struct

Enums can be fields within structs:

```flux
enum Status { Active { id: Double }, Inactive }
struct Record { name: Double, status: Status }

let r = Record { name: 1.0, status: Status.Active(100.0) }
r.status     # Status.Active { id: 100.0 }
```

### `match` Expression

The `match` expression destructures enum values by variant. Each arm uses `Variant(payload) -> body` syntax:

```flux
enum Color { Red, Green, Blue }

let c = Color.Green

match c {
    Color.Red -> 1.0,
    Color.Green -> 2.0,
    Color.Blue -> 3.0
}   # 2.0
```

For variants with payloads, the payload is bound to a variable:

```flux
enum Option { Some(value: Double), None }

let x = Option.Some(42.0)

match x {
    Option.Some(v) -> v,
    Option.None -> 0.0
}   # 42.0
```

For multi-field braced payloads, the entire payload struct is bound to a single variable:

```flux
enum Person { Named { age: Double, score: Double }, Empty }

let p = Person.Named { age: 25.0, score: 98.0 }

match p {
    Person.Named(pl) -> pl.score,
    Person.Empty -> 0.0
}   # 98.0
```

Similarly for multi-field positional payloads:

```flux
enum Shape { Circle(r: Double), Rect(w: Double, h: Double), Empty }

let s = Shape.Rect { w: 3.0, h: 4.0 }

match s {
    Shape.Circle(r) -> r,
    Shape.Rect(pl) -> pl.w * pl.h,
    Shape.Empty -> 0.0
}   # 12.0
```

When all shapes of a variant are positional:

```flux
enum Shape { Circle { radius: Double }, Rect { w: Double, h: Double }, Nothing }

let c = Shape.Circle(2.0)

match c {
    Shape.Circle(r) -> 3.14159 * r * r,
    Shape.Rect(w, h) -> w * h,
    default -> 0.0
}   # ~12.566
```

### `default` Arm

A `default` catch-all arm matches any variant not previously matched:

```flux
enum Opt { Some { value: Double }, None }

let x = Opt.None

match x {
    Opt.Some(v) -> v,
    default -> 0.0
}   # 0.0
```

### Enum `switch`/`case` (Alternative Syntax)

Enums can also be matched with `switch`/`case`:

```flux
let a = 3.0

switch (a) {
    1.0 => 10.0,
    2.0 => 20.0,
    3.0 => 30.0,
    ~ => -1.0
}   # 30.0
```

---

## Generics

### Generic Structs

Structs can declare type parameters in square brackets `[T, U, ...]`. Generic parameters must appear in field types:

```flux
struct Pair[T] { first: Double, second: Double }

let p: Pair = Pair { first: 1.0, second: 2.0 }
p.first     # 1.0
```

Multiple type parameters:

```flux
struct KeyValue[K, V] { key: K, value: V }
```

The compiler performs **monomorphization** — each unique set of concrete type arguments produces a specialized copy of the struct. Type arguments are inferred from the constructor when possible:

```flux
struct Result[T, U] { ok: T, err: U }
```

### Generic Enums

Enums can also be generic:

```flux
enum Option[T] { Some { value: T }, None }
```

### Generic Functions

Functions use bracket syntax `[T]` for type parameters:

```flux
def identity[T](x: T) -> T { x }

identity[Double](42.0)   # explicit
identity(42.0)           # inferred
```

Type arguments can be nested for multi-parameter generics:

```flux
def pair[T, U](a: T, b: U) -> Pair[T, U] {
    Pair { first: a, second: b }
}
```

---

## Type Inference Rules

FluxScript infers types in the following cases:

1. **Default numeric type**: All numeric literals are `Double` unless constrained by context.

2. **`let` bindings**: The type is inferred from the initializer expression:
   ```flux
   let x = 42.0          # Double
   let p = Point { x: 1.0, y: 2.0 }  # Point
   ```

3. **`var` bindings**: Same as `let`, but the variable is mutable.

4. **Explicit type annotation**: Overrides inference:
   ```flux
   let x: Int = 42.0
   let p: Pair = Pair { first: 1.0, second: 2.0 }
   ```

5. **Function return types**: Must be explicitly annotated with `-> Type`. The exception is functions whose body is a `StructConstructExprAST` — the return type is inferred as `UserStruct`:
   ```flux
   def make_vec3(x: Double, y: Double, z: Double) -> Vec3 {
       Vec3 { x: x, y: y, z: z }
   }
   ```

6. **Generic type arguments**: Inferred from usage context when not explicitly provided:
   ```flux
   def identity[T](x: T) -> T { x }
   identity(42.0)     # T inferred as Double
   ```

7. **`match` arm types**: All arms must return the same type. The type is unified across all branches.

8. **Struct field types**: Explicitly annotated in the struct definition — no inference for field types.

9. **Enum variant payloads**: Explicitly annotated in the enum definition — no inference for payload types.

10. **Reference types**: `&x` produces `Ref(Double)` if `x` is `Double`. The inner type is inferred from the referent:
    ```flux
    let x = 42.0
    let r = &x    # type is &Double
    ```
# Classes and Traits

FluxScript provides classes (syntactic sugar over structs + impls), traits (interfaces with vtable dispatch), and associated types. All examples below are verifiable against the integration test suite.

---

## Classes

Classes combine field declarations and method definitions into a single block. At parse time, every class is desugared into a **struct** (for fields) and an **impl block** (for methods) with zero runtime overhead.

### Fields and Methods

```flux
class Counter {
    value: Double
    def increment(self: Counter) -> Double {
        self.value = self.value + 1.0;
        self.value
    }
}
def main() -> Double { 0.0 }
```

- **Fields** are declared as `name: Type`, separated by newlines, commas, or semicolons.
- **Methods** are declared with `def`, identical to standalone function syntax.
- The **`self` parameter** is the first argument and is typed explicitly as the class name (e.g. `self: Counter`). It receives the instance when the method is called.

### Instantiation

Classes are instantiated using struct-literal syntax:

```flux
let c = Counter { value: 0.0 }
c.increment()
```

There is no separate constructor — the struct literal syntax constructs an instance directly. Field access uses dot notation: `c.value`.

### Method Dispatch

Methods defined inside a class are compiled as module-level functions with a name-mangled signature. When invoked via dot notation (`c.increment()`), the compiler rewrites the call to pass the receiver as the first argument. Without dot notation, the explicit mangled call also works (see [Impl Blocks](#impl-blocks) below).

---

## Class Desugaring

The parser transforms a class declaration into two separate AST nodes:

1. **`StructDeclAST`** — contains all field declarations.
2. **`ImplDeclAST`** — contains all method definitions, associated with the same type name.

This means a class definition like:

```flux
class Counter {
    value: Double
    def increment(self: Counter) -> Double {
        self.value = self.value + 1.0;
        self.value
    }
}
```

Is equivalent to:

```flux
struct Counter {
    value: Double
}
impl Counter {
    def increment(self: Counter) -> Double {
        self.value = self.value + 1.0;
        self.value
    }
}
```

The desugaring is performed during parsing (`ParseClassDecl` in `type_parser.cpp`), so there is no runtime overhead — the resulting LLVM IR is identical to manually written struct + impl code.

---

## Generic Classes

Classes support generic type parameters using bracket syntax `[T]`:

```flux
class Box[T] {
    content: Double
    def get(self: Box) -> Double {
        self.content
    }
}
def main() -> Double { 0.0 }
```

Generic parameters are parsed between the class name and the opening brace. At instantiation, the concrete type is supplied in brackets:

```flux
let b = Box[Double] { content: 42.0 }
b.get()
```

The generic parameter is monomorphized at compile time — the compiler generates a specialized version of the class for each concrete type used.

---

## Impl Blocks

Impl blocks attach methods to an existing struct or class type. They do not define new types — they only add behavior.

### Syntax

```flux
impl MyMath {
    def double_it(self: MyMath, x: Double) -> Double { x * 2.0 }
}
def main() -> Double { MyMath_double_it(0.0, 21.0) }
```

- `impl TypeName { ... }` opens a block of methods for `TypeName`.
- Each method's `self` parameter is typed as the impl target (e.g. `self: MyMath`).
- Methods are compiled to module-level functions with a name-mangled signature (`TypeName_method_name`).

### Method Dispatch

Without dot notation, methods are called using the mangled name directly:

```flux
MyMath_double_it(instance, 21.0)
```

The first argument is the instance (passed as a `Double` at the ABI level, since all value types are 64-bit floats in FluxScript's JIT).

---

## Method Call Syntax (Dot Notation)

FluxScript supports dot-notation method calls. The compiler rewrites `receiver.method(args)` into a call to the mangled function with the receiver as the first argument:

```flux
impl MyMath {
    def double_it(self: MyMath, x: Double) -> Double { x * 2.0 }
}
def main() -> Double {
    let m: MyMath = 0.0;
    m.double_it(21.0)
}
```

Here `m.double_it(21.0)` is rewritten to `MyMath_double_it(m, 21.0)`.

Dot notation works on both struct instances and class instances. The self parameter is automatically passed as the receiver — it does not appear in the argument list at the call site.

---

## Pipe Operator with Methods

The pipe operator `|>` can be used to call methods on existing instances, passing the left-hand value as an additional argument after `self`:

```flux
struct Box { val: Double }
impl Box {
    def apply(self: Box, f: Double) -> Double { self.val * f }
}
def main() -> Double {
    let b: Box = Box { val: 3.0 };
    5.0 |> b.apply
}
```

In this example, `5.0 |> b.apply` is equivalent to `b.apply(5.0)`. The pipe operator inserts the left-hand value as the argument following `self` in the method signature.

The pipe operator also works with struct field access as the target:

```flux
struct Box { val: Double }
impl Box {
    def scale(self: Box, f: Double) -> Double { self.val * f }
}
def main() -> Double {
    let b: Box = Box { val: 3.0 };
    5.0 |> b.scale
}
```

This is equivalent to `b.scale(5.0)`. The pipe operator provides a functional-style chaining mechanism for method calls.

---

## Traits

Traits define a set of method signatures that a type must implement. They serve as interfaces and support both static and dynamic dispatch.

### Trait Definition

```flux
trait Math {
    def add(a: Double, b: Double) -> Double;
    def mul(a: Double, b: Double) -> Double;
}
```

- `trait TraitName { ... }` opens a trait definition.
- Methods are declared with `def` followed by the signature, ending with a semicolon (`;`).
- Method bodies are not provided — they are supplied by the implementing type.
- Traits can declare methods **with** or **without** a `self` parameter. Methods without `self` are associated (static) functions.

### Implementing a Trait

```flux
struct Calc { }
impl Math for Calc {
    def add(a: Double, b: Double) -> Double { a + b }
    def mul(a: Double, b: Double) -> Double { a * b }
}
def main() -> Double { 0.0 }
```

- `impl TraitName for TypeName { ... }` associates a type with a trait.
- All methods declared in the trait must be implemented.
- Method bodies are provided inline within the impl block.

### Trait with `self` Parameter

Traits can also define methods that take a receiver:

```flux
trait Doubler {
    def double_it(self) -> Double;
}
impl Doubler for Double {
    def double_it(self) -> Double { self * 2.0 }
}
def main() -> Double { test(21.0) }
```

Here `self` refers to the implementing type. When `Double` implements `Doubler`, `self` is the `Double` value.

---

## Dynamic Dispatch (`dyn Trait`)

Dynamic dispatch allows calling trait methods on values whose concrete type is known only at runtime. The `dyn Trait` annotation boxes the value and dispatches through a vtable:

```flux
trait Doubler {
    def double_it(self) -> Double;
}
impl Doubler for Double {
    def double_it(self) -> Double { self * 2.0 }
}
def test(x: Double) -> Double {
    let d: dyn Doubler = x;
    d.double_it()
}
def main() -> Double { test(21.0) }
```

- `let d: dyn Doubler = x` wraps `x` in a dynamic dispatch object.
- `d.double_it()` calls through the vtable, resolving to `Doubler`'s implementation for `Double`.
- The function `test` returns `42.0` (21.0 * 2.0).

Dynamic dispatch is useful when the concrete type is not known at compile time — for example, when a function must accept any type that implements a given trait.

---

## Vtable Mechanism

When a trait is used with `dyn Trait`, the compiler generates a **vtable** — a table of function pointers for each method declared in the trait. For each type that implements the trait, the vtable stores pointers to the concrete method implementations.

The vtable is created at the point where `impl Trait for Type` is compiled. When a value is cast to `dyn Trait`, the compiler:

1. Allocates space for the value (boxed on the heap for small types).
2. Attaches a pointer to the vtable for the implementing type.
3. On method call, dereferences the vtable to find the correct function pointer and invokes it.

This enables runtime polymorphism — a single function can operate on any type that implements the trait, with the correct method selected at call time rather than compile time.

---

## Associated Types in Traits

Traits can declare associated types using the `type` keyword. Associated types are type-level placeholders that the implementing type must resolve:

### Single Associated Type

```flux
trait Container {
    type Item;
    def get(self: Container) -> Double;
}
struct MyBox { val: Double }
impl Container for MyBox {
    type Item = Double;
    def get(self: MyBox) -> Double { self.val }
}
def main() -> Double {
    0.0
}
```

- `type Item;` declares an associated type within the trait.
- In the impl block, `type Item = Double;` binds the associated type to a concrete type.
- Associated types allow trait methods to refer to related types without naming them in the trait signature.

### Multiple Associated Types

A trait can declare more than one associated type:

```flux
trait Pair {
    type First;
    type Second;
    def first(self: Pair) -> Double;
    def second(self: Pair) -> Double;
}
struct TwoVals { a: Double, b: Double }
impl Pair for TwoVals {
    type First = Double;
    type Second = Double;
    def first(self: TwoVals) -> Double { self.a }
    def second(self: TwoVals) -> Double { self.b }
}
def main() -> Double {
    0.0
}
```

Here the `Pair` trait declares two associated types (`First` and `Second`), both resolved to `Double` in the `TwoVals` implementation. Associated types enforce a 1:1 mapping — each implementing type provides exactly one concrete type per associated type declaration.

---

## Summary

| Feature | Syntax | Notes |
|---------|--------|-------|
| Class definition | `class Name { fields; methods }` | Desugars to struct + impl |
| Generic class | `class Name[T] { ... }` | Monomorphized at compile time |
| Impl block | `impl Name { def method(self: Name, ...) }` | Adds methods to existing type |
| Trait definition | `trait Name { def method(...); }` | Interface without implementation |
| Trait impl | `impl Trait for Type { ... }` | Provides concrete implementations |
| Dynamic dispatch | `let d: dyn Trait = val` | Vtable-based runtime polymorphism |
| Associated type | `type Item;` in trait, `type Item = T;` in impl | Type-level placeholder |
| Dot notation | `receiver.method(args)` | Auto-rewrites to mangled call |
| Pipe to method | `value \|> instance.method` | Passes value as argument after self |
# Control Flow and Error Handling

FluxScript provides first-class control flow constructs that double as expressions — every `if`, `match`, `switch`, `try`, and block `{ ... }` returns a value. This enables concise, functional-style code where control structures participate in assignments and function returns.

## if / else

The `if` expression uses the `then` keyword for the true branch and an optional `else` for the false branch. Both branches must be present for the expression to produce a value.

### Basic syntax

```flux
if condition then true_value else false_value
```

### Example — factorial

```flux
def fact(n: Double) -> Double {
    if n <= 1.0 then 1.0 else n * fact(n - 1.0)
}
```
*Source: `test_106_Recursive_Factorial.flux`*

### Example — Fibonacci

```flux
def fib(n: Double) -> Double {
    if n <= 1.0 then n else fib(n - 1.0) + fib(n - 2.0)
}
```
*Source: `test_138_Fibonacci_Recursive.flux`*

### Chained if / else if

Chain multiple conditions by nesting `else if` after each `else`:

```flux
let r = if x < 0.0 then -1.0
        else if x == 0.0 then 0.0
        else if x < 10.0 then 1.0
        else if x < 100.0 then 2.0
        else 3.0;
```
*Source: `test_129_Nested_Ternary_Chains.flux`*

### Nested if inside for-loop assignment

`if` as an expression can be assigned directly inside a loop body:

```flux
for i in 0, 100 do {
    sum = if i < 50.0 then sum + i else sum + i * 2.0
}
```
*Source: `test_303_Stress_Control_Flow_Loops.flux`*

### if / else with brace body

When the branch contains multiple statements, wrap the body in `{ ... }`:

```flux
if (x > 5.0) { make_ok(x + 1.0) } else { make_err(1.0) }
```
*Source: `test_160_SelfHost__question_mark_ok.flux`*

### Deeply nested if

Braces allow deeply nested conditionals:

```flux
def classify(x: Double) -> Double {
    if x < 0.0 then
        if x < -10.0 then
            if x < -100.0 then -3.0 else -2.0
        else -1.0
    else
        if x == 0.0 then 0.0
        else if x < 10.0 then 1.0
        else if x < 100.0 then 2.0
        else 3.0
}
```
*Source: `test_109_Deeply_Nested_If-Else_Chains.flux`*

### if as a return value

Because `if` is an expression, it can be the final expression of a function (implicit return):

```flux
def main() -> Double {
    var x = 0.0;
    var found = 0.0;
    while x < 10.0 do {
        x = x + 1.0;
        if (x == 7.0) { found = 1.0; break }
    };
    found
}
```
*Source: `test_151_SelfHost__while_conditional_break.flux`*

---

## while Loops

The `while` loop evaluates a condition and executes a body repeatedly as long as the condition is true. The body is introduced with the `do` keyword.

### Basic syntax

```flux
while condition do body
```

### Example — counting loop

```flux
def main() -> Double {
    var x = 0.0;
    while x < 3.0 do { x = x + 1.0 };
    x
}
```
*Source: `test_136_SelfHost__while_loop.flux`*

### Example — accumulating a sum

```flux
def main() -> Double {
    var i = 0.0;
    var s = 0.0;
    while i < 10.0 do {
        s = s + i;
        i = i + 1.0;
    }
    assert(s == 45.0, "while sum wrong");
    assert(i == 10.0, "while count wrong");
    1.0
}
```
*Source: `test_113_While_Loop_Basic.flux`*

### break

`break` exits the innermost loop immediately:

```flux
def main() -> Double {
    var x = 0.0;
    while true do {
        x = x + 1.0;
        if (x > 3.0) { break }
    };
    x
}
```
*Source: `test_137_SelfHost__break_in_while.flux`*

`break` can also be used with the `then` syntax inside an `if`:

```flux
var s = 0.0;
var i = 0.0;
while i < 100.0 do {
    s = s + i;
    i = i + 1.0;
    if s > 20.0 then break;
}
```
*Source: `test_114_While_Loop_Break.flux`*

### continue

`continue` skips the rest of the current iteration and jumps to the next loop evaluation:

```flux
def main() -> Double {
    var x = 0.0;
    var sum = 0.0;
    while x < 5.0 do {
        x = x + 1.0;
        if (x == 3.0) { continue };
        sum = sum + x
    };
    sum
}
```
*Source: `test_138_SelfHost__continue_in_while.flux`*

---

## for Loops

The `for` loop iterates over a numeric range. It uses a comma-separated start and end value with the `in` keyword and a `do` keyword before the body.

### Basic syntax

```flux
for variable in start, end do body
```

The loop variable starts at `start` and increments by `1.0` on each iteration, running while strictly less than `end` (exclusive upper bound).

### Example — simple iteration

```flux
for i in 1, 3 do i
```
*Source: `test_001_For_Loop.flux`*

### Example — summing values

```flux
def main() -> Double {
    var sum1 = 0.0;
    for i in 0, 100 do {
        sum1 = sum1 + i
    }
    assert(sum1 == 4950.0, "for sum wrong");
    1.0
}
```
*Source: `test_318_Stress_Control_Flow_Complex.flux`*

### break in for

```flux
def main() -> Double {
    var s = 0.0;
    var i = 0.0;
    for i in 1, 10 do {
        s = s + i;
        if i >= 5.0 then break;
    }
    assert(s == 45.0, "for-break sum wrong");
    1.0
}
```
*Source: `test_111_For_Loop_With_Break.flux`*

### continue in for

```flux
def main() -> Double {
    var sum_odd = 0.0;
    for i in 1, 10 do {
        if i % 2.0 == 0.0 then continue;
        sum_odd = sum_odd + i;
    }
    assert(sum_odd == 45.0, "for-continue odd sum wrong");
    1.0
}
```
*Source: `test_112_For_Loop_With_Continue.flux`*

### Nested for loops

```flux
def main() -> Double {
    var s = 0.0;
    for i in 0, 3 do
        for j in 0, 3 do
            s = s + i * j;
    assert(s == 9.0, "nested for sum wrong");
    1.0
}
```
*Source: `test_110_Nested_For_Loops_Product.flux`*

Triple-nested for loops work identically:

```flux
var triple_sum = 0.0
for i in 0, 10 do {
    for j in 0, 10 do {
        for k in 0, 10 do {
            triple_sum = triple_sum + 1.0
        }
    }
}
assert(triple_sum == 1000.0, "triple nested wrong")
```
*Source: `test_303_Stress_Control_Flow_Loops.flux`*

---

## match Expressions

The `match` expression performs pattern matching against enum variants or literal values. It is an expression — each arm returns a value.

### Basic syntax

```flux
match value {
    pattern1 -> result1,
    pattern2 -> result2,
    ...
}
```

Arms are separated by commas `,`. The last arm may omit the trailing comma.

### Simple literal patterns

Match against numeric literals with a wildcard `_` for the default arm:

```flux
def test() match(5.0) { 5.0 -> 10.0, _ -> 0.0 }; 1.0
```
*Source: `test_006_Pattern_Match.flux`*

### Enum variant patterns (positional)

Match on enum variants by name. Payload variables bind the inner value:

```flux
enum Wrap { Value { x: Double }, Empty }
def sq(w: Wrap) -> Double {
    match w {
        Wrap.Value(v) -> v * v,
        default -> 0.0
    }
}
```
*Source: `test_138_Match_Variable_Arithmetic.flux`*

### Multi-payload enum patterns

Variants with multiple fields bind each field positionally:

```flux
enum Maybe3 {
    One { a: Double },
    Two { a: Double, b: Double },
    Three { a: Double, b: Double, c: Double }
}
def maybe3_sum(m: Maybe3) -> Double {
    match m {
        Maybe3.One(a) -> a,
        Maybe3.Two(a, b) -> a + b,
        Maybe3.Three(a, b, c) -> a + b + c
    }
}
```
*Source: `test_313_Stress_Enum_Patterns_Options.flux`*

### Braced destructuring (named-field patterns)

Match patterns can destructure named fields with `{ field: binding }` syntax:

```flux
enum Option {
    Some { value: Double },
    None {}
}
def unwrap_add(opt: Option) -> Double {
    match opt {
        Option.Some { value: v } -> v + 1.0,
        Option.None {} -> 0.0
    }
}
```
*Source: `test_216_Match_Named_Field_Single.flux`*

Named-field patterns can bind fields in any order and rename bindings:

```flux
enum Point2D {
    Cartesian { x: Double, y: Double },
    Polar { r: Double, theta: Double }
}
def distance(pt: Point2D) -> Double {
    match pt {
        Point2D.Cartesian { x: px, y: py } -> px * px + py * py,
        Point2D.Polar { r: radius, theta: angle } -> radius
    }
}
```
*Source: `test_217_Match_Named_Field_Multi.flux`*

Named-field and positional patterns can be mixed in the same match:

```flux
enum Shape {
    Circle { radius: Double },
    Rect { width: Double, height: Double },
    Dot {}
}
def area(s: Shape) -> Double {
    match s {
        Shape.Circle { radius: r } -> r * r * 3.14159265,
        Shape.Rect { width: w, height: h } -> w * h,
        Shape.Dot {} -> 0.0
    }
}
```
*Source: `test_218_Match_Named_Field_Mixed.flux`*

### default arm

Use `default` to catch all unmatched variants. The wildcard `_` also works for literal matches:

```flux
enum Color { Red, Green, Blue }
def run_check() -> Double {
    let c = Color.Red;
    match c {
        Color.Red -> 1.0,
        default -> 0.0
    }
}
```
*Source: `test_068_Match_Exhaustiveness_-_default_arm.flux`*

When all variants are covered explicitly, `default` is optional:

```flux
enum Color { Red, Green, Blue }
def run_check() -> Double {
    let c = Color.Red;
    match c {
        Color.Red -> 1.0,
        Color.Green -> 2.0,
        Color.Blue -> 3.0
    }
}
```
*Source: `test_068_Match_Exhaustiveness_-_all_covered.flux`*

### Boxed (large) payload matching

For enum variants containing structs larger than 16 bytes, the payload is heap-allocated. The match syntax remains the same — the runtime transparently handles the indirection:

```flux
struct LargePayload {
    a: Double,
    b: Double,
    c: Double
}
enum BoxedOption {
    Some(LargePayload),
    None
}
def verify_leak_prevention() -> Double {
    let opt = create_boxed(42.0);
    match opt {
        BoxedOption.Some(payload) -> {
            assert(payload.a == 42.0, "Large payload field a wrong");
            assert(payload.b == 43.0, "Large payload field b wrong");
            assert(payload.c == 44.0, "Large payload field c wrong");
            1.0
        },
        default -> -1.0
    }
}
```
*Source: `test_045_Boxed_Enum_JIT_Arena_Deallocation.flux`*

### Or-patterns (`|`)

Multiple patterns can share the same arm using `|`. The same binding variable can be used across alternatives:

```flux
enum MyEnum { Val1 { value: Double }, Val2 { value: Double }, Val3 }
def test_match(x: MyEnum) -> Double {
    match x {
        MyEnum.Val1(v) | MyEnum.Val2(v) -> v,
        MyEnum.Val3 -> 100.0
    }
}
```
*Source: `test_042_Match_Or-Patterns.flux`, `test_163_SelfHost__match_or-patterns.flux`*

### match with block bodies

Arm bodies can contain multiple statements inside `{ ... }`:

```flux
match opt {
    Option.Some(payload) -> {
        assert(payload.a == 42.0, "field wrong");
        1.0
    },
    default -> -1.0
}
```
*Source: `test_045_Boxed_Enum_JIT_Arena_Deallocation.flux`*

### Complex enum patterns with full expressions

```flux
enum Shape { Circle { radius: Double }, Rect { w: Double, h: Double }, Dot }
def area(s: Shape) -> Double {
    match s {
        Shape.Circle(r) -> 3.14159 * r * r,
        Shape.Rect(w, h) -> w * h,
        Shape.Dot -> 0.0
    }
}
```
*Source: `test_330_Stress_Enum_Complex_Patterns.flux`*

---

## switch / case

The `switch` expression compares a value against literal constants using `=>` syntax. It is distinct from `match` — `switch` works on scalar values, while `match` works on enum variants.

### Basic syntax

```flux
switch (value) {
    literal1 => result1,
    literal2 => result2,
    ~ => default_result
}
```

The `~` (tilde) token acts as the default/wildcard arm.

### Example — basic dispatch

```flux
let a = 1.0
switch (a) {
    1.0 => 10.0,
    2.0 => 20.0,
    ~ => 0.0
}
```
*Source: `test_071_Switch_basic.flux`*

### Example — default fallthrough

```flux
let a = 99.0
switch (a) {
    1.0 => 10.0,
    2.0 => 20.0,
    ~ => -1.0
}
```
*Source: `test_071_Switch_default.flux`*

### switch as a function

```flux
def classify_char(c: Double) -> Double {
    switch (c) {
        0.0 => 1.0,
        1.0 => 2.0,
        2.0 => 3.0,
        3.0 => 4.0,
        4.0 => 5.0,
        5.0 => 6.0,
        6.0 => 7.0,
        7.0 => 8.0,
        8.0 => 9.0,
        9.0 => 10.0,
        ~ => -1.0
    }
}
```
*Source: `test_316_Stress_Switch_Case_Coverage.flux`*

### switch as an inline expression

The result of `switch` can be assigned directly:

```flux
let s1 = switch (0.0) { 0.0 => 100.0, ~ => 0.0 }
assert(s1 == 100.0, "switch expr 0")
let s2 = switch (5.0) { 0.0 => 100.0, ~ => 0.0 }
assert(s2 == 0.0, "switch expr default")
```
*Source: `test_316_Stress_Switch_Case_Coverage.flux`*

---

## try / catch

FluxScript provides `try` / `catch` for runtime error handling. A `throw` expression unwinds the call stack to the nearest enclosing `catch`.

### Basic syntax

```flux
try { expression } catch variable { handler_expression }
```

### throw

`throw` sends a value to the nearest `catch` handler. It can throw numeric values or error objects:

```flux
def risky(n: Double) -> Double {
    if (n < 0.0) { throw -1.0 } else { n * 2.0 }
}
```
*Source: `test_101_Try-Catch-Throw_JIT.flux`*

### Basic try / catch

```flux
def foo() { 10.0 }
def test() try { foo() } catch e { 0.0 }; 1.0
```
*Source: `test_008_Try-Catch.flux`*

### try / catch in a function

```flux
def safe_run(n: Double) -> Double {
    try { risky(n) } catch e { e + 100.0 }
}
```
*Source: `test_101_Try-Catch-Throw_JIT.flux`*

### try / catch returning a value

The catch block can return a fallback value that integrates with the surrounding expression:

```flux
let r1 = try { risky_add(3.0, 4.0) } catch(e) { 0.0 }
assert(r1 == 7.0, "try ok wrong")

let r2 = try { risky_add(-1.0, 4.0) } catch(e) { -1.0 }
assert(r2 == -1.0, "try catch wrong")
```
*Source: `test_308_Stress_Error_Handling.flux`*

### try / catch inside a loop

```flux
var sum = 0.0
var i = 0.0
while i < 20.0 do {
    let val = try { risky_add(i, i) } catch(e) { -1.0 }
    sum = sum + val
    i = i + 1.0
}
assert(sum == 380.0, "try-catch loop wrong")
```
*Source: `test_308_Stress_Error_Handling.flux`*

### throw with string messages

```flux
def risky_add(a: Double, b: Double) -> Double {
    if a < 0.0 then throw("neg a")
    else if b < 0.0 then throw("neg b")
    else a + b
}
```
*Source: `test_308_Stress_Error_Handling.flux`*

---

## `?` Operator (Result Propagation)

The `?` operator provides early-return error propagation for functions that return a `Result` enum with `Ok` and `Err` variants. When applied to a `Result` value:

- If the value is `Ok`, the inner value is extracted and binding continues.
- If the value is `Err`, the function immediately returns that `Err` value.

### Required Result enum structure

The `?` operator expects an enum with `Ok { value: ... }` and `Err { msg: ... }` variants:

```flux
enum Result { Ok { value: Double }, Err { msg: Double } }
```

### Basic propagation

```flux
def div_with(a: Double, b: Double) -> Result {
    if (b == 0.0) { make_err(99.0) } else { make_ok(a / b) }
}
def chain() -> Result {
    let a = div_with(10.0, 2.0)?;
    div_with(a, 5.0)
}
```
*Source: `test_103_Question_Ok_propagation.flux`*

### Early return on error

When any step returns `Err`, the entire function returns that error immediately:

```flux
def chain_err() -> Result {
    let a = div_with(10.0, 0.0)?;
    div_with(a, 5.0)  // never reached
}
```
*Source: `test_104_Question_Err_early-return.flux`*

### Chaining multiple `?` operators

```flux
def pipeline() -> Result {
    let a = step1()?;
    let b = step2(a)?;
    step3(b)
}
```
*Source: `test_105_Question_chained.flux`*

### `?` in if/else context

```flux
def main() -> Double {
    let a = step1()?;
    let b = step2(a)?;
    b
}
```
*Source: `test_160_SelfHost__question_mark_ok.flux`*

---

## Block Expressions `{ ... }`

A block `{ ... }` is an expression. The value of the last expression in the block is the block's result. Blocks are used extensively as loop bodies, function bodies, match arm bodies, and let-in expressions.

### Basic block expression

```flux
def main() -> Double {
    var x = 1.0;
    {
        x = 2.0;
    };
    assert(x == 2.0, "var mutated inside block should persist");
    1.0
}
```
*Source: `test_133_Var_Mutable_In_Block.flux`*

### Let-in blocks

A block can contain `let` bindings, and the last expression is the block's value:

```flux
let b = {
    let a2 = a * 2.0
    let b2 = a2 + 5.0
    a2 + b2
}
assert(b == 45.0, "let-in block")
```
*Source: `test_317_Stress_Shadowing_LetIn.flux`*

### Nested blocks

Blocks can be nested arbitrarily. Inner bindings are visible to the enclosing block:

```flux
let nested = {
    let outer = 10.0
    let inner = {
        let deep = outer * 3.0
        deep + 1.0
    }
    inner + outer
}
assert(nested == 41.0, "nested blocks")
```
*Source: `test_317_Stress_Shadowing_LetIn.flux`*

### Blocks in loops

Blocks inside loops create temporary scoped bindings:

```flux
while i < 100.0 do {
    let val = {
        let base = i
        let modified = base * 2.0 + 1.0
        modified
    }
    acc = acc + val
    i = i + 1.0
}
assert(acc == 10000.0, "let-in loop wrong")
```
*Source: `test_317_Stress_Shadowing_LetIn.flux`*

---

## Return Values from Control Flow

All control flow constructs in FluxScript are expressions and produce values. This eliminates the need for explicit `return` in most cases.

### if/else as an expression

```flux
def main() -> Double {
    var sum = 0.0
    for i in 0, 100 do {
        sum = sum + i
    }
    assert(sum == 4950.0, "for sum wrong");
    1.0
}
```
*Source: `test_318_Stress_Control_Flow_Complex.flux`*

### match as an expression

```flux
def main() -> Double {
    let a = 10.0
    let b = {
        let a2 = a * 2.0
        let b2 = a2 + 5.0
        a2 + b2
    }
    assert(b == 45.0, "let-in block");
    1.0
}
```
*Source: `test_317_Stress_Shadowing_LetIn.flux`*

### switch as an expression

```flux
let s1 = switch (0.0) { 0.0 => 100.0, ~ => 0.0 }
assert(s1 == 100.0, "switch expr 0")
```
*Source: `test_316_Stress_Switch_Case_Coverage.flux`*

### try/catch as an expression

```flux
let r1 = try { risky_add(3.0, 4.0) } catch(e) { 0.0 }
assert(r1 == 7.0, "try ok wrong")
```
*Source: `test_308_Stress_Error_Handling.flux`*

### Implicit return

The last expression in a function body is its return value. No `return` keyword is needed:

```flux
def fact(n: Double) -> Double {
    if n <= 1.0 then 1.0 else n * fact(n - 1.0)
}
```
*Source: `test_106_Recursive_Factorial.flux`*

### Block as the final expression

```flux
def main() -> Double {
    var x = 0.0;
    while true do {
        x = x + 1.0;
        if (x > 3.0) { break }
    };
    x  // <-- this is the return value
}
```
*Source: `test_137_SelfHost__break_in_while.flux`*

---

## Summary Table

| Construct | Keyword(s) | Returns Value | Mutable Binding |
|-----------|-----------|---------------|-----------------|
| `if / else` | `then`, `else` | Yes | No |
| `while` | `while ... do` | No (use last expression) | Yes (`var`) |
| `for` | `for ... in ... do` | No (use last expression) | Yes (`var`) |
| `match` | `match ... { ... }` | Yes (each arm) | No |
| `switch` | `switch (expr) { ... }` | Yes (each arm) | No |
| `try / catch` | `try { } catch e { }` | Yes | No |
| `?` operator | `?` | Yes (extracts Ok value) | No |
| Block `{ }` | `{ ... }` | Yes (last expression) | No |
| `break` | `break` | — (exits loop) | — |
| `continue` | `continue` | — (skips iteration) | — |
| `throw` | `throw expr` | — (unwinds to catch) | — |
# Advanced Features

This section covers generics, async/await, concurrency, operator overloading, pipe operator, and switch/case syntax in FluxScript. All code examples are verifiable against the integration test suite.

---

## Generics

FluxScript supports compile-time generics using square bracket syntax `[T]`. The compiler monomorphizes generic code — generating specialized LLVM IR for each concrete type combination used.

### Generic Functions

Type parameters are declared after the function name in square brackets:

```flux
def id[T](x: T) -> T { x }
```

Type arguments can be provided explicitly or inferred from usage:

```flux
def id[T](x: T) -> T { x }
id[Double](42.0)    # explicit — T = Double
id(3.14)            # inferred — T = Double
```

> Verified in `test_046_Generics___Templates__Bracket_Syntax__T.flux`, `test_192_Generic_Identity_Multi-Type.flux`

### Generic Structs

Structs declare type parameters in brackets. The parameters must appear in field types:

```flux
struct Pair[T] {
    first: T,
    second: T
}

let p = Pair[Double] { first: 10.0, second: 20.0 }
p.first     # 10.0
p.second    # 20.0
```

> Verified in `test_046_Generics___Templates__Bracket_Syntax__T.flux`

Multiple type parameters use comma-separated names:

```flux
struct Pair[T, U] { first: T, second: U }

let p = Pair[Double, Double] { first: 3.0, second: 4.0 }
```

> Verified in `test_198_Match_Payload_Generic_Struct.flux`, `test_213_Stress_Cross_Feature.flux`

### Nested Generics

Generic structs can be nested to arbitrary depth. Each level is monomorphized independently:

```flux
struct Box[T] { val: T }

let b1 = Box[Double] { val: 1.0 }
let b2 = Box[Box[Double]] { val: Box[Double] { val: 2.0 } }
let b3 = Box[Box[Box[Double]]] { val: Box[Box[Double]] { val: Box[Double] { val: 3.0 } } }

b1.val             # 1.0
b2.val.val         # 2.0
b3.val.val.val     # 3.0
```

Multi-parameter generics also nest:

```flux
struct Pair[A, B] { left: A, right: B }

let p1 = Pair[Double, Double] { left: 10.0, right: 20.0 }
let p2 = Pair[Pair[Double, Double], Double] { left: p1, right: 30.0 }

p2.left.left      # 10.0
p2.left.right     # 20.0
p2.right          # 30.0
```

> Verified in `test_300_Stress_Deeply_Nested_Generics.flux`

### Generic Functions with Nested Calls

Generic functions can be called in chains and nested contexts:

```flux
def identity[T](x: T) -> T { x }

def deep_generic(x: Double) -> Double {
    let a = identity[Double](x)
    let b = identity[Double](a + 1.0)
    let c = identity[Double](b + 2.0)
    let d = identity[Double](c + 3.0)
    let e = identity[Double](d + 4.0)
    e
}

deep_generic(0.0)   # 10.0
```

> Verified in `test_211_Stress_Async_Generics.flux`

### Generic Structs in Pattern Matching

Generic structs can be used as enum payloads and matched:

```flux
struct Pair[T, U] { first: T, second: U }
enum Opt { Some(Pair[Double, Double]), None }

def pick(opt: Opt) -> Double {
    match opt {
        Opt.Some(p) -> p.first + p.second,
        Opt.None -> -1.0
    }
}

let a = Opt.Some(Pair[Double, Double] { first: 3.0, second: 4.0 })
pick(a)     # 7.0
```

> Verified in `test_198_Match_Payload_Generic_Struct.flux`

---

## Async/Await

FluxScript provides first-class async/await support. Functions marked with `async def` can suspend execution at `await` points and resume later, implemented via a state machine at the LLVM IR level.

### Basic Async Function

An `async def` function can suspend at `await` expressions:

```flux
async def fetch_data() -> Double {
    await 0.5
    42.0
}

async def process_data(val: Double) -> Double {
    let data = await fetch_data()
    data + val
}

def verify_async() -> Double {
    let result = process_data(1.0)
    assert(abs(result - 43.0) < 0.0001, "async result")
    1.0
}

verify_async()
```

> Verified in `test_058_Async_Await.flux`

### Conditional Await

`await` can appear inside conditional branches. Only the executed branch suspends:

```flux
async def maybe_await(flag: Double) -> Double {
    if flag > 0.5 {
        await 0.5
        100.0
    } else {
        200.0
    }
}

def run_check() -> Double {
    let r1 = maybe_await(1.0)       # takes the if-branch, awaits
    let r2 = maybe_await(0.0)       # takes the else-branch, no await
    1.0
}

run_check()
```

> Verified in `test_058_Async_-_conditional_await.flux`

### Chained Async Calls

Async functions can be chained — calling an `await` on the result of another async function:

```flux
async def delayed_add(a: Double, b: Double) -> Double {
    await 0.5
    a + b
}

async def chain3(x: Double) -> Double {
    let a = await delayed_add(x, 1.0)
    let b = await delayed_add(a, 2.0)
    let c = await delayed_add(b, 3.0)
    c
}

def main() -> Double {
    let r1 = chain3(10.0)
    assert(r1 == 16.0, "chain3 wrong")
    1.0
}

main()
```

> Verified in `test_304_Stress_Async_Complex_Patterns.flux`

### Recursive Async

Async functions can call themselves recursively:

```flux
async def delayed_add(a: Double, b: Double) -> Double {
    await 0.5
    a + b
}

async def nested_async(depth: Double, val: Double) -> Double {
    if depth <= 0.0 then val
    else {
        let half = await delayed_add(val, depth)
        nested_async(depth - 1.0, half)
    }
}

def main() -> Double {
    let r1 = nested_async(5.0, 0.0)
    assert(r1 == 15.0, "nested_async wrong")
    1.0
}

main()
```

> Verified in `test_304_Stress_Async_Complex_Patterns.flux`

### Implementation Details

The compiler transforms `async def` functions into state machines:

- A hidden `__async_state` pointer parameter tracks the current state index.
- A dispatcher switch jumps to the correct resume point based on state.
- `await` checks if the awaited value is ready (non-zero = ready, no suspend).
- On suspension, the state index is saved and the function returns `0.0` (Pending).
- On re-entry, the dispatcher resumes at the saved state, re-polling the awaited expression.
- `AsyncResultAlloca` in the entry block stores values across suspension points for SSA dominance.

---

## Concurrency

FluxScript provides lightweight concurrency primitives: `spawn` for launching parallel tasks, `join` for collecting results, and channels for inter-task communication.

### spawn and join

`spawn` launches a function call on a new thread and returns a task handle. `join` blocks until the task completes and returns its result:

```flux
def worker(x: Double) -> Double { x * 2.0 }

def main() -> Double {
    let t = spawn worker(21.0)
    join(t)
}

main()     # 42.0
```

> Verified in `test_139_SelfHost__spawn_join.flux`

`spawn` supports functions with multiple arguments:

```flux
def add(a: Double, b: Double, c: Double) -> Double { a + b + c }

def main() -> Double {
    let t = spawn add(10.0, 20.0, 30.0)
    join(t)
}

main()     # 60.0
```

> Verified in `test_140_SelfHost__spawn_multi-arg.flux`

### Channels

Channels provide synchronous message passing between tasks. A channel is created, used for send/recv, and destroyed when no longer needed.

#### Basic Channel Operations

```flux
def main() -> Double {
    let ch = flux_chan_create()
    flux_chan_send(ch, 42.0)
    let val = flux_chan_recv(ch)
    flux_chan_destroy(ch)
    assert(val == 42.0, "channel send/recv failed")
    1.0
}

main()
```

> Verified in `test_096_Channel_Send_Recv.flux`, `test_097_Channel_Send_Recv.flux`

#### Spawn with Channel Communication

Channels are the primary mechanism for passing data between spawned tasks:

```flux
def producer(ch: Double) -> Double {
    flux_chan_send(ch, 99.0)
    1.0
}

def main() -> Double {
    let ch = flux_chan_create()
    spawn producer(ch)
    let val = flux_chan_recv(ch)
    flux_chan_destroy(ch)
    assert(val == 99.0, "spawn+channel failed")
    1.0
}

main()
```

> Verified in `test_097_Spawn___Channel.flux`, `test_098_Spawn___Channel.flux`

#### Channel API Reference

| Function | Description |
|----------|-------------|
| `flux_chan_create()` | Creates a new channel, returns a handle (`Double`) |
| `flux_chan_send(ch, val)` | Sends `val` through channel `ch` |
| `flux_chan_recv(ch)` | Receives a value from channel `ch` (blocks until available) |
| `flux_chan_destroy(ch)` | Destroys the channel, releasing resources |

---

## Operator Overloading

FluxScript supports operator overloading through two mechanisms: **trait-based overloading** (the primary approach) and **free-function conventions** (a simpler alternative).

### Trait-Based Operator Overloading

The compiler recognizes five built-in traits that map to operators. Each trait defines a single method:

| Trait | Operator | Method Signature |
|-------|----------|-----------------|
| `Add` | `+` | `def add(self, rhs: T) -> T` |
| `Sub` | `-` | `def sub(self, rhs: T) -> T` |
| `Mul` | `*` | `def mul(self, rhs: T) -> T` |
| `Neg` | unary `-` | `def neg(self) -> T` |
| `Eq` | `==` | `def eq(self, other: T) -> Bool` |

#### Full Example: Complex Number Arithmetic

```flux
trait Add {
    def add(self, rhs: Complex) -> Complex
}

trait Sub {
    def sub(self, rhs: Complex) -> Complex
}

trait Mul {
    def mul(self, rhs: Complex) -> Complex
}

trait Neg {
    def neg(self) -> Complex
}

trait Eq {
    def eq(self, other: Complex) -> Bool
}

struct Complex { re: Double, im: Double }

impl Add for Complex {
    def add(self, rhs: Complex) -> Complex {
        Complex { re: self.re + rhs.re, im: self.im + rhs.im }
    }
}

impl Sub for Complex {
    def sub(self, rhs: Complex) -> Complex {
        Complex { re: self.re - rhs.re, im: self.im - rhs.im }
    }
}

impl Mul for Complex {
    def mul(self, rhs: Complex) -> Complex {
        let r1 = self.re * rhs.re
        let r2 = self.im * rhs.im
        let i1 = self.re * rhs.im
        let i2 = self.im * rhs.re
        Complex { re: r1 - r2, im: i1 + i2 }
    }
}

impl Neg for Complex {
    def neg(self) -> Complex {
        Complex { re: -self.re, im: -self.im }
    }
}

impl Eq for Complex {
    def eq(self, other: Complex) -> Bool {
        (abs(self.re - other.re) < 0.0001) && (abs(self.im - other.im) < 0.0001)
    }
}

def verify_overloads() -> Double {
    let a = Complex { re: 1.0, im: 2.0 }
    let b = Complex { re: 3.0, im: 4.0 }

    let sum = a + b          # Complex { re: 4.0, im: 6.0 }
    let diff = a - b         # Complex { re: -2.0, im: -2.0 }
    let prod = a * b         # Complex { re: -5.0, im: 10.0 }
    let neg_a = -a           # Complex { re: -1.0, im: -2.0 }
    let eq_check = a == a    # true
    let ne_check = a == b    # false

    1.0
}

verify_overloads()
```

> Verified in `test_054_Operator_Overloading.flux`

### Free-Function Operator Convention

For simpler cases, FluxScript also supports a naming convention where a free function `TypeName_op` is called when the operator is used on that type. The function receives the operands directly (unwrapped from the struct).

#### `+` and unary `-` (Add/Neg)

```flux
struct Vec { x: Double }

def Vec_add(a: Double, b: Double) -> Double { 42.0 }
def Vec_neg(a: Double) -> Double { 43.0 }

def main() -> Double {
    let a = Vec { x: 1.0 }
    let b = Vec { x: 2.0 }
    let r1 = a + b      # calls Vec_add(1.0, 2.0) -> 42.0
    let r2 = -a          # calls Vec_neg(1.0) -> 43.0
    r1 + r2              # 85.0
}
```

> Verified in `test_154_SelfHost__operator_add_neg.flux`

#### `-`, `*`, and `==` (Sub/Mul/Eq)

```flux
struct Wrap { val: Double }

def Wrap_sub(a: Double, b: Double) -> Double { 10.0 }
def Wrap_mul(a: Double, b: Double) -> Double { 20.0 }
def Wrap_eq(a: Double, b: Double) -> Double { 30.0 }

def main() -> Double {
    let a: Wrap = Wrap { val: 1.0 }
    let b: Wrap = Wrap { val: 2.0 }
    let r1 = a - b      # calls Wrap_sub(1.0, 2.0) -> 10.0
    let r2 = a * b      # calls Wrap_mul(1.0, 2.0) -> 20.0
    let r3 = a == b     # calls Wrap_eq(1.0, 2.0) -> 30.0
    r1 + r2 + r3        # 60.0
}
```

> Verified in `test_155_SelfHost__operator_sub_mul_eq.flux`

### Pattern: Vector Math via Free Functions

```flux
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
    let s = vec3_add(a, b)    # Vec3 { x: 5.0, y: 7.0, z: 9.0 }
    let d = vec3_dot(a, b)    # 32.0
    1.0
}
```

> Verified in `test_310_Stress_Operator_Overloading_Complex.flux`

---

## Pipe Operator

The pipe operator `|>` passes the left-hand value as the first argument to the right-hand function. It enables left-to-right data flow and method chaining.

### Basic Piping

The simplest form pipes a value into a function:

```flux
def double_it(x: Double) -> Double { x * 2.0 }
def add_one(x: Double) -> Double { x + 1.0 }

3.0 |> double_it |> add_one     # (3.0 * 2.0) + 1.0 = 7.0
```

> Verified in `test_129_SelfHost__pipe_basic.flux`

### Partial Application in Pipes

When piping into a multi-argument function, provide all arguments except the first (which is supplied by the pipe):

```flux
def scale(s: Double, x: Double) -> Double { s * x }

let mul = 2.5
10.0 |> scale(mul)     # scale(2.5, 10.0) = 25.0
```

Here `scale(mul)` is a partial application — `s` is bound to `mul`, and the piped value fills `x`.

> Verified in `test_130_SelfHost__pipe_multi-arg.flux`

### Method Pipes

The pipe operator can call methods on existing instances. The piped value is passed as the argument after `self`:

```flux
struct Box { val: Double }

impl Box {
    def apply(self: Box, f: Double) -> Double { self.val * f }
}

let b: Box = Box { val: 3.0 }
5.0 |> b.apply     # b.apply(5.0) = 3.0 * 5.0 = 15.0
```

> Verified in `test_131_SelfHost__pipe_method_call.flux`, `test_058_Pipe_Operator_-_method_call.flux`

Struct field access also works as the pipe target:

```flux
struct Box { val: Double }

impl Box {
    def scale(self: Box, f: Double) -> Double { self.val * f }
}

let b: Box = Box { val: 3.0 }
5.0 |> b.scale     # b.scale(5.0) = 15.0
```

> Verified in `test_152_SelfHost__pipe_struct_field.flux`

### Pipe Chains

Multiple pipes chain left-to-right. Each `|>` passes its result as the next function's first argument:

```flux
def double_it(x) -> Double { x * 2.0 }
def add_one(x) -> Double { x + 1.0 }
def square(x) -> Double { x * x }
def negate(x) -> Double { -x }

5.0 |> double_it |> add_one          # 11.0
3.0 |> square |> double_it |> add_one     # 19.0
10.0 |> add_one |> square |> negate       # -121.0
5.0 |> double_it |> double_it |> double_it    # 40.0
2.0 |> square |> square |> square             # 256.0
1.0 |> add_one |> add_one |> add_one |> add_one |> add_one   # 6.0
```

Pipes work inside loops:

```flux
var total = 0.0
var i = 0.0
while i < 50.0 do {
    let val = i |> double_it |> add_one |> square
    total = total + val
    i = i + 1.0
}
# total == 166650.0
```

> Verified in `test_311_Stress_Pipe_Operator_Chains.flux`

---

## Switch/Case

The `switch` expression evaluates a value and branches based on equality matching. It supports arbitrary literal match values and a wildcard default.

### Basic Syntax

```flux
let a = 1.0

switch (a) {
    1.0 => 10.0,
    2.0 => 20.0,
    ~ => 0.0
}   # 10.0
```

- The expression in parentheses is evaluated once.
- Each `value => result` clause tests for equality.
- The `~` wildcard matches any unmatched value (acts as a default).

> Verified in `test_071_Switch_basic.flux`

### Default Wildcard

When no case matches, the wildcard `~` provides a fallback:

```flux
let a = 99.0

switch (a) {
    1.0 => 10.0,
    2.0 => 20.0,
    ~ => -1.0
}   # -1.0
```

> Verified in `test_071_Switch_default.flux`

### Switch as Expression

Switch is an expression — it returns a value that can be bound or used inline:

```flux
let s1 = switch (0.0) { 0.0 => 100.0, ~ => 0.0 }
# s1 == 100.0

let s2 = switch (5.0) { 0.0 => 100.0, ~ => 0.0 }
# s2 == 0.0
```

> Verified in `test_316_Stress_Switch_Case_Coverage.flux`

### Many-Case Switch

Switch handles large case lists. The compiler generates an efficient dispatch:

```flux
def classify_char(c: Double) -> Double {
    switch (c) {
        0.0 => 1.0,
        1.0 => 2.0,
        2.0 => 3.0,
        3.0 => 4.0,
        4.0 => 5.0,
        5.0 => 6.0,
        6.0 => 7.0,
        7.0 => 8.0,
        8.0 => 9.0,
        9.0 => 10.0,
        ~ => -1.0
    }
}

classify_char(0.0)     # 1.0
classify_char(5.0)     # 6.0
classify_char(9.0)     # 10.0
classify_char(10.0)    # -1.0
```

> Verified in `test_316_Stress_Switch_Case_Coverage.flux`

### Switch in Loops

Switch expressions work naturally inside loops:

```flux
var sum = 0.0
var i = 0.0
while i < 10.0 do {
    sum = sum + classify_char(i)
    i = i + 1.0
}
# sum == 55.0
```

> Verified in `test_316_Stress_Switch_Case_Coverage.flux`

---

## Advanced Stress Test Examples

The integration test suite includes stress tests that combine multiple features. Below are representative examples demonstrating real-world patterns.

### Generic Struct Nesting and Iteration

Builds nested generic containers, accesses fields at each depth, and combines them in arithmetic:

```flux
struct Box[T] { val: T }
struct Pair[A, B] { left: A, right: B }

def main() -> Double {
    let b1 = Box[Double] { val: 1.0 }
    let b2 = Box[Box[Double]] { val: Box[Double] { val: 2.0 } }
    let b3 = Box[Box[Box[Double]]] { val: Box[Box[Double]] { val: Box[Double] { val: 3.0 } } }

    assert(b1.val == 1.0, "b1 wrong")
    assert(b2.val.val == 2.0, "b2 wrong")
    assert(b3.val.val.val == 3.0, "b3 wrong")

    let p1 = Pair[Double, Double] { left: 10.0, right: 20.0 }
    let p2 = Pair[Pair[Double, Double], Double] { left: p1, right: 30.0 }
    assert(p2.left.left == 10.0, "p2 deep left wrong")

    let nested = Pair[Pair[Double, Double], Pair[Double, Double]] {
        left: Pair[Double, Double] { left: 1.0, right: 2.0 },
        right: Pair[Double, Double] { left: 3.0, right: 4.0 }
    }
    assert(nested.left.left + nested.left.right + nested.right.left + nested.right.right == 10.0, "nested sum")
    1.0
}
main()
```

> Verified in `test_300_Stress_Deeply_Nested_Generics.flux`

### Higher-Order Functions and Composition

Functions accepting functions as parameters — composition, repeated application:

```flux
def apply_twice(f, x) -> Double { f(f(x)) }
def compose(f, g, x) -> Double { f(g(x)) }
def double_it(x) -> Double { x * 2.0 }
def add_one(x) -> Double { x + 1.0 }
def square(x) -> Double { x * x }

def main() -> Double {
    assert(apply_twice(double_it, 3.0) == 12.0, "apply_twice double wrong")
    assert(apply_twice(add_one, 5.0) == 7.0, "apply_twice add_one wrong")
    assert(compose(double_it, add_one, 5.0) == 12.0, "compose wrong")
    assert(compose(add_one, double_it, 3.0) == 7.0, "compose rev wrong")
    assert(compose(square, add_one, 2.0) == 9.0, "compose square+add wrong")
    assert(apply_twice(square, 3.0) == 81.0, "apply_twice square wrong")
    1.0
}
main()
```

> Verified in `test_301_Stress_Lambda_Closures.flux`

### Enum with Multi-Field Payloads

Multi-variant enums with 1, 2, and 3 field payloads, matched with positional destructuring:

```flux
enum Maybe3 {
    One { a: Double },
    Two { a: Double, b: Double },
    Three { a: Double, b: Double, c: Double }
}

def maybe3_sum(m: Maybe3) -> Double {
    match m {
        Maybe3.One(a) -> a,
        Maybe3.Two(a, b) -> a + b,
        Maybe3.Three(a, b, c) -> a + b + c
    }
}

def main() -> Double {
    let m1 = Maybe3.One { a: 1.0 }
    let m2 = Maybe3.Two { a: 2.0, b: 3.0 }
    let m3 = Maybe3.Three { a: 4.0, b: 5.0, c: 6.0 }
    assert(maybe3_sum(m1) == 1.0, "maybe3 one")
    assert(maybe3_sum(m2) == 5.0, "maybe3 two")
    assert(maybe3_sum(m3) == 15.0, "maybe3 three")
    1.0
}
main()
```

> Verified in `test_313_Stress_Enum_Patterns_Options.flux`

### Enum Geometric Types with Computed Properties

Enums as geometric primitives with area/perimeter computations and classification:

```flux
enum Shape { Circle { radius: Double }, Rect { w: Double, h: Double }, Dot }

def area(s: Shape) -> Double {
    match s {
        Shape.Circle(r) -> 3.14159 * r * r,
        Shape.Rect(w, h) -> w * h,
        Shape.Dot -> 0.0
    }
}

def perimeter(s: Shape) -> Double {
    match s {
        Shape.Circle(r) -> 2.0 * 3.14159 * r,
        Shape.Rect(w, h) -> 2.0 * (w + h),
        Shape.Dot -> 0.0
    }
}

def classify_size(s: Shape) -> Double {
    let a = area(s)
    if a > 100.0 then 3.0
    else if a > 10.0 then 2.0
    else if a > 0.0 then 1.0
    else 0.0
}

def main() -> Double {
    let c = Shape.Circle { radius: 5.0 }
    assert(abs(area(c) - 78.53975) < 0.01, "circle area")
    assert(abs(perimeter(c) - 31.4159) < 0.01, "circle perimeter")
    let r = Shape.Rect { w: 4.0, h: 5.0 }
    assert(area(r) == 20.0, "rect area")
    assert(perimeter(r) == 18.0, "rect perimeter")
    let d = Shape.Dot
    assert(area(d) == 0.0, "dot area")
    1.0
}
main()
```

> Verified in `test_330_Stress_Enum_Complex_Patterns.flux`

### Nested Struct Field Access

Deep struct nesting with geometric computations (AABB, Circle, ShapeUnion):

```flux
struct Vec2 { x: Double, y: Double }
struct AABB { min: Vec2, max: Vec2 }
struct Circle { center: Vec2, radius: Double }
struct ShapeUnion { circle: Circle, rect: AABB, kind: Double }

def aabb_area(bb: AABB) -> Double {
    let w = bb.max.x - bb.min.x
    let h = bb.max.y - bb.min.y
    w * h
}

def circle_area(c: Circle) -> Double {
    3.14159 * c.radius * c.radius
}

def main() -> Double {
    let bb = AABB { min: Vec2 { x: 0.0, y: 0.0 }, max: Vec2 { x: 10.0, y: 5.0 } }
    assert(aabb_area(bb) == 50.0, "aabb area")

    let c = Circle { center: Vec2 { x: 5.0, y: 5.0 }, radius: 3.0 }
    assert(abs(circle_area(c) - 28.27431) < 0.01, "circle area")

    let su = ShapeUnion { circle: c, rect: bb, kind: 1.0 }
    assert(su.circle.radius == 3.0, "union circle radius")
    assert(aabb_area(su.rect) == 50.0, "union rect area")
    1.0
}
main()
```

> Verified in `test_329_Stress_Struct_Field_Access_Complex.flux`

### Deeply Nested Let-In Blocks

Scoped let-in blocks for local variable shadowing and computation:

```flux
def main() -> Double {
    let a = 10.0
    let b = {
        let a2 = a * 2.0
        let b2 = a2 + 5.0
        a2 + b2
    }
    assert(b == 45.0, "let-in block")

    let nested = {
        let outer = 10.0
        let inner = {
            let deep = outer * 3.0
            deep + 1.0
        }
        inner + outer
    }
    assert(nested == 41.0, "nested blocks")

    var acc = 0.0
    var i = 0.0
    while i < 100.0 do {
        let val = {
            let base = i
            let modified = base * 2.0 + 1.0
            modified
        }
        acc = acc + val
        i = i + 1.0
    }
    assert(acc == 10000.0, "let-in loop wrong")
    1.0
}
main()
```

> Verified in `test_317_Stress_Shadowing_LetIn.flux`

### Error Handling with Try/Catch

Throw and catch exceptions, including inside loops:

```flux
def risky_add(a: Double, b: Double) -> Double {
    if a < 0.0 then throw("neg a")
    else if b < 0.0 then throw("neg b")
    else a + b
}

def main() -> Double {
    var caught_count = 0.0
    try { risky_add(-1.0, 5.0) } catch(e) {
        caught_count = caught_count + 1.0
    }
    try { risky_add(5.0, -1.0) } catch(e) {
        caught_count = caught_count + 1.0
    }
    assert(caught_count == 2.0, "catch count wrong")

    let r1 = try { risky_add(3.0, 4.0) } catch(e) { 0.0 }
    assert(r1 == 7.0, "try ok wrong")

    let r2 = try { risky_add(-1.0, 4.0) } catch(e) { -1.0 }
    assert(r2 == -1.0, "try catch wrong")

    var sum = 0.0
    var i = 0.0
    while i < 20.0 do {
        let val = try { risky_add(i, i) } catch(e) { -1.0 }
        sum = sum + val
        i = i + 1.0
    }
    assert(sum == 380.0, "try-catch loop wrong")
    1.0
}
main()
```

> Verified in `test_308_Stress_Error_Handling.flux`

### Lifetime Annotations

Reference types with explicit lifetime parameters in struct definitions:

```flux
struct Foo<'a> { x: &'a Double }
struct Bar<'a, 'b> { first: &'a Double, second: &'b Double }

def lifetime_basic() -> Double {
    let val = 42.0
    let r = &val
    assert(*r == 42.0, "lifetime basic deref")
    let foo = Foo { x: r }
    assert(*foo.x == 42.0, "lifetime struct field")
    1.0
}

def lifetime_multiple() -> Double {
    let a = 10.0
    let b = 20.0
    let bar = Bar { first: &a, second: &b }
    assert(*bar.first == 10.0, "bar first")
    assert(*bar.second == 20.0, "bar second")
    assert(*bar.first + *bar.second == 30.0, "bar sum")
    1.0
}

def lifetime_in_loop() -> Double {
    var total = 0.0
    var i = 0.0
    while i < 50.0 do {
        let val = i * 2.0
        let r = &val
        total = total + *r
        i = i + 1.0
    }
    assert(total == 2450.0, "lifetime loop wrong")
    1.0
}
```

> Verified in `test_322_Stress_Lifetime_Patterns.flux`

### Operator Precedence and Arithmetic Edge Cases

Demonstrates precedence rules, left-associativity, modulo, floating-point division, and large-number arithmetic:

```flux
def main() -> Double {
    let a = 1.0 + 2.0 * 3.0
    assert(a == 7.0, "precedence 1")
    let b = (1.0 + 2.0) * 3.0
    assert(b == 9.0, "precedence 2")

    let e = 100.0 / 4.0 / 5.0
    assert(e == 5.0, "left assoc div")
    let g = 10.0 - 3.0 - 2.0
    assert(g == 5.0, "left assoc sub")

    assert(2.0 % 3.0 == 2.0, "mod 1")
    assert(10.0 % 3.0 == 1.0, "mod 2")

    let big = 1.0e18
    let small = 1.0e-18
    assert(abs(big * small - 1.0) < 0.001, "big*small wrong")

    var factorial = 1.0
    var j = 1.0
    while j <= 20.0 do {
        factorial = factorial * j
        j = j + 1.0
    }
    assert(abs(factorial - 2432902008176640000.0) < 1.0e8, "20! wrong")
    1.0
}
main()
```

> Verified in `test_315_Stress_Arithmetic_Edge_Cases.flux`

---

## Summary

| Feature | Syntax | Key Detail |
|---------|--------|------------|
| Generic function | `def f[T](x: T) -> T` | Explicit or inferred type args |
| Generic struct | `struct S[T] { field: T }` | Monomorphized per concrete type |
| Nested generics | `Box[Box[Double]]` | Arbitrary nesting depth |
| Async function | `async def f() -> T` | State machine, `await` suspension |
| Await | `await expr` | Non-zero = ready, zero = suspend |
| Spawn | `let t = spawn f(args)` | Launches parallel task |
| Join | `join(t)` | Blocks until task completes |
| Channel create | `flux_chan_create()` | Returns handle |
| Channel send | `flux_chan_send(ch, val)` | Sends value |
| Channel recv | `flux_chan_recv(ch)` | Receives value (blocking) |
| Add trait | `impl Add for T { def add(self, rhs) }` | Maps to `+` |
| Sub trait | `impl Sub for T { def sub(self, rhs) }` | Maps to `-` |
| Mul trait | `impl Mul for T { def mul(self, rhs) }` | Maps to `*` |
| Neg trait | `impl Neg for T { def neg(self) }` | Maps to unary `-` |
| Eq trait | `impl Eq for T { def eq(self, other) }` | Maps to `==` |
| Free-function op | `def Type_op(a, b) -> T` | Simpler alternative to traits |
| Pipe | `x \|> f \|> g` | Left-to-right data flow |
| Pipe partial | `x \|> f(arg)` | Partial application |
| Pipe method | `x \|> obj.method` | Method call on instance |
| Switch | `switch (expr) { val => result, ~ => default }` | Equality-based branching |
# FluxScript Standard Library & SPICE Integration

## Table of Contents

1. [Standard Library Overview](#standard-library-overview)
2. [math.flux — Math Utilities](#mathflux--math-utilities)
3. [trig.flux — Trigonometric Functions](#trigflux--trigonometric-functions)
4. [array.flux — Array & Matrix Operations](#arrayflux--array--matrix-operations)
5. [stats.flux — Statistics Functions](#statsflux--statistics-functions)
6. [string.flux — String Operations](#stringflux--string-operations)
7. [signal.flux — Signal Processing](#signalflux--signal-processing)
8. [cmatrix.flux — Complex Matrix Operations](#cmatrixflux--complex-matrix-operations)
9. [Matrix Operations Reference](#matrix-operations-reference)
10. [SPICE Integration](#spice-integration)

---

## Standard Library Overview

FluxScript provides 7 standard library modules, imported via `import`:

```
import math
import trig
import array
import stats
import string
import signal
import cmatrix
```

All functions operate on FluxScript's native `Double` type and `matrix` type. Matrix operations use row-major indexing with 0-based indices.

---

## math.flux — Math Utilities

Core math functions for numerical computation.

### `max(a, b)` → `Double`

Return the larger of two numbers.

```
max(3.0, 5.0)    # → 5.0
max(-1.0, 0.0)   # → 0.0
```

### `min(a, b)` → `Double`

Return the smaller of two numbers.

```
min(3.0, 5.0)    # → 3.0
```

### `clamp(x, lo, hi)` → `Double`

Constrain `x` to the `[lo, hi]` interval. Throws if `lo > hi`.

```
clamp(5.0, 0.0, 3.0)   # → 3.0
clamp(1.5, 0.0, 3.0)   # → 1.5
clamp(-1.0, 0.0, 3.0)  # → 0.0
```

### `lerp(a, b, t)` → `Double`

Linear interpolation between `a` and `b` by factor `t`. Returns `a + (b - a) * t`.

```
lerp(0.0, 10.0, 0.5)   # → 5.0
lerp(2.0, 8.0, 0.25)   # → 3.5
```

### `rad2deg(r)` → `Double`

Convert radians to degrees. Returns `r * 180/pi`.

```
rad2deg(pi())    # → 180.0
```

### `deg2rad(d)` → `Double`

Convert degrees to radians. Returns `d * pi/180`.

```
deg2rad(180.0)   # → 3.14159...
```

### `sign(x)` → `Double`

Return -1, 0, or +1 indicating sign of `x`.

```
sign(-5.0)   # → -1.0
sign(0.0)    # → 0.0
sign(3.0)    # → 1.0
```

### `is_close(a, b)` → `Double`

Test if `a` and `b` are approximately equal (within `1e-9`).

```
is_close(1.0, 1.0 + 1e-10)   # → 1.0 (true)
is_close(1.0, 2.0)           # → 0.0 (false)
```

### `step(x)` → `Double`

Unit step function. Returns `0` if `x < 0`, `1` if `x >= 0`.

```
step(-1.0)   # → 0.0
step(0.0)    # → 1.0
step(5.0)    # → 1.0
```

### `factorial(n)` → `Double`

Compute `n!` (n factorial). Throws if `n < 0`.

```
factorial(5.0)   # → 120.0
factorial(0.0)   # → 1.0
```

### `gcd(a, b)` → `Double`

Greatest common divisor using Euclidean algorithm.

```
gcd(12.0, 8.0)   # → 4.0
gcd(7.0, 13.0)   # → 1.0
```

### `lcm(a, b)` → `Double`

Least common multiple.

```
lcm(4.0, 6.0)   # → 12.0
lcm(0.0, 5.0)   # → 0.0
```

### `map_range(x, in_lo, in_hi, out_lo, out_hi)` → `Double`

Remap `x` from `[in_lo, in_hi]` to `[out_lo, out_hi]`. Throws if `in_lo == in_hi`.

```
map_range(5.0, 0.0, 10.0, 0.0, 100.0)   # → 50.0
```

### `smoothstep(x)` → `Double`

Cubic Hermite interpolation `3t² - 2t³`. Input is clamped to `[0, 1]`.

```
smoothstep(0.5)   # → 0.5
```

### `round_to(x, n)` → `Double`

Round `x` to `n` decimal places.

```
round_to(3.14159, 2.0)   # → 3.14
```

### `frac(x)` → `Double`

Fractional part of `x`. Returns `x - floor(x)`.

```
frac(3.7)   # → 0.7
frac(-2.3)  # → 0.7
```

### `wrap(x, lo, hi)` → `Double`

Wrap `x` into `[lo, hi)` range. Throws if `lo >= hi`.

```
wrap(15.0, 0.0, 10.0)   # → 5.0
```

### `pingpong(x, lo, hi)` → `Double`

Bounce `x` back and forth between `lo` and `hi`. Throws if `lo >= hi`.

```
pingpong(15.0, 0.0, 10.0)   # → 5.0
```

### `inverse_lerp(x, a, b)` → `Double`

Determine where `x` lies between `a` and `b`. Returns `0` if `x == a`, `1` if `x == b`.

```
inverse_lerp(5.0, 0.0, 10.0)   # → 0.5
```

### `sigmoid(x)` → `Double`

Logistic sigmoid function. Returns `1 / (1 + e^(-x))`.

```
sigmoid(0.0)   # → 0.5
```

### `normalize_angle(x)` → `Double`

Wrap angle to `[0, 2π)`.

```
normalize_angle(3*pi())   # → 3.14159...
```

### `normalize_angle_sym(x)` → `Double`

Wrap angle to `[-π, π]`.

```
normalize_angle_sym(3*pi())   # → 3.14159...
```

### `softplus(x)` → `Double`

Smooth ReLU: `log(1 + e^x)`.

```
softplus(0.0)   # → 0.6931...
```

### `clamp01(x)` → `Double`

Clamp `x` to `[0, 1]`.

```
clamp01(1.5)   # → 1.0
clamp01(-0.5)  # → 0.0
```

### `relu(x)` → `Double`

Rectified linear unit. Returns `0` if `x < 0`, `x` otherwise.

```
relu(-3.0)   # → 0.0
relu(5.0)    # → 5.0
```

### `logit(x)` → `Double`

Log-odds function (inverse of sigmoid). Throws if `x <= 0` or `x >= 1`. Returns `log(x / (1 - x))`.

```
logit(0.5)   # → 0.0
```

### `avg(a, b)` → `Double`

Arithmetic mean of two numbers.

```
avg(2.0, 8.0)   # → 5.0
```

### `comb(n, k)` → `Double`

Binomial coefficient (n choose k). Throws if `n < 0` or `k < 0`.

```
comb(5.0, 2.0)   # → 10.0
```

### `perm(n, k)` → `Double`

Permutations P(n, k) = n! / (n-k)!. Throws if `n < 0` or `k < 0`.

```
perm(5.0, 2.0)   # → 20.0
```

### `reflect(v, n)` → `Double`

Reflect vector `v` about normal `n`. Returns `v - 2 * dot(v, n) * n`.

```
reflect(1.0, 1.0)   # → -1.0
```

---

## trig.flux — Trigonometric Functions

Extended trigonometric and hyperbolic functions.

### `sec(x)` → `Double`

Secant: `1 / cos(x)`. Domain: `x != π/2 + nπ`.

```
sec(0.0)   # → 1.0
```

### `csc(x)` → `Double`

Cosecant: `1 / sin(x)`. Domain: `x != nπ`.

```
csc(pi() / 2.0)   # → 1.0
```

### `cot(x)` → `Double`

Cotangent: `cos(x) / sin(x)`. Domain: `x != nπ`.

```
cot(pi() / 4.0)   # → 1.0
```

### `sinc(x)` → `Double`

Normalized sinc: `sin(x)/x`, with limit at 0. Returns `1` if `|x| < 1e-15`.

```
sinc(0.0)           # → 1.0
sinc(pi())          # → 0.0
```

### `asec(x)` → `Double`

Inverse secant: `acos(1/x)`. Domain: `|x| >= 1`.

```
asec(1.0)   # → 0.0
```

### `acsc(x)` → `Double`

Inverse cosecant: `asin(1/x)`. Domain: `|x| >= 1`.

```
acsc(1.0)   # → 1.5708...
```

### `acot(x)` → `Double`

Inverse cotangent: `atan2(1, x)`. Domain: all real.

```
acot(1.0)   # → 0.7854...
```

### `sech(x)` → `Double`

Hyperbolic secant: `2 / (e^x + e^(-x))`. Domain: all real.

```
sech(0.0)   # → 1.0
```

### `csch(x)` → `Double`

Hyperbolic cosecant: `2 / (e^x - e^(-x))`. Domain: `x != 0`.

```
csch(1.0)   # → 0.8509...
```

### `coth(x)` → `Double`

Hyperbolic cotangent: `(e^x + e^(-x)) / (e^x - e^(-x))`. Domain: `x != 0`.

```
coth(1.0)   # → 1.3130...
```

### `degrees(r)` → `Double`

Convert radians to degrees (alias for `rad2deg`).

```
degrees(pi())   # → 180.0
```

### `radians(d)` → `Double`

Convert degrees to radians (alias for `deg2rad`).

```
radians(90.0)   # → 1.5708...
```

### `haversine(x)` → `Double`

Haversine: `(1 - cos(x)) / 2 = sin²(x/2)`.

```
haversine(0.0)   # → 0.0
haversine(pi())  # → 1.0
```

### `vercosine(x)` → `Double`

Vercosine: `(1 + cos(x)) / 2 = cos²(x/2)`.

```
vercosine(0.0)   # → 1.0
vercosine(pi())  # → 0.0
```

---

## array.flux — Array & Matrix Operations

Matrix creation, manipulation, and linear algebra utilities.

### `eye(n)` → `matrix`

Create an `n × n` identity matrix.

```
eye(3.0)
# → [[1, 0, 0],
#     [0, 1, 0],
#     [0, 0, 1]]
```

### `ones(r, c)` → `matrix`

Create an `r × c` matrix of all ones.

```
ones(2.0, 3.0)
# → [[1, 1, 1],
#     [1, 1, 1]]
```

### `sum(m)` → `Double`

Sum of all matrix elements.

```
sum([[1, 2], [3, 4]])   # → 10.0
```

### `mean(m)` → `Double`

Mean of all matrix elements.

```
mean([[1, 2], [3, 4]])   # → 2.5
```

### `linspace(start, stop, n)` → `matrix`

Create a column vector of `n` evenly spaced values from `start` to `stop`. Throws if `n < 2`.

```
linspace(0.0, 10.0, 3.0)
# → [[0.0],
#     [5.0],
#     [10.0]]
```

### `logspace(start, stop, n)` → `matrix`

Create a column vector of `n` logarithmically spaced values from `10^start` to `10^stop`. Throws if `n < 2`.

```
logspace(0.0, 3.0, 4.0)
# → [[1.0],
#     [10.0],
#     [100.0],
#     [1000.0]]
```

### `arange(start, stop, step)` → `matrix`

Create a column vector from `start` to `stop` (exclusive) with given `step`. Throws if `step == 0`.

```
arange(0.0, 5.0, 1.0)
# → [[0.0], [1.0], [2.0], [3.0], [4.0]]
```

### `flatten(m)` → `matrix`

Reshape matrix into a column vector (row-major order).

```
flatten([[1, 2], [3, 4]])
# → [[1], [2], [3], [4]]
```

### `reshape(m, new_r, new_c)` → `matrix`

Reshape matrix to new dimensions. Throws if total element count doesn't match.

```
reshape([1, 2, 3, 4, 5, 6], 2.0, 3.0)
# → [[1, 2, 3],
#     [4, 5, 6]]
```

### `diff(m)` → `matrix`

Forward differences of matrix elements. Returns a column vector of `(n-1)` differences.

```
diff([1, 3, 6, 10])
# → [[2], [3], [4]]
```

### `cumsum(m)` → `matrix`

Cumulative sum of matrix elements. Returns matrix same shape as input.

```
cumsum([1, 2, 3])
# → [[1], [3], [6]]
```

### `cumprod(m)` → `matrix`

Cumulative product of matrix elements. Returns matrix same shape as input.

```
cumprod([1, 2, 3])
# → [[1], [2], [6]]
```

### `argsort(m)` → `matrix`

Indices that would sort the matrix in ascending order. Returns a column vector.

```
argsort([3, 1, 2])
# → [[1], [2], [0]]
```

### `unique(m)` → `matrix`

Extract unique values from matrix. Returns column vector in order of first occurrence.

```
unique([1, 2, 1, 3, 2])
# → [[1], [2], [3]]
```

### `pad_zeros(m, pad_r, pad_c)` → `matrix`

Pad matrix with zeros around edges. Returns `(r + 2*pad_r) × (c + 2*pad_c)` matrix.

```
pad_zeros([[1, 2], [3, 4]], 1.0, 1.0)
# → [[0, 0, 0, 0],
#     [0, 1, 2, 0],
#     [0, 3, 4, 0],
#     [0, 0, 0, 0]]
```

### `meshgrid(x, y)` → `matrix`

2D coordinate grids from `x` and `y` vectors. Returns `[X | Y]` matrix with `ny` rows and `2*nx` columns.

```
x = [1.0, 2.0]
y = [3.0, 4.0]
meshgrid(x, y)
# → X grid columns | Y grid columns
```

### `flipud(m)` → `matrix`

Flip matrix upside down (reverse row order).

### `fliplr(m)` → `matrix`

Flip matrix left-right (reverse column order).

### `rot90(m)` → `matrix`

Rotate matrix 90 degrees counter-clockwise. Returns a `(c × r)` matrix.

### `triu(m)` → `matrix`

Upper triangular part of matrix. Elements below diagonal set to 0.

### `tril(m)` → `matrix`

Lower triangular part of matrix. Elements above diagonal set to 0.

### `repmat(m, rep_r, rep_c)` → `matrix`

Replicate/tile matrix into larger matrix. Throws if `rep_r < 1` or `rep_c < 1`.

```
repmat([1, 2], 2.0, 3.0)
# → [[1, 2, 1, 2, 1, 2],
#     [1, 2, 1, 2, 1, 2]]
```

### `trapz(x, y)` → `Double`

Trapezoidal integration. `x` and `y` are column vectors.

```
x = [0.0, 1.0, 2.0]
y = [0.0, 1.0, 4.0]
trapz(x, y)   # → 3.0
```

### `nonzero(m)` → `matrix`

Find non-zero element positions. Returns `(k × 2)` matrix where each row is `[row_index, col_index]`.

### `clip(m, lo, hi)` → `matrix`

Clamp every element of matrix to `[lo, hi]`. Throws if `lo > hi`.

### `squeeze(m)` → `matrix`

Remove singleton dimensions. `1×1 → 1×1`, `1×N → 1×N`, `N×1 → N×1`.

### `flip(m)` → `matrix`

Reverse element order (180-degree rotation of flattened data).

### `sort(m)` → `matrix`

Sort matrix elements in ascending order. Returns a column vector.

```
sort([3, 1, 2])
# → [[1], [2], [3]]
```

### `replace(m, old, new)` → `matrix`

Replace all occurrences of `old` value with `new` value.

### `count_eq(m, val)` → `Double`

Count occurrences of `val` in matrix.

### `find_eq(m, val)` → `matrix`

Find all positions where matrix element equals `val`. Returns `(k × 2)` matrix.

---

## stats.flux — Statistics Functions

Statistical analysis functions operating on matrices.

### `variance(m)` → `Double`

Population variance: `Σ(x - mean)² / N`.

```
variance([1, 2, 3, 4, 5])   # → 2.0
```

### `std(m)` → `Double`

Population standard deviation: `√(variance(m))`.

```
std([1, 2, 3, 4, 5])   # → 1.4142...
```

### `geometric_mean(m)` → `Double`

Geometric mean: `exp(Σ log(x) / N)`. Throws if any element `<= 0`.

```
geometric_mean([1, 2, 4])   # → 2.0
```

### `harmonic_mean(m)` → `Double`

Harmonic mean: `N / Σ(1/x)`. Throws if any element `== 0`.

```
harmonic_mean([1, 2, 4])   # → 1.7142...
```

### `cv(m)` → `Double`

Coefficient of variation: `std / |mean|`. Throws if mean `== 0`.

```
cv([1, 2, 3, 4, 5])   # → 0.4714...
```

### `min_element(m)` → `Double`

Find minimum value in matrix.

```
min_element([3, 1, 4, 1, 5])   # → 1.0
```

### `max_element(m)` → `Double`

Find maximum value in matrix.

```
max_element([3, 1, 4, 1, 5])   # → 5.0
```

### `median(m)` → `Double`

Median value of matrix elements. Returns middle value (or average of two middle values if even count).

```
median([1, 3, 5])       # → 3.0
median([1, 2, 3, 4])    # → 2.5
```

### `zscore(m)` → `matrix`

Standardize matrix to zero mean, unit variance. Throws if `std == 0`.

```
zscore([1, 2, 3])
# → [[-1.2247], [0.0], [1.2247]]
```

### `entropy(p)` → `Double`

Shannon entropy: `-Σ(p_i * log(p_i))` with `0*log(0) = 0` convention.

```
entropy([0.5, 0.5])   # → 0.6931...
```

### `mad(m)` → `Double`

Mean absolute deviation from the mean: `Σ|x - mean| / N`.

```
mad([1, 2, 3, 4, 5])   # → 1.2
```

### `rms(m)` → `Double`

Root mean square: `√(Σ(x²) / N)`.

```
rms([1, 2, 3])   # → 2.1602...
```

### `mode(m)` → `Double`

Most frequent value in matrix.

```
mode([1, 2, 2, 3])   # → 2.0
```

### `iqr(m)` → `Double`

Interquartile range (75th - 25th percentile).

```
iqr([1, 2, 3, 4, 5, 6, 7, 8])   # → 4.0
```

### `moment(m, k)` → `Double`

k-th central moment: `Σ(x - mean)^k / N`.

```
moment([1, 2, 3, 4, 5], 2.0)   # → 2.0 (variance)
```

### `normalize_minmax(m)` → `matrix`

Scale matrix to `[0, 1]` range: `(x - min) / (max - min)`. Returns zeros if all equal.

```
normalize_minmax([0, 5, 10])
# → [[0.0], [0.5], [1.0]]
```

### `range_val(m)` → `Double`

Range (max - min) of matrix elements.

```
range_val([1, 5, 3])   # → 4.0
```

---

## string.flux — String Operations

Low-level string manipulation functions wrapping C FFI.

### `len(s)` → `Double`

Return the length of string `s`.

```
len("hello")   # → 5.0
```

### `at(s, i)` → `Double`

Return the ASCII value of the character at index `i`.

```
at("ABC", 1.0)   # → 66.0 (ASCII for 'B')
```

### `cmp(a, b)` → `Double`

Compare two strings lexicographically. Returns `< 0` if `a < b`, `0` if equal, `> 0` if `a > b`.

### `slice(s, start, cnt)` → `string`

Extract `cnt` characters starting at index `start`.

```
slice("hello", 1.0, 3.0)   # → "ell"
```

### `find(s, sub)` → `Double`

Find first occurrence of substring `sub` in `s`. Returns index or `-1` if not found.

```
find("hello world", "world")   # → 6.0
find("hello", "xyz")           # → -1.0
```

### `regex(s, pat)` → `Double`

Test if string `s` matches regular expression pattern `pat`. Returns `1.0` if match, `0.0` otherwise.

```
regex("123", "[0-9]+")   # → 1.0
regex("abc", "[0-9]+")   # → 0.0
```

---

## signal.flux — Signal Processing

Window functions, waveform generators, and FFT utilities.

### Window Functions

All window functions take `n` (window length, `>= 2`) and return a column vector of length `n`.

#### `hann(n)` → `matrix`

Hann window (raised cosine): `0.5 * (1 - cos(2πi / (n-1)))`.

```
hann(5.0)
# → [[0.0], [0.5], [1.0], [0.5], [0.0]]
```

#### `hamming(n)` → `matrix`

Hamming window: `0.54 - 0.46 * cos(2πi / (n-1))`.

#### `blackman(n)` → `matrix`

Blackman window: `0.42 - 0.5 * cos(2πi/(n-1)) + 0.08 * cos(4πi/(n-1))`.

#### `bartlett(n)` → `matrix`

Bartlett (triangle) window: `1 - |2i/(n-1) - 1|`.

### Waveform Generators

#### `square(t, duty)` → `Double`

Square wave at normalized time `t` (period = 1). `duty` is the fraction of period at `+1` (must be in `(0, 1)`).

```
square(0.25, 0.5)   # → 1.0
square(0.75, 0.5)   # → -1.0
```

#### `sawtooth(t)` → `Double`

Sawtooth wave at normalized time `t` (period = 1). Returns `frac(t)`.

```
sawtooth(2.3)   # → 0.3
```

#### `triangle(t)` → `Double`

Triangle wave at normalized time `t` (period = 1). Ranges `-1` to `1`.

```
triangle(0.25)   # → 1.0
```

#### `pulse_train(t, width)` → `Double`

Pulse train at normalized time `t` (period = 1). `width` is pulse width in `(0, 1)`.

```
pulse_train(0.1, 0.3)   # → 1.0
pulse_train(0.5, 0.3)   # → 0.0
```

#### `chirp(t, f0, f1, t1)` → `Double`

Linear chirp signal. Sweeps from `f0` to `f1` Hz over `[0, t1]` seconds.

```
chirp(0.0, 100.0, 200.0, 1.0)   # → 0.0
```

### FFT Utilities

#### `fftfreq(n, d)` → `matrix`

FFT frequency bins (numpy-compatible). `n` is number of FFT points, `d` is sample spacing in seconds. Returns column vector of `n` frequency values.

```
fftfreq(8.0, 1.0)
# → [[0.0], [0.125], [0.25], [0.375], [-0.5], [-0.375], [-0.25], [-0.125]]
```

#### `unwrap(phase)` → `matrix`

Unwrap radian phase vector. Corrects discontinuities greater than `π`.

```
unwrap([0.0, 3.0, -3.0, 0.0])
# → [[0.0], [3.0], [3.2832], [6.2832]]
```

---

## cmatrix.flux — Complex Matrix Operations

Complex matrix construction and manipulation. All functions return complex-valued matrices (`<2 x double>` elements).

### `cmatrix(rows, cols)` → `matrix`

Create an empty (zero) complex matrix of size `rows × cols`.

### `complex_eye(n)` → `matrix`

Identity complex matrix of size `n × n`.

### `complex_ones(rows, cols)` → `matrix`

Complex matrix of all ones.

### `complex_zeros(rows, cols)` → `matrix`

Complex matrix of all zeros.

### `ctranspose(m)` → `matrix`

Conjugate transpose (Hermitian transpose) of complex matrix.

### `conj(m)` → `matrix`

Element-wise complex conjugate of matrix.

### `complex_transpose(m)` → `matrix`

Transpose (without conjugation) of complex matrix.

### `complex_inv(m)` → `matrix`

Inverse of a complex square matrix.

### `complex_det(m)` → `Double`

Determinant magnitude of a complex square matrix.

### `complex_trace(m)` → `Double`

Trace (sum of diagonal real parts) of complex matrix.

### `cmatrix_get(mat, row, col)` → `complex`

Get element at `(row, col)` as a complex number.

### `cmatrix_set_ri(mat, row, col, real, imag)` → `void`

Set element at `(row, col)` from real and imaginary parts.

### `rows(m)` → `Double`

Number of rows of a complex matrix.

### `cols(m)` → `Double`

Number of columns of a complex matrix.

---

## Matrix Operations Reference

FluxScript provides built-in matrix operations accessible without import:

| Function | Description |
|---|---|
| `matrix_zeros(r, c)` | Create `r × c` zero matrix |
| `matrix_eye(n)` | Create `n × n` identity matrix |
| `matrix_ones(r, c)` | Create `r × c` matrix of ones |
| `matrix_rows(m)` | Get number of rows |
| `matrix_cols(m)` | Get number of columns |
| `matrix_get(m, r, c)` | Get element at `(r, c)` |
| `matrix_set(m, r, c, val)` | Set element at `(r, c)` |
| `matrix_sum(m)` | Sum of all elements |
| `matrix_mean(m)` | Mean of all elements |
| `matrix_copy(m)` | Deep copy of matrix |
| `matrix_solve(A, b)` | Solve linear system `Ax = b` |
| `matrix_slice(m, r0, r1, c0, c1)` | Extract submatrix |

Matrix arithmetic operators:
- `+` — element-wise addition
- `-` — element-wise subtraction
- `*` — element-wise multiplication (not matrix multiply)
- `/` — element-wise division

Matrix literals: `[[1, 2, 3], [4, 5, 6]]` creates a 2×3 matrix.

---

## SPICE Integration

FluxScript includes a complete SPICE-compatible circuit simulation framework with:

- **Component enum** — 21 circuit element types
- **Circuit class** — component storage and management
- **MNA solver** — Modified Nodal Analysis for DC operating point
- **Netlist parser** — SPICE `.sp`/`.cir` file reader

### Component Enum Variants

All circuit elements are represented as variants of the `Component` enum:

```flux
import circuit

enum Component {
    R { n_plus: Double, n_minus: Double, r_val: Double }
    C { n_plus: Double, n_minus: Double, c_val: Double, init_v: Double }
    L { n_plus: Double, n_minus: Double, l_val: Double, init_i: Double }
    Vdc { n_plus: Double, n_minus: Double, v_val: Double }
    Idc { n_plus: Double, n_minus: Double, i_val: Double }
    Vac { n_plus: Double, n_minus: Double, dc_val: Double, ac_mag: Double, ac_freq: Double, ac_phase: Double }
    Iac { n_plus: Double, n_minus: Double, dc_val: Double, ac_mag: Double, ac_freq: Double, ac_phase: Double }
    Vpulse { n_plus: Double, n_minus: Double, per: Double, v1: Double, v2: Double, td: Double, tr: Double, tfall: Double, pw: Double }
    Vsin { n_plus: Double, n_minus: Double, voff: Double, vamp: Double, freq: Double, phase: Double }
    E { n_op: Double, n_on: Double, n_ip: Double, n_in: Double, gain: Double }    # VCVS
    G { n_op: Double, n_on: Double, n_ip: Double, n_in: Double, gm: Double }     # VCCS
    D { n_plus: Double, n_minus: Double, isat: Double, n_factor: Double, vt: Double }
    Qnpn { n_c: Double, n_b: Double, n_e: Double, bf: Double, isat: Double, vaf: Double, vt: Double }
    Qpnp { n_c: Double, n_b: Double, n_e: Double, bf: Double, isat: Double, vaf: Double, vt: Double }
    Mnmos { n_d: Double, n_g: Double, n_s: Double, n_bulk: Double, kp: Double, vto: Double, lambda_val: Double }
    Mpmos { n_d: Double, n_g: Double, n_s: Double, n_bulk: Double, kp: Double, vto: Double, lambda_val: Double }
    Njf { n_d: Double, n_g: Double, n_s: Double, beta_val: Double, vto: Double, lambda_val: Double }
    Pjf { n_d: Double, n_g: Double, n_s: Double, beta_val: Double, vto: Double, lambda_val: Double }
    S { n_plus: Double, n_minus: Double, n_cp: Double, n_cm: Double, ron: Double, roff: Double, von: Double, voff: Double }
    W { n_plus: Double, n_minus: Double, vsrc_idx: Double, ron: Double, roff: Double, ion: Double, ioff: Double }
}
```

**Type codes** (stored in column 0 of the component matrix):

| Code | Type |
|---|---|
| 0 | Resistor (R) |
| 1 | Capacitor (C) |
| 2 | Inductor (L) |
| 3 | DC Voltage source (Vdc) |
| 4 | DC Current source (Idc) |
| 5 | AC Voltage source (Vac) |
| 6 | AC Current source (Iac) |
| 7 | VCVS (E) |
| 8 | VCCS (G) |
| 9 | Diode (D) |
| 10 | NPN BJT (Qnpn) |
| 11 | PNP BJT (Qpnp) |
| 12 | NMOS (Mnmos) |
| 13 | PMOS (Mpmos) |
| 14 | N-channel JFET (Njf) |
| 15 | P-channel JFET (Pjf) |
| 16 | Voltage-controlled switch (S) |
| 17 | Current-controlled switch (W) |

### Circuit Class

The `Circuit` class manages component storage and provides methods for circuit construction.

#### `circuit_create(num_nodes, max_comps)` → `Circuit`

Create a new circuit with space for `num_nodes` nodes and `max_comps` components.

```flux
var c = circuit_create(10.0, 100.0)
```

#### `Circuit.add(comp)` → `Double`

Add a `Component` to the circuit. Returns the component index.

```flux
circuit.add(Component.R { n_plus: 1.0, n_minus: 0.0, r_val: 1000.0 })
```

#### `Circuit.count()` → `Double`

Return the number of components in the circuit.

#### `Circuit.num_nodes()` → `Double`

Return the number of non-ground nodes.

#### `Circuit.type_code(idx)` → `Double`

Return the type code of component at index `idx`.

#### `Circuit.node_p(idx)` → `Double`

Return the positive node of component at index `idx`.

#### `Circuit.node_n(idx)` → `Double`

Return the negative node of component at index `idx`.

#### `Circuit.node_3(idx)` → `Double`

Return the third node (if applicable) of component at index `idx`.

#### `Circuit.get_param(idx, col)` → `Double`

Get a parameter value from component at index `idx`, column `col`.

#### `Circuit.set_param(idx, col, val)` → `void`

Set a parameter value in component at index `idx`, column `col`.

### Convenience Circuit Functions

These functions take a `Circuit` directly and add components:

```flux
circuit_add_resistor(c, n_plus, n_minus, r_val)        # → component index
circuit_add_capacitor(c, n_plus, n_minus, c_val, init_v)
circuit_add_inductor(c, n_plus, n_minus, l_val, init_i)
circuit_add_vdc(c, n_plus, n_minus, v_val)
circuit_add_idc(c, n_plus, n_minus, i_val)
circuit_add_vac(c, n_plus, n_minus, dc_val, ac_mag, ac_freq, ac_phase)
circuit_add_iac(c, n_plus, n_minus, dc_val, ac_mag, ac_freq, ac_phase)
circuit_add_vpulse(c, n_plus, n_minus, per, v1, v2, td, tr, tfall, pw)
circuit_add_vsin(c, n_plus, n_minus, voff, vamp, freq, phase)
circuit_add_vcvs(c, n_op, n_on, n_ip, n_in, gain)     # VCVS (E)
circuit_add_vccs(c, n_op, n_on, n_ip, n_in, gm)       # VCCS (G)
circuit_add_diode(c, n_plus, n_minus, isat, n_factor, vt)
circuit_add_npn(c, n_c, n_b, n_e, bf, isat, vaf, vt)
circuit_add_pnp(c, n_c, n_b, n_e, bf, isat, vaf, vt)
circuit_add_nmos(c, n_d, n_g, n_s, n_b, kp, vto, lambda_val)
circuit_add_pmos(c, n_d, n_g, n_s, n_b, kp, vto, lambda_val)
circuit_add_njf(c, n_d, n_g, n_s, beta_val, vto, lambda_val)
circuit_add_pjf(c, n_d, n_g, n_s, beta_val, vto, lambda_val)
circuit_add_vcsw(c, n_plus, n_minus, n_cp, n_cm, ron, roff, von, voff)  # VCSW (S)
circuit_add_ccsw(c, n_plus, n_minus, vsrc_idx, ron, roff, ion, ioff)     # CCSW (W)
```

### Legacy Free Functions

For backward compatibility, raw-matrix versions are also available:

```flux
circuit_count(ctrl)                          # → component count
circuit_num_nodes(ctrl)                      # → number of nodes
circuit_component_type(comps, idx)           # → type code
circuit_node_p(comps, idx)                   # → positive node
circuit_node_n(comps, idx)                   # → negative node
circuit_node_3(comps, idx)                   # → third node
circuit_param(comps, idx, col)               # → parameter value
circuit_set_param(comps, idx, col, val)      # → void
```

### MNA Solver (Modified Nodal Analysis)

The MNA solver computes DC operating point by stamping components into the conductance matrix and solving the linear system.

#### Core Stamp Functions

```flux
mna_stamp_g(A, np, nm, g, N)
```
Stamp a conductance `g` between nodes `np` and `nm` into matrix `A`. `N` is the total number of nodes.

```flux
mna_stamp_vsource(A, b, np, nm, v_val, N, bi)
```
Stamp a voltage source of value `v_val` between nodes `np` and `nm`. `bi` is the branch index for the extra equation.

```flux
mna_stamp_isource(b, np, nm, i_val, N)
```
Stamp a current source of value `i_val` from node `np` to `nm` into the RHS vector `b`.

```flux
mna_stamp_vcvs(A, np, nm, n_ctrl_p, n_ctrl_n, gain, N, bi)
```
Stamp a voltage-controlled voltage source (VCVS) with gain `gain`.

```flux
mna_stamp_vccs(A, np, nm, n_ctrl_p, n_ctrl_n, gm, N)
```
Stamp a voltage-controlled current source (VCCS) with transconductance `gm`.

#### Solver Functions

```flux
mna_dc_solve(c)         # → solution vector (Circuit version)
mna_dc_solve_raw(comps, ctrl)   # → solution vector (raw matrix version)
```

Returns a solution vector with format:
- `sol[0]` = number of nodes (N)
- `sol[1]` = number of branches (M)
- `sol[2..N+1]` = node voltages (V1 through VN)
- `sol[N+2..N+M+1]` = branch currents

#### Solution Extraction

```flux
mna_get_node_voltage(sol, nd)           # → voltage at node nd (node 0 = ground = 0V)
mna_get_branch_current(sol, branch_idx)  # → current through branch branch_idx
mna_extract_voltages(sol)                # → column vector of all node voltages [0, V1, ..., VN]
```

#### Helper Functions

```flux
mna_ndx(n)                    # → node-to-matrix index (n - 1)
mna_branch_index(comps, ctrl, comp_idx)  # → branch index for component at comp_idx
mna_count_branches(comps, ctrl)          # → total number of branches (inductors + V sources)
mna_build_A_b(c)                         # → build A matrix and b vector (returns [M, dim])
```

### Complete Example: Voltage Divider

```flux
import circuit
import mna

# Create circuit: 2 nodes, 10 components max
var c = circuit_create(2.0, 10.0)

# Add 5V source between node 1 and ground (node 0)
circuit_add_vdc(c, 1.0, 0.0, 5.0)

# Add R1 = 1kΩ between node 1 and node 2
circuit_add_resistor(c, 1.0, 2.0, 1000.0)

# Add R2 = 1kΩ between node 2 and ground
circuit_add_resistor(c, 2.0, 0.0, 1000.0)

# Solve DC operating point
var sol = mna_dc_solve(c)

# Get voltages
var v1 = mna_get_node_voltage(sol, 1.0)   # → 5.0V
var v2 = mna_get_node_voltage(sol, 2.0)   # → 2.5V
```

### Netlist Parser

The netlist parser reads standard SPICE `.sp`/`.cir` files into the component matrix format.

#### `netlist_parse(filepath, comps, ctrl)` → `Double`

Parse a SPICE netlist file. Returns the number of nodes in the circuit.

```flux
import circuit
import mna

var c = circuit_create(100.0, 500.0)
var N = netlist_parse("circuit.sp", c.comps, c.ctrl)
var sol = mna_dc_solve(c)
```

#### Supported Elements

| Prefix | Element | Syntax |
|---|---|---|
| `R` | Resistor | `Rxxx n+ n- value` |
| `C` | Capacitor | `Cxxx n+ n- value [IC=v0]` |
| `L` | Inductor | `Lxxx n+ n- value [IC=i0]` |
| `V` | Voltage source | `Vxxx n+ n- DC value [AC mag phase]` |
| `I` | Current source | `Ixxx n+ n- value` |
| `D` | Diode | `Dxxx n+ n- model` |
| `Q` | BJT | `Qxxx nc nb ne model` |
| `M` | MOSFET | `Mxxx nd ng ns nb model` |
| `J` | JFET | `Jxxx nd ng ns model` |
| `S` | V-controlled switch | `Sxxx n+ n- nc+ nc- model` |

#### Supported Directives

| Directive | Description |
|---|---|
| `.model` | Define device model parameters |
| `*` or `;` or `#` | Comment lines |

#### Model Types

`.model` supports these device types:

| Type | Devices | Parameters |
|---|---|---|
| `NPN` | NPN BJT | Is, Bf, Vaf, Vt |
| `PNP` | PNP BJT | Is, Bf, Vaf, Vt |
| `NMOS` | NMOS MOSFET | Is, Kp, Vto, lambda |
| `PMOS` | PMOS MOSFET | Is, Kp, Vto, lambda |
| `NJF` | N-channel JFET | Is, Beta, Vto, lambda |
| `PJF` | P-channel JFET | Is, Beta, Vto, lambda |
| `VSW` | Voltage switch | Ron, Roff, Von, Voff |
| `ISW` | Current switch | Ron, Roff, Ion, Ioff |
| `D` | Diode | Is, N, Vt |

#### Example Netlist

```spice
* Voltage Divider
V1 1 0 DC 5
R1 1 2 1k
R2 2 0 2k
.op
```

#### SI Suffixes

The parser supports SI suffixes for component values:

| Suffix | Multiplier |
|---|---|
| `k` | 1e3 (kilo) |
| `M` | 1e6 (mega) |
| `m` | 1e-3 (milli) |
| `u` | 1e-6 (micro) |
| `n` | 1e-9 (nano) |
| `p` | 1e-12 (pico) |
| `f` | 1e-15 (femto) |
# FluxScript Examples & Quickstart

This section provides a comprehensive collection of runnable FluxScript examples, progressing from basic language features to advanced circuit simulation. Every example is self-contained and can be executed with the `flux` CLI.

---

## Quickstart

### Installation

```bash
git clone https://github.com/Janadasroor/fluxscript.git
cd fluxscript
make build        # Release build (Ninja + mold + ccache)
make test         # Run test suite
```

**Dependencies:** LLVM 21, CMake, Ninja, mold linker, ccache.

### Your First Program

Create a file called `hello.flux`:

```flux
println("Hello, FluxScript!")
42.0
```

Run it:

```bash
flux hello.flux
```

FluxScript JIT-compiles your code via LLVM. The last expression is the return value. Every numeric literal is `Double` by default.

### CLI Basics

```bash
flux <file.flux>          # JIT execute (default)
flux check <file.flux>    # Parse + type-check without executing
flux --emit=llvm <file.flux>  # Emit LLVM IR
flux --cache=0 <file.flux>    # Disable JIT cache (use after rebuilds)
```

---

## Example 1: Variables and Basic Arithmetic

The fundamental building blocks: variables, arithmetic, and printing.

```flux
# Immutable binding
let pi = 3.14159265
let radius = 5.0

# Mutable binding
var circumference = 0.0
circumference = 2.0 * pi * radius

println("Circumference = ")
println(circumference)

# Implicit return: last expression is the function's return value
circumference
```

**Output:** `31.4159265`

**Key points:**
- `let` creates an immutable binding (cannot be reassigned).
- `var` creates a mutable binding (can be reassigned with `=`).
- All numeric literals are `Double` by default.
- The last expression in a block is the implicit return value.

---

## Example 2: Functions and Composition

Functions are first-class citizens. They can be passed as arguments, returned from other functions, and composed together.

```flux
def double_it(x: Double) -> Double {
    x * 2.0
}

def add_one(x: Double) -> Double {
    x + 1.0
}

def apply_twice(f, x) -> Double {
    f(f(x))
}

def compose(f, g, x) -> Double {
    f(g(x))
}

def main() -> Double {
    # Direct calls
    println(double_it(5.0))       # 10.0

    # Higher-order: apply_twice applies a function twice
    let r1 = apply_twice(double_it, 3.0)
    println(r1)                    # 12.0

    # Compose: compose(f, g, x) = f(g(x))
    let r2 = compose(double_it, add_one, 5.0)
    println(r2)                    # 12.0  (double_it(add_one(5.0)) = double_it(6.0))

    let r3 = compose(add_one, double_it, 3.0)
    println(r3)                    # 7.0   (add_one(double_it(3.0)) = add_one(6.0))

    1.0
}
main()
```

**Key points:**
- Functions accept typed parameters (`x: Double`) and declare return types (`-> Double`).
- Functions can be passed as arguments without explicit type annotations.
- `compose(f, g, x)` builds a pipeline: `f(g(x))`.

---

## Example 3: Control Flow — While, If, and Break

```flux
def main() -> Double {
    # While loop: sum 0..9
    var i = 0.0
    var s = 0.0
    while i < 10.0 do {
        s = s + i
        i = i + 1.0
    }
    println("Sum 0..9 = ")
    println(s)                      # 45.0

    # While with break: sum until threshold
    var total = 0.0
    var j = 0.0
    while j < 100.0 do {
        total = total + j
        j = j + 1.0
        if total > 30.0 then break
    }
    println("Break at j = ")
    println(j)                      # 9.0 (breaks when total first exceeds 30)

    # If/else expression
    let x = 42.0
    let label = if x > 0.0 then "positive" else "non-positive"
    println(label)

    1.0
}
main()
```

**Key points:**
- `while condition do { ... }` loops with optional `break`.
- `if condition then expr else expr` is an expression (returns a value).
- `var` is required for mutable loop variables.

---

## Example 4: For Loops and Foreach

FluxScript supports `for` ranges and `foreach` over vectors.

```flux
# Range-based for
for i in 0, 5 do println(i)
# prints: 0, 1, 2, 3, 4

# Foreach over a vector literal
def test() foreach x in [1.0, 2.0, 3.0] do println(x)

test()
```

**Key points:**
- `for i in start, end do expr` iterates from `start` to `end - 1`.
- `foreach x in vector do expr` iterates over each element.
- Both are expression forms — they produce values.

---

## Example 5: Structs, Methods, and Mutation

Structs define typed data bundles with methods.

```flux
struct Point { x: Double, y: Double }

impl Point {
    def distance_from_origin(self: Point) -> Double {
        sqrt(self.x * self.x + self.y * self.y)
    }
}

def main() -> Double {
    # Create a struct
    let p = Point { x: 3.0, y: 4.0 }
    println(p.x)                   # 3.0
    println(p.y)                   # 4.0
    println(p.distance_from_origin())  # 5.0

    # Mutable struct: field mutation
    var q = Point { x: 1.0, y: 2.0 }
    q.x = 42.0
    println(q.x)                   # 42.0

    # Struct returned from a function
    let v = make_vec3(1.0, 2.0, 3.0)
    println(v.x)                   # 1.0
    println(v.y)                   # 2.0
    println(v.z)                   # 3.0

    1.0
}

struct Vec3 { x: Double, y: Double, z: Double }
def make_vec3(x: Double, y: Double, z: Double) -> Vec3 {
    Vec3 { x: x, y: y, z: z }
}

main()
```

**Key points:**
- `struct Name { field: Type }` defines a data type.
- `impl Name { def method(self) ... }` attaches methods.
- `var s = ...` enables field mutation with `s.field = value`.
- Functions can return structs via implicit return (last expression).

---

## Example 6: Enums and Pattern Matching

Enums represent variants. Pattern matching dispatches on variants.

```flux
enum Color { Red, Green, Blue }

enum Option {
    Some { value: Double },
    None
}

def main() -> Double {
    # Simple enum matching
    let c = Color.Green
    let val = match c {
        Color.Red -> 1.0,
        Color.Green -> 2.0,
        Color.Blue -> 3.0
    }
    println(val)                   # 2.0

    # Payload enum matching
    let opt = Option.Some { value: 42.0 }
    let result = match opt {
        Option.Some(v) -> v * 2.0,
        Option.None -> 0.0
    }
    println(result)                # 84.0

    # Braced constructor syntax
    let person = Option.Some { value: 25.0 }
    match person {
        Option.Some(payload) -> println(payload.value),
        Option.None -> println("empty")
    }

    1.0
}
main()
```

**Key points:**
- `enum Name { Variant1, Variant2 { field: Type } }` defines variants.
- `match expr { Pattern -> body, ... }` dispatches on variants.
- Payload variables in patterns bind the payload value.
- Braced constructor: `Option.Some { value: 42.0 }`.

---

## Example 7: Switch/Case Expressions

Switch provides expression-based value dispatch on numeric/constant values.

```flux
# Basic switch with => syntax
let a = 2.0
let result = switch (a) {
    1.0 => 10.0,
    2.0 => 20.0,
    ~ => 0.0
}
println(result)                    # 20.0

# Switch with block bodies
let b = 3.0
let r2 = switch (b) {
    1.0 => {
        100.0
    }
    ~ => {
        b + 1.0
    }
}
println(r2)                        # 4.0

# Switch in an expression context
let c = 2.0
let r3 = 1.0 + switch (c) { 1.0 => 10.0, ~ => 0.0 }
println(r3)                        # 1.0 (c=2.0 -> default 0.0, 1.0+0.0)

# Switch with code after (mixed return/non-return)
let d = 5.0
let r4 = switch (d) {
    1.0 => { return 99.0 }
    ~ => { 0.0 }
}
r4 * 2.0                           # 0.0 (default matched, then 0.0 * 2.0)
```

**Key points:**
- `switch (expr) { value => result, ~ => default }` — `~` is the wildcard.
- Block bodies `{ ... }` allow multi-expression branches.
- Code after a switch executes after the expression completes.
- Returning branches exit the function early.

---

## Example 8: Pipes and Functional Style

The pipe operator `|>` passes the left value as the first argument to the right function.

```flux
def double_it(x: Double) -> Double { x * 2.0 }
def add_one(x: Double) -> Double { x + 1.0 }
def scale(s: Double, x: Double) -> Double { s * x }

struct Box { val: Double }
impl Box {
    def apply(self: Box, f: Double) -> Double { self.val * f }
}

def main() -> Double {
    # Basic pipe: 3.0 |> double_it |> add_one
    # equivalent to add_one(double_it(3.0)) = add_one(6.0) = 7.0
    let r1 = 3.0 |> double_it |> add_one
    println(r1)                    # 7.0

    # Pipe with extra arguments
    let mul = 2.5
    let r2 = 10.0 |> scale(mul)
    println(r2)                    # 25.0  (scale(2.5, 10.0))

    # Pipe into struct method
    let b: Box = Box { val: 3.0 }
    let r3 = 5.0 |> b.apply
    println(r3)                    # 15.0  (b.apply(5.0) = 3.0 * 5.0)

    1.0
}
main()
```

**Key points:**
- `value |> func` is syntactic sugar for `func(value)`.
- `value |> func(arg)` is `func(value, arg)` — extra args go to the right.
- `value |> obj.method` calls the method on the object.

---

## Example 9: Traits and Dynamic Dispatch

Traits define interfaces. Types implement traits via `impl Trait for Type`.

```flux
trait Doubler {
    def double_it(self) -> Double
}

# Implement trait for built-in Double
impl Doubler for Double {
    def double_it(self) -> Double { self * 2.0 }
}

trait Math {
    def add(a: Double, b: Double) -> Double
    def mul(a: Double, b: Double) -> Double
}

struct Calc { }
impl Math for Calc {
    def add(a: Double, b: Double) -> Double { a + b }
    def mul(a: Double, b: Double) -> Double { a * b }
}

def test(x: Double) -> Double {
    let d: dyn Doubler = x
    d.double_it()
}

def main() -> Double {
    let result = test(21.0)
    println(result)                # 42.0

    1.0
}
main()
```

**Key points:**
- `trait Name { def method(self) -> Type; }` defines an interface.
- `impl Trait for Type { ... }` implements the interface.
- `let d: dyn Trait = value` creates a dynamic dispatch handle.
- Traits can have multiple methods.

---

## Example 10: Generics

Generic functions and structs work with any type via monomorphization.

```flux
# Generic identity function
def id[T](x: T) -> T { x }

# Generic struct
struct Pair[T] { first: Double, second: Double }

# Generic class
class Box[T] {
    content: Double
    def get(self: Box) -> Double {
        self.content
    }
}

def main() -> Double {
    # Generic function call
    assert(id[Double](42.0) == 42.0, "generic id double wrong")

    let s = id[Double](3.14)
    assert(abs(s - 3.14) < 0.001, "generic id pi wrong")

    # Generic struct
    let p: Pair = Pair { first: 1.0, second: 2.0 }
    assert(p.first == 1.0, "pair first wrong")

    1.0
}
main()
```

**Key points:**
- `def name[T](x: T) -> T` defines a generic function.
- `id[Double](42.0)` explicitly specializes the type parameter.
- `struct Pair[T]` defines a generic struct.
- `class Box[T]` defines a generic class with methods.

---

## Example 11: Vectors and Indexing

Vectors are first-class array values with literal syntax and indexing.

```flux
def main() -> Double {
    # Vector literal
    let v = [10.0, 20.0, 30.0]

    # Indexing (0-based)
    assert(v[0] == 10.0, "index 0 wrong")
    assert(v[1] == 20.0, "index 1 wrong")
    assert(v[2] == 30.0, "index 2 wrong")

    # Inline literal + index
    assert([1.0, 2.0, 3.0][1] == 2.0, "inline index wrong")

    # Expression in index
    assert([1.0, 2.0, 3.0][2] * 2.0 == 6.0, "index expr wrong")

    # Vector equality
    assert([10.0, 20.0, 30.0][0] == 10.0, "vector element eq wrong")

    1.0
}
main()
```

**Key points:**
- `[1.0, 2.0, 3.0]` creates a vector literal.
- `v[i]` accesses element at index `i` (0-based).
- Index expressions can contain arbitrary expressions.
- Vectors support `==` / `!=` comparison.

---

## Example 12: Error Handling with Try/Catch

FluxScript supports exception-based error handling.

```flux
def risky_operation() -> Double {
    # Simulate a failing operation
    1.0 / 0.0
}

def safe_wrapper() -> Double {
    try {
        risky_operation()
    } catch e {
        0.0
    }
}

def main() -> Double {
    let result = safe_wrapper()
    println(result)                # 0.0 (caught the error)

    1.0
}
main()
```

**Key points:**
- `try { ... } catch e { ... }` catches runtime errors.
- The caught value `e` contains the error information.
- Without try/catch, errors propagate up the call stack.

---

## Example 13: Let Bindings

`let` creates local scoped bindings.

```flux
def main() -> Double {
    let x = 42.0
    let y = x * 2.0
    println(y)                     # 84.0

    # Let in expression form
    let result = let a = 3.0 in a * a
    println(result)                # 9.0

    1.0
}
main()
```

**Key points:**
- `let name = expr in body` scopes a binding within an expression.
- `let name = expr` at block level creates a local binding for the rest of the block.

---

## Example 14: Classes with Methods

Classes combine data and behavior, desugared to structs + impls at compile time.

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
}

class Box[T] {
    content: Double
    def get(self: Box) -> Double {
        self.content
    }
}

def main() -> Double {
    var c = Counter { value: 0.0 }
    c.increment()                  # 1.0
    c.increment()                  # 2.0
    println(c.get())               # 2.0

    1.0
}
main()
```

**Key points:**
- `class Name { field: Type; def method(self) ... }` bundles data + methods.
- Classes support `self` for field access and mutation.
- Generic classes use `[T]` syntax.

---

## Circuit Simulation Examples

The following examples use the built-in `circuit`, `mna`, `dcsolve`, and `trsim` modules for analog circuit simulation.

---

## Example 15: Voltage Divider (DC Operating Point)

The simplest circuit: two resistors forming a voltage divider.

```flux
import circuit
import mna

def main() -> var {
    # Create circuit: 2 nodes, 500 max components
    var ctrl = circuit_create(2, 500)

    # Add components
    circuit_add_resistor(ctrl, 1, 2, 1000.0)    # R1 = 1kΩ between nodes 1 and 2
    circuit_add_resistor(ctrl, 2, 0, 1000.0)    # R2 = 1kΩ between nodes 2 and ground
    circuit_add_vdc(ctrl, 1, 0, 5.0)            # 5V source between node 1 and ground

    # Solve for DC operating point
    var sol = mna_dc_solve(ctrl)

    # Read node voltages
    println("V(1) = ")
    println(mna_get_node_voltage(sol, 1))       # 5.0V (source node)
    println("V(2) = ")
    println(mna_get_node_voltage(sol, 2))       # 2.5V (divider midpoint)

    sol
}
main()
```

**Circuit diagram:**
```
    Vdc = 5V
  ┌────┤+├────┐
  │           │
  R1=1kΩ    R2=1kΩ
  │           │
Node 1     Node 2
  │           │
  └────GND───┘
```

**Expected output:** V(2) = 2.5V (equal resistors, equal voltage split).

---

## Example 16: RC Low-Pass Filter (DC Steady State)

A resistor-capacitor circuit. In DC steady state, the capacitor acts as an open circuit.

```flux
import circuit
import mna

def main() -> var {
    var ctrl = circuit_create(2, 500)

    circuit_add_resistor(ctrl, 1, 2, 1000.0)    # R = 1kΩ
    circuit_add_capacitor(ctrl, 2, 0, 1e-6, 0.0) # C = 1μF, initial voltage = 0
    circuit_add_vdc(ctrl, 1, 0, 5.0)             # 5V source

    var sol = mna_dc_solve(ctrl)

    println("V(1) = ")
    println(mna_get_node_voltage(sol, 1))        # 5.0V
    println("V(2) = ")
    println(mna_get_node_voltage(sol, 2))        # 5.0V (DC: cap is open)

    sol
}
main()
```

**Expected output:** V(2) = 5.0V (no DC current through capacitor).

---

## Example 17: Series RLC Circuit (DC)

An RLC series circuit under DC conditions.

```flux
import circuit
import mna

def main() -> var {
    var ctrl = circuit_create(3, 500)

    circuit_add_resistor(ctrl, 1, 2, 1000.0)    # R = 1kΩ
    circuit_add_inductor(ctrl, 2, 3, 1e-3, 0.0) # L = 1mH, initial current = 0
    circuit_add_capacitor(ctrl, 3, 0, 1e-6, 0.0) # C = 1μF
    circuit_add_vdc(ctrl, 1, 0, 5.0)             # 5V source

    var sol = mna_dc_solve(ctrl)

    println("V(1) = ")
    println(mna_get_node_voltage(sol, 1))        # 5.0V
    println("V(2) = ")
    println(mna_get_node_voltage(sol, 2))        # 5.0V (DC: inductor is short)
    println("V(3) = ")
    println(mna_get_node_voltage(sol, 3))        # 5.0V (DC: cap is open)

    sol
}
main()
```

**Expected output:** All nodes at 5.0V (DC: inductor shorted, capacitor open).

---

## Example 18: Wheatstone Bridge

A bridge circuit with four resistors and a load resistor.

```flux
import circuit
import mna

def main() -> var {
    var ctrl = circuit_create(3, 500)

    # Bridge arms
    circuit_add_resistor(ctrl, 1, 2, 1000.0)    # R1 = 1kΩ
    circuit_add_resistor(ctrl, 2, 0, 2000.0)    # R2 = 2kΩ
    circuit_add_resistor(ctrl, 1, 3, 2000.0)    # R3 = 2kΩ
    circuit_add_resistor(ctrl, 3, 0, 1000.0)    # R4 = 1kΩ
    # Load between midpoints
    circuit_add_resistor(ctrl, 2, 3, 5000.0)    # R5 = 5kΩ (load)
    # Source
    circuit_add_vdc(ctrl, 1, 0, 10.0)           # 10V source

    var sol = mna_dc_solve(ctrl)

    println("V(1) = ")
    println(mna_get_node_voltage(sol, 1))        # 10.0V
    println("V(2) = ")
    println(mna_get_node_voltage(sol, 2))
    println("V(3) = ")
    println(mna_get_node_voltage(sol, 3))
    println("Vdiff = ")
    println(mna_get_node_voltage(sol, 2) - mna_get_node_voltage(sol, 3))

    sol
}
main()
```

---

## Example 19: Op-Amp Inverting Amplifier

An inverting amplifier using a VCVS (voltage-controlled voltage source) to model an ideal op-amp.

```flux
import circuit
import mna

def main() -> var {
    var ctrl = circuit_create(3, 500)

    circuit_add_resistor(ctrl, 1, 2, 1000.0)    # R1 = 1kΩ (input)
    circuit_add_resistor(ctrl, 2, 3, 10000.0)   # Rf = 10kΩ (feedback)
    circuit_add_vcvs(ctrl, 3, 0, 2, 0, 1e6)     # Op-amp: Vout = 1e6 * V(2)
    circuit_add_vdc(ctrl, 1, 0, 1.0)            # Vin = 1V

    var sol = mna_dc_solve(ctrl)

    println("V(3) = ")
    println(mna_get_node_voltage(sol, 3))        # ≈ -10V (gain = -Rf/R1)

    sol
}
main()
```

**Expected output:** V(3) ≈ -10.0V (inverting gain = -Rf/R1 = -10k/1k = -10).

---

## Example 20: Diode Bias Point (Nonlinear)

A diode circuit solved with Newton-Raphson iteration.

```flux
import circuit
import dcsolve

def main() -> var {
    var ctrl = circuit_create(2, 10)

    circuit_add_vdc(ctrl, 1, 0, 5.0)            # 5V source
    circuit_add_resistor(ctrl, 1, 2, 1000.0)    # R = 1kΩ
    circuit_add_diode(ctrl, 2, 0, 1e-14, 1.0, 0.02585)  # Diode: Is=1e-14, N=1, Vt=25.85mV

    # Newton-Raphson solver with gmin stepping
    var Vd = dc_solve(ctrl.get_comps(), ctrl.get_ctrl(), 30.0, 1e-9)

    println("Vd = ")
    println(matrix_get(Vd, 2, 0))               # ≈ 0.65-0.7V (silicon diode drop)
    println("Vs = ")
    println(matrix_get(Vd, 1, 0))               # 5.0V

    Vd
}
main()
```

**Expected output:** Vd ≈ 0.65–0.7V (forward-biased silicon diode voltage drop).

---

## Example 21: Transient Simulation — Diode Half-Wave Rectifier

A time-domain simulation of a diode rectifier with an RC filter.

```flux
import circuit
import trsim

def main() -> var {
    var ctrl = circuit_create(3, 20)

    # AC source: 5V amplitude, 1kHz sine
    circuit_add_vsin(ctrl, 1, 0, 0.0, 5.0, 1000.0, 0.0)
    # Diode
    circuit_add_diode(ctrl, 1, 2, 1e-14, 1.0, 0.02585)
    # Load resistor
    circuit_add_resistor(ctrl, 2, 3, 1000.0)
    # Filter capacitor
    circuit_add_capacitor(ctrl, 3, 0, 10e-6, 0.0)

    # Transient solve: t_start=0, t_end=3ms, dt=10μs
    var r = tr_solve(ctrl.get_comps(), ctrl.get_ctrl(), 0.0, 0.003, 1e-5)

    var nr = floor(0.003 / 1e-5) + 1.0
    println("Points: ")
    println(nr)

    # Check capacitor voltage at end of simulation
    var vcap_end = matrix_get(r, nr - 3.0, 3)
    println("Vcap at end: ")
    println(vcap_end)

    if (vcap_end > 0.0 && vcap_end < 5.0) {
        println("PASS: diode charges capacitor")
    } else {
        println("FAIL: unexpected capacitor voltage")
    }

    r
}
main()
```

**What it does:** Simulates 3ms of a 1kHz sine wave through a diode. The diode half-wave rectifies the signal, and the RC filter smooths the output. The capacitor voltage should settle between 0 and 5V.

---

## Example 22: NMOS Common-Source Amplifier (DC)

An NMOS transistor biasing example with iterative Newton-Raphson solving.

```flux
import circuit
import dcsolve

println("=== NMOS Common-Source ===")

var ctrl = circuit_create(3, 10)
circuit_add_vdc(ctrl, 1.0, 0.0, 5.0)             # Vdd = 5V
circuit_add_vdc(ctrl, 2.0, 0.0, 3.3)             # Vgs = 3.3V (gate bias)
circuit_add_resistor(ctrl, 1.0, 3.0, 1000.0)     # Rd = 1kΩ
circuit_add_nmos(ctrl, 3.0, 2.0, 0.0, 0.0, 1e-4, 1.0, 0.01)
# NMOS: drain=3, gate=2, source=0, bulk=0, Kp=1e-4, Vto=1.0, lambda=0.01

var Vm = dc_solve(ctrl.get_comps(), ctrl.get_ctrl(), 30.0, 1e-9)

print("Vgs = ")
print(matrix_get(Vm, 2, 0))
println(" V  (expected 3.3)")

print("Vd = ")
print(matrix_get(Vm, 3, 0))
println(" V  (expected ~4.72)")

var info = dc_mos_info(ctrl.get_comps(), ctrl.get_ctrl(), 3.0, Vm)
print("Id = ")
print(matrix_get(info, 2, 0))
println(" A")

print("Region = ")
print(matrix_get(info, 3, 0))
println(" (2=sat)")

println("=== Done ===")
```

**What it does:** Simulates an NMOS in saturation region. Vgs=3.3V exceeds Vto=1.0V, so the transistor is on. The drain voltage depends on the drain current through Rd.

---

## Example 23: Transient — BJT and MOSFET Switching

Time-domain simulation of BJT and MOSFET switching behavior.

```flux
import circuit
import trsim

def main() -> var {
    # === BJT Common-Emitter Switch ===
    var ctrl = circuit_create(4, 20)
    circuit_add_vdc(ctrl, 1, 0, 5.0)              # Vcc
    circuit_add_resistor(ctrl, 1, 3, 1000.0)      # Rc
    circuit_add_npn(ctrl, 3, 2, 0, 100.0, 1e-14, 50.0, 0.02585)
    # NPN: collector=3, base=2, emitter=0, Bf=100, Is=1e-14, Vaf=50, Vt=25.85mV
    circuit_add_resistor(ctrl, 4, 2, 10000.0)     # Rb
    circuit_add_vpulse(ctrl, 4, 0, 0.001, 0.0, 3.0, 0.0, 1e-5, 1e-5, 0.0005)
    # Vpulse: V1=1mV, V2=3V, delay=0, rise=10μs, fall=10μs, width=0.5ms

    var r2 = tr_solve(ctrl.get_comps(), ctrl.get_ctrl(), 0.0, 0.002, 1e-5)
    var nr2 = floor(0.002 / 1e-5) + 1.0
    println("BJT Points: ")
    println(nr2)

    # Check if BJT switches on (Vce < 1V at t≈0.5ms)
    var si = 0.0
    var found_switch = 0.0
    while (si < nr2) {
        var tt = matrix_get(r2, si, 0)
        var vc = matrix_get(r2, si, 3)
        if (tt >= 0.0004 && tt <= 0.0006) {
            if (vc - 0.0 < 1.0) { found_switch = 1.0 }
        }
        si = si + 1.0
    }
    if (found_switch > 0.0) {
        println("PASS: BJT switches on")
    } else {
        println("FAIL: BJT did not switch")
    }

    r2
}
main()
```

**What it does:** Applies a pulse to the BJT base through Rb. When the pulse is high (3V), the BJT turns on and pulls the collector low. The simulation verifies the switching behavior in the time domain.

---

## Example 24: Functional Composition with Closures

Higher-order functions enable powerful abstractions.

```flux
def apply_twice(f, x) -> Double {
    f(f(x))
}

def compose(f, g, x) -> Double {
    f(g(x))
}

def double_it(x) -> Double { x * 2.0 }
def add_one(x) -> Double { x + 1.0 }
def square(x) -> Double { x * x }

def main() -> Double {
    # Apply a function twice
    let r1 = apply_twice(double_it, 3.0)
    assert(r1 == 12.0, "apply_twice double wrong")   # double(double(3)) = 12

    let r2 = apply_twice(add_one, 5.0)
    assert(r2 == 7.0, "apply_twice add_one wrong")   # add_one(add_one(5)) = 7

    # Compose two functions
    let r3 = compose(double_it, add_one, 5.0)
    assert(r3 == 12.0, "compose wrong")              # double_it(add_one(5)) = 12

    let r4 = compose(add_one, double_it, 3.0)
    assert(r4 == 7.0, "compose rev wrong")           # add_one(double_it(3)) = 7

    # Compose with square
    let r5 = compose(square, add_one, 2.0)
    assert(r5 == 9.0, "compose square+add wrong")    # square(add_one(2)) = 9

    let r6 = apply_twice(square, 3.0)
    assert(r6 == 81.0, "apply_twice square wrong")   # square(square(3)) = 81

    1.0
}
main()
```

**Key points:**
- Functions without explicit type annotations accept any type.
- `apply_twice(f, x)` applies `f` to `x` twice: `f(f(x))`.
- `compose(f, g, x)` builds a pipeline: `f(g(x))`.
- These patterns enable functional programming in FluxScript.

---

## Example 25: SPICE Netlist Parsing

FluxScript can parse standard SPICE netlist files and solve them.

```flux
import circuit
import mna
import netlist

def main() -> var {
    var max_comps = 500.0
    var max_nodes = 50.0

    println("=== SPICE Netlist Parser ===")

    # Voltage divider from netlist file
    var ctrl1 = circuit_create(max_nodes, max_comps)
    var N1 = netlist_parse("circuits/voltage_divider.sp", ctrl1.get_comps(), ctrl1.get_ctrl())
    print("N = "); print(N1); println(" (expected 2)")
    var sol1 = mna_dc_solve(ctrl1)
    print("V(1) = "); print(mna_get_node_voltage(sol1, 1)); println(" V (expected 5)")
    print("V(2) = "); print(mna_get_node_voltage(sol1, 2)); println(" V (expected 3.333)")

    # RC low-pass from netlist
    var ctrl2 = circuit_create(max_nodes, max_comps)
    var N2 = netlist_parse("circuits/rc_lowpass.sp", ctrl2.get_comps(), ctrl2.get_ctrl())
    var sol2 = mna_dc_solve(ctrl2)
    print("V(2) = "); print(mna_get_node_voltage(sol2, 2)); println(" V (expected 5)")

    # Series RLC from netlist
    var ctrl3 = circuit_create(max_nodes, max_comps)
    var N3 = netlist_parse("circuits/rlc_series.sp", ctrl3.get_comps(), ctrl3.get_ctrl())
    var sol3 = mna_dc_solve(ctrl3)
    print("V(2) = "); print(mna_get_node_voltage(sol3, 2)); println(" V (expected 5)")

    println("=== All netlist tests complete ===")

    sol1
}
main()
```

**Key points:**
- `netlist_parse(filename, comps, ctrl)` reads a SPICE `.sp` file.
- Supported components: R, L, C, Vdc, Idc, D, Qnpn, Qpnp, Mnmos, Mpmos, E (VCVS), G (VCCS).
- `.model` cards are parsed for device parameters.

---

## Example 26: DC Sweep Analysis

Sweep a source value across a range and record the response.

```flux
import circuit
import dcsolve

def main() -> var {
    # Build a voltage divider
    var c = circuit_create(2.0, 500.0)
    circuit_add_resistor(c, 1.0, 2.0, 1000.0)
    circuit_add_resistor(c, 2.0, 0.0, 1000.0)
    circuit_add_vdc(c, 1.0, 0.0, 0.0)

    # Sweep V1 from 0V to 10V in 1V steps
    var sweep = dc_sweep(c.get_comps(), c.get_ctrl(), 2.0, 0.0, 10.0, 10.0, 100.0, 1e-9)

    println("Vin  Vout")
    var i = 0.0
    while (i <= 10.0) {
        print(matrix_get(sweep, i, 0.0))
        print("  ")
        println(matrix_get(sweep, i, 3.0))
        i = i + 1.0
    }

    sweep
}
main()
```

**What it does:** Sweeps the input voltage from 0 to 10V in 1V steps, recording V(2) at each point. The output should show Vout = Vin/2 (linear divider).

---

## Example 27: Matrix Operations

FluxScript includes built-in matrix support for linear algebra.

```flux
def main() -> Double {
    # Create a 3x3 identity matrix
    let A = matrix_zeros(3, 3)
    matrix_set(A, 0, 0, 1.0)
    matrix_set(A, 1, 1, 1.0)
    matrix_set(A, 2, 2, 1.0)

    # Determinant of identity = 1.0
    let det_val = matrix_det(A)
    println(det_val)

    # Solve Ax = b
    let b = matrix_zeros(3, 1)
    matrix_set(b, 0, 0, 1.0)
    matrix_set(b, 1, 0, 2.0)
    matrix_set(b, 2, 0, 3.0)

    let x = matrix_solve(A, b)
    println(matrix_get(x, 0, 0))   # 1.0
    println(matrix_get(x, 1, 0))   # 2.0
    println(matrix_get(x, 2, 0))   # 3.0

    1.0
}
main()
```

---

## Example 28: MNA Stamping (Building the Conductance Matrix)

The Modified Nodal Analysis (MNA) stamp functions build the system matrix for circuit simulation.

```flux
import circuit
import mna

def main() -> var {
    var ctrl = circuit_create(2, 500)
    circuit_add_resistor(ctrl, 1, 2, 1000.0)
    circuit_add_resistor(ctrl, 2, 0, 1000.0)
    circuit_add_vdc(ctrl, 1, 0, 5.0)

    var comps = ctrl.comps
    var ctrl_data = ctrl.ctrl
    var N = ctrl.num_nodes()
    var M = mna_count_branches(comps, ctrl_data)
    var dim = N + M

    # Build conductance matrix manually
    var A = matrix_zeros(dim, dim)
    var b = matrix_zeros(dim, 1)

    # Stamp resistor: R(1→2) = 1kΩ → G = 1/1000
    mna_stamp_g(A, 1, 2, 0.001, N)
    # Stamp resistor: R(2→0) = 1kΩ → G = 1/1000
    mna_stamp_g(A, 2, 0, 0.001, N)

    # Print the A matrix
    var i = 0.0
    while (i < dim) {
        var j = 0.0
        while (j < dim) {
            print(matrix_get(A, i, j))
            print(" ")
            j = j + 1.0
        }
        println("")
        i = i + 1.0
    }

    ctrl
}
main()
```

**What it does:** Manually stamps resistor conductances into the MNA matrix. `mna_stamp_g(A, n+, n-, G, N)` adds conductance G between nodes n+ and n- into the N×N conductance submatrix.

---

## Example 29: ~Copy Annotation

The `~Copy` annotation prevents implicit value copying for structs.

```flux
struct NoCopy ~Copy {
    val: Double
}

def main() -> Double {
    let a = NoCopy { val: 42.0 }
    a.val                          # 42.0 — access works fine
}
main()
```

**Key points:**
- `~Copy` marks a struct as non-copyable (prevents implicit duplication).
- Useful for types that own resources or should be moved, not copied.
- Regular structs (without `~Copy`) are copyable by default.

---

## Summary

| Feature | Example |
|---------|---------|
| Variables & arithmetic | Example 1 |
| Functions & composition | Example 2, 24 |
| Control flow (while, if, break) | Example 3 |
| For loops & foreach | Example 4 |
| Structs & methods | Example 5 |
| Enums & pattern matching | Example 6 |
| Switch/case | Example 7 |
| Pipes | Example 8 |
| Traits & dynamic dispatch | Example 9 |
| Generics | Example 10 |
| Vectors & indexing | Example 11 |
| Error handling | Example 12 |
| Let bindings | Example 13 |
| Classes | Example 14 |
| Voltage divider | Example 15 |
| RC filter | Example 16 |
| RLC circuit | Example 17 |
| Wheatstone bridge | Example 18 |
| Op-amp amplifier | Example 19 |
| Diode bias | Example 20 |
| Transient rectifier | Example 21 |
| NMOS amplifier | Example 22 |
| BJT/MOSFET switching | Example 23 |
| Higher-order functions | Example 24 |
| SPICE netlist | Example 25 |
| DC sweep | Example 26 |
| Matrix operations | Example 27 |
| MNA stamping | Example 28 |
| ~Copy annotation | Example 29 |
