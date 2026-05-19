# Submitting FluxScript to GitHub Linguist

To make `.flux` files recognized as FluxScript on all GitHub repos, submit a PR to [github-linguist/linguist](https://github.com/github-linguist/linguist).

## Languages.yml Entry

Add to `lib/linguist/languages.yml`:

```yaml
FluxScript:
  type: programming
  ace_mode: text
  codemirror_mode: clojure
  codemirror_mime_type: text/x-flux
  color: "#7c3aed"
  aliases: []
  extensions:
  - .flux
  interpreters: []
  tm_scope: source.flux
  language_id: (run `uuidgen` to generate a unique ID)
```

## Grammar

A TextMate grammar is included in this repo at:

```
.vscode/fluxscript.tmLanguage.json
```

In the Linguist PR, either:
1. Reference the grammar as a git submodule in `vendor/grammars/`, or
2. Submit the grammar inline in the PR for review

## Samples

Linguist requires sample files for classification testing. Existing examples:

- `examples/mna/mna.flux` — netlist analysis
- `examples/math/math_utils.flux` — math utilities
- `examples/spice/netlist_parser.flux` — SPICE netlist parsing
- `tests/flux/test_all_features.flux` — comprehensive feature test

## Heuristics (optional)

If `.flux` files could be confused with other languages, add heuristics to `lib/linguist/heuristics.yml`:

```yaml
- disambiguations:
  - extensions: [".flux"]
    rules:
    - language: FluxScript
      pattern: "\\b(def|extern|var|let)\\b"
```

## PR Checklist

- [ ] Fork `github-linguist/linguist`
- [ ] Add entry to `lib/linguist/languages.yml`
- [ ] Add grammar to `vendor/grammars/` (or reference external grammar)
- [ ] Add sample files to `samples/FluxScript/`
- [ ] Run `bundle exec rake samples` to verify classification
- [ ] Run `bundle exec rake test` to ensure no regressions
- [ ] Submit PR

## Color

Suggested color: `#7c3aed` (violet/purple)
