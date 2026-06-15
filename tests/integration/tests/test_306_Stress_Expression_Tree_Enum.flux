def eval_expr(opcode, a, b) -> Double {
    if opcode == 0.0 then a + b
    else if opcode == 1.0 then a * b
    else if opcode == 2.0 then a - b
    else if opcode == 3.0 then a / b
    else 0.0
}
def main() -> Double {
    assert(eval_expr(0.0, 3.0, 4.0) == 7.0, "add eval")
    assert(eval_expr(1.0, 3.0, 4.0) == 12.0, "mul eval")
    assert(eval_expr(2.0, 7.0, 3.0) == 4.0, "sub eval")
    assert(eval_expr(3.0, 12.0, 4.0) == 3.0, "div eval")
    assert(eval_expr(0.0, eval_expr(1.0, 2.0, 3.0), eval_expr(2.0, 10.0, 4.0)) == 12.0, "nested eval")
    assert(eval_expr(0.0, 1.0, 2.0) + eval_expr(1.0, 1.0, 2.0) + eval_expr(2.0, 2.0, 3.0) + eval_expr(3.0, 3.0, 4.0) == 4.75, "expr sum")
    1.0
}
main()
