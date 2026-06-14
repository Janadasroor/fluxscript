enum Option {
    Some { value: Double },
    None
}
enum Info {
    Person { age: Double, score: Double },
    Empty
}
def main() -> Double {
    let opt = Option.Some { value: 42.0 };
    let person = Info.Person { age: 25.0, score: 98.0 };
    let none_val = Option.None;
    let opt_some = match opt {
        Option.Some(payload) -> { 1.0 },
        default -> 0.0
    };
    assert(opt_some == 1.0, "some constructor wrong");
    let person_person = match person {
        Info.Person(payload) -> { 1.0 },
        default -> 0.0
    };
    assert(person_person == 1.0, "person constructor wrong");
    let none_none = match none_val {
        Option.Some(payload) -> { 1.0 },
        default -> { 0.0 }
    };
    assert(none_none == 0.0, "none constructor wrong");
    1.0
}
main()
