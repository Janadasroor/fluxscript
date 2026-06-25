// FluxScript bootstrapped compiler — lexer + single-pass codegen.
// Syntax notes: if (cond) { block }   while cond do { block }   for i in range do { block }
//               extern def name(args) -> Type

enum TokenType {
    Number, StringLiteral, Identifier,
    KwDef, KwDefault, KwExtern, KwLet, KwVar, KwIf, KwElse, KwMatch, KwFor, KwWhile,
    KwReturn, KwSpawn, KwJoin, KwStruct, KwEnum, KwImpl, KwClass, KwTrait, KwAsync, KwAwait, KwBreak, KwContinue, KwFn,
    KwTrue, KwFalse, KwIn, KwEnd, KwDo, KwImport, KwModule, KwPub,
    KwDouble, KwString, KwBool, KwVoid, KwInt, KwVector,
    Plus, Minus, Star, Slash, Percent,
    Eq, Neq, Lt, Gt, Leq, Geq, LogicalAnd, LogicalOr,
    Not, Assign, Arrow, FatArrow, Pipe, DotDot, Dot,
    Colon, Semicolon, Comma, Hash, At,
    LParen, RParen, LBrace, RBrace, LBracket, RBracket,
    Pipe2, Amp, Tilde, Caret, Question,
    Lifetime, Error, Eof
}

struct Token {
    toktype: TokenType
    value: String
    line: Double
    col: Double
}

// ---- LLVM + utility functions (externs registered in C++ runtime) ----
// All LLVM C-API, HashMap, and string utility functions are available
// at JIT time via registerRuntimeFunctions() and regExtern().
// No extern def declarations needed — the C++ parser auto-declares them.

// ---- Char helpers ----
def is_digit(c: Double) -> Double {
    if (c >= 48.0 && c <= 57.0) { 1.0 } else { 0.0 }
}
def is_alpha(c: Double) -> Double {
    if ((c >= 65.0 && c <= 90.0) || (c >= 97.0 && c <= 122.0) || c == 95.0) { 1.0 } else { 0.0 }
}
def is_alnum(c: Double) -> Double {
    if (is_digit(c) == 1.0 || is_alpha(c) == 1.0) { 1.0 } else { 0.0 }
}

// ---- Keyword dispatch ----
def keyword_kind(name: String) -> TokenType {
    let n = flux_str_len(name);
    if (n < 2.0) { return TokenType.Identifier; }
    let c0 = flux_str_at(name, 0.0);
    if (c0 == 83.0) {
        if (name == "String") { return TokenType.KwString; }
        return TokenType.Identifier;
    }
    if (c0 == 97.0) {
        if (name == "async") { return TokenType.KwAsync; }
        if (name == "await") { return TokenType.KwAwait; }
        return TokenType.Identifier;
    }
    if (c0 == 98.0) {
        if (name == "bool") { return TokenType.KwBool; }
        if (name == "break") { return TokenType.KwBreak; }
        return TokenType.Identifier;
    }
    if (c0 == 99.0) {
        if (name == "class") { return TokenType.KwClass; }
        if (name == "continue") { return TokenType.KwContinue; }
        return TokenType.Identifier;
    }
    if (c0 == 100.0) {
        if (name == "def") { return TokenType.KwDef; }
        if (name == "default") { return TokenType.KwDefault; }
        if (name == "do") { return TokenType.KwDo; }
        if (name == "double") { return TokenType.KwDouble; }
        return TokenType.Identifier;
    }
    if (c0 == 101.0) {
        if (name == "else") { return TokenType.KwElse; }
        if (name == "end") { return TokenType.KwEnd; }
        if (name == "enum") { return TokenType.KwEnum; }
        if (name == "extern") { return TokenType.KwExtern; }
        return TokenType.Identifier;
    }
    if (c0 == 102.0) {
        if (name == "false") { return TokenType.KwFalse; }
        if (name == "fn") { return TokenType.KwFn; }
        if (name == "for") { return TokenType.KwFor; }
        return TokenType.Identifier;
    }
    if (c0 == 105.0) {
        if (name == "if") { return TokenType.KwIf; }
        if (name == "impl") { return TokenType.KwImpl; }
        if (name == "import") { return TokenType.KwImport; }
        if (name == "in") { return TokenType.KwIn; }
        if (name == "int") { return TokenType.KwInt; }
        return TokenType.Identifier;
    }
    if (c0 == 106.0) { if (name == "join") { return TokenType.KwJoin; } return TokenType.Identifier; }
    if (c0 == 108.0) { if (name == "let") { return TokenType.KwLet; } return TokenType.Identifier; }
    if (c0 == 109.0) {
        if (name == "match") { return TokenType.KwMatch; }
        if (name == "module") { return TokenType.KwModule; }
        return TokenType.Identifier;
    }
    if (c0 == 112.0) { if (name == "pub") { return TokenType.KwPub; } return TokenType.Identifier; }
    if (c0 == 114.0) { if (name == "return") { return TokenType.KwReturn; } return TokenType.Identifier; }
    if (c0 == 115.0) {
        if (name == "spawn") { return TokenType.KwSpawn; }
        if (name == "struct") { return TokenType.KwStruct; }
        if (name == "string") { return TokenType.KwString; }
        return TokenType.Identifier;
    }
    if (c0 == 116.0) {
        if (name == "trait") { return TokenType.KwTrait; }
        if (name == "true") { return TokenType.KwTrue; }
        return TokenType.Identifier;
    }
    if (c0 == 118.0) {
        if (name == "var") { return TokenType.KwVar; }
        if (name == "vector") { return TokenType.KwVector; }
        if (name == "void") { return TokenType.KwVoid; }
        return TokenType.Identifier;
    }
    if (c0 == 119.0) { if (name == "while") { return TokenType.KwWhile; } return TokenType.Identifier; }
    TokenType.Identifier
}

// =============================================================================
// COMPILER CLASS
// =============================================================================

struct Compiler {
    source: String
    cursor: Double
    src_len: Double
    line: Double
    col: Double
    tok_type: TokenType
    tok_val: String
    ctx: Double
    mod: Double
    bld: Double
    globals: Double
    structs: Double
    locals: Double
    struct_fields: Double
    cur_fn: Double
    entry_bb: Double
    had_error: Double
    func_types: Double
    types: Double
    last_type: Double
    enum_types: Double
    enum_variants: Double
    enum_payloads: Double
    last_enum_name: String
    last_varname: String
    last_trait_name: String
    var_enum_names: Double
    var_type_names: Double
    var_trait_names: Double
    trait_method_counts: Double
    trait_method_indices: Double
    trait_impls: Double
    trait_vtables: Double
    trait_assoc_type_counts: Double
    trait_assoc_type_names: Double
    lambda_counter: Double
    loop_end: Double
    loop_cont: Double
    async_state_alloca: Double
    async_result_alloca: Double
    async_state_struct_ty: Double
    async_dispatcher_bb: Double
    async_resume_targets: Double
    async_await_count: Double
    last_addr: Double
    retval_store: Double
}

// ---- Type helpers ----
def t_double(ctx: Double) -> Double { flux_llvm_double_type_in_ctx(ctx) }
def t_int1(ctx: Double) -> Double { flux_llvm_int1_type_in_ctx(ctx) }
def t_int8(ctx: Double) -> Double { flux_llvm_int8_type_in_ctx(ctx) }
def t_int32(ctx: Double) -> Double { flux_llvm_int32_type_in_ctx(ctx) }
def t_ptr(ctx: Double) -> Double { flux_llvm_pointer_type_in_ctx(ctx, 0.0) }
def t_void(ctx: Double) -> Double { flux_llvm_void_type_in_ctx(ctx) }
def t_int64(ctx: Double) -> Double { flux_llvm_int64_type_in_ctx(ctx) }

