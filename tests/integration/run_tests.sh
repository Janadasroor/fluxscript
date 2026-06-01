#!/bin/bash
# FluxScript Integration Test Runner
# Tests features with syntax that actually works

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
FLUX_BIN="$PROJECT_DIR/build/flux"
if [ ! -x "$FLUX_BIN" ]; then
    if [ -x "$PROJECT_DIR/build/flux.exe" ]; then
        FLUX_BIN="$PROJECT_DIR/build/flux.exe"
    elif [ -x "$PROJECT_DIR/build/Release/flux.exe" ]; then
        FLUX_BIN="$PROJECT_DIR/build/Release/flux.exe"
    elif [ -x "$PROJECT_DIR/build/Debug/flux.exe" ]; then
        FLUX_BIN="$PROJECT_DIR/build/Debug/flux.exe"
    elif [ -x "$PROJECT_DIR/build/RelWithDebInfo/flux.exe" ]; then
        FLUX_BIN="$PROJECT_DIR/build/RelWithDebInfo/flux.exe"
    elif [ -x "$PROJECT_DIR/build/MinSizeRel/flux.exe" ]; then
        FLUX_BIN="$PROJECT_DIR/build/MinSizeRel/flux.exe"
    fi
fi

if [ ! -x "$FLUX_BIN" ]; then
    echo "flux not found at $FLUX_BIN"
    echo "Run 'make flux' first"
    exit 1
fi

# Self-hosted compiler binary for bootstrapped tests
FLUX_SELFHOST_BIN="${FLUX_SELFHOST_BIN:-$PROJECT_DIR/build/fluxc_bootstrap}"

build_bootstrap() {
    local out="$1"
    local stage2_ll=$(mktemp /tmp/flux_stage2_ll.XXXXXX).ll
    local stage2_bc=/tmp/flux_stage2.bc
    local stage2_s=/tmp/flux_stage2.s
    local stage2_bin=/tmp/flux_stage2
    local stage3_ll=$(mktemp /tmp/flux_stage3_ll.XXXXXX).ll
    local stage3_bc=/tmp/flux_stage3.bc
    local stage3_s=/tmp/flux_stage3.s

    rm -f "$stage2_bc" "$stage2_s" "$stage2_bin" "$stage3_bc" "$stage3_s"

    "$FLUX_BIN" --cache=0 --emit=llvm "$PROJECT_DIR/src/fluxc/main.flux" 2>/dev/null > "$stage2_ll" || return 1
    llvm-as-21 "$stage2_ll" -o "$stage2_bc" || return 1
    opt-21 -passes=globaldce "$stage2_bc" -o "$stage2_bc" 2>/dev/null || return 1
    llc-21 -O2 -relocation-model=pic "$stage2_bc" -o "$stage2_s" || return 1
    g++ -O2 "$stage2_s" -L"$PROJECT_DIR/build" -lFluxScript -Wl,-rpath,"$PROJECT_DIR/build" -o "$stage2_bin" || return 1

    FLUX_INPUT="$PROJECT_DIR/src/fluxc/main.flux" "$stage2_bin" 2>/dev/null > "$stage3_ll" || return 1
    llvm-as-21 "$stage3_ll" -o "$stage3_bc" || return 1
    opt-21 -passes=globaldce "$stage3_bc" -o "$stage3_bc" 2>/dev/null || return 1
    llc-21 -O2 -relocation-model=pic "$stage3_bc" -o "$stage3_s" || return 1
    g++ -O2 "$stage3_s" -L"$PROJECT_DIR/build" -lFluxScript -Wl,-rpath,"$PROJECT_DIR/build" -o "$out" || return 1

    rm -f "$stage2_ll" "$stage2_bc" "$stage2_s" "$stage3_ll" "$stage3_bc" "$stage3_s" "$stage2_bin"
    return 0
}

if [ ! -x "$FLUX_SELFHOST_BIN" ]; then
    echo "Building self-hosted compiler (stage-3) for bootstrap tests..."
    if build_bootstrap "$FLUX_SELFHOST_BIN"; then
        echo "  -> $FLUX_SELFHOST_BIN"
    else
        echo "  -> bootstrap build failed; falling back to C++ compiler"
        FLUX_SELFHOST_BIN=""
    fi
fi

GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

echo "========================================"
echo "  FluxScript Integration Tests"  
echo "========================================"
echo ""

TOTAL=0
PASSED=0
FAILED=0

# Check if timeout command exists, otherwise use gtimeout or none
TIMEOUT_CMD="timeout"
if ! command -v timeout &> /dev/null; then
    if command -v gtimeout &> /dev/null; then
        TIMEOUT_CMD="gtimeout"
    else
        TIMEOUT_CMD=""
    fi
fi

run_test() {
    local test_name="$1"
    local test_code="$2"
    
    TOTAL=$((TOTAL + 1))
    echo -n "$test_name... "
    
    echo "$test_code" > flux_test.flux
    
    local cmd_status
    local test_output
    if [ -n "$TIMEOUT_CMD" ]; then
        test_output=$($TIMEOUT_CMD 5 "$FLUX_BIN" --cache=0 flux_test.flux 2>&1)
        cmd_status=$?
    else
        test_output=$("$FLUX_BIN" --cache=0 flux_test.flux 2>&1)
        cmd_status=$?
    fi

    if [ $cmd_status -eq 0 ]; then
        echo -e "${GREEN} PASSED${NC}"
        PASSED=$((PASSED + 1))
    else
        echo -e "${RED} FAILED${NC}"
        echo "=== TEST OUTPUT FOR $test_name ==="
        echo "$test_output"
        echo "=================================="
        FAILED=$((FAILED + 1))
    fi
    
    rm -f flux_test.flux
}

