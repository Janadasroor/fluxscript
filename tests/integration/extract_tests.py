#!/usr/bin/env python3
"""Extract individual test cases from run_tests.sh into separate .flux files.

Handles both:
  run_test "Name" '
    code...
  '
and:
  run_test "Name" "
    code...
  "
"""
import os
import re
import sys

script_dir = os.path.dirname(os.path.abspath(__file__))
runner = os.path.join(script_dir, "run_tests.sh")
outdir = os.path.join(script_dir, "tests")

with open(runner) as f:
    lines = f.readlines()

os.makedirs(outdir, exist_ok=True)

idx = 0
i = 0
while i < len(lines):
    line = lines[i]
    # Match: run_test "Name"  or run_check_test "Name"  followed by quote on same line
    m = re.match(r'^((?:run|run_check)_test)\s+"([^"]+)"\s*([\'"])\s*$', line)
    if m:
        test_type = m.group(1)
        test_name = m.group(2)
        quote_char = m.group(3)
        idx += 1

        # Collect code lines until closing quote
        code_lines = []
        i += 1
        while i < len(lines):
            cl = lines[i]
            if cl.strip() == quote_char:
                break
            code_lines.append(cl)
            i += 1

        test_code = ''.join(code_lines).strip()

        # Sanitize name for filename
        fname = re.sub(r'[^a-zA-Z0-9._-]', '_', test_name)
        fname = re.sub(r'^_+|_+$', '', fname)
        if len(fname) > 80:
            fname = fname[:80]

        basename = f"test_{idx:03d}_{fname}"

        flux_path = os.path.join(outdir, f"{basename}.flux")
        meta_path = os.path.join(outdir, f"{basename}.meta")

        with open(flux_path, 'w') as f:
            f.write(test_code)
            if not test_code.endswith('\n'):
                f.write('\n')

        with open(meta_path, 'w') as f:
            f.write("check\n" if "check" in test_type else "run\n")
            f.write(f"{test_name}\n")

        print(f"  [{idx:3d}] {test_type:20s} \"{test_name}\"")
    i += 1

print(f"\nExtracted {idx} test cases into {outdir}/")
