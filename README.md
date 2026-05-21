# FluxScript

LLVM-based JIT-compiled scripting language for mixed-signal circuit simulation with compile-time dimensional analysis.

[![CI](https://github.com/Janadasroor/fluxscript/actions/workflows/ci.yml/badge.svg)](https://github.com/Janadasroor/fluxscript/actions/workflows/ci.yml)
[![Lint](https://github.com/Janadasroor/fluxscript/actions/workflows/lint.yml/badge.svg)](https://github.com/Janadasroor/fluxscript/actions/workflows/lint.yml)

## Platforms

| Platform | Status |
|----------|--------|
| Linux x86_64 (glibc 2.31+) | ✅ |
| Linux ARM64 | ✅ |
| macOS ARM64 | ✅ |
| Windows x86_64 | ✅ |
| Android arm64-v8a | ✅ |
| Android x86_64 | ✅ |

## Quick Start

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
./flux --help
```

## Testing

```bash
cd build && ctest --output-on-failure
```

## License

All rights reserved.
