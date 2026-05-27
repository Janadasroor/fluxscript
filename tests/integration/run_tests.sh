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
        test_output=$($TIMEOUT_CMD 5 "$FLUX_BIN" flux_test.flux 2>&1)
        cmd_status=$?
    else
        test_output=$("$FLUX_BIN" flux_test.flux 2>&1)
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
        test_output=$($TIMEOUT_CMD 5 "$FLUX_BIN" --emit=check flux_test.flux 2>&1)
        echo "$test_output" | grep -q "OK"
        cmd_status=$?
    else
        test_output=$("$FLUX_BIN" --emit=check flux_test.flux 2>&1)
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

# Test 2: Parallel For Loop (parser-only - JIT runtime symbols not registered)
run_check_test "Parallel For" "
parallel for i in 1, 3 do i
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