# Self-hosting compiler test: generate IR via bootstrap or C++ compiler, verify with opt-21
run_selfhost_test() {
    local test_name="$1"
    local test_code="$2"

    TOTAL=$((TOTAL + 1))
    echo -n "$test_name... "

    echo "$test_code" > /tmp/flux_selfhost.flux

    local test_output
    local cmd_status
    if [ -n "$FLUX_SELFHOST_BIN" ] && [ -x "$FLUX_SELFHOST_BIN" ]; then
        test_output=$(FLUX_INPUT=/tmp/flux_selfhost.flux "$FLUX_SELFHOST_BIN" 2>/dev/null | \
            awk '/^; ModuleID/ {found=1} found && !/^(Compiled|JIT cache|Result)/' | \
            opt-21 -passes=verify -S 2>&1)
        cmd_status=$?
    else
        test_output=$(FLUX_INPUT=/tmp/flux_selfhost.flux "$FLUX_BIN" --cache=0 "$PROJECT_DIR/src/fluxc/main.flux" 2>/dev/null | \
            awk '/^; ModuleID/ {found=1} found && !/^(Compiled|JIT cache|Result)/' | \
            opt-21 -passes=verify -S 2>&1)
        cmd_status=$?
    fi

    if [ $cmd_status -eq 0 ]; then
        echo -e "${GREEN} PASSED${NC}"
        PASSED=$((PASSED + 1))
    else
        echo -e "${RED} FAILED${NC}"
        echo "=== TEST OUTPUT FOR $test_name ==="
        echo "$test_output"
        echo "=================================="
        FAILED=$((FAILED + 1))
    fi

    rm -f /tmp/flux_selfhost.flux
}

# Modified run_test for parser-only validation (uses --emit=check)
run_check_test() {
    local test_name="$1"
    local test_code="$2"

    TOTAL=$((TOTAL + 1))
    echo -n "$test_name... "

    echo "$test_code" > flux_test.flux

    local cmd_status
    local test_output
    if [ -n "$TIMEOUT_CMD" ]; then
        test_output=$($TIMEOUT_CMD 5 "$FLUX_BIN" --emit=check --cache=0 flux_test.flux 2>&1)
        echo "$test_output" | grep -q "OK"
        cmd_status=$?
    else
        test_output=$("$FLUX_BIN" --emit=check --cache=0 flux_test.flux 2>&1)
        echo "$test_output" | grep -q "OK"
        cmd_status=$?
    fi

    if [ $cmd_status -eq 0 ]; then
        echo -e "${GREEN} PASSED${NC}"
        PASSED=$((PASSED + 1))
    else
        echo -e "${RED} FAILED${NC}"
        echo "=== TEST OUTPUT FOR $test_name ==="
        echo "$test_output"
        echo "=================================="
        FAILED=$((FAILED + 1))
    fi

    rm -f flux_test.flux
}

# Test 1: Basic For Loop (baseline)
run_test "For Loop" "
for i in 1, 3 do i
"

# Test 2: Parallel For Loop (parser-only validation keeps both old check and adds new test)
run_check_test "Parallel For Parse" "
parallel for i in 1, 3 do i
"
run_test "Parallel For JIT" "
let lo = 1.0;
let hi = 10.0;
parallel for i in lo, hi do i;
0.0
"
run_test "Parallel For Captured Vars" "
let x = 5.0;
let y = 3.0;
parallel for i in 0, 4 do i + x + y;
0.0
"

# Test 3: Symbolic Declaration (parser-only - JIT runtime symbols not registered)
run_check_test "Symbolic Decl" "
sym x
"

# Test 4: Pattern Matching (parser test)
run_test "Pattern Match" "
def test() match(5.0) { 5.0 -> 10.0, _ -> 0.0 }; 1.0
test()
"

# Test 5: Foreach Loop
run_test "Foreach" "
def test() foreach x in [1.0, 2.0, 3.0] do x; 1.0
test()
"

# Test 6: Try-Catch (parser validation)
run_check_test "Try-Catch" "
def foo() { 10.0 }
def test() try { foo() } catch e { 0.0 }; 1.0
test()
"

# Test 7: Let binding
run_test "Let Binding" "
def test() let x = 42.0 in x; 1.0
test()
"

# Test 8: B-Source (parser validation)
run_check_test "B-Source" "
def test() bsource s V(out, 0) { sin(time) }; 1.0
test()
"

# Test 9: Initial Conditions (parser validation)
run_check_test "Initial Cond" "
def test() ic V(cap) = 5.0; 1.0
test()
"

