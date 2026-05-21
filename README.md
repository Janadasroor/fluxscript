# FluxScript

LLVM-based JIT-compiled scripting language for mixed-signal circuit simulation with compile-time dimensional analysis.

[![CI](https://github.com/Janadasroor/fluxscript/actions/workflows/ci.yml/badge.svg)](https://github.com/Janadasroor/fluxscript/actions/workflows/ci.yml)
[![Lint](https://github.com/Janadasroor/fluxscript/actions/workflows/lint.yml/badge.svg)](https://github.com/Janadasroor/fluxscript/actions/workflows/lint.yml)

FluxScript compiles to native code via LLVM ORC JIT with tiered compilation and PGO. Its type system enforces SI unit correctness at compile time — mismatching volts and amps is a type error, not a runtime surprise.

## Quick Start

```bash
git clone https://github.com/Janadasroor/fluxscript.git
cd fluxscript && mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
./flux --help
```

### Example: Voltage Divider

```rust
def voltage_divider() {
    var G1 = 1.0 / 1000.0
    var G2 = 1.0 / 2000.0
    var A = matrix_zeros(3, 3)
    matrix_set(A, 0, 0, G1);  matrix_set(A, 0, 1, -G1); matrix_set(A, 0, 2, 1.0)
    matrix_set(A, 1, 0, -G1); matrix_set(A, 1, 1, G1+G2)
    matrix_set(A, 2, 0, 1.0)
    var b = matrix_zeros(3, 1)
    matrix_set(b, 2, 0, 5.0)
    matrix_get(matrix_solve(A, b), 1, 0)
}
```

```bash
./flux --entry=voltage_divider --cache=false examples/mna/mna.flux
# → 3.333
```

## Documentation

- [CLI Tools](docs/cli.md) — all 18 subcommands and their options
- [Language Reference](docs/language.md) — types, syntax, DSL features
- [Standard Library](docs/stdlib.md) — math, trig, array, stats, string

## Requirements

- CMake ≥ 3.16, C++17
- LLVM 21.x ([apt.llvm.org](https://apt.llvm.org) or [releases](https://github.com/llvm/llvm-project/releases))
- Eigen3 3.4.0 (fetched via FetchContent)
- ngspice (optional), CURL (optional)

## Platforms

| Platform | Status |
|---|---|
| Linux x86_64 / ARM64 | ✅ |
| macOS ARM64 | ✅ |
| Windows x86_64 | ✅ |
| Android arm64-v8a / x86_64 | ✅ |

## Testing

```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug -DFLUX_BUILD_TESTS=ON
cmake --build . -j$(nproc)
ctest --output-on-failure
```

## License

Apache 2.0 — see [LICENSE](LICENSE). Copyright 2026 Janada Sroor.
