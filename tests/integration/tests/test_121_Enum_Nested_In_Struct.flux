enum Status { Active { id: Double }, Inactive }
struct Record { name: Double, status: Status }
def main() -> Double {
    let r = Record { name: 1.0, status: Status.Active(100.0) };
    let s = r.status;
    let tag = match s {
        Status.Active(payload) -> 1.0,
        default -> 0.0
    };
    assert(tag == 1.0, "nested enum active variant wrong");
    1.0
}
main()
