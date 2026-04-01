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

    // operators
    tok_power = -16,        // ^
    tok_logical_and = -17,  // &&
    tok_logical_or = -18,   // ||
    tok_logical_not = -19,  // !
    tok_equal = -20,        // ==
    tok_not_equal = -21,    // !=
    tok_less_equal = -22,   // <=
    tok_greater_equal = -23, // >=

    // Bitwise operators
    tok_bitwise_and = -24,  // &
    tok_bitwise_or = -25,   // |
    tok_bitwise_xor = -26,  // xor keyword
    tok_bitwise_not = -27,  // ~
    tok_bitwise_shl = -28,  // <<
    tok_bitwise_shr = -29,  // >>
    tok_bitwise_compl = -30, // ~ (alias for bitwise_not)

    // Type keywords
    tok_type_float = -31,
    tok_type_double = -32,
    tok_type_int = -33,
    tok_type_void = -34,
    tok_type_complex = -35,

    // Type annotation
    tok_arrow = -36,         // ->

    // Complex numbers
    tok_imaginary = -37,     // Number with 'j' suffix (e.g., 2.0j)
    tok_complex = -38,       // Full complex literal (e.g., 1.0+2.0j)

    // String literals
    tok_string = -39,         // Quoted string (e.g., "hello")

    // Element-wise operators (MATLAB-style)
    tok_ew_mul = -40,         // .*  element-wise multiply
    tok_ew_div = -41,         // ./  element-wise divide
    tok_ew_power = -42,       // .^  element-wise power

    // Block delimiters
    tok_lbrace = -43,         // {
    tok_rbrace = -44          // }
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

private:
    int gettok();
    int advance();

    std::string m_input;
    size_t m_pos;
    int m_lastChar;
};

} // namespace Flux

#endif // FLUX_COMPILER_LEXER_H