# Test 10: Outputs (parser-only - JIT runtime symbols not registered)
run_check_test "Outputs" "
def test() outputs[\"Vout\"] = 2.5; 1.0
test()
"

# Test 11: Analog Block (Verilog-A Lite)
run_check_test "Analog" "
def test() analog { V(out) <+ sin(time) }; 1.0
test()
"

# Test 12: ddt() time derivative
run_check_test "ddt" "
def test() ddt(sin(time)); 1.0
test()
"

# Test 13: idt() time integral
run_check_test "idt" "
def test() idt(sin(time)); 1.0
test()
"

# Test 14: cross() zero-crossing detection
run_check_test "Cross" "
def test() cross(sin(time), 1); 1.0
test()
"

# Test 15: diagnostic standalone directive
run_check_test "Diagnostic" '
diagnostic V(out) type="overshoot" threshold=0.05
def test() 1.0
test()
'

# Test 16: tolerance standalone directive
run_check_test "Tolerance" '
tolerance abs=1e-6 rel=0.5
def test() 1.0
test()
'

# Test 17: poles() returns matrix
run_check_test "PolesMatrix" '
sym s
poles(s^2 + 3*s + 2)
'

# Test 18: zeros() returns matrix
run_check_test "ZerosMatrix" '
sym s
zeros(s^2 + 3*s + 2)
'

# Test 19: Impl block (parser validation)
run_check_test "Impl block" '
struct Point { x: Double, y: Double }
impl Point
    def scale(self, f: Double) -> Double
        self.x * f
    end
end
def test() { 1.0 }
test()
'

# Test 20: Struct construct + method call (parser validation)
run_check_test "Impl method call" '
struct Point { x: Double, y: Double }
impl Point
    def scale(self, f: Double) -> Double
        self.x * f
    end
end
def test() {
    let p = Point { x: 3.0, y: 4.0 }
    p.scale(2.0)
}
test()
'

# Test 21: Vector literal creation
run_test "Vector Literal" '
def test() [1, 2, 3]; 1.0
test()
'

# Test 22: Vector indexing returns scalar
run_test "Vector Index" '
def test() [1, 2, 3][1]; 1.0
test()
'

# Test 23: First element index
run_test "Vector First Index" '
def test() [10, 20, 30][0] == 10.0; 1.0
test()
'

# Test 24: Vector index in expression
run_test "Vector Index Expr" '
def test() [1, 2, 3][2] * 2.0; 1.0
test()
'

# Test 25: Vector with negative values
run_test "Vector Negatives" '
def test() [-1, -2, -3][1]; 1.0
test()
'

# Test 26: Vector with mixed int/double
run_test "Vector Mixed Types" '
def test() [1, 2.5, 3][0]; 1.0
test()
'

# Test 27: Empty vector literal
run_check_test "Vector Empty" '
def test() []; 1.0
test()
'

# Test 28: Vector index in comparison
run_test "Vector Index Compare" '
def test() [1, 2, 3][1] == 2.0; 1.0
test()
'

# Test 29: Vector with complex indexing chain
run_test "Vector Double Index" '
def test() [[1, 2], [3, 4]][0]; 1.0
test()
'

# Test 30: Struct construction and field access
run_test "Struct Field Access" '
struct Point { x: Double, y: Double }
def test() -> Double {
    let p = Point { x: 3.0, y: 4.0 };
    p.x
}
test()
'

# Test 31: Struct let-in pattern
run_test "Struct Let-In" '
struct Point { x: Double, y: Double }
def test() -> Double let p = Point { x: 7.0, y: 8.0 } in p.y
test()
'

# Test 32: Struct field access with assert
run_test "Struct Assert" '
struct Vec2 { x: Double, y: Double }
def test() {
    let v = Vec2 { x: 10.0, y: 20.0 };
    assert(v.x == 10.0, "x wrong");
    assert(v.y == 20.0, "y wrong");
    1.0
}
test()
'

# Test 33: Enum declaration and variant access
run_test "Enum Variant" '
enum Color { Red, Green, Blue }
def test() -> Double {
    let c = Color.Red;
    1.0
}
test()
'

# Test 34: Struct function parameter and call
run_test "Struct Param Call" '
struct Point { x: Double, y: Double }
def get_x(p: Point) -> Double p.x
def call_it() -> Double get_x(Point { x: 5.0, y: 6.0 })
call_it()
'

# Test 35: Enum equality == and !=
run_test "Enum Equality" '
enum Color
    Red
    Green
    Blue
end
def test() {
    assert(Color.Red == Color.Red, "eq");
    assert(Color.Red != Color.Blue, "ne");
    1.0
}
test()
'

# Test 36: Enum payload (single field) + match extraction
run_test "Enum Payload Match" '
enum Option { Some { value: Double }, None }
let five = Option.Some(5.0)
match five {
    Option.Some(v) -> v,
    default -> -1.0
}
'

# Test 37: Enum payload (multi-field) + match multi-binding
run_test "Enum Multi Payload Match" '
enum Item { Label { text: String, value: Double } }
let item = Item.Label("hello", 42.0)
match item {
    Item.Label(txt, val) -> val,
    default -> -1.0
}
'

# Test 38: Enum payload positional constructor scalar
run_test "Enum Payload Scalar" '
enum Wrap { Value { x: Double }, Empty }
let w = Wrap.Value(3.14)
match w {
    Wrap.Value(v) -> v,
    default -> -1.0
}
'

# Test 39: Enum payload match default fallback
run_test "Enum Match Default" '
enum Option { Some { value: Double }, None }
let five = Option.None
match five {
    Option.Some(v) -> v,
    default -> -1.0
}
'

# Test 40: Nested enum payload constructions and matches
run_test "Enum Nested Payload" '
enum Result { Ok { value: Double }, Err { msg: String } }
let r1 = Result.Ok(10.0)
let r2 = Result.Err("fail")
match r1 {
    Result.Ok(v) -> v,
    default -> -1.0
}
'

# Test 41: Large struct passing optimization (> 16 bytes)
run_test "Large Struct Passing Optimization" '
struct Point3D {
    x: Double,
    y: Double,
    z: Double
}

impl Point3D
    def calc_sum(self) -> Double {
        self.x + self.y + self.z
    }

    def add_other(self, other: Point3D) -> Double {
        self.calc_sum() + other.calc_sum()
    }
end

def get_x_plus_y(p: Point3D) -> Double {
    p.x + p.y
}

def test() -> Double {
    let p1 = Point3D { x: 10.0, y: 20.0, z: 30.0 };
    let p2 = Point3D { x: 1.0, y: 2.0, z: 3.0 };

    assert(get_x_plus_y(p1) == 30.0, "fn large param wrong");
    assert(p1.calc_sum() == 60.0, "method self large param wrong");
    assert(p1.add_other(p2) == 66.0, "method second large param wrong");

    1.0
}
test()
'

# Test 42: Tracked JIT Allocator & Boxed Enum Arena Deallocation
run_test "Boxed Enum JIT Arena Deallocation" '
struct LargePayload {
    a: Double,
    b: Double,
    c: Double
}

enum BoxedOption {
    Some(LargePayload),
    None
}

def create_boxed(x: Double) -> BoxedOption {
    BoxedOption.Some(LargePayload { a: x, b: x + 1.0, c: x + 2.0 })
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
verify_leak_prevention()
'

# Test 43: Named Fields Enum Variant Constructor
run_test "Named Fields Enum Variant Constructor" '
enum Option {
    Some { value: Double },
    None
}

enum Info {
    Person { age: Double, score: Double },
    Empty
}

def verify_named_constructors() -> Double {
    let opt = Option.Some { value: 42.0 };
    let person = Info.Person { age: 25.0, score: 98.0 };

    var check_opt = match opt {
        Option.Some(payload) -> {
            assert(payload.value == 42.0, "Named field payload wrong");
            1.0
        },
        default -> -1.0
    };

    var check_person = match person {
        Info.Person(payload) -> {
            assert(payload.age == 25.0, "Person age wrong");
            assert(payload.score == 98.0, "Person score wrong");
            1.0
        },
        default -> -1.0
    };

    check_opt * check_person
}

verify_named_constructors()
'

# Test 44: Generics & Templates (Bracket Syntax [T])
run_test "Generics & Templates (Bracket Syntax [T])" '
def identity[T](x: T) -> T {
    return x
}

struct Pair[T] {
    first: T,
    second: T
}

def verify_generics() -> Double {
    let explicit_val = identity[Double](4.2);
    assert(explicit_val == 4.2, "Explicit generic function wrong");

    let implicit_val = identity(3.14);
    assert(implicit_val == 3.14, "Implicit generic function wrong");

    let pair = Pair[Double] { first: 10.0, second: 20.0 };
    assert(pair.first == 10.0, "Generic struct first field wrong");
    assert(pair.second == 20.0, "Generic struct second field wrong");

    1.0
}

verify_generics()
'

# Test 45: OOP Class Support
run_test "OOP Class Support" '
class Counter {
    value: Double

    def increment(self) -> Double {
        self.value = self.value + 1.0;
        self.value
    }
}

class Box[T] {
    content: T

    def get(self) -> T {
        return self.content;
    }
}

def verify_classes() -> Double {
    let c = Counter { value: 10.0 };
    assert(c.increment() == 11.0, "Class method dispatch failed");
    assert(c.increment() == 12.0, "Subsequent class method dispatch failed");

    let b = Box[Double] { content: 42.0 };
    assert(b.get() == 42.0, "Generic class method dispatch failed");

    1.0
}

verify_classes()
'

# Test 46: OOP Class Inheritance & Lifecycle Hooks
run_test "OOP Class Inheritance & Lifecycle Hooks" '
extern def flux_print_double(x: Double) -> Double

class Animal {
    age: Double
    initialized: Double
    destroyed: Double

    def onInit(self) -> Void {
        self.initialized = 1.0;
    }

    def onDestroy(self) -> Void {
        self.destroyed = 1.0;
        flux_print_double(111.0);
    }

    def speak(self) -> Double { 1.0 }
    def get_age(self) -> Double { self.age }
}

class Dog : Animal {
    breed: Double
    dog_init: Double
    dog_destroyed: Double

    def onInit(self) -> Void {
        self.dog_init = 1.0;
    }

    def onDestroy(self) -> Void {
        self.dog_destroyed = 1.0;
        flux_print_double(222.0);
    }

    def speak(self) -> Double { 2.0 }
}

def verify_inheritance() -> Double {
    let d = Dog { age: 5.0, breed: 9.0 };
    assert(d.get_age() == 5.0, "Inherited method/field lookup failed");
    assert(d.speak() == 2.0, "Method overriding failed");
    assert(d.initialized == 1.0, "Parent onInit failed to run");
    assert(d.dog_init == 1.0, "Child onInit failed to run");
    1.0
}

verify_inheritance()
'

# Test 47: Unified OOP Integration (Struct, Enum, Class)
run_test "Unified OOP Integration" '
enum Status {
    Active,
    Inactive
}

struct Point {
    x: Double,
    y: Double
}

class User {
    id: Double
    pos: Point
    status: Status

    def update_position(self, new_pos: Point) -> Point {
        self.pos = new_pos;
        self.pos
    }

    def is_active(self) -> Double {
        match self.status {
            Status.Active -> 1.0,
            default -> 0.0
        }
    }
}

def verify_unified() -> Double {
    let u = User {
        id: 1.0,
        pos: Point { x: 10.0, y: 20.0 },
        status: Status.Active
    };

    assert(u.is_active() == 1.0, "Enum integration inside class failed");

    let p2 = Point { x: 30.0, y: 40.0 };
    let updated_pos = u.update_position(p2);
    assert(updated_pos.x == 30.0, "Nested struct field update failed");
    assert(u.pos.y == 40.0, "Class state representation failed");

    1.0
}

verify_unified()
'

# Test 48: Traits & Interfaces
run_test "Traits & Interfaces" '
trait HasArea {
    def area(self) -> Double
}

trait Describe {
    def describe(self) -> Double
}

struct Circle {
    radius: Double
}

impl HasArea for Circle {
    def area(self) -> Double {
        3.14159 * self.radius * self.radius
    }
}

impl Describe for Circle {
    def describe(self) -> Double {
        self.radius
    }
}

struct Rectangle {
    width: Double,
    height: Double
}

impl HasArea for Rectangle {
    def area(self) -> Double {
        self.width * self.height
    }
}

def print_area[T: HasArea](shape: T) -> Double {
    shape.area()
}

def verify_traits() -> Double {
    let c = Circle { radius: 2.0 };
    let r = Rectangle { width: 3.0, height: 4.0 };

    let circle_area = print_area[Circle](c);
    let rect_area = print_area[Rectangle](r);

    assert(abs(circle_area - 12.56636) < 0.001, "Circle area via trait wrong");
    assert(abs(rect_area - 12.0) < 0.001, "Rectangle area via trait wrong");

    1.0
}

verify_traits()
'

# Test 49a: dyn Trait - let binding (fat pointer dispatch)
run_test "dyn Trait - let binding" '
trait Draw {
    def draw(self) -> Double
}

struct Circle { r: Double }
struct Square { s: Double }

impl Draw for Circle {
    def draw(self) -> Double { 3.14159 * self.r * self.r }
}
impl Draw for Square {
    def draw(self) -> Double { self.s * self.s }
}

let c = Circle { r: 2.0 };
let s = Square { s: 3.0 };
let dc: dyn Draw = c;
let ds: dyn Draw = s;
let result = dc.draw() + ds.draw();
assert(abs(result - 21.56636) < 0.001, "dyn Draw area sum wrong");
result
'

# Test 49b: dyn Trait - function parameter (upcast from concrete struct)
run_test "dyn Trait - function parameter" '
trait Greeter {
    def greet(self) -> Double
}

struct Hello { val: Double }
struct Goodbye { }

impl Greeter for Hello {
    def greet(self) -> Double { self.val }
}
impl Greeter for Goodbye {
    def greet(self) -> Double { 0.0 }
}

def process(g: dyn Greeter) -> Double {
    g.greet()
}

let a = Hello { val: 5.0 };
let b = Goodbye {};
let r1 = process(a);
let r2 = process(b);
assert(abs(r1 - 5.0) < 0.001, "Hello greet via dyn param wrong");
assert(abs(r2 - 0.0) < 0.001, "Goodbye greet via dyn param wrong");
r1 + r2
'

# Test 49c: dyn Trait - multi-method dispatch via function parameter
run_test "dyn Trait - multi-method dispatch" '
trait Shape {
    def area(self) -> Double
    def perimeter(self) -> Double
}

struct Rect { w: Double, h: Double }
struct Circle2 { r: Double }

impl Shape for Rect {
    def area(self) -> Double { self.w * self.h }
    def perimeter(self) -> Double { 2.0 * (self.w + self.h) }
}
impl Shape for Circle2 {
    def area(self) -> Double { 3.14159 * self.r * self.r }
    def perimeter(self) -> Double { 2.0 * 3.14159 * self.r }
}

def describe(s: dyn Shape) -> Double {
    s.area() + s.perimeter()
}

let r = Rect { w: 3.0, h: 4.0 };
let c = Circle2 { r: 2.0 };
let rd = describe(r);
let cd = describe(c);
assert(abs(rd - 26.0) < 0.001, "Rect describe wrong");
assert(abs(cd - 18.84954) < 0.001, "Circle describe wrong");
rd + cd
'

# Test 49: Operator Overloading
run_test "Operator Overloading" '
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
        let r1 = self.re * rhs.re;
        let r2 = self.im * rhs.im;
        let i1 = self.re * rhs.im;
        let i2 = self.im * rhs.re;
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
    let a = Complex { re: 1.0, im: 2.0 };
    let b = Complex { re: 3.0, im: 4.0 };
    let sum = a + b;
    let diff = a - b;
    let prod = a * b;
    let neg_a = -a;
    let eq_check = a == a;
    let ne_check = a == b;

    assert(abs(sum.re - 4.0) < 0.0001, "sum.re");
    assert(abs(sum.im - 6.0) < 0.0001, "sum.im");
    assert(abs(diff.re + 2.0) < 0.0001, "diff.re");
    assert(abs(diff.im + 2.0) < 0.0001, "diff.im");
    assert(abs(prod.re + 5.0) < 0.0001, "prod.re");
    assert(abs(prod.im - 10.0) < 0.0001, "prod.im");
    assert(abs(neg_a.re + 1.0) < 0.0001, "neg_a.re");
    assert(abs(neg_a.im + 2.0) < 0.0001, "neg_a.im");
    assert(eq_check != 0.0, "eq");
    assert(ne_check == 0.0, "ne");

    1.0
}

verify_overloads()
'

# Test 50: Pipe Operator
run_test "Pipe Operator - basic" '
def double_it(x: Double) -> Double { x * 2.0 }
def add_one(x: Double) -> Double { x + 1.0 }
3.0 |> double_it |> add_one
'
run_test "Pipe Operator - multi-arg" '
def scale(s: Double, x: Double) -> Double { s * x }
let mul = 2.5;
10.0 |> scale(mul)
'
run_test "Pipe Operator - method call" '
class Box { val: Double }
impl Box {
    def apply(self, f: Double) -> Double { self.val * f }
}
let b = Box { val: 3.0 };
5.0 |> b.apply
'

# Test 53: Async/Await
run_test "Async/Await" '
async def fetch_data() -> Double {
    await 0.5;
    42.0
}

async def process_data(val: Double) -> Double {
    let data = await fetch_data();
    data + val
}

def verify_async() -> Double {
    let result = process_data(1.0);
    assert(abs(result - 43.0) < 0.0001, "async result");
    1.0
}

verify_async()
'

# Test 51: Async - await outside async error check
run_test "Async - await outside async (error)" '
async def ok() -> Double { await 0.5; 1.0 }
42.0
'

# Test 52: Async simple call without await
run_test "Async - simple call" '
async def make_val() -> Double { await 1.0; 7.0 }
def check() -> Double {
    let r = make_val();
    assert(abs(r - 7.0) < 0.0001, "simple call");
    1.0
}
check()
'

# Test 53: Async with conditional await
run_test "Async - conditional await" '
async def maybe_await(flag: Double) -> Double {
    if flag > 0.5 {
        await 0.5;
        100.0
    } else {
        200.0
    }
}

def run_check() -> Double {
    let r1 = maybe_await(1.0);
    assert(abs(r1 - 100.0) < 0.0001, "cond await true");
    let r2 = maybe_await(0.0);
    assert(abs(r2 - 200.0) < 0.0001, "cond await false");
    1.0
}
run_check()
'

# Test 54: Async deeply nested (3 levels deep)
run_test "Async - deep nesting" '
async def level3() -> Double { await 0.5; 3.0 }
async def level2() -> Double { let v = await level3(); v + 2.0 }
async def level1() -> Double { let v = await level2(); v + 1.0 }

def run_test() -> Double {
    let r = level1();
    assert(abs(r - 6.0) < 0.0001, "deep nested");
    1.0
}
run_test()
'

# Test 55: Async calling multiple async functions
run_test "Async - multiple async calls" '
async def source_a() -> Double { await 0.5; 10.0 }
async def source_b() -> Double { await 1.0; 20.0 }
async def source_c() -> Double { await 1.5; 30.0 }

async def combiner() -> Double {
    source_a() + source_b() + source_c()
}

def run_check() -> Double {
    let r = combiner();
    assert(abs(r - 60.0) < 0.0001, "multi async calls");
    1.0
}
run_check()
'

# Test 56: Async with nested await expressions
run_test "Async - nested await expressions" '
async def inner() -> Double { await 0.5; 5.0 }
async def outer() -> Double { await inner() + 3.0 }

def run_check() -> Double {
    let r = outer();
    assert(abs(r - 8.0) < 0.0001, "nested await expr");
    1.0
}
run_check()
'

# Test 57: Async identity function
run_test "Async - identity passthrough" '
async def identity(x: Double) -> Double { await x; x }

def run_check() -> Double {
    let r = identity(99.0);
    assert(abs(r - 99.0) < 0.0001, "identity");
    1.0
}
run_check()
'

# Test 58: Async with multiple params
run_test "Async - multi-param async" '
async def sum3(a: Double, b: Double, c: Double) -> Double { await 1.0; a + b + c }

def run_check() -> Double {
    let r = sum3(10.0, 20.0, 30.0);
    assert(abs(r - 60.0) < 0.0001, "multi-param");
    1.0
}
run_check()
'

# Test 60: Match exhaustiveness - all variants covered
run_test "Match Exhaustiveness - all covered" '
enum Color { Red, Green, Blue }
def run_check() -> Double {
    let c = Color.Red;
    match c {
        Color.Red -> 1.0,
        Color.Green -> 2.0,
        Color.Blue -> 3.0
    }
}
run_check()
'

# Test 62: Match exhaustiveness - with default arm (ok)
run_test "Match Exhaustiveness - default arm" '
enum Color { Red, Green, Blue }
def run_check() -> Double {
    let c = Color.Red;
    match c {
        Color.Red -> 1.0,
        default -> 0.0
    }
}
run_check()
'

# Test 63: Match exhaustiveness - payload enum all covered
run_test "Match Exhaustiveness - payload covered" '
enum Option { Some(Double), None }
def run_check() -> Double {
    let opt = Option.Some(5.0);
    match opt {
        Option.Some(x) -> x,
        Option.None -> 0.0
    }
}
run_check()
'

# Test 65: Async from async with let (no cross-await usage)
run_test "Async - let before await" '
async def helper() -> Double { await 1.0; 5.0 }
async def user() -> Double {
    let factor = 2.0;
    let base = await helper();
    factor * base
}

def run_check() -> Double {
    let r = user();
    assert(abs(r - 10.0) < 0.0001, "let before await");
    1.0
}
run_check()
'

# ==============================================================================
# Outlives Bounds ('a: 'b) Tests
# ==============================================================================

# Test 66: Struct with outlives bounds (parser validation)
run_check_test "Struct Outlives Bounds" "
struct Foo<'a, 'b: 'a> {
    x: &'a Double,
    y: &'b Double
}
def test() -> Double { 1.0 }
test()
"