impl Compiler {
def compiler_init(self: Compiler) {
    self.ctx = flux_llvm_context_create();
    self.mod = flux_llvm_module_create_with_name_in_ctx("fluxc", self.ctx);
    self.bld = flux_llvm_create_builder_in_ctx(self.ctx);
    self.globals = flux_hm_create();
    self.structs = flux_hm_create();
    self.locals = flux_hm_create();
    self.struct_fields = flux_hm_create();
    self.func_types = flux_hm_create();
    self.types = flux_hm_create();
    self.enum_types = flux_hm_create();
    self.enum_variants = flux_hm_create();
    self.enum_payloads = flux_hm_create();
    self.var_enum_names = flux_hm_create();
    self.var_type_names = flux_hm_create();
    self.var_trait_names = flux_hm_create();
    self.trait_method_counts = flux_hm_create();
    self.trait_method_indices = flux_hm_create();
    self.trait_impls = flux_hm_create();
    self.trait_vtables = flux_hm_create();
    self.trait_assoc_type_counts = flux_hm_create();
    self.trait_assoc_type_names = flux_hm_create();
    self.last_type = 0.0;
    self.last_enum_name = "";
    self.last_trait_name = "";
    self.cur_fn = 0.0;
    self.entry_bb = 0.0;
    self.had_error = 0.0;
    self.lambda_counter = 0.0;
    self.loop_end = 0.0;
    self.loop_cont = 0.0;
    self.async_state_alloca = 0.0;
    self.async_result_alloca = 0.0;
    self.async_state_struct_ty = 0.0;
    self.async_dispatcher_bb = 0.0;
    self.async_resume_targets = flux_hm_create();
    self.async_await_count = 0.0;
    self.last_addr = 0.0;
    self.retval_store = 0.0;
    // Pre-declare string runtime helpers
    flux_llvm_type_arg_reset();
    flux_llvm_type_arg_push(t_double(self.ctx));
    flux_llvm_type_arg_push(t_double(self.ctx));
    let str_bin_ft = flux_llvm_function_type(t_double(self.ctx), 2.0, 0.0);
    let sc = flux_llvm_add_function(self.mod, "flux_str_concat", str_bin_ft);
    flux_hm_put(self.globals, "flux_str_concat", sc);
    flux_hm_put(self.func_types, "flux_str_concat", str_bin_ft);
    let scmp = flux_llvm_add_function(self.mod, "flux_strcmp", str_bin_ft);
    flux_hm_put(self.globals, "flux_strcmp", scmp);
    flux_hm_put(self.func_types, "flux_strcmp", str_bin_ft);
    // Pre-declare dyn ptr runtime helpers
    flux_llvm_type_arg_reset();
    flux_llvm_type_arg_push(t_double(self.ctx));
    flux_llvm_type_arg_push(t_double(self.ctx));
    let dpp_ft = flux_llvm_function_type(t_double(self.ctx), 2.0, 0.0);
    let dpp = flux_llvm_add_function(self.mod, "flux_dyn_ptr_push", dpp_ft);
    flux_hm_put(self.globals, "flux_dyn_ptr_push", dpp);
    flux_hm_put(self.func_types, "flux_dyn_ptr_push", dpp_ft);
    flux_llvm_type_arg_reset();
    flux_llvm_type_arg_push(t_double(self.ctx));
    let dp1_ft = flux_llvm_function_type(t_double(self.ctx), 1.0, 0.0);
    let dpgd = flux_llvm_add_function(self.mod, "flux_dyn_ptr_get_data", dp1_ft);
    flux_hm_put(self.globals, "flux_dyn_ptr_get_data", dpgd);
    flux_hm_put(self.func_types, "flux_dyn_ptr_get_data", dp1_ft);
    let dpgv = flux_llvm_add_function(self.mod, "flux_dyn_ptr_get_vtable", dp1_ft);
    flux_hm_put(self.globals, "flux_dyn_ptr_get_vtable", dpgv);
    flux_hm_put(self.func_types, "flux_dyn_ptr_get_vtable", dp1_ft);
    // Pre-declare common runtime helpers (unary double->double)
    let sl = flux_llvm_add_function(self.mod, "flux_str_len", dp1_ft);
    flux_hm_put(self.globals, "flux_str_len", sl);
    flux_hm_put(self.func_types, "flux_str_len", dp1_ft);
    let ps = flux_llvm_add_function(self.mod, "flux_print_string", dp1_ft);
    flux_hm_put(self.globals, "flux_print_string", ps);
    flux_hm_put(self.func_types, "flux_print_string", dp1_ft);
    let pn = flux_llvm_add_function(self.mod, "flux_parse_number", dp1_ft);
    flux_hm_put(self.globals, "flux_parse_number", pn);
    flux_hm_put(self.func_types, "flux_parse_number", dp1_ft);
    let rf = flux_llvm_add_function(self.mod, "flux_read_file", dp1_ft);
    flux_hm_put(self.globals, "flux_read_file", rf);
    flux_hm_put(self.func_types, "flux_read_file", dp1_ft);
    let ge = flux_llvm_add_function(self.mod, "flux_get_env", dp1_ft);
    flux_hm_put(self.globals, "flux_get_env", ge);
    flux_hm_put(self.func_types, "flux_get_env", dp1_ft);
    let dtoa = flux_llvm_add_function(self.mod, "flux_dtoa", dp1_ft);
    flux_hm_put(self.globals, "flux_dtoa", dtoa);
    flux_hm_put(self.func_types, "flux_dtoa", dp1_ft);
    // Pre-declare ternary runtime helpers (double,double,double -> double)
    flux_llvm_type_arg_reset();
    flux_llvm_type_arg_push(t_double(self.ctx));
    flux_llvm_type_arg_push(t_double(self.ctx));
    flux_llvm_type_arg_push(t_double(self.ctx));
    let str_ter_ft = flux_llvm_function_type(t_double(self.ctx), 3.0, 0.0);
    let ss = flux_llvm_add_function(self.mod, "flux_str_slice", str_ter_ft);
    flux_hm_put(self.globals, "flux_str_slice", ss);
    flux_hm_put(self.func_types, "flux_str_slice", str_ter_ft);
    // Pre-declare string at (double,double -> double) — reuse str_bin_ft
    let sa = flux_llvm_add_function(self.mod, "flux_str_at", str_bin_ft);
    flux_hm_put(self.globals, "flux_str_at", sa);
    flux_hm_put(self.func_types, "flux_str_at", str_bin_ft);
}

def set_src(self: Compiler, src: String) {
    self.source = src;
    self.cursor = 0.0;
    self.src_len = flux_str_len(src);
    self.line = 1.0;
    self.col = 1.0;
    self.lex_next();
}

def destroy(self: Compiler) {
    flux_llvm_dispose_builder(self.bld);
    flux_llvm_dispose_module(self.mod);
    flux_llvm_context_dispose(self.ctx);
}

def get_ir(self: Compiler) -> String {
    flux_llvm_print_module_to_string(self.mod)
}

// ===================================================================
// LEXER
// ===================================================================

def lex_end(self: Compiler) -> Double {
    if (self.cursor >= self.src_len) { 1.0 } else { 0.0 }
}

def lex_adv(self: Compiler) -> Double {
    let ch = flux_str_at(self.source, self.cursor);
    self.cursor = self.cursor + 1.0;
    self.col = self.col + 1.0;
    ch
}

def lex_peek(self: Compiler) -> Double {
    if (self.lex_end() == 1.0) { -1.0 } else { flux_str_at(self.source, self.cursor) }
}

def lex_peek2(self: Compiler) -> Double {
    let n = self.cursor + 1.0;
    if (n >= self.src_len) { -1.0 } else { flux_str_at(self.source, n) }
}

def lex_skip_ws(self: Compiler) {
    let done = 0.0;
    while (done == 0.0) {
        if (self.lex_end() == 1.0) { done = 1.0; }
        else {
            let ch = self.lex_peek();
            if (ch == 32.0 || ch == 9.0) { self.lex_adv(); }
            else if (ch == 10.0) {
                self.lex_adv();
                self.line = self.line + 1.0;
                self.col = 1.0;
            }
            else if (ch == 13.0) {
                self.lex_adv();
                if (self.lex_peek() == 10.0) { self.lex_adv(); }
                self.line = self.line + 1.0;
                self.col = 1.0;
            }
            else if (ch == 47.0 && self.lex_peek2() == 47.0) {
                while (self.lex_end() == 0.0 && self.lex_peek() != 10.0) { self.lex_adv(); }
            }
            else { done = 1.0; }
        }
    }
}

def lex_next(self: Compiler) {
    self.lex_skip_ws();
    if (self.lex_end() == 1.0) {
        self.tok_type = TokenType.Eof;
        self.tok_val = "";
        return 0.0;
    }
    let start = self.cursor;
    let ch = self.lex_adv();

    // Single-char delimiters
    if (ch == 40.0) { self.tok_type = TokenType.LParen; self.tok_val = "("; return 0.0; }
    if (ch == 41.0) { self.tok_type = TokenType.RParen; self.tok_val = ")"; return 0.0; }
    if (ch == 123.0) { self.tok_type = TokenType.LBrace; self.tok_val = "{"; return 0.0; }
    if (ch == 125.0) { self.tok_type = TokenType.RBrace; self.tok_val = "}"; return 0.0; }
    if (ch == 91.0) { self.tok_type = TokenType.LBracket; self.tok_val = "["; return 0.0; }
    if (ch == 93.0) { self.tok_type = TokenType.RBracket; self.tok_val = "]"; return 0.0; }
    if (ch == 44.0) { self.tok_type = TokenType.Comma; self.tok_val = ","; return 0.0; }
    if (ch == 59.0) { self.tok_type = TokenType.Semicolon; self.tok_val = ";"; return 0.0; }
    if (ch == 63.0) { self.tok_type = TokenType.Question; self.tok_val = "?"; return 0.0; }
    if (ch == 64.0) { self.tok_type = TokenType.At; self.tok_val = "@"; return 0.0; }
    if (ch == 35.0) { self.tok_type = TokenType.Hash; self.tok_val = "#"; return 0.0; }
    if (ch == 126.0) { self.tok_type = TokenType.Tilde; self.tok_val = "~"; return 0.0; }
    if (ch == 94.0) { self.tok_type = TokenType.Caret; self.tok_val = "^"; return 0.0; }
    if (ch == 37.0) { self.tok_type = TokenType.Percent; self.tok_val = "%"; return 0.0; }

    // Number
    if (is_digit(ch) == 1.0) {
        while (self.lex_end() == 0.0 && is_digit(self.lex_peek()) == 1.0) { self.lex_adv(); }
        if (self.lex_peek() == 46.0 && is_digit(self.lex_peek2()) == 1.0) {
            self.lex_adv();
            while (self.lex_end() == 0.0 && is_digit(self.lex_peek()) == 1.0) { self.lex_adv(); }
        }
        self.tok_type = TokenType.Number;
        self.tok_val = flux_str_slice(self.source, start, self.cursor);
        return 0.0;
    }

    // String literal
    if (ch == 34.0) {
        let result = "";
        while (self.lex_end() == 0.0) {
            let c2 = self.lex_adv();
            if (c2 == 34.0) {
                self.tok_type = TokenType.StringLiteral;
                self.tok_val = result;
                return 0.0;
            }
            if (c2 == 92.0) {
                let esc = self.lex_adv();
                if (esc == 110.0) { result = result + flux_str_from_char(10.0); }
                else if (esc == 116.0) { result = result + flux_str_from_char(9.0); }
                else if (esc == 114.0) { result = result + flux_str_from_char(13.0); }
                else if (esc == 92.0) { result = result + flux_str_from_char(92.0); }
                else if (esc == 34.0) { result = result + flux_str_from_char(34.0); }
                else if (esc == 48.0) { result = result + flux_str_from_char(0.0); }
                else { result = result + flux_str_from_char(esc); }
            }
            else { result = result + flux_str_from_char(c2); }
        }
        self.tok_type = TokenType.StringLiteral;
        self.tok_val = result;
        return 0.0;
    }

    // Identifier or keyword
    if (is_alpha(ch) == 1.0) {
        while (self.lex_end() == 0.0 && is_alnum(self.lex_peek()) == 1.0) { self.lex_adv(); }
        let name = flux_str_slice(self.source, start, self.cursor);
        self.tok_type = keyword_kind(name);
        self.tok_val = name;
        return 0.0;
    }

    // Lifetime 'name
    if (ch == 39.0) {
        while (self.lex_end() == 0.0 && is_alnum(self.lex_peek()) == 1.0) { self.lex_adv(); }
        self.tok_type = TokenType.Lifetime;
        self.tok_val = flux_str_slice(self.source, start + 1.0, self.cursor);
        return 0.0;
    }

    // Two-char operators
    if (ch == 33.0) {  // !
        if (self.lex_peek() == 61.0) { self.lex_adv(); self.tok_type = TokenType.Neq; self.tok_val = "!="; return 0.0; }
        self.tok_type = TokenType.Not; self.tok_val = "!"; return 0.0;
    }
    if (ch == 61.0) {  // =
        if (self.lex_peek() == 61.0) { self.lex_adv(); self.tok_type = TokenType.Eq; self.tok_val = "=="; return 0.0; }
        self.tok_type = TokenType.Assign; self.tok_val = "="; return 0.0;
    }
    if (ch == 62.0) {  // >
        if (self.lex_peek() == 61.0) { self.lex_adv(); self.tok_type = TokenType.Geq; self.tok_val = ">="; return 0.0; }
        self.tok_type = TokenType.Gt; self.tok_val = ">"; return 0.0;
    }
    if (ch == 60.0) {  // <
        if (self.lex_peek() == 61.0) { self.lex_adv(); self.tok_type = TokenType.Leq; self.tok_val = "<="; return 0.0; }
        self.tok_type = TokenType.Lt; self.tok_val = "<"; return 0.0;
    }
    if (ch == 38.0) {  // &
        if (self.lex_peek() == 38.0) { self.lex_adv(); self.tok_type = TokenType.LogicalAnd; self.tok_val = "&&"; return 0.0; }
        self.tok_type = TokenType.Amp; self.tok_val = "&"; return 0.0;
    }
    if (ch == 124.0) {  // |
        if (self.lex_peek() == 124.0) { self.lex_adv(); self.tok_type = TokenType.LogicalOr; self.tok_val = "||"; return 0.0; }
        if (self.lex_peek() == 62.0) { self.lex_adv(); self.tok_type = TokenType.Pipe; self.tok_val = "|>"; return 0.0; }
        self.tok_type = TokenType.Pipe2; self.tok_val = "|"; return 0.0;
    }
    if (ch == 45.0 && self.lex_peek() == 62.0) { self.lex_adv(); self.tok_type = TokenType.Arrow; self.tok_val = "->"; return 0.0; }
    if (ch == 61.0 && self.lex_peek() == 62.0) { self.lex_adv(); self.tok_type = TokenType.FatArrow; self.tok_val = "=>"; return 0.0; }
    if (ch == 46.0 && self.lex_peek() == 46.0) { self.lex_adv(); self.tok_type = TokenType.DotDot; self.tok_val = ".."; return 0.0; }

    // Single-char operators
    if (ch == 43.0) { self.tok_type = TokenType.Plus; self.tok_val = "+"; return 0.0; }
    if (ch == 45.0) { self.tok_type = TokenType.Minus; self.tok_val = "-"; return 0.0; }
    if (ch == 42.0) { self.tok_type = TokenType.Star; self.tok_val = "*"; return 0.0; }
    if (ch == 47.0) { self.tok_type = TokenType.Slash; self.tok_val = "/"; return 0.0; }
    if (ch == 58.0) { self.tok_type = TokenType.Colon; self.tok_val = ":"; return 0.0; }
    if (ch == 46.0) { self.tok_type = TokenType.Dot; self.tok_val = "."; return 0.0; }

    self.tok_type = TokenType.Error;
    self.tok_val = "?";
}

// ===================================================================
// PARSER HELPERS
// ===================================================================

def parse_err(self: Compiler, msg: String) {
    flux_print_string("[err] ");
    flux_print_string(msg);
    self.had_error = 1.0;
}

def tok_is(self: Compiler, tt: TokenType) -> Double {
    if (self.tok_type == tt) { 1.0 } else { 0.0 }
}

def tok_match(self: Compiler, tt: TokenType) -> Double {
    if (self.tok_type == tt) { self.lex_next(); 1.0 } else { 0.0 }
}

def tok_expect(self: Compiler, tt: TokenType, msg: String) {
    if (self.tok_type == tt) { self.lex_next(); return 0.0; }
    self.parse_err(msg);
}

// ===================================================================
// EXPRESSION PARSING
// ===================================================================

def get_prec(self: Compiler) -> Double {
    if (self.tok_type == TokenType.Pipe) { 1.0 }
    else if (self.tok_type == TokenType.LogicalOr) { 5.0 }
    else if (self.tok_type == TokenType.LogicalAnd) { 10.0 }
    else if (self.tok_type == TokenType.Eq || self.tok_type == TokenType.Neq) { 15.0 }
    else if (self.tok_type == TokenType.Lt || self.tok_type == TokenType.Gt
             || self.tok_type == TokenType.Leq || self.tok_type == TokenType.Geq) { 20.0 }
    else if (self.tok_type == TokenType.Plus || self.tok_type == TokenType.Minus) { 30.0 }
    else if (self.tok_type == TokenType.Star || self.tok_type == TokenType.Slash
             || self.tok_type == TokenType.Percent) { 40.0 }
    else { 0.0 }
}

def parse_expr(self: Compiler) -> Double {
    self.parse_expr_prec(0.0)
}

def parse_expr_prec(self: Compiler, min_prec: Double) -> Double {
    let left = self.parse_unary();
    let left_type = self.last_type;
    let cont = 1.0;
    while (cont == 1.0) {
        let prec = self.get_prec();
        if (prec <= min_prec) { cont = 0.0; }
        else {
            let tt = self.tok_type;
            let op_str = self.tok_val;
            self.lex_next();
            if (tt == TokenType.Pipe) {
                // Pipe operator: left |> right
                // right can be:
                //   fn_name           => fn(left)
                //   fn_name(args)      => fn(left, args...)
                //   obj.meth           => type_meth(obj, left)
                //   obj.meth(args)     => type_meth(obj, left, args...)
                if (self.tok_type == TokenType.Identifier) {
                    let name = self.tok_val;
                    self.lex_next();
                    if (self.tok_type == TokenType.Dot) {
                        // x |> obj.method or x |> obj.method(args)
                        self.lex_next(); // eat '.'
                        let obj_name = name;
                        let memb = self.tok_val;
                        self.tok_expect(TokenType.Identifier, "expected method name");
                        // Look up type of obj
                        let type_name = "";
                        if (flux_hm_has(self.var_type_names, obj_name) == 1.0) {
                            type_name = flux_hm_get(self.var_type_names, obj_name);
                        }
                        if (type_name != "") {
                            let fn_name = type_name + "_" + memb;
                            let fn_val = flux_llvm_get_named_function(self.mod, fn_name);
                            if (fn_val != 0.0) {
                                flux_hm_put(self.globals, fn_name, fn_val);
                                let ft = flux_llvm_global_get_value_type(fn_val);
                                flux_hm_put(self.func_types, fn_name, ft);
                                // Get obj value from variable
                                let obj_val = 0.0;
                                if (flux_hm_has(self.locals, obj_name) == 1.0) {
                                    let addr = flux_hm_get(self.locals, obj_name);
                                    obj_val = flux_llvm_build_load2(self.bld, t_double(self.ctx), addr, obj_name);
                                }
                                if (self.tok_type == TokenType.LParen) {
                                    // x |> obj.method(args)
                                    self.lex_next(); // eat '('
                                    flux_llvm_call_arg_reset();
                                    flux_llvm_call_arg_push(obj_val);
                                    flux_llvm_call_arg_push(left);
                                    let count = 2.0;
                                    if (self.tok_type != TokenType.RParen) {
                                        let saved = flux_llvm_call_arg_save();
                                        let a = self.parse_expr();
                                        flux_llvm_call_arg_restore(saved);
                                        flux_llvm_call_arg_push(a);
                                        count = count + 1.0;
                                        while (self.tok_type == TokenType.Comma) {
                                            self.lex_next();
                                            let saved2 = flux_llvm_call_arg_save();
                                            let a2 = self.parse_expr();
                                            flux_llvm_call_arg_restore(saved2);
                                            flux_llvm_call_arg_push(a2);
                                            count = count + 1.0;
                                        }
                                    }
    self.tok_expect(TokenType.RParen, "expected ')'");
                                    flux_llvm_type_arg_reset();
                                    let arg_i = 0.0;
                                    while (arg_i < count) {
                                        flux_llvm_type_arg_push(t_double(self.ctx));
                                        arg_i = arg_i + 1.0;
                                    }
                                    let ft2 = flux_llvm_function_type(t_double(self.ctx), count, 0.0);
                                    left = flux_llvm_build_call2(self.bld, ft2, fn_val, count, "pipetmp");
                                } else {
                                    // x |> obj.method
                                    flux_llvm_call_arg_reset();
                                    flux_llvm_call_arg_push(obj_val);
                                    flux_llvm_call_arg_push(left);
                                    flux_llvm_type_arg_reset();
                                    flux_llvm_type_arg_push(t_double(self.ctx));
                                    flux_llvm_type_arg_push(t_double(self.ctx));
                                    let ft2 = flux_llvm_function_type(t_double(self.ctx), 2.0, 0.0);
                                    left = flux_llvm_build_call2(self.bld, ft2, fn_val, 2.0, "pipetmp");
                                }
                                left_type = 1.0;
                            } else {
                                self.parse_err("unknown method: " + fn_name);
                            }
                        } else {
                            self.parse_err("unknown type for: " + obj_name);
                        }
                    }
                    else if (self.tok_type == TokenType.LParen) {
                        // x |> f(y) — call with LHS prepended
                        self.lex_next(); // eat '('
                        flux_llvm_call_arg_reset();
                        flux_llvm_call_arg_push(left);
                        let count = 1.0;
                        if (self.tok_type != TokenType.RParen) {
                            let saved = flux_llvm_call_arg_save();
                            let a = self.parse_expr();
                            flux_llvm_call_arg_restore(saved);
                            flux_llvm_call_arg_push(a);
                            count = count + 1.0;
                            while (self.tok_type == TokenType.Comma) {
                                self.lex_next();
                                let saved2 = flux_llvm_call_arg_save();
                                let a2 = self.parse_expr();
                                flux_llvm_call_arg_restore(saved2);
                                flux_llvm_call_arg_push(a2);
                                count = count + 1.0;
                            }
                        }
                        self.tok_expect(TokenType.RParen, "expected ')'");
                        let fn_val = flux_llvm_get_named_function(self.mod, name);
                        if (fn_val != 0.0) {
                            flux_hm_put(self.globals, name, fn_val);
                            let ft = flux_llvm_global_get_value_type(fn_val);
                            flux_hm_put(self.func_types, name, ft);
                            flux_llvm_type_arg_reset();
                            let arg_i = 0.0;
                            while (arg_i < count) {
                                flux_llvm_type_arg_push(t_double(self.ctx));
                                arg_i = arg_i + 1.0;
                            }
                            let ft2 = flux_llvm_function_type(t_double(self.ctx), count, 0.0);
                            left = flux_llvm_build_call2(self.bld, ft2, fn_val, count, "pipetmp");
                            left_type = 1.0;
                        } else {
                            self.parse_err("unknown function: " + name);
                        }
                    } else {
                        // x |> f — simple function call
                        let fn_val = flux_llvm_get_named_function(self.mod, name);
                        if (fn_val != 0.0) {
                            flux_hm_put(self.globals, name, fn_val);
                            let ft = flux_llvm_global_get_value_type(fn_val);
                            flux_hm_put(self.func_types, name, ft);
                            flux_llvm_call_arg_reset();
                            flux_llvm_call_arg_push(left);
                            flux_llvm_type_arg_reset();
                            flux_llvm_type_arg_push(t_double(self.ctx));
                            let ft2 = flux_llvm_function_type(t_double(self.ctx), 1.0, 0.0);
                            left = flux_llvm_build_call2(self.bld, ft2, fn_val, 1.0, "pipetmp");
                            left_type = 1.0;
                        } else {
                            self.parse_err("unknown function: " + name);
                        }
                    }
                } else {
                    self.parse_err("expected function after |>");
                }
            }
            else {
                let left_varname = self.last_varname;
                let right = self.parse_expr_prec(prec + 1.0);
                let right_type = self.last_type;
                self.last_type = left_type;
                left = self.emit_binop(tt, op_str, left, left_type, right, right_type, left_varname);
                left_type = self.last_type;
            }
        }
    }
    self.last_type = left_type;
    left
}

def parse_unary(self: Compiler) -> Double {
    if (self.tok_type == TokenType.Minus) {
        self.lex_next();
        let val = self.parse_unary();
        // Check for operator overloading: unary - → neg method
        let varname = self.last_varname;
        let type_name = "";
        if (flux_hm_has(self.structs, varname) == 1.0) {
            type_name = varname;
        }
        else if (flux_hm_has(self.var_type_names, varname) == 1.0) {
            let tn = flux_hm_get(self.var_type_names, varname);
            if (flux_hm_has(self.structs, tn) == 1.0) {
                type_name = tn;
            }
        }
        if (type_name != "") {
            let fn_name = type_name + "_neg";
            let neg_fn = flux_llvm_get_named_function(self.mod, fn_name);
            if (neg_fn != 0.0) {
                let neg_ft = flux_llvm_global_get_value_type(neg_fn);
                let saved = flux_llvm_call_arg_save();
                flux_llvm_call_arg_reset();
                flux_llvm_call_arg_push(val);
                let result = flux_llvm_build_call2(self.bld, neg_ft, neg_fn, 1.0, fn_name);
                flux_llvm_call_arg_restore(saved);
                self.last_type = 1.0;
                return result;
            }
        }
        self.last_type = 1.0;
        flux_llvm_build_fneg(self.bld, val, "negtmp")
    }
    else if (self.tok_type == TokenType.Not) {
        self.lex_next();
        let val = self.parse_unary();
        self.last_type = 1.0;
        let zero = flux_llvm_const_real(t_double(self.ctx), 0.0);
        let fcmp_res = flux_llvm_build_fcmp(self.bld, 1.0, zero, val, "lognot");
        flux_llvm_build_uitofp(self.bld, fcmp_res, t_double(self.ctx), "booltmp")
    }
    else if (self.tok_type == TokenType.Amp) {
        self.lex_next();
        if (self.tok_type == TokenType.Lifetime) { self.lex_next(); }
        if (self.tok_type == TokenType.Identifier && self.tok_val == "mut") { self.lex_next(); }
        let val = self.parse_unary();
        let addr = self.last_addr;
        if (addr == 0.0) {
            addr = flux_llvm_build_alloca(self.bld, t_double(self.ctx), "temp_borrow");
            flux_llvm_build_store(self.bld, val, addr);
        }
        let as_i64 = flux_llvm_build_ptrtoint(self.bld, addr, t_int64(self.ctx), "ptr_i64");
        let as_dbl = flux_llvm_build_bitcast(self.bld, as_i64, t_double(self.ctx), "ptr_dbl");
        self.last_type = 1.0;
        as_dbl
    }
    else if (self.tok_type == TokenType.Star) {
        self.lex_next();
        let val = self.parse_unary();
        let as_i64 = flux_llvm_build_bitcast(self.bld, val, t_int64(self.ctx), "ptr_i64");
        let ptr_val = flux_llvm_build_inttoptr(self.bld, as_i64, t_ptr(self.ctx), "ptrval");
        self.last_addr = ptr_val;
        let loaded = flux_llvm_build_load2(self.bld, t_double(self.ctx), ptr_val, "dereftmp");
        self.last_type = 1.0;
        loaded
    }
    else {
        self.parse_postfix()
    }
}

def parse_postfix(self: Compiler) -> Double {
    let is_ident = (self.tok_type == TokenType.Identifier);
    let ident_name = self.tok_val;
    self.last_addr = 0.0;
    let left = self.parse_primary();
    let cont = 1.0;
    while (cont == 1.0) {
        if (self.tok_type == TokenType.LParen) {
            self.lex_next();
            left = self.parse_call(left);
        }
        else if (self.tok_type == TokenType.LBracket) {
            self.lex_next();
            let idx = self.parse_expr();
            self.tok_expect(TokenType.RBracket, "expected ']'");
            left = self.emit_index(left, idx);
        }
        else if (self.tok_type == TokenType.Dot) {
            self.lex_next();
            if (self.tok_type == TokenType.Identifier) {
                let mname = self.tok_val;
                self.lex_next();
                        if (self.tok_type == TokenType.LParen && is_ident && (flux_hm_has(self.var_type_names, ident_name) == 1.0 || flux_hm_has(self.structs, ident_name) == 1.0)) {
                    self.lex_next(); // eat '('
                    left = self.parse_method_call(left, ident_name, mname);
                }
                else {
                    left = self.emit_member(left, mname);
                }
            }
            else { self.parse_err("expected member name"); }
        }
        else if (self.tok_type == TokenType.Question) {
            // expr? — early-return on Err/None (tagged-union propagation)
            self.lex_next(); // eat '?'
            left = self.emit_qmark_propagate(left);
        }
        else { cont = 0.0; }
    }
    // Handle assignment: x = expr or self.field = expr
    if (self.tok_type == TokenType.Assign) {
        self.lex_next();
        let assign_addr = self.last_addr;
        let val = self.parse_expr();
        if (assign_addr != 0.0) {
            flux_llvm_build_store(self.bld, val, assign_addr);
        }
        else if (is_ident && flux_hm_has(self.locals, ident_name) == 1.0) {
            let addr = flux_hm_get(self.locals, ident_name);
            flux_llvm_build_store(self.bld, val, addr);
        }
        self.last_type = 0.0;
        val
    }
    else {
        left
    }
}

def parse_primary(self: Compiler) -> Double {
    if (self.tok_type == TokenType.Number) {
        let text = self.tok_val;
        self.lex_next();
        let r = self.emit_number(text);
        r
    }
    else if (self.tok_type == TokenType.StringLiteral) {
        let text = self.tok_val;
        self.lex_next();
        self.emit_string(text)
    }
    else if (self.tok_type == TokenType.KwTrue) {
        self.lex_next();
        flux_llvm_const_real(t_double(self.ctx), 1.0)
    }
    else if (self.tok_type == TokenType.KwFalse) {
        self.lex_next();
        flux_llvm_const_real(t_double(self.ctx), 0.0)
    }
    else if (self.tok_type == TokenType.KwIf) {
        self.parse_if_stmt()
    }
    else if (self.tok_type == TokenType.KwReturn) {
        self.lex_next();
        let v = self.parse_expr();
        flux_llvm_build_ret(self.bld, v)
    }
    else if (self.tok_type == TokenType.KwLet) {
        self.parse_let()
    }
    else if (self.tok_type == TokenType.KwVar) {
        self.parse_var()
    }
    else if (self.tok_type == TokenType.KwFn) {
        self.parse_lambda()
    }
    else if (self.tok_type == TokenType.KwFor) {
        self.parse_for()
    }
    else if (self.tok_type == TokenType.KwWhile) {
        self.parse_while()
    }
    else if (self.tok_type == TokenType.KwSpawn) {
        self.parse_spawn()
    }
    else if (self.tok_type == TokenType.KwJoin) {
        self.parse_join()
    }
    else if (self.tok_type == TokenType.KwAwait) {
        self.parse_await()
    }
    else if (self.tok_type == TokenType.KwMatch) {
        self.parse_match()
    }
    else if (self.tok_type == TokenType.KwBreak) {
        self.lex_next();
        if (self.loop_end == 0.0) {
            self.parse_err("break outside loop");
            flux_llvm_const_real(t_double(self.ctx), 0.0)
        }
        else {
            flux_llvm_build_br(self.bld, self.loop_end);
            let dead = flux_llvm_append_basic_block(self.cur_fn, "dead");
            flux_llvm_position_builder_at_end(self.bld, dead);
            flux_llvm_const_real(t_double(self.ctx), 0.0)
        }
    }
    else if (self.tok_type == TokenType.KwContinue) {
        self.lex_next();
        if (self.loop_cont == 0.0) {
            self.parse_err("continue outside loop");
            flux_llvm_const_real(t_double(self.ctx), 0.0)
        }
        else {
            flux_llvm_build_br(self.bld, self.loop_cont);
            let dead = flux_llvm_append_basic_block(self.cur_fn, "dead");
            flux_llvm_position_builder_at_end(self.bld, dead);
            flux_llvm_const_real(t_double(self.ctx), 0.0)
        }
    }
    else if (self.tok_type == TokenType.LBrace) {
        self.parse_block()
    }
    else if (self.tok_type == TokenType.LBracket) {
        self.parse_vector()
    }
    else if (self.tok_type == TokenType.LParen) {
        self.lex_next();
        let e = self.parse_expr();
        self.tok_expect(TokenType.RParen, "expected ')'");
        e
    }
    else if (self.tok_type == TokenType.Identifier) {
        let name = self.tok_val;
        self.lex_next();
        self.emit_var(name)
    }
    else {
        self.parse_err("unexpected: " + self.tok_val);
        0.0
    }
}

// ===================================================================
// SPECIFIC PARSE FUNCTIONS
// ===================================================================

def parse_if_stmt(self: Compiler) -> Double {
    self.lex_next(); // eat 'if'
    self.tok_expect(TokenType.LParen, "expected '(' after if");
    let cond = self.parse_expr();
    let zero = flux_llvm_const_real(t_double(self.ctx), 0.0);
    let cond_i1 = flux_llvm_build_fcmp(self.bld, 6.0, cond, zero, "cond_i1");
    self.tok_expect(TokenType.RParen, "expected ')' after if condition");

    let then_bb = flux_llvm_append_basic_block(self.cur_fn, "then");
    let else_bb = flux_llvm_append_basic_block(self.cur_fn, "else");
    let merge_bb = flux_llvm_append_basic_block(self.cur_fn, "ifcont");

    flux_llvm_build_cond_br(self.bld, cond_i1, then_bb, else_bb);
    flux_llvm_position_builder_at_end(self.bld, then_bb);
    let then_val = self.parse_expr();
    let then_type = self.last_type;
    let after_then = flux_llvm_get_insert_block(self.bld);
    let then_terminated = flux_llvm_get_basic_block_terminator(after_then) != 0.0;
    if (then_terminated != 1.0) {
        flux_llvm_build_br(self.bld, merge_bb);
    }

    flux_llvm_position_builder_at_end(self.bld, else_bb);
    let else_val = flux_llvm_const_real(t_double(self.ctx), 0.0);
    if (self.tok_is(TokenType.KwElse) == 1.0) {
        self.lex_next();
        else_val = self.parse_expr();
    }
    let else_type = self.last_type;
    let after_else = flux_llvm_get_insert_block(self.bld);
    let else_terminated = flux_llvm_get_basic_block_terminator(after_else) != 0.0;
    if (else_terminated != 1.0) {
        flux_llvm_build_br(self.bld, merge_bb);
    }

    flux_llvm_position_builder_at_end(self.bld, merge_bb);
    if (then_terminated == 1.0 && else_terminated == 1.0) {
        // Both blocks terminated (e.g. via return) — merge block unreachable
        0.0
    }
    else {
        let phi = flux_llvm_build_phi(self.bld, t_double(self.ctx), "ifphi");
        if (then_terminated != 1.0) {
            flux_llvm_add_incoming(phi, then_val, after_then);
        }
        if (else_terminated != 1.0) {
            flux_llvm_add_incoming(phi, else_val, after_else);
        }
        if (then_type == else_type) { self.last_type = then_type; }
        else { self.last_type = 0.0; }
        phi
    }
}

def parse_block(self: Compiler) -> Double {
    self.lex_next(); // eat '{'
    let result = 0.0;
    while (self.tok_type != TokenType.RBrace && self.tok_type != TokenType.Eof) {
        self.had_error = 0.0;
        result = self.parse_expr();
        while (self.tok_type == TokenType.Semicolon) { self.lex_next(); }
        if (self.had_error == 1.0 && self.tok_type != TokenType.RBrace && self.tok_type != TokenType.Eof) {
            self.lex_next();
            self.had_error = 0.0;
        }
    }
    self.tok_expect(TokenType.RBrace, "expected '}'");
    result
}

def parse_spawn(self: Compiler) -> Double {
    self.lex_next(); // eat 'spawn'
    let callee_name = self.tok_val;
    self.tok_expect(TokenType.Identifier, "expected function name after spawn");
    self.tok_expect(TokenType.LParen, "expected '('");

    let saved = flux_llvm_call_arg_save();
    let arg_vals = flux_hm_create();
    let arg_count = 0.0;
    if (self.tok_type != TokenType.RParen) {
        let a = self.parse_expr();
        let aa = flux_llvm_build_alloca(self.bld, t_double(self.ctx), "sa0");
        flux_llvm_build_store(self.bld, a, aa);
        flux_hm_put(arg_vals, "0", aa);
        arg_count = arg_count + 1.0;
        while (self.tok_type == TokenType.Comma) {
            self.lex_next();
            let a2 = self.parse_expr();
            let aa2 = flux_llvm_build_alloca(self.bld, t_double(self.ctx), "sa" + flux_dtoa(arg_count));
            flux_llvm_build_store(self.bld, a2, aa2);
            flux_hm_put(arg_vals, flux_dtoa(arg_count), aa2);
            arg_count = arg_count + 1.0;
        }
    }
    self.tok_expect(TokenType.RParen, "expected ')'");

    let fn_val = flux_llvm_get_named_function(self.mod, callee_name);
    if (fn_val == 0.0) {
        flux_llvm_type_arg_reset();
        let ai = 0.0;
        while (ai < arg_count) { flux_llvm_type_arg_push(t_double(self.ctx)); ai = ai + 1.0; }
        let ft = flux_llvm_function_type(t_double(self.ctx), arg_count, 0.0);
        fn_val = flux_llvm_add_function(self.mod, callee_name, ft);
    }
    let fn_ptr = flux_llvm_build_bitcast(self.bld, fn_val, t_ptr(self.ctx), "spawn_fn");

    let arr_ty = flux_llvm_array_type(t_double(self.ctx), arg_count);
    let args_alloca = flux_llvm_build_alloca(self.bld, arr_ty, "spawn_args");

    let ai2 = 0.0;
    while (ai2 < arg_count) {
        let src_ptr = flux_hm_get(arg_vals, flux_dtoa(ai2));
        let arg_val = flux_llvm_build_load2(self.bld, t_double(self.ctx), src_ptr, "sargl");
        flux_llvm_index_arg_reset();
        let zero_v = flux_llvm_const_int(t_int32(self.ctx), 0.0, 0.0);
        flux_llvm_index_arg_push(zero_v);
        let idx_v = flux_llvm_const_int(t_int32(self.ctx), ai2, 0.0);
        flux_llvm_index_arg_push(idx_v);
        let ep = flux_llvm_build_gep2(self.bld, arr_ty, args_alloca, 2.0, "earg");
        flux_llvm_build_store(self.bld, arg_val, ep);
        ai2 = ai2 + 1.0;
    }
    flux_hm_destroy(arg_vals);

    let args_ptr = flux_llvm_build_bitcast(self.bld, args_alloca, t_ptr(self.ctx), "spawn_args_p");
    let nargs_val = flux_llvm_const_int(t_int64(self.ctx), arg_count, 0.0);

    let spawn_fn = flux_llvm_get_named_function(self.mod, "flux_spawn");
    let spawn_ft = 0.0;
    if (spawn_fn == 0.0) {
        flux_llvm_type_arg_reset();
        flux_llvm_type_arg_push(t_ptr(self.ctx));
        flux_llvm_type_arg_push(t_ptr(self.ctx));
        flux_llvm_type_arg_push(t_int64(self.ctx));
        spawn_ft = flux_llvm_function_type(t_double(self.ctx), 3.0, 0.0);
        spawn_fn = flux_llvm_add_function(self.mod, "flux_spawn", spawn_ft);
    }
    else {
        spawn_ft = flux_llvm_global_get_value_type(spawn_fn);
    }

    flux_llvm_call_arg_reset();
    flux_llvm_call_arg_push(fn_ptr);
    flux_llvm_call_arg_push(args_ptr);
    flux_llvm_call_arg_push(nargs_val);
    let result = flux_llvm_build_call2(self.bld, spawn_ft, spawn_fn, 3.0, "spawn_ret");
    flux_llvm_call_arg_restore(saved);
    self.last_type = 1.0;
    result
}

def parse_join(self: Compiler) -> Double {
    self.lex_next(); // eat 'join'
    self.tok_expect(TokenType.LParen, "expected '(' after join");
    let handle = self.parse_expr();
    self.tok_expect(TokenType.RParen, "expected ')' after join expression");

    let saved = flux_llvm_call_arg_save();
    let join_fn = flux_llvm_get_named_function(self.mod, "flux_join");
    let join_ft = 0.0;
    if (join_fn == 0.0) {
        flux_llvm_type_arg_reset();
        flux_llvm_type_arg_push(t_double(self.ctx));
        join_ft = flux_llvm_function_type(t_double(self.ctx), 1.0, 0.0);
        join_fn = flux_llvm_add_function(self.mod, "flux_join", join_ft);
    }
    else {
        join_ft = flux_llvm_global_get_value_type(join_fn);
    }

    flux_llvm_call_arg_reset();
    flux_llvm_call_arg_push(handle);
    let result = flux_llvm_build_call2(self.bld, join_ft, join_fn, 1.0, "join_ret");
    flux_llvm_call_arg_restore(saved);
    self.last_type = 1.0;
    result
}

def parse_let(self: Compiler) -> Double {
    self.lex_next(); // eat 'let'
    if (self.tok_type == TokenType.Identifier) {
        let name = self.tok_val;
        self.lex_next();
        let old_local = 0.0;
        let has_old_local = flux_hm_has(self.locals, name);
        if (has_old_local == 1.0) { old_local = flux_hm_get(self.locals, name); }
        let old_enum = 0.0;
        let has_old_enum = flux_hm_has(self.var_enum_names, name);
        if (has_old_enum == 1.0) { old_enum = flux_hm_get(self.var_enum_names, name); }
        let old_type_name = 0.0;
        let has_old_type_name = flux_hm_has(self.var_type_names, name);
        if (has_old_type_name == 1.0) { old_type_name = flux_hm_get(self.var_type_names, name); }
        let old_trait_name = 0.0;
        let has_old_trait_name = flux_hm_has(self.var_trait_names, name);
        if (has_old_trait_name == 1.0) { old_trait_name = flux_hm_get(self.var_trait_names, name); }
        let old_type_store = 0.0;
        let has_old_type_store = flux_type_has(name);
        if (has_old_type_store == 1.0) { old_type_store = flux_type_load(name); }
        let is_dyn = 0.0;
        let dyn_trait = "";
        if (self.tok_type == TokenType.Colon) {
            self.lex_next();
            let tname = "";
            if (self.tok_type == TokenType.Identifier) { tname = self.tok_val; }
            self.parse_type_hint();
            if (self.last_type == 5.0) {
                is_dyn = 1.0;
                dyn_trait = self.last_trait_name;
                flux_hm_put(self.var_type_names, name, dyn_trait);
                flux_hm_put(self.var_trait_names, name, dyn_trait);
            }
            else if (tname != "") { flux_hm_put(self.var_type_names, name, tname); }
        }
        self.tok_expect(TokenType.Assign, "expected '=' in let");
        let val = self.parse_expr();
        flux_type_store(name, self.last_type);
        if (self.last_type == 4.0) { flux_hm_put(self.var_enum_names, name, self.last_enum_name); }
        let alloca = flux_llvm_build_alloca(self.bld, t_double(self.ctx), name);
        flux_llvm_build_store(self.bld, val, alloca);
        if (is_dyn == 1.0) {
            let concrete_type = "";
            if (self.last_type == 2.0) { concrete_type = "String"; }
            else if (self.last_type == 1.0) {
                // Could be Double or a struct — check last_varname
                if (flux_hm_has(self.structs, self.last_varname) == 1.0) {
                    concrete_type = self.last_varname;
                }
                else if (flux_hm_has(self.var_type_names, self.last_varname) == 1.0) {
                    let tn = flux_hm_get(self.var_type_names, self.last_varname);
                    if (flux_hm_has(self.structs, tn) == 1.0) {
                        concrete_type = tn;
                    }
                }
                else {
                    concrete_type = "Double";
                }
            }
            else {
                let last_vname = self.last_varname;
                if (flux_hm_has(self.var_type_names, last_vname) == 1.0) {
                    concrete_type = flux_hm_get(self.var_type_names, last_vname);
                }
            }
            if (concrete_type != "") {
                let vtable_key = dyn_trait + "." + concrete_type;
                let vtable_gv = flux_hm_get(self.trait_vtables, vtable_key);
                if (vtable_gv != 0.0) {
                    let vtab_i64 = flux_llvm_build_ptrtoint(self.bld, vtable_gv, t_int64(self.ctx), "vtab_i64");
                    let vtab_dbl = flux_llvm_build_bitcast(self.bld, vtab_i64, t_double(self.ctx), "vtab_dbl");
                    let dpp_fn = flux_hm_get(self.globals, "flux_dyn_ptr_push");
                    let dpp_ft = flux_hm_get(self.func_types, "flux_dyn_ptr_push");
                    flux_llvm_call_arg_reset();
                    flux_llvm_call_arg_push(val);
                    flux_llvm_call_arg_push(vtab_dbl);
                    let idx = flux_llvm_build_call2(self.bld, dpp_ft, dpp_fn, 2.0, "dynidx");
                    flux_llvm_build_store(self.bld, idx, alloca);
                    self.last_type = 5.0;
                }
            }
        }
        if (flux_hm_has(self.var_type_names, name) != 1.0 && flux_hm_has(self.structs, self.last_varname) == 1.0) {
            flux_hm_put(self.var_type_names, name, self.last_varname);
        }
        flux_hm_put(self.locals, name, alloca);
        if (self.tok_type == TokenType.KwIn) {
            self.lex_next();
            let body = self.parse_expr();
            if (has_old_local == 1.0) { flux_hm_put(self.locals, name, old_local); }
            else { flux_hm_remove(self.locals, name); }
            if (has_old_enum == 1.0) { flux_hm_put(self.var_enum_names, name, old_enum); }
            else { flux_hm_remove(self.var_enum_names, name); }
            if (has_old_type_name == 1.0) { flux_hm_put(self.var_type_names, name, old_type_name); }
            else { flux_hm_remove(self.var_type_names, name); }
            if (has_old_trait_name == 1.0) { flux_hm_put(self.var_trait_names, name, old_trait_name); }
            else { flux_hm_remove(self.var_trait_names, name); }
            if (has_old_type_store == 1.0) { flux_type_store(name, old_type_store); }
            else { flux_type_store(name, 0.0); }
            body
        }
        else { val }
    }
    else { self.parse_err("expected name after let"); 0.0 }
}

def parse_var(self: Compiler) -> Double {
    self.lex_next(); // eat 'var'
    if (self.tok_type == TokenType.Identifier) {
        let name = self.tok_val;
        self.lex_next();
        if (self.tok_type == TokenType.Colon) {
            self.lex_next();
            let tname = "";
            if (self.tok_type == TokenType.Identifier) { tname = self.tok_val; }
            self.parse_type_hint();
            if (self.last_type == 5.0) {
                flux_hm_put(self.var_type_names, name, self.last_trait_name);
                flux_hm_put(self.var_trait_names, name, self.last_trait_name);
            }
            else if (tname != "") { flux_hm_put(self.var_type_names, name, tname); }
        }
        self.tok_expect(TokenType.Assign, "expected '=' in var");
        let val = self.parse_expr();
        flux_type_store(name, self.last_type);
        if (self.last_type == 4.0) { flux_hm_put(self.var_enum_names, name, self.last_enum_name); }
        let alloca = flux_llvm_build_alloca(self.bld, t_double(self.ctx), name);
        flux_llvm_build_store(self.bld, val, alloca);
        flux_hm_put(self.locals, name, alloca);
        if (flux_hm_has(self.var_type_names, name) != 1.0 && flux_hm_has(self.structs, self.last_varname) == 1.0) {
            flux_hm_put(self.var_type_names, name, self.last_varname);
        }
        val
    }
    else { self.parse_err("expected name after var"); 0.0 }
}

def parse_while(self: Compiler) -> Double {
    self.lex_next(); // eat 'while'
    let cond_bb = flux_llvm_append_basic_block(self.cur_fn, "wcond");
    let body_bb = flux_llvm_append_basic_block(self.cur_fn, "wbody");
    let end_bb = flux_llvm_append_basic_block(self.cur_fn, "wend");

    // Alloca to carry the loop result across the loop boundary,
    // avoiding dominance violations when the body contains if-else
    let res_alloca = flux_llvm_build_alloca(self.bld, t_double(self.ctx), "wres");
    let zero_val = flux_llvm_const_real(t_double(self.ctx), 0.0);
    flux_llvm_build_store(self.bld, zero_val, res_alloca);

    let prev_loop_end = self.loop_end;
    let prev_loop_cont = self.loop_cont;
    self.loop_end = end_bb;
    self.loop_cont = cond_bb;

    flux_llvm_build_br(self.bld, cond_bb);
    flux_llvm_position_builder_at_end(self.bld, cond_bb);
    let cond = self.parse_expr();
    let wzero = flux_llvm_const_real(t_double(self.ctx), 0.0);
    let cond_i1 = flux_llvm_build_fcmp(self.bld, 6.0, cond, wzero, "wcond_i1");
    if (self.tok_type == TokenType.KwDo) {
        self.lex_next();
    }
    flux_llvm_build_cond_br(self.bld, cond_i1, body_bb, end_bb);

    flux_llvm_position_builder_at_end(self.bld, body_bb);
    let body = self.parse_expr();
    flux_llvm_build_store(self.bld, body, res_alloca);
    flux_llvm_build_br(self.bld, cond_bb);

    flux_llvm_position_builder_at_end(self.bld, end_bb);
    self.loop_end = prev_loop_end;
    self.loop_cont = prev_loop_cont;
    flux_llvm_build_load2(self.bld, t_double(self.ctx), res_alloca, "wresult")
}

def parse_await(self: Compiler) -> Double {
    self.lex_next(); // eat 'await'
    if (self.async_state_alloca == 0.0) {
        self.parse_err("await outside async function");
        flux_llvm_const_real(t_double(self.ctx), 0.0)
    }
    else {
        let val = self.parse_expr();
        flux_llvm_build_store(self.bld, val, self.async_result_alloca);
        let cont_bb = flux_llvm_append_basic_block(self.cur_fn, "await_cont");
        let zero = flux_llvm_const_real(t_double(self.ctx), 0.0);
        let is_ready = flux_llvm_build_fcmp(self.bld, 6.0, val, zero, "await_ready");
        let suspend_bb = flux_llvm_append_basic_block(self.cur_fn, "await_suspend");
        flux_llvm_build_cond_br(self.bld, is_ready, cont_bb, suspend_bb);
        // Suspend block: save state and return 0.0
        flux_llvm_position_builder_at_end(self.bld, suspend_bb);
        let state_idx = self.async_await_count + 1.0;
        self.async_await_count = state_idx;
        let state_val = flux_llvm_const_int(t_int32(self.ctx), state_idx, 0.0);
        let state_ptr = flux_llvm_build_struct_gep2(self.bld, self.async_state_struct_ty, self.async_state_alloca, 0.0, "sp");
        flux_llvm_build_store(self.bld, state_val, state_ptr);
        flux_llvm_build_ret(self.bld, zero);
        // Resume block: load from alloca and check readiness
        let resume_bb = flux_llvm_append_basic_block(self.cur_fn, "await_resume");
        flux_hm_put(self.async_resume_targets, flux_dtoa(state_idx), resume_bb);
        flux_llvm_position_builder_at_end(self.bld, resume_bb);
        let resume_val = flux_llvm_build_load2(self.bld, t_double(self.ctx), self.async_result_alloca, "await_rv");
        let resume_ready = flux_llvm_build_fcmp(self.bld, 6.0, resume_val, zero, "await_resume_ready");
        let resuspend_bb = flux_llvm_append_basic_block(self.cur_fn, "await_resuspend");
        flux_llvm_build_cond_br(self.bld, resume_ready, cont_bb, resuspend_bb);
        // Re-suspend block
        flux_llvm_position_builder_at_end(self.bld, resuspend_bb);
        let re_state_ptr = flux_llvm_build_struct_gep2(self.bld, self.async_state_struct_ty, self.async_state_alloca, 0.0, "rsp");
        flux_llvm_build_store(self.bld, state_val, re_state_ptr);
        flux_llvm_build_ret(self.bld, zero);
        // Continuation block: load result value
        flux_llvm_position_builder_at_end(self.bld, cont_bb);
        flux_llvm_build_load2(self.bld, t_double(self.ctx), self.async_result_alloca, "await_val")
    }
}

def parse_for(self: Compiler) -> Double {
    self.lex_next(); // eat 'for'
    let var_name = self.tok_val;
    self.tok_expect(TokenType.Identifier, "expected loop variable");
    let old_local = 0.0;
    let has_old_local = flux_hm_has(self.locals, var_name);
    if (has_old_local == 1.0) { old_local = flux_hm_get(self.locals, var_name); }
    let old_enum = 0.0;
    let has_old_enum = flux_hm_has(self.var_enum_names, var_name);
    if (has_old_enum == 1.0) { old_enum = flux_hm_get(self.var_enum_names, var_name); }
    let old_type_name = 0.0;
    let has_old_type_name = flux_hm_has(self.var_type_names, var_name);
    if (has_old_type_name == 1.0) { old_type_name = flux_hm_get(self.var_type_names, var_name); }
    let old_trait_name = 0.0;
    let has_old_trait_name = flux_hm_has(self.var_trait_names, var_name);
    if (has_old_trait_name == 1.0) { old_trait_name = flux_hm_get(self.var_trait_names, var_name); }
    let old_type_store = 0.0;
    let has_old_type_store = flux_type_has(var_name);
    if (has_old_type_store == 1.0) { old_type_store = flux_type_load(var_name); }
    self.tok_expect(TokenType.KwIn, "expected 'in'");
    let start = self.parse_expr();
    self.tok_expect(TokenType.Comma, "expected ',' between start and end");
    let end_val = self.parse_expr();
    self.tok_expect(TokenType.KwDo, "expected 'do' after for clause");

    let cond_bb = flux_llvm_append_basic_block(self.cur_fn, "fcond");
    let body_bb = flux_llvm_append_basic_block(self.cur_fn, "fbody");
    let step_bb = flux_llvm_append_basic_block(self.cur_fn, "fstep");
    let end_bb = flux_llvm_append_basic_block(self.cur_fn, "fend");

    let alloca = flux_llvm_build_alloca(self.bld, t_double(self.ctx), var_name);
    let res = flux_llvm_build_alloca(self.bld, t_double(self.ctx), "fres");
    flux_llvm_build_store(self.bld, start, res);
    flux_llvm_build_store(self.bld, start, alloca);
    flux_type_store(var_name, 0.0);
    flux_hm_put(self.locals, var_name, alloca);

    let prev_loop_end = self.loop_end;
    let prev_loop_cont = self.loop_cont;
    self.loop_end = end_bb;
    self.loop_cont = step_bb;

    flux_llvm_build_br(self.bld, cond_bb);

    flux_llvm_position_builder_at_end(self.bld, cond_bb);
    let i_val = flux_llvm_build_load2(self.bld, t_double(self.ctx), alloca, var_name);
    let cond_i1 = flux_llvm_build_fcmp(self.bld, 4.0, i_val, end_val, "fcond_i1");
    flux_llvm_build_cond_br(self.bld, cond_i1, body_bb, end_bb);

    flux_llvm_position_builder_at_end(self.bld, body_bb);
    let body = self.parse_expr();
    flux_llvm_build_store(self.bld, body, res);
    flux_llvm_build_br(self.bld, step_bb);

    flux_llvm_position_builder_at_end(self.bld, step_bb);
    let iv = flux_llvm_build_load2(self.bld, t_double(self.ctx), alloca, var_name);
    let one = flux_llvm_const_real(t_double(self.ctx), 1.0);
    let next_i = flux_llvm_build_fadd(self.bld, iv, one, "next");
    flux_llvm_build_store(self.bld, next_i, alloca);
    flux_llvm_build_br(self.bld, cond_bb);

    flux_llvm_position_builder_at_end(self.bld, end_bb);
    self.loop_end = prev_loop_end;
    self.loop_cont = prev_loop_cont;
    if (has_old_local == 1.0) { flux_hm_put(self.locals, var_name, old_local); }
    else { flux_hm_remove(self.locals, var_name); }
    if (has_old_enum == 1.0) { flux_hm_put(self.var_enum_names, var_name, old_enum); }
    else { flux_hm_remove(self.var_enum_names, var_name); }
    if (has_old_type_name == 1.0) { flux_hm_put(self.var_type_names, var_name, old_type_name); }
    else { flux_hm_remove(self.var_type_names, var_name); }
    if (has_old_trait_name == 1.0) { flux_hm_put(self.var_trait_names, var_name, old_trait_name); }
    else { flux_hm_remove(self.var_trait_names, var_name); }
    if (has_old_type_store == 1.0) { flux_type_store(var_name, old_type_store); }
    else { flux_type_store(var_name, 0.0); }
    flux_llvm_build_load2(self.bld, t_double(self.ctx), res, "fres")
}

def parse_vector(self: Compiler) -> Double {
    self.lex_next(); // eat '['
    let count = 0.0;
    let elems = flux_hm_create();
    while (self.tok_type != TokenType.RBracket && self.tok_type != TokenType.Eof) {
        let ev = self.parse_expr();
        flux_hm_put(elems, flux_str_from_char(48.0 + count), ev);
        count = count + 1.0;
        if (self.tok_type == TokenType.Comma) { self.lex_next(); }
    }
    self.tok_expect(TokenType.RBracket, "expected ']'");
    if (count == 0.0) { flux_hm_destroy(elems); return 0.0; }
    let arr_ty = flux_llvm_array_type(t_double(self.ctx), count);
    let arr_ptr = flux_llvm_build_alloca(self.bld, arr_ty, "vec");
    let i = 0.0;
    while (i < count) {
        let ev = flux_hm_get(elems, flux_str_from_char(48.0 + i));
        let z32 = flux_llvm_const_int(t_int32(self.ctx), 0.0, 0.0);
        let i32 = flux_llvm_const_int(t_int32(self.ctx), i, 0.0);
        flux_llvm_index_arg_reset();
        flux_llvm_index_arg_push(z32);
        flux_llvm_index_arg_push(i32);
        let gep = flux_llvm_build_gep2(self.bld, arr_ty, arr_ptr, 2.0, "vgep");
        flux_llvm_build_store(self.bld, ev, gep);
        i = i + 1.0;
    }
    flux_hm_destroy(elems);
    // Return pointer to the first element (pointee = double)
    let z32 = flux_llvm_const_int(t_int32(self.ctx), 0.0, 0.0);
    let z32b = flux_llvm_const_int(t_int32(self.ctx), 0.0, 0.0);
    flux_llvm_index_arg_reset();
    flux_llvm_index_arg_push(z32);
    flux_llvm_index_arg_push(z32b);
    let first = flux_llvm_build_gep2(self.bld, arr_ty, arr_ptr, 2.0, "vfirst");
    let as_i64 = flux_llvm_build_ptrtoint(self.bld, first, t_int64(self.ctx), "vi64");
    self.last_type = 1.0;
    flux_llvm_build_bitcast(self.bld, as_i64, t_double(self.ctx), "vdbl")
}

def parse_lambda(self: Compiler) -> Double {
    self.lex_next(); // eat 'fn'
    self.tok_expect(TokenType.LParen, "expected '(' in lambda");
    // Parse params
    let argc = 0.0;
    let pnames = flux_hm_create();
    if (self.tok_type != TokenType.RParen) {
        let pname = self.tok_val;
        self.tok_expect(TokenType.Identifier, "expected parameter name");
        flux_hm_put(pnames, flux_str_from_char(48.0), pname);
        argc = argc + 1.0;
        while (self.tok_type == TokenType.Comma) {
            self.lex_next();
            let pname2 = self.tok_val;
            self.tok_expect(TokenType.Identifier, "expected parameter name");
            flux_hm_put(pnames, flux_str_from_char(48.0 + argc), pname2);
            argc = argc + 1.0;
        }
    }
    self.tok_expect(TokenType.RParen, "expected ')'");
    self.tok_expect(TokenType.Arrow, "expected '->' in lambda");
    // Save state
    let prev_fn = self.cur_fn;
    let prev_entry = self.entry_bb;
    let prev_locals = self.locals;
    let prev_bb = flux_llvm_get_insert_block(self.bld);
    // Create lambda function
    let lam_name = "__lambda_" + flux_dtoa(self.lambda_counter);
    self.lambda_counter = self.lambda_counter + 1.0;
    flux_llvm_type_arg_reset();
    let i = 0.0;
    while (i < argc) {
        flux_llvm_type_arg_push(t_double(self.ctx));
        i = i + 1.0;
    }
    let ft = flux_llvm_function_type(t_double(self.ctx), argc, 0.0);
    let func = flux_llvm_add_function(self.mod, lam_name, ft);
    // Set up lambda function
    self.cur_fn = func;
    self.entry_bb = flux_llvm_append_basic_block(func, "entry");
    self.locals = flux_hm_create();
    flux_llvm_position_builder_at_end(self.bld, self.entry_bb);
    // Store params as locals
    let pi = 0.0;
    while (pi < argc) {
        let pval = flux_llvm_get_param(func, pi);
        let pname = flux_hm_get(pnames, flux_str_from_char(48.0 + pi));
        let a = flux_llvm_build_alloca(self.bld, t_double(self.ctx), pname);
        flux_llvm_build_store(self.bld, pval, a);
        flux_hm_put(self.locals, pname, a);
        pi = pi + 1.0;
    }
    flux_hm_destroy(pnames);
    // Parse body and emit ret
    let body = self.parse_expr();
    flux_llvm_build_ret(self.bld, body);
    // Restore state
    self.cur_fn = prev_fn;
    self.entry_bb = prev_entry;
    self.locals = prev_locals;
    flux_llvm_position_builder_at_end(self.bld, prev_bb);
    self.last_type = 1.0;
    let lam_int = flux_llvm_build_ptrtoint(self.bld, func, t_int64(self.ctx), "lam_int");
    flux_llvm_build_bitcast(self.bld, lam_int, t_double(self.ctx), "lam_fn")
}

def parse_match(self: Compiler) -> Double {
    self.lex_next(); // eat 'match'
    let val = self.parse_expr();
    let is_pay = (self.last_type == 4.0);
    let pay_ty = if (is_pay) { flux_hm_get(self.enum_types, self.last_enum_name) } else { 0.0 };
    let pay_ptr = if (is_pay) {
        let ai = flux_llvm_build_bitcast(self.bld, val, t_int64(self.ctx), "tui");
        flux_llvm_build_inttoptr(self.bld, ai, t_ptr(self.ctx), "tup")
    } else { 0.0 };
    let tag_val = if (is_pay) {
        let ld = flux_llvm_build_load2(self.bld, pay_ty, pay_ptr, "tuv");
        flux_llvm_build_extract_value(self.bld, ld, 0.0, "tag")
    }
    else {
        flux_llvm_build_fptosi(self.bld, val, t_int32(self.ctx), "mval")
    };
    self.tok_expect(TokenType.LBrace, "expected '{' after match");

    // Alloca for result
    let res = flux_llvm_build_alloca(self.bld, t_double(self.ctx), "mres");
    let merge = flux_llvm_append_basic_block(self.cur_fn, "mmerge");
    let have_default = 0.0;

    while (self.tok_type != TokenType.RBrace && self.tok_type != TokenType.Eof) {
        if (self.tok_type == TokenType.KwDefault) {
            self.lex_next();
            self.tok_expect(TokenType.Arrow, "expected '->'");
            let def_bb = flux_llvm_append_basic_block(self.cur_fn, "mdef");
            flux_llvm_build_br(self.bld, def_bb);
            flux_llvm_position_builder_at_end(self.bld, def_bb);
            let dv = self.parse_expr();
            flux_llvm_build_store(self.bld, dv, res);
            flux_llvm_build_br(self.bld, merge);
            have_default = 1.0;
        }
        else {
            let patterns_en = flux_hm_create();
            let patterns_vn = flux_hm_create();
            let patterns_pvar = flux_hm_create();
            let patterns_has_pvar = flux_hm_create();
            let pat_count = 0.0;
            let parsing_patterns = 1.0;
            while (parsing_patterns == 1.0) {
                let en = self.tok_val;
                self.tok_expect(TokenType.Identifier, "expected enum name");
                self.tok_expect(TokenType.Dot, "expected '.'");
                let vn = self.tok_val;
                self.tok_expect(TokenType.Identifier, "expected variant");
                let payload_var = "";
                let has_payload_var = 0.0;
                if (self.tok_type == TokenType.LParen) {
                    self.lex_next();
                    payload_var = self.tok_val;
                    self.tok_expect(TokenType.Identifier, "expected payload var");
                    self.tok_expect(TokenType.RParen, "expected ')'");
                    has_payload_var = 1.0;
                }
                flux_hm_put(patterns_en, flux_dtoa(pat_count), en);
                flux_hm_put(patterns_vn, flux_dtoa(pat_count), vn);
                flux_hm_put(patterns_pvar, flux_dtoa(pat_count), payload_var);
                flux_hm_put(patterns_has_pvar, flux_dtoa(pat_count), has_payload_var);
                pat_count = pat_count + 1.0;

                if (self.tok_type == TokenType.Pipe2) {
                    self.lex_next();
                } else {
                    parsing_patterns = 0.0;
                }
            }
            self.tok_expect(TokenType.Arrow, "expected '->'");

            // Find if any pattern has a payload variable
            let has_any_payload_var = 0.0;
            let payload_var_name = "";
            let pi = 0.0;
            while (pi < pat_count) {
                if (flux_hm_get(patterns_has_pvar, flux_dtoa(pi)) == 1.0) {
                    has_any_payload_var = 1.0;
                    payload_var_name = flux_hm_get(patterns_pvar, flux_dtoa(pi));
                }
                pi = pi + 1.0;
            }

            // Save old variables for shadowing/restoring
            let old_local = 0.0;
            let has_old_local = 0.0;
            let old_enum = 0.0;
            let has_old_enum = 0.0;
            let old_type_name = 0.0;
            let has_old_type_name = 0.0;
            let old_trait_name = 0.0;
            let has_old_trait_name = 0.0;
            let old_type_store = 0.0;
            let has_old_type_store = 0.0;

            if (has_any_payload_var == 1.0) {
                has_old_local = flux_hm_has(self.locals, payload_var_name);
                if (has_old_local == 1.0) { old_local = flux_hm_get(self.locals, payload_var_name); }
                has_old_enum = flux_hm_has(self.var_enum_names, payload_var_name);
                if (has_old_enum == 1.0) { old_enum = flux_hm_get(self.var_enum_names, payload_var_name); }
                has_old_type_name = flux_hm_has(self.var_type_names, payload_var_name);
                if (has_old_type_name == 1.0) { old_type_name = flux_hm_get(self.var_type_names, payload_var_name); }
                has_old_trait_name = flux_hm_has(self.var_trait_names, payload_var_name);
                if (has_old_trait_name == 1.0) { old_trait_name = flux_hm_get(self.var_trait_names, payload_var_name); }
                has_old_type_store = flux_type_has(payload_var_name);
                if (has_old_type_store == 1.0) { old_type_store = flux_type_load(payload_var_name); }
            }

            // Create alloca in the entry block / current block
            let pa = 0.0;
            if (has_any_payload_var == 1.0) {
                pa = flux_llvm_build_alloca(self.bld, t_double(self.ctx), payload_var_name);
                flux_hm_put(self.locals, payload_var_name, pa);

                // Find the first pattern with payload to set the type metadata
                let first_pat_idx = 0.0;
                let found_first = 0.0;
                let k = 0.0;
                while (k < pat_count) {
                    if (found_first == 0.0 && flux_hm_get(patterns_has_pvar, flux_dtoa(k)) == 1.0) {
                        first_pat_idx = k;
                        found_first = 1.0;
                    }
                    k = k + 1.0;
                }
                let first_en = flux_hm_get(patterns_en, flux_dtoa(first_pat_idx));
                let first_vn = flux_hm_get(patterns_vn, flux_dtoa(first_pat_idx));
                let fcount = flux_hm_get(self.enum_payloads, first_en + "." + first_vn);
                if (fcount != 1.0) {
                    flux_type_store(payload_var_name, 4.0);
                    flux_hm_put(self.var_enum_names, payload_var_name, first_en);
                }
            }

            let arm_bb = flux_llvm_append_basic_block(self.cur_fn, "marm");
            let next_arm_bb = flux_llvm_append_basic_block(self.cur_fn, "mnext");

            let k = 0.0;
            while (k < pat_count) {
                let en = flux_hm_get(patterns_en, flux_dtoa(k));
                let vn = flux_hm_get(patterns_vn, flux_dtoa(k));
                let payload_var = flux_hm_get(patterns_pvar, flux_dtoa(k));
                let has_payload_var = flux_hm_get(patterns_has_pvar, flux_dtoa(k));

                let disc = flux_hm_get(self.enum_variants, en + "." + vn);
                let disc_i32 = flux_llvm_const_int(t_int32(self.ctx), disc, 0.0);

                let cmp = flux_llvm_build_icmp(self.bld, 32.0, tag_val, disc_i32, "mcmp");

                let next_pat_bb = 0.0;
                if (k + 1.0 < pat_count) {
                    next_pat_bb = flux_llvm_append_basic_block(self.cur_fn, "mpatcheck");
                } else {
                    next_pat_bb = next_arm_bb;
                }

                if (has_payload_var == 1.0 && is_pay) {
                    let extract_bb = flux_llvm_append_basic_block(self.cur_fn, "mextract");
                    flux_llvm_build_cond_br(self.bld, cmp, extract_bb, next_pat_bb);

                    flux_llvm_position_builder_at_end(self.bld, extract_bb);
                    let ld2 = flux_llvm_build_load2(self.bld, pay_ty, pay_ptr, "tu2");
                    let fcount = flux_hm_get(self.enum_payloads, en + "." + vn);
                    if (fcount == 1.0) {
                        let pv = flux_llvm_build_extract_value(self.bld, ld2, 1.0, payload_var);
                        flux_llvm_build_store(self.bld, pv, pa);
                    }
                    else {
                        let as_i64 = flux_llvm_build_ptrtoint(self.bld, pay_ptr, t_int64(self.ctx), "pp64");
                        let as_dbl = flux_llvm_build_bitcast(self.bld, as_i64, t_double(self.ctx), "pp_dbl");
                        flux_llvm_build_store(self.bld, as_dbl, pa);
                    }
                    flux_llvm_build_br(self.bld, arm_bb);
                }
                else {
                    flux_llvm_build_cond_br(self.bld, cmp, arm_bb, next_pat_bb);
                }

                if (k + 1.0 < pat_count) {
                    flux_llvm_position_builder_at_end(self.bld, next_pat_bb);
                }
                k = k + 1.0;
            }

            flux_llvm_position_builder_at_end(self.bld, arm_bb);
            let rv = self.parse_expr();

            if (has_any_payload_var == 1.0) {
                if (has_old_local == 1.0) { flux_hm_put(self.locals, payload_var_name, old_local); }
                else { flux_hm_remove(self.locals, payload_var_name); }
                if (has_old_enum == 1.0) { flux_hm_put(self.var_enum_names, payload_var_name, old_enum); }
                else { flux_hm_remove(self.var_enum_names, payload_var_name); }
                if (has_old_type_name == 1.0) { flux_hm_put(self.var_type_names, payload_var_name, old_type_name); }
                else { flux_hm_remove(self.var_type_names, payload_var_name); }
                if (has_old_trait_name == 1.0) { flux_hm_put(self.var_trait_names, payload_var_name, old_trait_name); }
                else { flux_hm_remove(self.var_trait_names, payload_var_name); }
                if (has_old_type_store == 1.0) { flux_type_store(payload_var_name, old_type_store); }
                else { flux_type_store(payload_var_name, 0.0); }
            }

            flux_llvm_build_store(self.bld, rv, res);
            flux_llvm_build_br(self.bld, merge);

            flux_llvm_position_builder_at_end(self.bld, next_arm_bb);

            flux_hm_destroy(patterns_en);
            flux_hm_destroy(patterns_vn);
            flux_hm_destroy(patterns_pvar);
            flux_hm_destroy(patterns_has_pvar);
        }
        if (self.tok_type == TokenType.Comma) { self.lex_next(); }
    }
    // store 0.0 if no arm matched and no default
    if (have_default == 0.0) {
        let zero = flux_llvm_const_real(t_double(self.ctx), 0.0);
        flux_llvm_build_store(self.bld, zero, res);
    }
    self.tok_expect(TokenType.RBrace, "expected '}'");
    flux_llvm_build_br(self.bld, merge);
    flux_llvm_position_builder_at_end(self.bld, merge);
    flux_llvm_build_load2(self.bld, t_double(self.ctx), res, "mval")
}

def parse_call(self: Compiler, callee: Double) -> Double {
    flux_llvm_call_arg_reset();
    let callee_name = self.last_varname;
    let count = 0.0;
    if (self.tok_type != TokenType.RParen) {
        let saved = flux_llvm_call_arg_save();
        let a = self.parse_expr();
        flux_llvm_call_arg_restore(saved);
        flux_llvm_call_arg_push(a);
        count = count + 1.0;
        while (self.tok_type == TokenType.Comma) {
            self.lex_next();
            let saved2 = flux_llvm_call_arg_save();
            let a2 = self.parse_expr();
            flux_llvm_call_arg_restore(saved2);
            flux_llvm_call_arg_push(a2);
            count = count + 1.0;
        }
    }
    self.tok_expect(TokenType.RParen, "expected ')'");
    self.last_type = 1.0;
    if (callee == 0.0) {
        // Auto-declare unknown extern with correct arg count
        let fname = callee_name;
        if (fname != "") {
            flux_llvm_type_arg_reset();
            let ai = 0.0;
            while (ai < count) { flux_llvm_type_arg_push(t_double(self.ctx)); ai = ai + 1.0; }
            let ft2 = flux_llvm_function_type(t_double(self.ctx), count, 0.0);
            let f2 = flux_llvm_add_function(self.mod, fname, ft2);
            flux_hm_put(self.globals, fname, f2);
            flux_hm_put(self.func_types, fname, ft2);
            flux_llvm_build_call2(self.bld, ft2, f2, count, "calltmp")
        }
        else { 0.0 }
    }
    else {
        let func_ty = flux_llvm_global_get_value_type(callee);
        flux_llvm_build_call2(self.bld, func_ty, callee, count, "calltmp")
    }
}

def parse_method_call(self: Compiler, self_val: Double, var_name: String, method_name: String) -> Double {
    // Check for dyn trait dispatch
    if (flux_hm_has(self.var_trait_names, var_name) == 1.0) {
        let trait_name = flux_hm_get(self.var_trait_names, var_name);
        let method_key = trait_name + "." + method_name;
        if (flux_hm_has(self.trait_method_indices, method_key) == 1.0) {
            let method_idx = flux_hm_get(self.trait_method_indices, method_key);
            // Get data and vtable from dyn ptr table
            let gd_fn = flux_hm_get(self.globals, "flux_dyn_ptr_get_data");
            let gd_ft = flux_hm_get(self.func_types, "flux_dyn_ptr_get_data");
            flux_llvm_call_arg_reset();
            flux_llvm_call_arg_push(self_val);
            let data_dbl = flux_llvm_build_call2(self.bld, gd_ft, gd_fn, 1.0, "ddata");
            let gv_fn = flux_hm_get(self.globals, "flux_dyn_ptr_get_vtable");
            let gv_ft = flux_hm_get(self.func_types, "flux_dyn_ptr_get_vtable");
            flux_llvm_call_arg_reset();
            flux_llvm_call_arg_push(self_val);
            let vtab_dbl = flux_llvm_build_call2(self.bld, gv_ft, gv_fn, 1.0, "dvtab");
            // Convert vtable double -> i64 -> ptr
            let vtab_i64 = flux_llvm_build_bitcast(self.bld, vtab_dbl, t_int64(self.ctx), "vt_i64");
            let vtab_ptr = flux_llvm_build_inttoptr(self.bld, vtab_i64, t_ptr(self.ctx), "vt_ptr");
            // GEP into vtable at slot (1 + method_idx)
            flux_llvm_index_arg_reset();
            let idx_val = flux_llvm_const_int(t_int32(self.ctx), 1.0 + method_idx, 0.0);
            flux_llvm_index_arg_push(idx_val);
            let slot_gep = flux_llvm_build_gep2(self.bld, t_ptr(self.ctx), vtab_ptr, 1.0, "mslot");
            let fn_i8 = flux_llvm_build_load2(self.bld, t_ptr(self.ctx), slot_gep, "fn_i8");
            // Parse args and build fn type
            flux_llvm_call_arg_reset();
            flux_llvm_call_arg_push(data_dbl);
            let count = 1.0;
            if (self.tok_type != TokenType.RParen) {
                let saved = flux_llvm_call_arg_save();
                let a = self.parse_expr();
                flux_llvm_call_arg_restore(saved);
                flux_llvm_call_arg_push(a);
                count = count + 1.0;
                while (self.tok_type == TokenType.Comma) {
                    self.lex_next();
                    let saved2 = flux_llvm_call_arg_save();
                    let a2 = self.parse_expr();
                    flux_llvm_call_arg_restore(saved2);
                    flux_llvm_call_arg_push(a2);
                    count = count + 1.0;
                }
            }
            self.tok_expect(TokenType.RParen, "expected ')'");
            // Build fn type and bitcast
            flux_llvm_type_arg_reset();
            let arg_i = 0.0;
            while (arg_i < count) {
                flux_llvm_type_arg_push(t_double(self.ctx));
                arg_i = arg_i + 1.0;
            }
            let ft = flux_llvm_function_type(t_double(self.ctx), count, 0.0);
            let fn_cast = flux_llvm_build_bitcast(self.bld, fn_i8, ft, "fn_cast");
            flux_llvm_build_call2(self.bld, ft, fn_cast, count, "dynmeth")
        }
        else {
            self.parse_err("method not found: " + method_key);
            0.0
        }
    }
        else {
            // Static dispatch: first parse args, then resolve function, then emit
            let type_name = "";
            if (flux_hm_has(self.var_type_names, var_name) == 1.0) {
                type_name = flux_hm_get(self.var_type_names, var_name);
            }
            else if (flux_hm_has(self.structs, var_name) == 1.0) {
                type_name = var_name;
            }
            else {
                self.parse_err("unknown type for method call: " + var_name);
                return 0.0;
            }
            let fn_name = type_name + "_" + method_name;
            // Parse args first (push self + extra args to call arg stack)
            flux_llvm_call_arg_reset();
            flux_llvm_call_arg_push(self_val);
            let count = 1.0;
            if (self.tok_type != TokenType.RParen) {
                let saved = flux_llvm_call_arg_save();
                let a = self.parse_expr();
                flux_llvm_call_arg_restore(saved);
                flux_llvm_call_arg_push(a);
                count = count + 1.0;
                while (self.tok_type == TokenType.Comma) {
                    self.lex_next();
                    let saved2 = flux_llvm_call_arg_save();
                    let a2 = self.parse_expr();
                    flux_llvm_call_arg_restore(saved2);
                    flux_llvm_call_arg_push(a2);
                    count = count + 1.0;
                }
            }
            self.tok_expect(TokenType.RParen, "expected ')'");
            // Resolve or auto-declare function
            if (flux_hm_has(self.globals, fn_name) != 1.0) {
                let ai = 0.0;
                flux_llvm_type_arg_reset();
                while (ai < count) { flux_llvm_type_arg_push(t_double(self.ctx)); ai = ai + 1.0; }
                let ft_new = flux_llvm_function_type(t_double(self.ctx), count, 0.0);
                let f_new = flux_llvm_add_function(self.mod, fn_name, ft_new);
                flux_hm_put(self.globals, fn_name, f_new);
                flux_hm_put(self.func_types, fn_name, ft_new);
            }
            let func = flux_hm_get(self.globals, fn_name);
            let ft = flux_hm_get(self.func_types, fn_name);
            flux_llvm_build_call2(self.bld, ft, func, count, "methtmp")
        }
}

// ===================================================================
// IR EMITTERS
// ===================================================================

def emit_number(self: Compiler, text: String) -> Double {
    let num = flux_parse_number(text);
    let dbl_ty = t_double(self.ctx);
    let result = flux_llvm_const_real(dbl_ty, num);
    self.last_type = 1.0;
    result
}

def emit_string(self: Compiler, text: String) -> Double {
    self.last_type = 2.0;
    let str_ptr = flux_llvm_build_global_string_ptr(self.bld, text, "str");
    let as_i64 = flux_llvm_build_ptrtoint(self.bld, str_ptr, t_int64(self.ctx), "str_int");
    flux_llvm_build_bitcast(self.bld, as_i64, t_double(self.ctx), "str_dbl")
}

def emit_var(self: Compiler, name: String) -> Double {
    self.last_varname = name;
    if (flux_hm_has(self.locals, name) == 1.0) {
        let addr = flux_hm_get(self.locals, name);
        if (flux_type_has(name) == 1.0) {
            self.last_type = flux_type_load(name);
            if (self.last_type == 4.0 && flux_hm_has(self.var_enum_names, name) == 1.0) {
                self.last_enum_name = flux_hm_get(self.var_enum_names, name);
            }
            if (self.last_type == 5.0 && flux_hm_has(self.var_trait_names, name) == 1.0) {
                self.last_trait_name = flux_hm_get(self.var_trait_names, name);
            }
        }
        else { self.last_type = 0.0; }
        self.last_addr = addr;
        flux_llvm_build_load2(self.bld, t_double(self.ctx), addr, name)
    }
    else if (flux_hm_has(self.globals, name) == 1.0) {
        flux_hm_get(self.globals, name)
    }
    else if (flux_hm_has(self.enum_types, name) == 1.0) {
        self.last_type = 3.0;
        self.last_enum_name = name;
        0.0
    }
    else if (flux_hm_has(self.structs, name) == 1.0) {
        // Struct type reference or constructor
        if (self.tok_type == TokenType.LBrace) {
            // Struct constructor: TypeName { field: val, ... }
            self.lex_next(); // eat '{'
            let st = flux_hm_get(self.structs, name);
            let ptr = flux_llvm_build_alloca(self.bld, st, name);
            while (self.tok_type != TokenType.RBrace && self.tok_type != TokenType.Eof) {
                let fname = self.tok_val;
                self.tok_expect(TokenType.Identifier, "expected field name");
                self.tok_expect(TokenType.Colon, "expected ':'");
                let fv = self.parse_expr();
                let key = "." + fname;
                if (flux_hm_has(self.struct_fields, key) == 1.0) {
                    let fi = flux_hm_get(self.struct_fields, key);
                    let gep = flux_llvm_build_struct_gep2(self.bld, st, ptr, fi, fname);
                    flux_llvm_build_store(self.bld, fv, gep);
                }
                if (self.tok_type == TokenType.Comma) { self.lex_next(); }
                else if (self.tok_type != TokenType.RBrace) { break; }
            }
            self.tok_expect(TokenType.RBrace, "expected '}'");
            self.last_varname = name;
            let as_i64 = flux_llvm_build_ptrtoint(self.bld, ptr, t_int64(self.ctx), "st_int");
            self.last_type = 1.0;
            flux_llvm_build_bitcast(self.bld, as_i64, t_double(self.ctx), "st_dbl")
        }
        else {
            self.last_type = 1.0;
            0.0
        }
    }
    else {
        let f = flux_llvm_get_named_function(self.mod, name);
        if (f != 0.0) {
            flux_hm_put(self.globals, name, f);
            let ft = flux_llvm_global_get_value_type(f);
            flux_hm_put(self.func_types, name, ft);
            f
        }
        else {
            // Unknown function — return 0.0; parse_call will auto-declare if needed
            0.0
        }
    }
}

def emit_binop(self: Compiler, tt: TokenType, op_str: String, lhs: Double, left_type: Double, rhs: Double, right_type: Double, left_varname: String) -> Double {
    // Operator overloading: check if left operand has a user-defined operator method
    let op_method = "";
    if (tt == TokenType.Plus) { op_method = "add"; }
    else if (tt == TokenType.Minus) { op_method = "sub"; }
    else if (tt == TokenType.Star) { op_method = "mul"; }
    else if (tt == TokenType.Slash) { op_method = "div"; }
    else if (tt == TokenType.Percent) { op_method = "rem"; }
    else if (tt == TokenType.Eq) { op_method = "eq"; }
    else if (tt == TokenType.Neq) { op_method = "ne"; }
    else if (tt == TokenType.Lt) { op_method = "lt"; }
    else if (tt == TokenType.Gt) { op_method = "gt"; }
    else if (tt == TokenType.Leq) { op_method = "le"; }
    else if (tt == TokenType.Geq) { op_method = "ge"; }
    if (op_method != "") {
        let type_name = "";
        if (flux_hm_has(self.structs, left_varname) == 1.0) {
            type_name = left_varname;
        }
        else if (flux_hm_has(self.var_type_names, left_varname) == 1.0) {
            let tn = flux_hm_get(self.var_type_names, left_varname);
            if (flux_hm_has(self.structs, tn) == 1.0) {
                type_name = tn;
            }
        }
        if (type_name != "") {
            let fn_name = type_name + "_" + op_method;
            let op_fn = flux_llvm_get_named_function(self.mod, fn_name);
            if (op_fn != 0.0) {
                let op_ft = flux_llvm_global_get_value_type(op_fn);
                let saved = flux_llvm_call_arg_save();
                flux_llvm_call_arg_reset();
                flux_llvm_call_arg_push(lhs);
                flux_llvm_call_arg_push(rhs);
                let result = flux_llvm_build_call2(self.bld, op_ft, op_fn, 2.0, fn_name);
                flux_llvm_call_arg_restore(saved);
                self.last_type = 1.0;
                return result;
            }
        }
    }
    // Fall through to standard operations
    let both_str = left_type == 2.0 && right_type == 2.0;
    let any_str = left_type == 2.0 || right_type == 2.0;
    if (any_str && tt == TokenType.Plus) {
        self.last_type = 2.0;
        let sc = flux_hm_get(self.globals, "flux_str_concat");
        let sc_ty = flux_hm_get(self.func_types, "flux_str_concat");
        flux_llvm_call_arg_reset();
        flux_llvm_call_arg_push(lhs);
        flux_llvm_call_arg_push(rhs);
        flux_llvm_build_call2(self.bld, sc_ty, sc, 2.0, "strcat")
    }
    else if (both_str && (tt == TokenType.Eq || tt == TokenType.Neq)) {
        self.last_type = 1.0;
        let scmp = flux_hm_get(self.globals, "flux_strcmp");
        let scmp_ty = flux_hm_get(self.func_types, "flux_strcmp");
        flux_llvm_call_arg_reset();
        flux_llvm_call_arg_push(lhs);
        flux_llvm_call_arg_push(rhs);
        let cmp_result = flux_llvm_build_call2(self.bld, scmp_ty, scmp, 2.0, "strcmp");
        let zero = flux_llvm_const_real(t_double(self.ctx), 0.0);
        let bool_tmp = if (tt == TokenType.Eq) {
            flux_llvm_build_fcmp(self.bld, 1.0, cmp_result, zero, "streq")
        } else {
            flux_llvm_build_fcmp(self.bld, 6.0, cmp_result, zero, "strne")
        };
        flux_llvm_build_uitofp(self.bld, bool_tmp, t_double(self.ctx), "booltmp")
    }
    else if (tt == TokenType.Plus) { self.last_type = 1.0; flux_llvm_build_fadd(self.bld, lhs, rhs, "addtmp") }
    else if (tt == TokenType.Minus) { self.last_type = 1.0; flux_llvm_build_fsub(self.bld, lhs, rhs, "subtmp") }
    else if (tt == TokenType.Star) { self.last_type = 1.0; flux_llvm_build_fmul(self.bld, lhs, rhs, "multmp") }
    else if (tt == TokenType.Slash) { self.last_type = 1.0; flux_llvm_build_fdiv(self.bld, lhs, rhs, "divtmp") }
    else if (tt == TokenType.Percent) { self.last_type = 1.0; flux_llvm_build_frem(self.bld, lhs, rhs, "modtmp") }
    else if (tt == TokenType.Eq) { self.last_type = 1.0; let b = flux_llvm_build_fcmp(self.bld, 1.0, lhs, rhs, "eqtmp"); flux_llvm_build_uitofp(self.bld, b, t_double(self.ctx), "booltmp") }
    else if (tt == TokenType.Neq) { self.last_type = 1.0; let b = flux_llvm_build_fcmp(self.bld, 6.0, lhs, rhs, "neqtmp"); flux_llvm_build_uitofp(self.bld, b, t_double(self.ctx), "booltmp") }
    else if (tt == TokenType.Lt) { self.last_type = 1.0; let b = flux_llvm_build_fcmp(self.bld, 4.0, lhs, rhs, "lttmp"); flux_llvm_build_uitofp(self.bld, b, t_double(self.ctx), "booltmp") }
    else if (tt == TokenType.Gt) { self.last_type = 1.0; let b = flux_llvm_build_fcmp(self.bld, 2.0, lhs, rhs, "gttmp"); flux_llvm_build_uitofp(self.bld, b, t_double(self.ctx), "booltmp") }
    else if (tt == TokenType.Leq) { self.last_type = 1.0; let b = flux_llvm_build_fcmp(self.bld, 5.0, lhs, rhs, "leqtmp"); flux_llvm_build_uitofp(self.bld, b, t_double(self.ctx), "booltmp") }
    else if (tt == TokenType.Geq) { self.last_type = 1.0; let b = flux_llvm_build_fcmp(self.bld, 3.0, lhs, rhs, "geqtmp"); flux_llvm_build_uitofp(self.bld, b, t_double(self.ctx), "booltmp") }
    else if (tt == TokenType.LogicalAnd) {
        self.last_type = 1.0;
        let zero = flux_llvm_const_real(t_double(self.ctx), 0.0);
        let l_i1 = flux_llvm_build_fcmp(self.bld, 6.0, lhs, zero, "lbool");
        let r_i1 = flux_llvm_build_fcmp(self.bld, 6.0, rhs, zero, "rbool");
        let and_i1 = flux_llvm_build_and(self.bld, l_i1, r_i1, "andtmp");
        flux_llvm_build_uitofp(self.bld, and_i1, t_double(self.ctx), "booltmp")
    }
    else if (tt == TokenType.LogicalOr) {
        self.last_type = 1.0;
        let zero = flux_llvm_const_real(t_double(self.ctx), 0.0);
        let l_i1 = flux_llvm_build_fcmp(self.bld, 6.0, lhs, zero, "lbool");
        let r_i1 = flux_llvm_build_fcmp(self.bld, 6.0, rhs, zero, "rbool");
        let or_i1 = flux_llvm_build_or(self.bld, l_i1, r_i1, "ortmp");
        flux_llvm_build_uitofp(self.bld, or_i1, t_double(self.ctx), "booltmp")
    }
    else { self.parse_err("unknown op"); 0.0 }
}

def emit_index(self: Compiler, target: Double, index: Double) -> Double {
    let as_i64 = flux_llvm_build_bitcast(self.bld, target, t_int64(self.ctx), "p64");
    let ptr = flux_llvm_build_inttoptr(self.bld, as_i64, t_ptr(self.ctx), "ptr");
    let idx32 = flux_llvm_build_fptosi(self.bld, index, t_int32(self.ctx), "idx32");
    flux_llvm_index_arg_reset();
    flux_llvm_index_arg_push(idx32);
    let gep = flux_llvm_build_gep2(self.bld, t_double(self.ctx), ptr, 1.0, "idxtmp");
    flux_llvm_build_load2(self.bld, t_double(self.ctx), gep, "loadidx")
}

def emit_member(self: Compiler, target: Double, name: String) -> Double {
    if (self.last_type == 3.0) {
        let key = self.last_enum_name + "." + name;
        if (flux_hm_has(self.enum_variants, key) == 1.0) {
            let disc = flux_hm_get(self.enum_variants, key);
            if (flux_hm_has(self.enum_payloads, key) == 1.0 && (self.tok_type == TokenType.LParen || self.tok_type == TokenType.LBrace)) {
                let tu_type = flux_hm_get(self.enum_types, self.last_enum_name);
                let tu_ptr = flux_llvm_build_alloca(self.bld, tu_type, key);
                let disc_i32 = flux_llvm_const_int(t_int32(self.ctx), disc, 0.0);
                let tag_ptr = flux_llvm_build_struct_gep2(self.bld, tu_type, tu_ptr, 0.0, "tag_ptr");
                flux_llvm_build_store(self.bld, disc_i32, tag_ptr);
                if (self.tok_type == TokenType.LParen) {
                    self.lex_next();
                    let pv = self.parse_expr();
                    self.tok_expect(TokenType.RParen, "expected ')'");
                    let pp = flux_llvm_build_struct_gep2(self.bld, tu_type, tu_ptr, 1.0, "pp");
                    flux_llvm_build_store(self.bld, pv, pp);
                }
                else {
                    self.lex_next();
                    let fi = 1.0;
                    while (self.tok_type != TokenType.RBrace && self.tok_type != TokenType.Eof) {
                        let fname = self.tok_val;
                        self.tok_expect(TokenType.Identifier, "expected field name");
                        self.tok_expect(TokenType.Colon, "expected ':'");
                        let fv = self.parse_expr();
                        let fip = flux_llvm_build_struct_gep2(self.bld, tu_type, tu_ptr, fi, fname);
                        flux_llvm_build_store(self.bld, fv, fip);
                        fi = fi + 1.0;
                        if (self.tok_type == TokenType.Comma) { self.lex_next(); }
                    }
                    self.tok_expect(TokenType.RBrace, "expected '}'");
                }
                let as_i64 = flux_llvm_build_ptrtoint(self.bld, tu_ptr, t_int64(self.ctx), "tu_int");
                self.last_type = 4.0;
                flux_llvm_build_bitcast(self.bld, as_i64, t_double(self.ctx), "tu_dbl")
            }
            else {
                self.last_type = 1.0;
                let disc_i32 = flux_llvm_const_int(t_int32(self.ctx), disc, 0.0);
                flux_llvm_build_sitofp(self.bld, disc_i32, t_double(self.ctx), key)
            }
        }
        else {
            self.parse_err("unknown variant: " + name);
            0.0
        }
    }
    else if (self.last_type == 4.0) {
        let ek = "$" + self.last_enum_name + "." + name;
        if (flux_hm_has(self.struct_fields, ek) == 1.0) {
            let eidx = flux_hm_get(self.struct_fields, ek);
            let tu_type = flux_hm_get(self.enum_types, self.last_enum_name);
            let as_i64 = flux_llvm_build_bitcast(self.bld, target, t_int64(self.ctx), "p64");
            let eptr = flux_llvm_build_inttoptr(self.bld, as_i64, t_ptr(self.ctx), "eptr");
            let egep = flux_llvm_build_struct_gep2(self.bld, tu_type, eptr, eidx, name);
            flux_llvm_build_load2(self.bld, t_double(self.ctx), egep, "memtmp")
        }
        else {
            self.parse_err("unknown member: " + name + " in enum payload");
            0.0
        }
    }
    else {
        let key = "." + name;
        if (flux_hm_has(self.struct_fields, key) == 1.0) {
            let idx = flux_hm_get(self.struct_fields, key);
            let st = 0.0;
            if (flux_hm_has(self.structs, self.last_varname) == 1.0) {
                st = flux_hm_get(self.structs, self.last_varname);
            }
            else if (flux_hm_has(self.var_type_names, self.last_varname) == 1.0) {
                let tn = flux_hm_get(self.var_type_names, self.last_varname);
                if (flux_hm_has(self.structs, tn) == 1.0) {
                    st = flux_hm_get(self.structs, tn);
                }
            }
            if (st == 0.0) { st = t_double(self.ctx); }
            let as_i64 = flux_llvm_build_bitcast(self.bld, target, t_int64(self.ctx), "p64");
            let ptr = flux_llvm_build_inttoptr(self.bld, as_i64, t_ptr(self.ctx), "ptr");
            let gep = flux_llvm_build_struct_gep2(self.bld, st, ptr, idx, name);
            self.last_addr = gep;
            flux_llvm_build_load2(self.bld, t_double(self.ctx), gep, "memtmp")
        }
        else {
            self.parse_err("unknown member: " + name);
            0.0
        }
    }
}

// `?` postfix: extract Ok payload or early-return Err from the enclosing
// function. The inner expression is a tagged-union Double (encoded pointer
// to a {i32 tag, T payload} struct). We:
//   1. Bitcast the encoded pointer back to a typed pointer
//   2. Load the struct
//   3. Extract the tag
//   4. Branch: tag==0 (Ok) -> cont block extracts payload; tag!=0 (Err) -> ret the original Double
// The Ok payload becomes the new value of the expression.
def emit_qmark_propagate(self: Compiler, inner: Double) -> Double {
    let tu_type = flux_hm_get(self.enum_types, self.last_enum_name);
    let as_i64 = flux_llvm_build_bitcast(self.bld, inner, t_int64(self.ctx), "qmk_i64");
    let as_ptr = flux_llvm_build_inttoptr(self.bld, as_i64, t_ptr(self.ctx), "qmk_ptr");
    let struct_slot = flux_llvm_build_alloca(self.bld, tu_type, "qmk_struct");
    let loaded = flux_llvm_build_load2(self.bld, tu_type, as_ptr, "qmk_loaded");
    flux_llvm_build_store(self.bld, loaded, struct_slot);
    let tag = flux_llvm_build_extract_value(self.bld, loaded, 0.0, "qmk_tag");
    let the_fn = self.cur_fn;
    let err_bb = flux_llvm_append_basic_block(the_fn, "qmk_err");
    let cont_bb = flux_llvm_append_basic_block(the_fn, "qmk_cont");
    let zero = flux_llvm_const_int(t_int32(self.ctx), 0.0, 0.0);
    let is_err = flux_llvm_build_icmp(self.bld, 33.0, tag, zero, "qmk_cmp");
    flux_llvm_build_cond_br(self.bld, is_err, err_bb, cont_bb);
    flux_llvm_position_builder_at_end(self.bld, err_bb);
    flux_llvm_build_ret(self.bld, inner);
    flux_llvm_position_builder_at_end(self.bld, cont_bb);
    let reloaded = flux_llvm_build_load2(self.bld, tu_type, struct_slot, "qmk_reload");
    flux_llvm_build_extract_value(self.bld, reloaded, 1.0, "qmk_pay")
}

def parse_type_hint(self: Compiler) {
    if (self.tok_type == TokenType.KwDouble) { self.last_type = 1.0; self.lex_next(); }
    else if (self.tok_type == TokenType.KwString) { self.lex_next(); }
    else if (self.tok_type == TokenType.KwBool) { self.lex_next(); }
    else if (self.tok_type == TokenType.KwVoid) { self.lex_next(); }
    else if (self.tok_type == TokenType.KwInt) { self.lex_next(); }
    else if (self.tok_type == TokenType.Amp) {
        self.lex_next();
        if (self.tok_type == TokenType.Lifetime) { self.lex_next(); }
        self.parse_type_hint();
    }
    else if (self.tok_type == TokenType.Identifier) {
        let name = self.tok_val;
        self.lex_next();
        if (name == "dyn" && self.tok_type == TokenType.Identifier) {
            self.last_trait_name = self.tok_val;
            self.last_type = 5.0;
            self.lex_next();
        }
    }
    else { self.parse_err("expected type"); }
}

// ===================================================================
// DECLARATION PARSING
// ===================================================================

def finish_top(self: Compiler) {
    if (self.cur_fn != 0.0) {
        let last_val = flux_llvm_build_load2(self.bld, t_double(self.ctx), self.retval_store, "retval");
        flux_llvm_build_ret(self.bld, last_val);
        self.cur_fn = 0.0;
        self.entry_bb = 0.0;
        self.retval_store = 0.0;
        self.locals = flux_hm_create();
    }
}

def parse_all(self: Compiler) {
    while (self.tok_type != TokenType.Eof) {
        self.had_error = 0.0;
        self.parse_decl();
        if (self.had_error == 1.0) {
            self.lex_next();
            self.had_error = 0.0;
        }
        while (self.tok_type == TokenType.Semicolon) { self.lex_next(); }
    }
    self.finish_top();
}

def parse_decl(self: Compiler) {
    if (self.tok_type == TokenType.KwExtern) {
        self.parse_extern();
    }
    else if (self.tok_type == TokenType.KwDef) {
        self.finish_top();
        self.parse_fn_def(0.0, "");
    }
    else if (self.tok_type == TokenType.KwAsync) {
        self.finish_top();
        self.lex_next();
        self.tok_expect(TokenType.KwDef, "expected 'def' after async");
        self.parse_fn_def(1.0, "");
    }
    else if (self.tok_type == TokenType.KwStruct) {
        self.parse_struct();
    }
    else if (self.tok_type == TokenType.KwEnum) {
        self.parse_enum();
    }
    else if (self.tok_type == TokenType.KwClass) {
        self.parse_class();
    }
    else if (self.tok_type == TokenType.KwImpl) {
        self.parse_impl();
    }
    else if (self.tok_type == TokenType.KwTrait) {
        self.parse_trait();
    }
    else if (self.tok_type == TokenType.KwImport) {
        self.parse_import();
    }
    else {
        self.finish_top();
        if (self.cur_fn == 0.0) {
            let anon_name = "__anon_expr_" + flux_dtoa(self.lambda_counter);
            self.lambda_counter = self.lambda_counter + 1.0;
            let ft = flux_llvm_function_type(t_double(self.ctx), 0.0, 0.0);
            let func = flux_llvm_add_function(self.mod, anon_name, ft);
            flux_hm_put(self.globals, anon_name, func);
            flux_hm_put(self.func_types, anon_name, ft);
            self.cur_fn = func;
            let entry = flux_llvm_append_basic_block(func, "entry");
            flux_llvm_position_builder_at_end(self.bld, entry);
            self.retval_store = flux_llvm_build_alloca(self.bld, t_double(self.ctx), "retval");
        }
        let unary_val = self.parse_unary();
        flux_llvm_build_store(self.bld, unary_val, self.retval_store);
    }
}

// ===================================================================
// EXTERN DECLARATION
// ===================================================================

def parse_extern(self: Compiler) {
    self.lex_next(); // eat 'extern'
    self.tok_expect(TokenType.KwDef, "expected 'def' after extern");
    let name = self.tok_val;
    self.tok_expect(TokenType.Identifier, "expected name");

    self.tok_expect(TokenType.LParen, "expect '('");
    flux_llvm_type_arg_reset();
    let argc = 0.0;
    if (self.tok_type != TokenType.RParen) {
        // param name
        self.tok_expect(TokenType.Identifier, "expected param name");
        if (self.tok_type == TokenType.Colon) {
            self.lex_next();
            self.parse_type_hint();
        }
        flux_llvm_type_arg_push(t_double(self.ctx));
        argc = argc + 1.0;
        while (self.tok_type == TokenType.Comma) {
            self.lex_next();
            self.tok_expect(TokenType.Identifier, "expected param name");
            if (self.tok_type == TokenType.Colon) {
                self.lex_next();
                self.parse_type_hint();
            }
            flux_llvm_type_arg_push(t_double(self.ctx));
            argc = argc + 1.0;
        }
    }
    self.tok_expect(TokenType.RParen, "expect ')'");

    let ret_type = t_double(self.ctx);
    if (self.tok_type == TokenType.Arrow) {
        self.lex_next();
        ret_type = t_double(self.ctx);
        self.parse_type_hint();
    }

    let ft = flux_llvm_function_type(ret_type, argc, 0.0);
    let f = flux_llvm_add_function(self.mod, name, ft);
    flux_hm_put(self.globals, name, f);
    flux_hm_put(self.func_types, name, ft);
}

// ===================================================================
// IMPORT DECLARATION
// ===================================================================

def parse_import(self: Compiler) {
    self.lex_next(); // eat 'import'
    let module_name = self.tok_val;
    self.tok_expect(TokenType.Identifier, "expected module name");
    let file_path = "src/fluxc/" + module_name + ".flux";
    let import_src = flux_read_file(file_path);
    if (flux_str_len(import_src) == 0.0) {
        self.parse_err("cannot import: " + file_path);
        return 0.0;
    }
    // Save current source state
    let saved_src = self.source;
    let saved_cursor = self.cursor;
    let saved_len = self.src_len;
    let saved_line = self.line;
    let saved_col = self.col;
    let saved_tok_type = self.tok_type;
    let saved_tok_val = self.tok_val;
    // Parse the imported file
    self.set_src(import_src);
    self.parse_all();
    // Restore original source state
    self.source = saved_src;
    self.cursor = saved_cursor;
    self.src_len = saved_len;
    self.line = saved_line;
    self.col = saved_col;
    self.tok_type = saved_tok_type;
    self.tok_val = saved_tok_val;
}

// ===================================================================
// FUNCTION DEFINITION
// ===================================================================

def parse_fn_def(self: Compiler, async_flag: Double, prefix: String) {
    self.lex_next(); // eat 'def'
    let name = self.tok_val;
    self.tok_expect(TokenType.Identifier, "expected name");
    if (prefix != "") { name = prefix + "_" + name; }

    // Optional template params [T]
    let async_state_struct_ty_local = 0.0;
    if (async_flag == 1.0) {
        flux_llvm_type_arg_reset();
        flux_llvm_type_arg_push(t_int32(self.ctx));
        async_state_struct_ty_local = flux_llvm_struct_type_in_ctx(self.ctx, 1.0, 0.0);
        flux_llvm_struct_set_body(async_state_struct_ty_local, 1.0, 0.0);
        self.async_state_struct_ty = async_state_struct_ty_local;
    }
    if (self.tok_type == TokenType.LBracket) {
        self.lex_next();
        while (self.tok_type != TokenType.RBracket && self.tok_type != TokenType.Eof) {
            self.lex_next();
            if (self.tok_type == TokenType.Comma) { self.lex_next(); }
        }
        self.tok_expect(TokenType.RBracket, "expected ']'");
    }

    // Params — use a temp hashmap to remember param names
    self.tok_expect(TokenType.LParen, "expected '('");
    let argc = 0.0;
    flux_llvm_type_arg_reset();
    let pnames = flux_hm_create();
    if (self.tok_type != TokenType.RParen) {
        let pname = self.tok_val;
        self.tok_expect(TokenType.Identifier, "expected param");
        let ptype = 1.0;
        if (self.tok_type == TokenType.Colon) {
            self.lex_next();
            let type_tok = self.tok_type;
            let type_name = "";
            if (type_tok == TokenType.Identifier) { type_name = self.tok_val; }
            self.parse_type_hint();
            if (type_tok == TokenType.KwString) { ptype = 2.0; }
            if (type_name == "String") { ptype = 2.0; }
            if (type_name != "" && flux_hm_has(self.structs, type_name) == 1.0) {
                flux_hm_put(self.var_type_names, pname, type_name);
            }
        }
        flux_type_store(pname, ptype);
        flux_hm_put(pnames, flux_str_from_char(48.0), pname);
        flux_llvm_type_arg_push(t_double(self.ctx));
        argc = argc + 1.0;
        while (self.tok_type == TokenType.Comma) {
            self.lex_next();
            let pname2 = self.tok_val;
            self.tok_expect(TokenType.Identifier, "expected param");
            let ptype2 = 1.0;
            if (self.tok_type == TokenType.Colon) {
                self.lex_next();
                let type_tok = self.tok_type;
                let type_name2 = "";
                if (type_tok == TokenType.Identifier) { type_name2 = self.tok_val; }
                self.parse_type_hint();
                if (type_tok == TokenType.KwString) { ptype2 = 2.0; }
                if (type_name2 == "String") { ptype2 = 2.0; }
                if (type_name2 != "" && flux_hm_has(self.structs, type_name2) == 1.0) {
                    flux_hm_put(self.var_type_names, pname2, type_name2);
                }
            }
            flux_type_store(pname2, ptype2);
            flux_hm_put(pnames, flux_str_from_char(48.0 + argc), pname2);
            flux_llvm_type_arg_push(t_double(self.ctx));
            argc = argc + 1.0;
        }
    }
    self.tok_expect(TokenType.RParen, "expected ')'");

    let ret_type = t_double(self.ctx);
    if (self.tok_type == TokenType.Arrow) {
        self.lex_next();
        ret_type = t_double(self.ctx);
        self.parse_type_hint();
    }

    if (async_flag == 1.0) {
        flux_llvm_type_arg_push(flux_llvm_pointer_type_in_ctx(self.ctx, 0.0));
        argc = argc + 1.0;
    }

    let ft = flux_llvm_function_type(ret_type, argc, 0.0);
    let func = flux_llvm_get_named_function(self.mod, name);
    if (func == 0.0) {
        func = flux_llvm_add_function(self.mod, name, ft);
        flux_hm_put(self.globals, name, func);
    }
    else {
        flux_hm_put(self.globals, name, func);
    }
    flux_hm_put(self.func_types, name, ft);

    let prev_fn = self.cur_fn;
    self.cur_fn = func;

    let entry = flux_llvm_append_basic_block(func, "entry");
    self.entry_bb = entry;
    flux_llvm_position_builder_at_end(self.bld, entry);

    // Alloca params (skip hidden async state param)
    let real_argc = argc;
    if (async_flag == 1.0) { real_argc = argc - 1.0; }
    let i = 0.0;
    while (i < real_argc) {
        let pval = flux_llvm_get_param(func, i);
        let pname = flux_hm_get(pnames, flux_str_from_char(48.0 + i));
        let a = flux_llvm_build_alloca(self.bld, t_double(self.ctx), pname);
        flux_llvm_build_store(self.bld, pval, a);
        flux_hm_put(self.locals, pname, a);
        i = i + 1.0;
    }
    flux_hm_destroy(pnames);

    // Async setup: state param, result alloca, dispatcher, start block
    if (async_flag == 1.0) {
        self.async_state_alloca = flux_llvm_get_param(func, argc - 1.0);
        self.async_result_alloca = flux_llvm_build_alloca(self.bld, t_double(self.ctx), "async_result");
        let dispatcher_bb = flux_llvm_append_basic_block(func, "async_dispatch");
        self.async_dispatcher_bb = dispatcher_bb;
        flux_llvm_build_br(self.bld, dispatcher_bb);
        flux_llvm_position_builder_at_end(self.bld, dispatcher_bb);
        let start_bb = flux_llvm_append_basic_block(func, "async_start");
        flux_hm_put(self.async_resume_targets, "0", start_bb);
        self.async_await_count = 0.0;
        flux_llvm_position_builder_at_end(self.bld, start_bb);
    }

    // Body
    if (self.tok_type == TokenType.LBrace) {
        let body = self.parse_block();
        flux_llvm_build_ret(self.bld, body);
    }
    else {
        let body = self.parse_expr();
        flux_llvm_build_ret(self.bld, body);
    }

    // Build async dispatcher (cond_br chain)
    if (async_flag == 1.0) {
        flux_llvm_position_builder_at_end(self.bld, self.async_dispatcher_bb);
        let state_ptr = flux_llvm_build_struct_gep2(self.bld, async_state_struct_ty_local, self.async_state_alloca, 0.0, "sp");
        let state_val = flux_llvm_build_load2(self.bld, t_int32(self.ctx), state_ptr, "state");
        let total_targets = self.async_await_count + 1.0;
        let cond_eq = 0.0;
        let si = 0.0;
        while (si < total_targets) {
            let target = flux_hm_get(self.async_resume_targets, flux_dtoa(si));
            let next_cmp = flux_llvm_append_basic_block(self.cur_fn, "dnext");
            let si_const = flux_llvm_const_int(t_int32(self.ctx), si, 0.0);
            let cmp = flux_llvm_build_icmp(self.bld, cond_eq, state_val, si_const, "dcmp");
            flux_llvm_build_cond_br(self.bld, cmp, target, next_cmp);
            flux_llvm_position_builder_at_end(self.bld, next_cmp);
            si = si + 1.0;
        }
        let start_bb = flux_hm_get(self.async_resume_targets, "0");
        flux_llvm_build_br(self.bld, start_bb);
    }

    self.cur_fn = prev_fn;
    self.entry_bb = 0.0;
    self.locals = flux_hm_create();
}
// ===================================================================
// STRUCT / ENUM / CLASS / IMPL / TRAIT (stubs)
// ===================================================================

def parse_struct(self: Compiler) {
    self.lex_next(); // eat 'struct'
    let name = self.tok_val;
    self.tok_expect(TokenType.Identifier, "expected name");

    if (self.tok_type == TokenType.LBracket) {
        self.lex_next();
        while (self.tok_type != TokenType.RBracket && self.tok_type != TokenType.Eof) {
            self.lex_next();
            if (self.tok_type == TokenType.Comma) { self.lex_next(); }
        }
        self.tok_expect(TokenType.RBracket, "expected ']'");
    }

    let st = flux_llvm_struct_create_named(self.ctx, name);
    flux_hm_put(self.structs, name, st);

    self.tok_expect(TokenType.LBrace, "expected '{'");
    flux_llvm_type_arg_reset();
    let fc = 0.0;
    while (self.tok_type != TokenType.RBrace && self.tok_type != TokenType.Eof) {
        let fname = self.tok_val;
        self.tok_expect(TokenType.Identifier, "expected field name");
        self.tok_expect(TokenType.Colon, "expected ':'");
        self.parse_type_hint();
        flux_llvm_type_arg_push(t_double(self.ctx));
        flux_hm_put(self.struct_fields, "." + fname, fc);
        fc = fc + 1.0;
        if (self.tok_type == TokenType.Comma) { self.lex_next(); }
    }
    self.tok_expect(TokenType.RBrace, "expected '}'");
    flux_llvm_struct_set_body(st, fc, 0.0);
}

def parse_enum(self: Compiler) {
    self.lex_next();
    let ename = self.tok_val;
    self.tok_expect(TokenType.Identifier, "expected enum name");
    flux_hm_put(self.enum_types, ename, 0.0);
    let has_any_payload = 0.0;
    let max_fields = 0.0;
    let disc = 0.0;
    if (self.tok_type == TokenType.LBrace) {
        self.lex_next();
        while (self.tok_type != TokenType.RBrace && self.tok_type != TokenType.Eof) {
            let var_name = self.tok_val;
            self.tok_expect(TokenType.Identifier, "expected variant name");
            let has_payload = 0.0;
            let field_count = 0.0;
            if (self.tok_type == TokenType.LBrace) {
                self.lex_next();
                let fc = 0.0;
                while (self.tok_type != TokenType.RBrace && self.tok_type != TokenType.Eof) {
                    let fname = self.tok_val;
                    self.tok_expect(TokenType.Identifier, "expected field");
                    self.tok_expect(TokenType.Colon, "expected ':'");
                    self.parse_type_hint();
                    flux_llvm_type_arg_push(t_double(self.ctx));
                    flux_hm_put(self.struct_fields, "$" + ename + "." + fname, fc + 1.0);
                    fc = fc + 1.0;
                    if (self.tok_type == TokenType.Comma) { self.lex_next(); }
                }
                self.tok_expect(TokenType.RBrace, "expected '}'");
                has_payload = 1.0;
                field_count = fc;
                if (fc > max_fields) { max_fields = fc; }
            }
            else if (self.tok_type == TokenType.LParen) {
                self.lex_next();
                if (self.tok_type == TokenType.Identifier) {
                    self.lex_next();
                    if (self.tok_type == TokenType.Colon) {
                        self.lex_next();
                    }
                }
                self.parse_type_hint();
                self.tok_expect(TokenType.RParen, "expected ')'");
                has_payload = 1.0;
                field_count = 1.0;
                if (1.0 > max_fields) { max_fields = 1.0; }
            }
            flux_hm_put(self.enum_variants, ename + "." + var_name, disc);
            if (has_payload == 1.0) {
                flux_hm_put(self.enum_payloads, ename + "." + var_name, field_count);
                has_any_payload = 1.0;
            }
            disc = disc + 1.0;
            if (self.tok_type == TokenType.Comma) { self.lex_next(); }
        }
        self.tok_expect(TokenType.RBrace, "expected '}'");
    }
    if (has_any_payload == 1.0) {
        flux_llvm_type_arg_reset();
        flux_llvm_type_arg_push(t_int32(self.ctx));
        let fi = 0.0;
        while (fi < max_fields) {
            flux_llvm_type_arg_push(t_double(self.ctx));
            fi = fi + 1.0;
        }
        let tu = flux_llvm_struct_create_named(self.ctx, ename);
        flux_llvm_struct_set_body(tu, max_fields + 1.0, 0.0);
        flux_hm_put(self.enum_types, ename, tu);
    }
}

def parse_class(self: Compiler) {
    self.lex_next();
    let name = self.tok_val;
    self.tok_expect(TokenType.Identifier, "expected class name");
    if (self.tok_type == TokenType.LBracket) {
        self.lex_next();
        while (self.tok_type != TokenType.RBracket && self.tok_type != TokenType.Eof) {
            self.lex_next();
            if (self.tok_type == TokenType.Comma) { self.lex_next(); }
        }
        self.tok_expect(TokenType.RBracket, "expected ']'");
    }
    let st = flux_llvm_struct_create_named(self.ctx, name);
    flux_hm_put(self.structs, name, st);
    self.tok_expect(TokenType.LBrace, "expected '{'");
    flux_llvm_type_arg_reset();
    let fc = 0.0;
    while (self.tok_type != TokenType.RBrace && self.tok_type != TokenType.Eof) {
        if (self.tok_type == TokenType.KwDef) {
            self.parse_fn_def(0.0, name);
        }
        else {
            let fname = self.tok_val;
            self.tok_expect(TokenType.Identifier, "expected field name");
            self.tok_expect(TokenType.Colon, "expected ':'");
            self.parse_type_hint();
            flux_llvm_type_arg_push(t_double(self.ctx));
            flux_hm_put(self.struct_fields, "." + fname, fc);
            fc = fc + 1.0;
        }
    }
    self.tok_expect(TokenType.RBrace, "expected '}'");
    flux_llvm_struct_set_body(st, fc, 0.0);
}

def parse_impl(self: Compiler) {
    self.lex_next();
    if (self.tok_type == TokenType.LBracket) {
        self.lex_next();
        while (self.tok_type != TokenType.RBracket && self.tok_type != TokenType.Eof) {
            self.lex_next();
            if (self.tok_type == TokenType.Comma) { self.lex_next(); }
        }
        self.tok_expect(TokenType.RBracket, "expected ']'");
    }
    let first_ident = self.tok_val;
    self.tok_expect(TokenType.Identifier, "expected type/trait name");

    let is_trait_impl = 0.0;
    let trait_name = "";
    let type_name = first_ident;

    // Check for "for" keyword indicating a trait impl: impl TraitName for TypeName
    if (self.tok_type == TokenType.KwFor) {
        trait_name = first_ident;
        self.lex_next(); // eat 'for'
        type_name = self.tok_val;
        self.tok_expect(TokenType.Identifier, "expected type name after 'for'");
        is_trait_impl = 1.0;
    }

    let assoc_bind_count = 0.0;
    if (self.tok_type == TokenType.LBrace) {
        self.lex_next();
        while (self.tok_type != TokenType.RBrace && self.tok_type != TokenType.Eof) {
            if (self.tok_type == TokenType.KwDef) {
                self.parse_fn_def(0.0, type_name);
            }
            else if (self.tok_type == TokenType.Identifier && self.tok_val == "type") {
                self.lex_next();
                let assoc_name = "";
                if (self.tok_type == TokenType.Identifier) {
                    assoc_name = self.tok_val;
                    self.lex_next();
                }
                // Optional = TypeName
                if (self.tok_type == TokenType.Assign) {
                    self.lex_next();
                    self.parse_type_hint();
                }
                if (self.tok_type == TokenType.Semicolon) { self.lex_next(); }
                // Validate: check this assoc type exists in the trait
                if (is_trait_impl == 1.0 && assoc_name != "") {
                    if (flux_hm_has(self.trait_assoc_type_names, trait_name + "." + assoc_name) == 1.0) {
                        assoc_bind_count = assoc_bind_count + 1.0;
                    }
                    else {
                        self.parse_err("unknown associated type: " + assoc_name + " for trait " + trait_name);
                    }
                }
            }
            else { break; }
        }
        self.tok_expect(TokenType.RBrace, "expected '}'");
    }

    // Validate all associated types are bound
    if (is_trait_impl == 1.0) {
        let expected_count = flux_hm_get(self.trait_assoc_type_counts, trait_name);
        if (expected_count > 0.0 && assoc_bind_count < expected_count) {
            self.parse_err("missing associated type bindings in impl " + trait_name + " for " + type_name);
        }
    }

    // Generate vtable for trait impl
    if (is_trait_impl == 1.0) {
        flux_hm_put(self.trait_impls, trait_name + "." + type_name, 1.0);
        let method_count = flux_hm_get(self.trait_method_counts, trait_name);
        if (method_count > 0.0) {
            let vtable_type_name = "__vtable_" + trait_name + "_for_" + type_name;
            let i8_ptr = flux_llvm_pointer_type_in_ctx(self.ctx, 0.0);

            // Build vtable struct type: [method_count + 1 x i8*] (slot 0 reserved)
            flux_llvm_type_arg_reset();
            let f = 0.0;
            while (f <= method_count) {
                flux_llvm_type_arg_push(i8_ptr);
                f = f + 1.0;
            }
            let vtable_sty = flux_llvm_struct_create_named(self.ctx, vtable_type_name);
            flux_llvm_struct_set_body(vtable_sty, method_count + 1.0, 0.0);

            // Build initializer: slot 0 = null, slots 1..method_count = fn pointers
            flux_llvm_const_arg_reset();
            flux_llvm_const_arg_push(flux_llvm_const_null(i8_ptr)); // slot 0 reserved

            let dot_prefix = trait_name + ".";
            let dot_prefix_len = flux_str_len(dot_prefix);
            let m = 0.0;
            while (m < method_count) {
                let keys = flux_hm_keys(self.trait_method_indices);
                let keys_len = flux_str_len(keys);
                let line_start = 0.0;
                let found = 0.0;
                let k = 0.0;
                while (k <= keys_len && found == 0.0) {
                    let c = if (k < keys_len) { flux_str_at(keys, k) } else { 10.0 };
                    if (c == 10.0) {
                        if (k > line_start) {
                            let line = flux_str_slice(keys, line_start, k);
                            let line_len = flux_str_len(line);
                            if (line_len > dot_prefix_len) {
                                if (flux_strcmp(flux_str_slice(line, 0.0, dot_prefix_len), dot_prefix) == 0.0) {
                                    if (flux_hm_get(self.trait_method_indices, line) == m) {
                                        let method_name = flux_str_slice(line, dot_prefix_len, line_len);
                                        let fn_val = flux_llvm_get_named_function(self.mod, type_name + "_" + method_name);
                                        if (fn_val != 0.0) {
                                            flux_llvm_const_arg_push(flux_llvm_const_bitcast(fn_val, i8_ptr));
                                            found = 1.0;
                                        }
                                    }
                                }
                            }
                        }
                        line_start = k + 1.0;
                    }
                    k = k + 1.0;
                }
                if (found == 0.0) {
                    flux_llvm_const_arg_push(flux_llvm_const_null(i8_ptr));
                }
                m = m + 1.0;
            }

            let vtable_init = flux_llvm_const_named_struct(vtable_sty, method_count + 1.0);
            let vtable_global = flux_llvm_add_global(self.mod, vtable_sty, vtable_type_name);
            flux_llvm_set_initializer(vtable_global, vtable_init);
            flux_llvm_set_linkage(vtable_global, 1.0); // InternalLinkage

            flux_hm_put(self.trait_vtables, trait_name + "." + type_name, vtable_global);
        }
    }
}

def parse_trait(self: Compiler) {
    self.lex_next();
    let name = self.tok_val;
    self.tok_expect(TokenType.Identifier, "expected trait name");
    self.tok_expect(TokenType.LBrace, "expected '{'");
    let idx = 0.0;
    let assoc_idx = 0.0;
    while (self.tok_type != TokenType.RBrace && self.tok_type != TokenType.Eof) {
        if (self.tok_type == TokenType.KwDef) {
            self.lex_next();
            let mname = self.tok_val;
            self.tok_expect(TokenType.Identifier, "expected method name");
            // Skip params (self, ...)
            if (self.tok_type == TokenType.LParen) {
                self.lex_next();
                let depth = 1.0;
                while (depth > 0.0 && self.tok_type != TokenType.Eof) {
                    if (self.tok_type == TokenType.LParen) { depth = depth + 1.0; }
                    if (self.tok_type == TokenType.RParen) { depth = depth - 1.0; }
                    if (depth > 0.0) { self.lex_next(); }
                }
            }
            // Skip -> ReturnType
            if (self.tok_type == TokenType.Arrow) {
                self.lex_next();
                self.parse_type_hint();
            }
            // Skip optional body
            if (self.tok_type == TokenType.LBrace) {
                self.lex_next();
                let depth2 = 1.0;
                while (depth2 > 0.0 && self.tok_type != TokenType.Eof) {
                    if (self.tok_type == TokenType.LBrace) { depth2 = depth2 + 1.0; }
                    if (self.tok_type == TokenType.RBrace) { depth2 = depth2 - 1.0; }
                    if (depth2 > 0.0) { self.lex_next(); }
                }
            }
            flux_hm_put(self.trait_method_indices, name + "." + mname, idx);
            idx = idx + 1.0;
        }
        else if (self.tok_val == "type" && self.tok_type == TokenType.Identifier) {
            self.lex_next();
            if (self.tok_type == TokenType.Identifier) {
                let assoc_name = self.tok_val;
                self.lex_next();
                self.tok_expect(TokenType.Semicolon, "expected ';' after associated type");
                flux_hm_put(self.trait_assoc_type_names, name + "." + assoc_name, assoc_idx);
                assoc_idx = assoc_idx + 1.0;
            }
            else { self.lex_next(); }
        }
        else { self.lex_next(); }
    }
    self.tok_expect(TokenType.RBrace, "expected '}'");
    flux_hm_put(self.trait_method_counts, name, idx);
    flux_hm_put(self.trait_assoc_type_counts, name, assoc_idx);
}
}
