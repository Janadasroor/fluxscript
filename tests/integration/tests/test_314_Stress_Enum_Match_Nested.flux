enum Color { Rgb { r: Double, g: Double, b: Double }, Named { id: Double } }
def color_brightness(c: Color) -> Double {
    match c {
        Color.Rgb(r, g, b) -> r * 0.299 + g * 0.587 + b * 0.114,
        Color.Named(id) -> id * 10.0
    }
}
def color_is_rgb(c: Color) -> Double {
    match c {
        Color.Rgb(r, g, b) -> 1.0,
        default -> 0.0
    }
}
def main() -> Double {
    let red = Color.Rgb { r: 255.0, g: 0.0, b: 0.0 }
    let blue = Color.Rgb { r: 0.0, g: 0.0, b: 255.0 }
    assert(abs(color_brightness(red) - 76.245) < 0.01, "red brightness")
    assert(abs(color_brightness(blue) - 29.07) < 0.01, "blue brightness")
    let named = Color.Named { id: 5.0 }
    assert(abs(color_brightness(named) - 50.0) < 0.01, "named brightness")
    assert(color_is_rgb(red) == 1.0, "red is rgb")
    assert(color_is_rgb(named) == 0.0, "named is not rgb")
    var total = 0.0
    var i = 0.0
    while i < 100.0 do {
        let c = Color.Rgb { r: i, g: i * 0.5, b: i * 0.25 }
        total = total + color_brightness(c)
        i = i + 1.0
    }
    assert(abs(total - 3074.0) < 1.0, "color loop wrong")
    1.0
}
main()