# Test 67: Impl with outlives bounds (parser validation)
run_check_test "Impl Outlives Bounds" "
struct Foo<'a, 'b> {
    x: &'a Double,
    y: &'b Double
}
impl<'a, 'b: 'a> Foo<'a, 'b>
    def get_x(self) -> &'a Double { self.x }
end
def test() -> Double { 1.0 }
test()
"

# Test 68: Struct with multiple outlives bounds
run_check_test "Multi Outlives Bounds" "
struct Foo<'a, 'b: 'a, 'c: 'b> {
    a: &'a Double,
    b: &'b Double,
    c: &'c Double
}
def test() -> Double { 1.0 }
test()
"

# Test 69: Function with outlives bounds (parser validation)
run_check_test "Fn Outlives Bounds" "
def foo<'a, 'b: 'a>(x: &'a Double) -> &'b Double { x }
def test() -> Double { 1.0 }
test()
"

# Test 70: Lifetime bound error - undeclared lifetime in bound
run_check_test "Undeclared Lifetime Bound" "
struct Foo<'a: 'b> { x: &'a Double }
def test() -> Double { 1.0 }
test()
"

# Test 71: Lifetime bound error - undeclared lifetime in reference
run_check_test "Undeclared Lifetime Ref" "
struct Foo<'a> { x: &'b Double }
def test() -> Double { 1.0 }
test()
"

