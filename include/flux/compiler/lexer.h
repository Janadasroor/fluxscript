/* Copyright 2026 Janada Sroor
 SPDX-License-Identifier: Apache-2.0
 http://www.apache.org/licenses/LICENSE-2.0
 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at
     http://www.apache.org/licenses/LICENSE-2.0
 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License. */

#ifndef FLUX_COMPILER_LEXER_H
#define FLUX_COMPILER_LEXER_H

#include <string>
#include <vector>
#include <memory>
#include <cstddef>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/SMLoc.h>
#include <llvm/Support/SourceMgr.h>

namespace Flux {

enum class TokenType {
    tok_eof = -1,

    // commands
    tok_def = -2,
    tok_extern = -3,

    // primary
    tok_identifier = -4,
    tok_number = -5,
    tok_return = -6,
    tok_var = -7,
    tok_if = -8,
    tok_then = -9,
    tok_else = -10,
    tok_for = -11,
    tok_in = -12,
    tok_do = -13,
    tok_while = -14,
    tok_let = -15,
    tok_fn = -16,
    tok_import = -17,
    tok_debug = -136,
    tok_sensitivity = -137,
    tok_ask = -138,
    tok_explain = -139,
    tok_substitute = -140,
    tok_corner = -141,
    tok_yield = -142,

    // operators
    tok_power = -18,        // ^
    tok_logical_and = -19,  // &&
    tok_logical_or = -20,   // ||
    tok_logical_not = -21,  // !
    tok_equal = -22,        // ==
    tok_not_equal = -23,    // !=
    tok_less_equal = -24,   // <=
    tok_greater_equal = -25, // >=

    // Bitwise operators
    tok_bitwise_and = -26,  // &
    tok_bitwise_or = -27,   // |
    tok_bitwise_xor = -28,  // xor keyword
    tok_bitwise_not = -29,  // ~
    tok_bitwise_shl = -30,  // <<
    tok_bitwise_shr = -31,  // >>
    tok_bitwise_compl = -32, // ~ (alias for bitwise_not)

    // Type keywords
    tok_type_float = -33,
    tok_type_double = -34,
    tok_type_int = -35,
    tok_type_void = -36,
    tok_type_complex = -37,
    tok_type_string = -38,
    tok_type_vector = -39,
    tok_type_matrix = -40,

    // Type annotation
    tok_arrow = -41,         // ->

    // Complex numbers
    tok_imaginary = -42,     // Number with 'j' suffix (e.g., 2.0j)
    tok_complex = -43,       // Full complex literal (e.g., 1.0+2.0j)

    // String literals
    tok_string = -44,         // Quoted string (e.g., "hello")

    // Element-wise operators (MATLAB-style)
    tok_ew_mul = -45,         // .*  element-wise multiply
    tok_ew_div = -46,         // ./  element-wise divide
    tok_ew_power = -47,       // .^  element-wise power

    // Block delimiters
    tok_lbrace = -48,         // {
    tok_rbrace = -49,         // }

    // Matrix row separator
    tok_semicolon = -50,       // ;  row separator in matrix literals

    // Slicing
    tok_colon = -51,           // :  range/slice operator

    // Conditional compilation (preprocessor)
    tok_pp_ifdef = -52,        // #ifdef
    tok_pp_ifndef = -53,       // #ifndef
    tok_pp_endif = -54,        // #endif
    tok_pp_define = -55,       // #define
    tok_pp_undef = -56,        // #undef
    tok_pp_if = -57,           // #if
    tok_pp_elif = -58,         // #elif
    tok_pp_else = -59,         // #else
    tok_pp_defined = -60,      // defined()
    tok_pp_warning = -143,     // #warning
    tok_pp_error = -144,       // #error

    // Compound assignment operators
    tok_plus_equal = -145,     // +=
    tok_minus_equal = -146,    // -=
    tok_star_equal = -147,     // *=
    tok_slash_equal = -148,    // /=

    // Control flow
    tok_break = -149,
    tok_continue = -68,
    tok_switch = -69,
    tok_case = -70,
    tok_default = -71,

    // Matrix operations
    tok_transpose = -72,       // ' (tick)
    tok_dot = -73,             // . (member access)
    
    // Namespace operator
    tok_namespace_sep = -74,   // :: (namespace separator)
    
