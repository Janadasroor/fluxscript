#ifndef FLUX_COMPILER_LEXER_H
#define FLUX_COMPILER_LEXER_H

#include <string>
#include <vector>

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

    // Type annotation
    tok_arrow = -39,         // ->

    // Complex numbers
    tok_imaginary = -40,     // Number with 'j' suffix (e.g., 2.0j)
    tok_complex = -41,       // Full complex literal (e.g., 1.0+2.0j)

    // String literals
    tok_string = -42,         // Quoted string (e.g., "hello")

    // Element-wise operators (MATLAB-style)
    tok_ew_mul = -43,         // .*  element-wise multiply
    tok_ew_div = -44,         // ./  element-wise divide
    tok_ew_power = -45,       // .^  element-wise power

    // Block delimiters
    tok_lbrace = -46,         // {
    tok_rbrace = -47,         // }

    // Matrix row separator
    tok_semicolon = -48,       // ;  row separator in matrix literals

    // Slicing
    tok_colon = -58,           // :  range/slice operator

    // Compound assignment operators
    tok_plus_equal = -49,      // +=
    tok_minus_equal = -50,     // -=
    tok_star_equal = -51,      // *=
    tok_slash_equal = -52,     // /=

    // Control flow
    tok_break = -53,
    tok_continue = -54,
    tok_switch = -55,
    tok_case = -56,
    tok_default = -57,

    // Matrix operations
    tok_transpose = -59        // ' (tick)
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
    std::string getCurrentLineText() const;

private:
    int gettok();
    int advance();

    std::string m_input;
    size_t m_pos;
    int m_lastChar;
    int m_line;
    int m_column;
    size_t m_lineStart;
};

} // namespace Flux

#endif // FLUX_COMPILER_LEXER_H