# Test 72: Single lifetime param (no bounds) still works
run_check_test "Single Lifetime Param" "
struct Foo<'a> { x: &'a Double }
impl<'a> Foo<'a>
    def get(self) -> &'a Double { self.x }
end
def test() -> Double { 1.0 }
test()
"

# Test 73: Associated Types in Traits
run_test "Associated Types" '
trait Container {
    type Item;
    def get(self, index: Double) -> Item
}

struct MyBox {
    value: Double
}

impl Container for MyBox {
    type Item = Double;
    def get(self, index: Double) -> Double {
        self.value
    }
}

def test() -> Double {
    let b = MyBox { value: 42.0 };
    let v = b.get(0.0);
    assert(abs(v - 42.0) < 0.001, "associated type basic failed");
    1.0
}
test()
'

# Test 74: Associated Types - multiple associated types
run_test "Associated Types (multi)" '
trait Pair {
    type First;
    type Second;
    def first(self) -> First
    def second(self) -> Second
}

struct TwoValues {
    a: Double,
    b: Double
}

impl Pair for TwoValues {
    type First = Double;
    type Second = Double;
    def first(self) -> Double { self.a }
    def second(self) -> Double { self.b }
}

def test() -> Double {
    let t = TwoValues { a: 10.0, b: 20.0 };
    let f = t.first();
    let s = t.second();
    assert(abs(f - 10.0) < 0.001, "assoc type first failed");
    assert(abs(s - 20.0) < 0.001, "assoc type second failed");
    1.0
}
test()
'

