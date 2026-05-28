enum Item { Label { text: String, value: Double } }
let item = Item.Label("hello", 42.0)
match item {
    Item.Label(txt, val) -> val,
    default -> -1.0
}
