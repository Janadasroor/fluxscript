import json_types
import json_explorer

def run_test(name: String, got: Double, expected: Double, stats: matrix) {
    flux_print_string("\n=== ")
    flux_print_string(name)
    flux_print_string(" ===\n")
    if (got == expected) {
        flux_print_string("  PASS: ")
        matrix_set(stats, 0, 0, matrix_get(stats, 0, 0) + 1.0)
    } else {
        flux_print_string("  FAIL: ")
        matrix_set(stats, 0, 1, matrix_get(stats, 0, 1) + 1.0)
    }
    flux_print_string(name)
    flux_print_string(" (hash=")
    flux_print_double(got)
    flux_print_string(", expected=")
    flux_print_double(expected)
    flux_print_string(")\n")
}

var stats = matrix_zeros(1, 2)

run_test("null", explore_json("null"), 0.0, stats)

run_test("true", explore_json("true"), 2.0, stats)

run_test("false", explore_json("false"), 1.0, stats)

run_test("integer", explore_json("42"), 42.0, stats)

run_test("negative", explore_json("-17"), -17.0, stats)

run_test("float", explore_json("3.14"), 3.14, stats)

run_test("string", explore_json("\"hello\""), 5.0, stats)

run_test("string with escape", explore_json("\"hello\\nworld\""), 11.0, stats)

run_test("string with quotes", explore_json("\"she said \\\"hi\\\"\""), 13.0, stats)

run_test("unicode escape", explore_json("\"smile \\u263A\""), 7.0, stats)

run_test("empty array", explore_json("[]"), 0.0, stats)

run_test("simple array", explore_json("[10, 20, 30]"), 140.0, stats)

var inner1 = 1.0 + 2.0 * 2.0
var inner2 = 3.0 + 2.0 * 4.0
var nested = 1.0 * inner1 + 2.0 * inner2
run_test("nested array", explore_json("[[1, 2], [3, 4]]"), nested, stats)

run_test("empty object", explore_json("{}"), 0.0, stats)

run_test("simple object", explore_json("{\"a\": 1}"), 4.0, stats)

var h_a = (1.0 + 1.0) * (1.0 + 1.0)
var h_b = (1.0 + 1.0) * (2.0 + 1.0)
run_test("multi-field object", explore_json("{\"a\": 1, \"b\": 2}"), h_a + h_b, stats)

var json_str = "{\"name\": \"Alice\", \"age\": 30.5, \"scores\": [90.0, 85.0, 95.0], \"active\": true, \"tag\": null}"
var name_h = (4.0 + 1.0) * (5.0 + 1.0)
var age_h = (3.0 + 1.0) * (30.5 + 1.0)
var scores_arr = 1.0 * 90.0 + 2.0 * 85.0 + 3.0 * 95.0
var scores_h = (6.0 + 1.0) * (scores_arr + 1.0)
var active_h = (6.0 + 1.0) * (2.0 + 1.0)
var tag_h = (3.0 + 1.0) * (0.0 + 1.0)
var total_h = name_h + age_h + scores_h + active_h + tag_h
run_test("complex JSON", explore_json(json_str), total_h, stats)

var rt_str = "{\"nums\": [1, -2.5, 3e2], \"flag\": false, \"empty\": null, \"msg\": \"ok\"}"
var rt_nums = 1.0 * 1.0 + 2.0 * (-2.5) + 3.0 * 300.0
var rt_flag = (4.0 + 1.0) * (1.0 + 1.0)
var rt_empty = (5.0 + 1.0) * (0.0 + 1.0)
var rt_msg = (3.0 + 1.0) * (2.0 + 1.0)
var rt_obj = (4.0 + 1.0) * (rt_nums + 1.0) + rt_flag + rt_empty + rt_msg
run_test("full round-trip", explore_json(rt_str), rt_obj, stats)

run_test("whitespace tolerance", explore_json("  [  1 , 2 , 3  ]  "), 14.0, stats)

run_test("deep nesting", explore_json("[[[[]]]]"), 0.0, stats)

flux_print_string("\n==============================\n")
flux_print_string("Results: ")
flux_print_double(matrix_get(stats, 0, 0))
flux_print_string(" passed, ")
flux_print_double(matrix_get(stats, 0, 1))
flux_print_string(" failed\n")
flux_print_string("==============================\n")