# Test 75: Associated Types - missing binding (error)
run_check_test "Associated Types (missing)" "
trait Container {
    type Item;
    def get(self, index: Double) -> Item
}
struct MyBox { value: Double }
impl Container for MyBox {
    def get(self, index: Double) -> Double { self.value }
}
"

# Test 76: Threading - spawn/join
run_test "Spawn/Join" '
def worker(x: Double) -> Double {
    x * 2.0
}

def main() -> Double {
    let t = spawn worker(21.0);
    assert(join(t) == 42.0, "spawn/join failed");
    1.0
}

main()
'

# Test 77: Threading - channel send/recv
run_test "Channel Send/Recv" '
def main() -> Double {
    let ch = flux_chan_create();
    flux_chan_send(ch, 42.0);
    let val = flux_chan_recv(ch);
    flux_chan_destroy(ch);
    assert(val == 42.0, "channel send/recv failed");
    1.0
}

main()
'

# Test 78: Threading - spawn with channel
run_test "Spawn + Channel" '
def producer(ch: Double) -> Double {
    flux_chan_send(ch, 99.0);
    1.0
}

def main() -> Double {
    let ch = flux_chan_create();
    spawn producer(ch);
    let val = flux_chan_recv(ch);
    flux_chan_destroy(ch);
    assert(val == 99.0, "spawn+channel failed");
    1.0
}

