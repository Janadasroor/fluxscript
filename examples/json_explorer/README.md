# JSON Explorer (FluxScript)

A recursive-descent JSON parser, validator, and pretty-printer written in FluxScript. Demonstrates most major language features.

## Files

| File | Purpose |
|------|---------|
| `json_types.flux` | Character code constants used by the parser |
| `json_explorer.flux` | Recursive descent parser + pretty printer + hash verifier |
| `run.flux` | 20-test suite with automated hash verification |

## Features Covered

| Feature | Where |
|---------|-------|
| **Enums** | `TokenType` (12 variants) in `json_types.flux` |
| **Generics** | (used implicitly in stdlib type inference) |
| **Recursion** | `parse_value()` → `parse_array()`/`parse_object()` cycle |
| **String ops** | `flux_string_at`, `flux_string_slice`, `flux_strlen`, `flux_string_concat` |
| **Matrix ops** | Mutable `pos` tracking via 1×1 matrices (pass-by-reference pattern) |
| **Modules/imports** | `json_types` + `json_explorer` from `run.flux` |
| **Runtime functions** | `flux_parse_number`, `flux_double_to_string`, `flux_print_string` |
| **Control flow** | `while` loops, nested `if/else`, `break` |
| **Pretty printer** | 2-space indent, proper comma/colon formatting |

## Running

```bash
cd examples/json_explorer
rm -rf ~/.cache/fluxscript
../../build/flux --cache=0 run.flux
```

## Test Coverage

| Test | What it verifies |
|------|------------------|
| `null` | Parses null → hash 0 |
| `true` / `false` | Boolean literals |
| `integer` / `negative` / `float` | Number parsing |
| `string` / `string with escape` / `string with quotes` / `unicode escape` | String escape handling |
| `empty array` / `simple array` / `nested array` | Array recursion and weighted hash |
| `empty object` / `simple object` / `multi-field object` | Object key-value parsing |
| `complex JSON` | Real-world multi-type JSON |
| `full round-trip` | All 6 value types + scientific notation |
| `whitespace tolerance` | Robust whitespace skipping |
| `deep nesting` | `[[[[]]]]` — 4 levels of empty arrays |
