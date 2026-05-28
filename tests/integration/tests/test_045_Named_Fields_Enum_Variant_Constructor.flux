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
