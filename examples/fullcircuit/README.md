# FullCircuit — Circuit Simulation Framework for FluxScript

A complete Modified Nodal Analysis (MNA) circuit simulator written in
[FluxScript](https://opencode.ai). Supports DC, transient, and AC
small-signal analysis with nonlinear device models (diode, BJT, MOSFET).

## Project Structure

```
examples/fullcircuit/
├── circuit.flux       # Data structures & netlist builder API
├── mna.flux           # MNA matrix stamping & linear DC solve
├── dcsolve.flux       # Nonlinear DC analysis (Newton-Raphson)
├── trsim.flux         # Transient (time-domain) analysis
├── acswp.flux         # AC small-signal frequency sweep
├── analysis.flux      # Sensitivity, Monte Carlo, noise, THD
├── examples.flux      # Built-in test circuit factories
├── demos/
│   └── run.flux       # Main demo suite (6 test circuits)
├── tests/
│   ├── test_import.flux
│   ├── test_minimal.flux
│   ├── test_mna.flux
│   ├── test_mna2.flux
│   └── test_stamp.flux
├── debug/
│   ├── debug.flux
│   ├── debug2.flux
│   └── debug3.flux
└── README.md
```

## Module Hierarchy

| Layer | Module | Depends On | Purpose |
|-------|--------|------------|---------|
| 0 | `circuit` | — | Netlist data structures |
| 1 | `mna` | `circuit` | MNA matrix stamping |
| 1 | `examples` | `circuit` | Demo circuit factories |
| 2 | `dcsolve` | `circuit`, `mna` | Nonlinear DC analysis |
| 2 | `trsim` | `circuit`, `mna` | Transient analysis |
| 2 | `acswp` | `circuit`, `mna` | AC small-signal analysis |
| 3 | `analysis` | `circuit`, `mna`, `dcsolve` | Advanced analysis |

## Usage

```bash
# Run the demo suite from the fullcircuit directory
cd examples/fullcircuit
flux demos/run.flux

# Clear JIT cache after modifying any module source
rm -rf ~/.cache/fluxscript/jit/*
```

### Clearing the JIT Cache

The JIT compiler caches compiled bitcode in `~/.cache/fluxscript/jit/`.
The cache key does NOT account for imported module file contents, only
the top-level script. **After modifying any `.flux` file, clear the JIT
cache** to force recompilation with your changes.

## Module API

Each library module exposes a set of public functions. All functions
defined in a `.flux` file are public when imported.

### `circuit` — Circuit Builder

- `circuit_create(num_nodes, max_comps)` — allocate new circuit
- `circuit_add_resistor(comps, ctrl, n+, n-, r)`
- `circuit_add_capacitor(comps, ctrl, n+, n-, c, init_v)`
- `circuit_add_inductor(comps, ctrl, n+, n-, l, init_i)`
- `circuit_add_vdc(comps, ctrl, n+, n-, v)` / `circuit_add_idc(...)`
- `circuit_add_vac(comps, ctrl, n+, n-, amp, freq)` / `circuit_add_iac(...)`
- `circuit_add_vcvs(comps, ctrl, n+, n-, nc+, nc-, gain)`
- `circuit_add_diode(comps, ctrl, n+, n-, isat, n_factor)`
- `circuit_add_npn/comps, ...)` / `circuit_add_pnp(...)`
- `circuit_add_nmos(comps, ...)` / `circuit_add_pmos(...)`
- `circuit_num_nodes(ctrl)`, `circuit_count(ctrl)`
- `circuit_component_type(comps, idx)`, `circuit_param(...)`

### `mna` — Linear DC Solver

- `mna_ndx(n)` — node-to-matrix-index helper
- `mna_stamp_g(A, np, nm, g, N)` — stamp conductance
- `mna_stamp_vsource(A, b, np, nm, v, N, bi)` — stamp voltage source
- `mna_build_A_b(comps, ctrl)` — build full MNA system
- `mna_dc_solve(comps, ctrl)` — solve linear DC system
- `mna_get_node_voltage(sol, nd)` / `mna_get_branch_current(...)`

### `dcsolve` — Nonlinear Solver

- `dc_solve(comps, ctrl, max_iter, tol)` — auto-detect & solve
- `dc_solve_linear(comps, ctrl)` — linear-only solve
- `dc_nonlinear_solve(comps, ctrl, max_iter, tol)` — Newton-Raphson
- `dc_sweep(comps, ctrl, src_idx, start, stop, nsteps, ...)`

### `trsim` — Transient Analysis

- `tr_solve(comps, ctrl, t_start, t_stop, dt)`

### `acswp` — AC Analysis

- `ac_sweep(comps, ctrl, f_start, f_stop, npoints, V)`

### `analysis` — Advanced Analysis

- `an_sensitivity(...)`, `an_sensitivity_all(...)`
- `an_monte_carlo(...)`, `an_mc_statistics(...)`
- `an_thd_estimate(...)`, `an_power_dissipation(...)`

## Test Circuits (demos/run.flux)

| # | Circuit | Nodes | Expected |
|---|---------|-------|----------|
| 1 | Voltage Divider (R1=R2=1k, 5V) | 2 | V(2)=2.5V |
| 2 | RC Low-Pass DC (1k, 1µF, 5V) | 2 | V(2)=5V |
| 3 | Series RLC (1k, 1mH, 1µF, 5V) | 3 | V(2)=V(3)=5V |
| 4 | Wheatstone Bridge | 3 | Vdiff=2.63V |
| 5 | Op-Amp Inverting (1k, 10k, 1V) | 3 | V(3)=-10V |
| 6 | VCVS Amplifier (gain=100) | 3 | V(2)=99V, V(3)=100V |

## Origin

This was the first real-world FluxScript project that uncovered several
core compiler bugs, most notably:
- Parser rejecting empty `{ }` bodies in `else if` chains, causing the
  entire if-else AST to be dropped and branches to silently vanish.
- Lexer missing `matrix`/`vector` keywords.
- Cross-module function parameter type inference bugs.

All issues encountered during development were fixed at the compiler
level rather than worked around in Flux code.
