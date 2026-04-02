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
    tok_debug = -62,
    tok_sensitivity = -63,
    tok_ask = -64,
    tok_explain = -65,
    tok_substitute = -61,
    tok_corner = -66,
    tok_yield = -67,

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
    tok_pp_warning = -61,      // #warning
    tok_pp_error = -62,        // #error

    // Compound assignment operators
    tok_plus_equal = -63,      // +=
    tok_minus_equal = -64,     // -=
    tok_star_equal = -65,      // *=
    tok_slash_equal = -66,     // /=

    // Control flow
    tok_break = -67,
    tok_continue = -68,
    tok_switch = -69,
    tok_case = -70,
    tok_default = -71,

    // Matrix operations
    tok_transpose = -72,       // ' (tick)
    tok_dot = -73,             // . (member access)
    
    // Namespace operator
    tok_namespace_sep = -74,   // :: (namespace separator)
};

class Lexer {
public:
    explicit Lexer(const std::string& input);

    int getNextToken();

    // Context for the current token
    std::string IdentifierStr; // Filled if tok_identifier
    double NumVal;             // Filled if tok_number
    std::string StringVal;     // Filled if tok_string

    int CurTok;
    
    // Position tracking for error messages
    int getCurrentLine() const { return m_line; }
    int getCurrentColumn() const { return m_column; }
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
    size_t m_lineStart;
    size_t m_currentTokenOffset;
    size_t m_currentTokenLength;
    std::unique_ptr<llvm::MemoryBuffer> m_buffer;
    std::unique_ptr<llvm::SourceMgr> m_sourceMgr;
};

} // namespace Flux

#endif // FLUX_COMPILER_LEXER_H
