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
def main() -> Double {
    let c = Counter { value: 10.0 }
    let r1 = c.increment()
    assert(r1 == 11.0, "inc 1")
    let r2 = c.increment()
    assert(r2 == 11.0, "inc same val")
    var total = 0.0
    var i = 0.0
    while i < 100.0 do {
        let ct = Counter { value: i }
        total = total + ct.increment()
        i = i + 1.0
    }
    assert(total == 5050.0, "class loop wrong")
    1.0
}
main()
