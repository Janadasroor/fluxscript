enum Person { Named { age: Double, score: Double }, Empty }
def main() -> Double {
    let p = Person.Named { age: 25.0, score: 98.0 };
    match p {
        Person.Named(pl) -> pl.score,
        Person.Empty -> 0.0
    }
}
