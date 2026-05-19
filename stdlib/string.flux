def len(s) { flux_strlen(s) }

def at(s, i) { flux_string_at(s, i) }

def slice(s, start, end) { flux_string_slice(s, start, end) }

def find(s, c) { flux_string_find(s, c) }

def cmp(a, b) { flux_strcmp(a, b) }

def parse_number(s) { flux_parse_number(s) }

def to_string(v) { flux_double_to_string(v) }

def print(s) { flux_print_string(s) }

def regex_match(s, pat) { flux_regex_match(s, pat) }

def regex_replace(s, pat, repl) { flux_regex_replace(s, pat, repl) }
