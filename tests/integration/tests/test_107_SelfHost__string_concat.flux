extern def flux_print_string(s: string)
def greet(name: string) -> string { "hello " + name }
def main() -> Double { flux_print_string(greet("world")); 0.0 }