    // Additional keywords
    tok_try = -75,             // try block
    tok_catch = -76,           // catch block
    tok_finally = -77,         // finally block
    tok_throw = -78,           // throw exception
    tok_assert = -79,          // assertion
    tok_static = -80,          // static variable
    tok_const = -81,           // const variable
    tok_enum = -82,            // enumeration
    tok_struct = -83,          // struct definition
    tok_class = -84,           // class definition
    tok_public = -85,          // public access
    tok_private = -86,         // private access
    tok_protected = -87,       // protected access
    tok_virtual = -88,         // virtual function
    tok_override = -89,        // override function
    tok_inline = -90,          // inline function
    tok_noexcept = -91,        // noexcept function
    tok_export = -92,          // export symbol
    tok_namespace = -93,       // namespace block
    tok_using = -94,           // using directive
    tok_typedef = -95,         // typedef
    tok_alias = -96,           // type alias
    tok_match = -97,           // pattern matching
    tok_guard = -99,           // guard condition
    tok_foreach = -102,        // foreach loop
    tok_repeat = -103,         // repeat loop
    tok_until = -104,          // until condition
    tok_parallel = -157,       // parallel execution
    tok_schematic = -158,       // schematic block
    tok_component = -159,       // component declaration
    tok_connect = -160,         // connection directive
    tok_port = -161,            // port declaration
    tok_net = -162,             // net declaration
    tok_endschematic = -163,    // end schematic block
    tok_pin = -164,             // pin specification
    tok_sym = -165,               // symbolic variable
    tok_solve = -166,             // solve equation
    tok_simplify = -167,          // simplify expression
    tok_differentiate = -168,     // derivative
    tok_integrate = -169,         // integral
    tok_laplace = -170,           // Laplace transform
    tok_inverse_laplace = -171,   // Inverse Laplace
    tok_evaluate = -172,          // Evaluate symbolic expression
    tok_jacobian = -195,          // Jacobian matrix
    tok_pde = -198,               // PDE solver
    tok_partial_diff = -199,      // Partial derivative
    tok_expand = -173,            // expand expression
    tok_factor = -174,            // factor expression
    tok_numerator = -175,         // get numerator
    tok_denominator = -176,       // get denominator
    tok_poles = -177,             // find poles
    tok_zeros = -178,             // find zeros
    tok_collect = -179,           // collect terms
    tok_time = -180,               // time built-in variable
    tok_inputs = -181,             // inputs dictionary
    tok_outputs = -182,            // outputs dictionary
    tok_update = -183,             // update function
    tok_bsource = -184,            // B-source declaration
    tok_montecarlo = -194,         // Monte Carlo analysis
    tok_worstcase = -195,          // Worst-case analysis
    tok_stability = -302,          // Stability analysis
    // tok_sensitivity already defined at -137
    tok_optimize = -303,           // Optimization
    tok_fft = -304,                // FFT analysis
    tok_phasor = -305,             // Phasor type
    tok_bode = -306,               // Bode plot
    tok_esource = -190,            // E-source (VCVS)
    tok_fsource = -191,            // F-source (CCCS)
    tok_gsource = -192,            // G-source (VCCS)
    tok_hsource = -193,            // H-source (CCVS)
    tok_dt = -196,                 // timestep
    tok_temp = -197,               // temperature
    tok_node = -185,               // node voltage
    tok_initial = -186,            // initial condition
    tok_transient = -187,          // transient analysis
    tok_timestep = -188,           // simulation timestep
    tok_simtime = -189,            // simulation time variable

    // Analysis Control
    tok_analysis = -116,       // analysis directive
    tok_tran = -117,           // .TRAN transient analysis
    tok_dc = -118,             // .DC sweep
    tok_ac = -119,             // .AC analysis
    tok_noise = -120,          // .NOISE analysis
    tok_op = -121,             // .OP operating point
    tok_tf = -122,             // .TF transfer function
    tok_sens = -123,           // .SENS sensitivity
    tok_fourier = -124,        // .FOUR fourier analysis

    // Measurements
    tok_measure = -125,        // .MEAS measurement
    tok_max = -126,            // MAX measurement
    tok_min = -127,            // MIN measurement
    tok_avg = -128,            // AVG measurement
    tok_rms = -129,            // RMS measurement
    tok_trig = -130,           // TRIG measurement
    tok_targ = -131,           // TARG measurement
    tok_meas_when = -132,      // WHEN measurement (renamed to avoid conflict with tok_when = -98)
    tok_find = -133,           // FIND measurement
    tok_deriv = -134,          // DERIV measurement
    tok_integ = -135,          // INTEG measurement

    // Probing
    tok_probe = -150,          // probe directive
    tok_save = -151,           // save directive

    // Subcircuits and Models
    tok_subckt = -152,         // subckt definition
    tok_ends = -153,           // ends subcircuit
    tok_model = -154,          // model declaration
    tok_param = -155,          // param keyword
    tok_ic = -156,             // initial condition
    
