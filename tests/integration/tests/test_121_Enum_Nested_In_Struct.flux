enum Status { Active { id: Double }, Inactive }
struct Record { name: Double, status: Status }
def main() -> Double {
    let r = Record { name: 1.0, status: Status.Active(100.0) };
    assert(r.status.id == 100.0, "nested enum field access wrong");
    1.0
}
main()