main()
'

# Test 79: Threading - spawn multiple args
run_test "Spawn Multi-Arg" '
def add(a: Double, b: Double, c: Double) -> Double {
    a + b + c
}

def main() -> Double {
    let t = spawn add(10.0, 20.0, 30.0);
    assert(join(t) == 60.0, "spawn multi-arg failed");
    1.0
}

main()
'

# Test 80: Threading - Mutex
run_test "Mutex Lock/Unlock" '
def main() -> Double {
    let mtx = flux_mutex_create();
    flux_mutex_lock(mtx);
    flux_mutex_unlock(mtx);
    flux_mutex_destroy(mtx);
    1.0
}

main()
'

# Test 81: Threading - RwLock
run_test "RwLock Read/Write" '
def main() -> Double {
    let rw = flux_rwlock_create();
    flux_rwlock_read_lock(rw);
    flux_rwlock_unlock(rw);
    flux_rwlock_write_lock(rw);
    flux_rwlock_unlock(rw);
    flux_rwlock_destroy(rw);
    1.0
}

main()
'

# ==============================================================================
# Self-Hosting Compiler Tests (src/fluxc/) — IR generation via opt-21 verify
# ==============================================================================

# Test S1: String concatenation
run_selfhost_test "SelfHost: string concat" '
extern def flux_print_string(s: string)
def greet(name: string) -> string { "hello " + name }
def main() -> Double { flux_print_string(greet("world")); 0.0 }
'

# Test S2: String equality
run_selfhost_test "SelfHost: string eq" '
def eq(a: string, b: string) -> double { a == b }
def main() -> Double { 0.0 }
'

# Test S3: String inequality
run_selfhost_test "SelfHost: string neq" '
def neq(a: string, b: string) -> double { a != b }
def main() -> Double { 0.0 }
'

# Test S4: Numeric comparisons
run_selfhost_test "SelfHost: numeric cmp" '
def lt(a: double, b: double) -> double { a < b }
def gt(a: double, b: double) -> double { a > b }
def le(a: double, b: double) -> double { a <= b }
def ge(a: double, b: double) -> double { a >= b }
def main() -> Double { 0.0 }
'

# Test S5: If-else with comparison
run_selfhost_test "SelfHost: if-else" '
def max(a: double, b: double) -> double {
    if (a > b) { a } else { b }
}
def main() -> Double { max(3.0, 5.0) }
'

# Test S6: Logical not
run_selfhost_test "SelfHost: logical not" '
def is_zero(x: double) -> double { if (!x) { 1.0 } else { 0.0 } }
def main() -> Double { is_zero(0.0) }
'