    /* Hierarchical Design */
    tok_instance = -190,           // subckt instance (X prefix)
    tok_params = -191,             // parameter block
    tok_hier = -192,               // hierarchical separator
    
    /* Verilog-A Lite */
    tok_analog = -193,             // analog block
    tok_contributor = -194,        // <+ operator
    tok_branch = -195,             // branch declaration
    tok_V = -196,                  // V() voltage access
    tok_I = -197,                  // I() current access
    tok_ddt = -198,                // ddt() time derivative
    tok_idt = -199,                // idt() time integral
    tok_abstol = -201,             // absolute tolerance
    tok_reltol = -202,             // relative tolerance
    
    /* Symbol Pin Mapping */
    tok_symbol = -203,             // symbol declaration
    tok_pinmap = -204,             // pin mapping
    tok_map = -205,                // map keyword
    tok_endsymbol = -206,          // end symbol
    tok_contributor_op = -207,     // <+ contributor operator
    
    /* Section 7.1: Model Quality & Verification */
    tok_golden = -212,               // golden waveform reference
    tok_compare = -213,              // waveform comparison
    tok_tolerance = -214,            // tolerance specification
    tok_converge = -215,             // convergence check
    tok_diagnostic = -216,           // diagnostic directive
    tok_discontinuity = -217,        // discontinuity detection
    tok_state = -218,                // hidden state detection
    tok_settle = -219,               // settling time analysis
    tok_verify = -220,               // verification block

    /* Section 7.2: Mixed-Signal & Modeling Extensions */
    // Event-driven constructs
    tok_cross = -221,                // cross() zero-crossing detection
    tok_above = -222,                // above() threshold detection
    tok_timer = -223,                // timer() event timer

    // Real-valued digital modeling
    tok_fsm = -224,                  // finite state machine
    tok_edge = -225,                 // edge trigger (posedge, negedge)
    tok_triggered = -226,            // triggered block
    tok_state_machine = -227,        // state machine declaration

    // Noise modeling primitives
    tok_noise_fn = -228,             // noise() function
    tok_white_noise = -229,          // white noise
    tok_flicker_noise = -230,        // flicker (1/f) noise
    tok_thermal_noise = -231,        // thermal noise

    // Piecewise and table-based models
    tok_piecewise = -232,            // piecewise function
    tok_table = -233,                // table lookup
    tok_interpolate = -234,          // interpolation mode
    tok_csv_import = -235,           // CSV import

    // Units and dimensional analysis
    tok_unit = -236,                 // unit annotation
    tok_dimension = -237,            // dimension check
    tok_convert = -238,              // unit conversion
    tok_has_unit = -239,             // has_unit() predicate

    // Advanced features tokens
    tok_title = -240,
    tok_color = -241,
    tok_grid = -242,
    tok_autoscale = -243,
    tok_metric = -244,
    tok_goals = -245,
    tok_tune = -246,
    tok_algorithm = -247,
    tok_runs = -248,
    tok_gpu = -249,
    tok_controls = -250,
    tok_slider = -251,
    tok_knob = -252,
    tok_sections = -253,
    tok_include = -254,
};

class Lexer {
public:
    explicit Lexer(const std::string& input);

    int getNextToken();
    int peekToken();  // Look ahead without consuming

    static std::string tokenSpelling(int token);

    // Context for the current token
    std::string IdentifierStr; // Filled if tok_identifier
    double NumVal;             // Filled if tok_number
    std::string StringVal;     // Filled if tok_string

    int CurTok;
    
    // Position tracking for error messages
    int getCurrentLine() const { return m_currentTokenLine; }
    int getCurrentColumn() const { return m_currentTokenColumn; }
    size_t getCurrentTokenOffset() const { return m_currentTokenOffset; }
    size_t getCurrentTokenLength() const { return m_currentTokenLength; }
    std::string getCurrentTokenText() const;
    std::string getCurrentLineText() const;
    void reportError(const std::string& message) const;

private:
    int gettok();
    int advance();
    llvm::SMLoc getCurrentTokenLoc() const;

    std::string m_input;
    size_t m_pos;
    size_t m_tokenStart;
    int m_lastChar;
    int m_line;
    int m_column;
    int m_currentTokenLine;
    int m_currentTokenColumn;
    size_t m_lineStart;
    size_t m_currentTokenOffset;
    size_t m_currentTokenLength;
    std::unique_ptr<llvm::MemoryBuffer> m_buffer;
    std::unique_ptr<llvm::SourceMgr> m_sourceMgr;
};

} // namespace Flux

#endif // FLUX_COMPILER_LEXER_H
