# string operations wrapping extern C functions
def len(s) flux_strlen(s)
def at(s, i) flux_string_at(s, i)
def cmp(a, b) flux_strcmp(a, b)
def slice(s, start, cnt) flux_string_slice(s, start, cnt)
def find(s, sub) flux_string_find(s, sub)
def regex(s, pat) flux_regex_match(s, pat)