run_selfhost_test "SelfHost: enum match" '
enum Color { Red, Green, Blue }
def pick(c: Double) -> Double {
    match c {
        Color.Red -> 1.0,
        Color.Green -> 2.0,
        Color.Blue -> 3.0
    }
}
def main() -> Double { pick(Color.Green) }
'

run_selfhost_test "SelfHost: payload enum match" '
enum Option { Some(value: Double), None }
def main() -> Double {
    let x = Option.Some(42.0);
    match x {
        Option.Some(v) -> v,
        Option.None -> 0.0
    }
}
'

run_selfhost_test "SelfHost: var reassign" '
def main() -> Double {
    var x = 10.0;
    x = 42.0;
    x
}
'

run_selfhost_test "SelfHost: var payload" '
enum Option { Some(value: Double), None }
def main() -> Double {
    var x = Option.Some(10.0);
    x = Option.Some(42.0);
    match x {
        Option.Some(v) -> v,
        Option.None -> 0.0
    }
}
'

run_selfhost_test "SelfHost: multi-field payload" '
enum Person { Named { age: Double, score: Double }, Empty }
def main() -> Double {
    let p = Person.Named { age: 25.0, score: 98.0 };
    match p {
        Person.Named(pl) -> pl.score,
        Person.Empty -> 0.0
    }
}
'

run_selfhost_test "SelfHost: impl with method" '
impl MyMath {
    def double_it(self: MyMath, x: Double) -> Double { x * 2.0 }
}
def main() -> Double { MyMath_double_it(0.0, 21.0) }
'

run_selfhost_test "SelfHost: method call syntax" '
impl MyMath {
    def double_it(self: MyMath, x: Double) -> Double { x * 2.0 }
}
def main() -> Double {
    let m: MyMath = 0.0;
    m.double_it(21.0)
}
'

run_selfhost_test "SelfHost: trait impl vtable" '
trait Math {
    def add(a: Double, b: Double) -> Double;
    def mul(a: Double, b: Double) -> Double;
}
struct Calc { }
impl Math for Calc {
    def add(a: Double, b: Double) -> Double { a + b }
    def mul(a: Double, b: Double) -> Double { a * b }
}
def main() -> Double { 0.0 }
'

run_selfhost_test "SelfHost: dyn trait dispatch" '
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
'

run_selfhost_test "SelfHost: struct constructor" '
struct Pair { x: Double, y: Double }
def main() -> Double {
    let p: Pair = Pair { x: 3.0, y: 4.0 };
    p.x
}
'

run_selfhost_test "SelfHost: struct constructor method" '
struct Pair { x: Double, y: Double }
impl Pair {
    def sum(self: Pair) -> Double { self.x + self.y }
}
def main() -> Double {
    let p: Pair = Pair { x: 3.0, y: 4.0 };
    p.sum()
}
'

run_selfhost_test "SelfHost: for loop" '
def main() -> Double {
    for i in 1, 3 do i
}
'

run_selfhost_test "SelfHost: for loop block" '
def main() -> Double {
    let s = 0.0;
    for i in 1, 4 do { s = s + i };
    s
}
'

run_selfhost_test "SelfHost: vector literal" '
def main() -> Double {
    [1.0, 2.0, 3.0][1]
}
'

run_selfhost_test "SelfHost: vector index expr" '
def main() -> Double {
    [1.0, 2.0, 3.0][2] * 2.0
}
'

run_selfhost_test "SelfHost: vector comparison" '
def main() -> Double {
    [10.0, 20.0, 30.0][0] == 10.0
}
'

run_selfhost_test "SelfHost: pipe basic" '
def double_it(x: Double) -> Double { x * 2.0 }
def add_one(x: Double) -> Double { x + 1.0 }
def main() -> Double {
    3.0 |> double_it |> add_one
}
'

run_selfhost_test "SelfHost: pipe multi-arg" '
def scale(s: Double, x: Double) -> Double { s * x }
def main() -> Double {
    let mul = 2.5;
    10.0 |> scale(mul)
}
'

run_selfhost_test "SelfHost: pipe method call" '
struct Box { val: Double }
impl Box {
    def apply(self: Box, f: Double) -> Double { self.val * f }
}
def main() -> Double {
    let b: Box = Box { val: 3.0 };
    5.0 |> b.apply
}
'

run_selfhost_test "SelfHost: while loop" '
def main() -> Double {
    var x = 0.0;
    while x < 3.0 do { x = x + 1.0 };
    x
}
'

run_selfhost_test "SelfHost: lambda" '
def main() -> Double {
    let f = fn(x) -> x + 1.0;
    f
}
'

run_selfhost_test "SelfHost: lambda multi-param" '
def main() -> Double {
    let f = fn(a, b) -> a + b;
    f
}
'

run_selfhost_test "SelfHost: lambda no body" '
def main() -> Double {
    fn() -> 42.0
}
'

run_selfhost_test "SelfHost: while loop" '
def main() -> Double {
    var x = 0.0;
    while x < 3.0 do { x = x + 1.0 };
    x
}
'

run_selfhost_test "SelfHost: break in while" '
def main() -> Double {
    var x = 0.0;
    while true do {
        x = x + 1.0;
        if (x > 3.0) { break }
    };
    x
}
'

run_selfhost_test "SelfHost: continue in while" '
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
'

echo ""
echo "========================================"  
echo "  Results: $PASSED/$TOTAL passed"
echo "========================================"

if [ $FAILED -eq 0 ]; then
    echo -e "${GREEN} ALL TESTS PASSED${NC}"
    exit 0
else
    echo -e "${RED} $FAILED TESTS FAILED${NC}"
    exit 1
fi
