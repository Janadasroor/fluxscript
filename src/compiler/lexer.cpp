#include "flux/compiler/lexer.h"
#include <cctype>
#include <cstdlib>
#include <iostream>

namespace Flux {

Lexer::Lexer(const std::string& input) 
    : m_input(input), m_pos(0), m_lastChar(' '), m_line(1), m_column(1), m_lineStart(0) {}

int Lexer::advance() {
    if (m_pos < m_input.size()) {
        m_lastChar = m_input[m_pos++];
        if (m_lastChar == '\n') {
            m_line++;
            m_column = 1;
            m_lineStart = m_pos;
        } else {
            m_column++;
        }
    } else {
        m_lastChar = EOF;
    }
    return m_lastChar;
}

std::string Lexer::getCurrentLineText() const {
    if (m_lineStart >= m_input.size()) return "";
    size_t end = m_input.find('\n', m_lineStart);
    if (end == std::string::npos) end = m_input.size();
    return m_input.substr(m_lineStart, end - m_lineStart);
}

int Lexer::gettok() {
    // Skip whitespace
    while (isspace(m_lastChar)) {
        advance();
    }

    if (isalpha(m_lastChar)) { // identifier: [a-zA-Z][a-zA-Z0-9]*
        IdentifierStr = m_lastChar;
        while (isalnum(advance()) || m_lastChar == '_') {
            IdentifierStr += m_lastChar;
        }

        if (IdentifierStr == "def") return static_cast<int>(TokenType::tok_def);
        if (IdentifierStr == "extern") return static_cast<int>(TokenType::tok_extern);
        if (IdentifierStr == "return") return static_cast<int>(TokenType::tok_return);
        if (IdentifierStr == "var") return static_cast<int>(TokenType::tok_var);
        if (IdentifierStr == "let") return static_cast<int>(TokenType::tok_let);
        if (IdentifierStr == "fn") return static_cast<int>(TokenType::tok_fn);
        if (IdentifierStr == "if") return static_cast<int>(TokenType::tok_if);
        if (IdentifierStr == "then") return static_cast<int>(TokenType::tok_then);
        if (IdentifierStr == "else") return static_cast<int>(TokenType::tok_else);
        if (IdentifierStr == "for") return static_cast<int>(TokenType::tok_for);
        if (IdentifierStr == "in") return static_cast<int>(TokenType::tok_in);
        if (IdentifierStr == "do") return static_cast<int>(TokenType::tok_do);
        if (IdentifierStr == "while") return static_cast<int>(TokenType::tok_while);
        if (IdentifierStr == "float") return static_cast<int>(TokenType::tok_type_float);
        if (IdentifierStr == "double") return static_cast<int>(TokenType::tok_type_double);
        if (IdentifierStr == "int") return static_cast<int>(TokenType::tok_type_int);
        if (IdentifierStr == "void") return static_cast<int>(TokenType::tok_type_void);
        if (IdentifierStr == "complex") return static_cast<int>(TokenType::tok_type_complex);
        if (IdentifierStr == "string") return static_cast<int>(TokenType::tok_type_string);
        if (IdentifierStr == "matrix") return static_cast<int>(TokenType::tok_type_matrix);
        if (IdentifierStr == "xor") return static_cast<int>(TokenType::tok_bitwise_xor);
        if (IdentifierStr == "break") return static_cast<int>(TokenType::tok_break);
        if (IdentifierStr == "continue") return static_cast<int>(TokenType::tok_continue);
        if (IdentifierStr == "switch") return static_cast<int>(TokenType::tok_switch);
        if (IdentifierStr == "case") return static_cast<int>(TokenType::tok_case);
        if (IdentifierStr == "default") return static_cast<int>(TokenType::tok_default);

        return static_cast<int>(TokenType::tok_identifier);
    }

    if (isdigit(m_lastChar)) { // Number: [0-9]+ or [0-9]+\.[0-9]*
        std::string NumStr;
        do {
            NumStr += m_lastChar;
            advance();
        } while (isdigit(m_lastChar));
        
        // Check for decimal point followed by digits
        if (m_lastChar == '.') {
            advance(); // consume the '.'
            NumStr += '.';
            // Only continue if there are digits after the decimal point
            if (isdigit(m_lastChar)) {
                do {
                    NumStr += m_lastChar;
                    advance();
                } while (isdigit(m_lastChar));
            }
        }

        // Check for scientific notation: 1e-10, 2.5E+5
        if (m_lastChar == 'e' || m_lastChar == 'E') {
            // Peek to see if it's a number (scientific notation) or a suffix (e.g., in 'meg')
            // If it's 'e' followed by + or - or digit, it's scientific notation.
            char peek = m_input[m_pos];
            if (isdigit(peek) || peek == '+' || peek == '-') {
                NumStr += m_lastChar;
                advance(); // consume 'e'
                if (m_lastChar == '+' || m_lastChar == '-') {
                    NumStr += m_lastChar;
                    advance();
                }
                while (isdigit(m_lastChar)) {
                    NumStr += m_lastChar;
                    advance();
                }
                // Once we have scientific notation, we don't allow SPICE suffixes on top of it.
                NumVal = strtod(NumStr.c_str(), nullptr);
                return static_cast<int>(TokenType::tok_number);
            }
        }

        // Check for imaginary suffix 'j' or 'J'
        if (m_lastChar == 'j' || m_lastChar == 'J') {
            advance(); // consume the 'j'
            NumVal = strtod(NumStr.c_str(), nullptr);
            return static_cast<int>(TokenType::tok_imaginary);
        }

        // Check for SPICE-style unit suffixes
        double multiplier = 1.0;
        bool hasSuffix = false;
        
        if (m_lastChar == 'k' || m_lastChar == 'K') {
            multiplier = 1e3; hasSuffix = true;
        } else if (m_lastChar == 'm' || m_lastChar == 'M') {
            // Peek to see if it's 'meg'
            char peek = m_input[m_pos]; // next char
            if (peek == 'e' || peek == 'E') {
                char peek2 = m_input[m_pos + 1];
                if (peek2 == 'g' || peek2 == 'G') {
                    multiplier = 1e6; hasSuffix = true;
                    advance(); advance(); // consume 'e' and 'g'
                } else {
                    multiplier = 1e-3; hasSuffix = true;
                }
            } else {
                multiplier = 1e-3; hasSuffix = true;
            }
        } else if (m_lastChar == 'u' || m_lastChar == 'U') {
            multiplier = 1e-6; hasSuffix = true;
        } else if (m_lastChar == 'n' || m_lastChar == 'N') {
            multiplier = 1e-9; hasSuffix = true;
        } else if (m_lastChar == 'p' || m_lastChar == 'P') {
            multiplier = 1e-12; hasSuffix = true;
        } else if (m_lastChar == 'f' || m_lastChar == 'F') {
            multiplier = 1e-15; hasSuffix = true;
        } else if (m_lastChar == 'a' || m_lastChar == 'A') {
            multiplier = 1e-18; hasSuffix = true;
        } else if (m_lastChar == 'g' || m_lastChar == 'G') {
            multiplier = 1e9; hasSuffix = true;
        } else if (m_lastChar == 't' || m_lastChar == 'T') {
            multiplier = 1e12; hasSuffix = true;
        }

        if (hasSuffix) {
            advance(); // consume the suffix
            // SPICE ignores any characters after the unit suffix (e.g. 10kOhm = 10k)
            while (isalpha(m_lastChar)) {
                advance();
            }
        }

        NumVal = strtod(NumStr.c_str(), nullptr) * multiplier;
        return static_cast<int>(TokenType::tok_number);
    }
    
    // Handle numbers starting with decimal point (.5) or element-wise operators (.*)
    if (m_lastChar == '.') {
        advance(); // consume the '.'
        if (isdigit(m_lastChar)) {
            std::string NumStr = ".";
            do {
                NumStr += m_lastChar;
                advance();
            } while (isdigit(m_lastChar));
            
            // Check for imaginary suffix
            if (m_lastChar == 'j' || m_lastChar == 'J') {
                advance();
                NumVal = strtod(NumStr.c_str(), nullptr);
                return static_cast<int>(TokenType::tok_imaginary);
            }
            
            NumVal = strtod(NumStr.c_str(), nullptr);
            return static_cast<int>(TokenType::tok_number);
        }
        
        // Element-wise operators (MATLAB-style): .*, ./, .^
        if (m_lastChar == '*') {
            advance();
            return static_cast<int>(TokenType::tok_ew_mul);
        }
        if (m_lastChar == '/') {
            advance();
            return static_cast<int>(TokenType::tok_ew_div);
        }
        if (m_lastChar == '^') {
            advance();
            return static_cast<int>(TokenType::tok_ew_power);
        }
        // Just a single . - could be part of a number or struct access
        return '.';
    }

    if (m_lastChar == '"') { // String literal: "..."
        advance(); // consume opening quote
        StringVal = "";
        while (m_lastChar != '"' && m_lastChar != EOF && m_lastChar != '\n') {
            if (m_lastChar == '\\') {
                // Handle escape sequences
                advance();
                switch (m_lastChar) {
                    case 'n': StringVal += '\n'; break;
                    case 't': StringVal += '\t'; break;
                    case 'r': StringVal += '\r'; break;
                    case '\\': StringVal += '\\'; break;
                    case '"': StringVal += '"'; break;
                    default: StringVal += m_lastChar; break;
                }
            } else {
                StringVal += m_lastChar;
            }
            advance();
        }
        if (m_lastChar == '"') {
            advance(); // consume closing quote
        } else {
            std::cerr << "Unterminated string literal" << std::endl;
        }
        return static_cast<int>(TokenType::tok_string);
    }

    if (m_lastChar == '#') {
        // Comment until end of line.
        do {
            advance();
        } while (m_lastChar != EOF && m_lastChar != '\n' && m_lastChar != '\r');

        if (m_lastChar != EOF) {
            // After eating comment, recursion to find next REAL token
            return gettok();
        }
    }

    // Handle C++ style // comments
    if (m_lastChar == '/') {
        advance();
        if (m_lastChar == '/') {
            // Comment until end of line
            do {
                advance();
            } while (m_lastChar != EOF && m_lastChar != '\n' && m_lastChar != '\r');

            if (m_lastChar != EOF) {
                return gettok();
            }
        }
        if (m_lastChar == '=') {
            advance();
            return static_cast<int>(TokenType::tok_slash_equal);
        }
        // Just a single / - return as division operator
        return '/';
    }

    // Handle semicolons as matrix row separators (MATLAB-style)
    if (m_lastChar == ';') {
        advance();
        return static_cast<int>(TokenType::tok_semicolon);
    }

    // Handle colon as slice operator (MATLAB-style)
    if (m_lastChar == ':') {
        advance();
        return static_cast<int>(TokenType::tok_colon);
    }

    if (m_lastChar == '^') {
        advance();
        return static_cast<int>(TokenType::tok_power);
    }

    if (m_lastChar == '&') {
        advance();
        if (m_lastChar == '&') {
            advance();
            return static_cast<int>(TokenType::tok_logical_and);
        }
        return static_cast<int>(TokenType::tok_bitwise_and);
    }

    // Compound assignment operators
    if (m_lastChar == '+') {
        advance();
        if (m_lastChar == '=') {
            advance();
            return static_cast<int>(TokenType::tok_plus_equal);
        }
        return '+';
    }

    if (m_lastChar == '-') {
        advance();
        if (m_lastChar == '=') {
            advance();
            return static_cast<int>(TokenType::tok_minus_equal);
        }
        if (m_lastChar == '>') {
            advance();
            return static_cast<int>(TokenType::tok_arrow);
        }
        return '-';
    }

    if (m_lastChar == '*') {
        advance();
        if (m_lastChar == '=') {
            advance();
            return static_cast<int>(TokenType::tok_star_equal);
        }
        return '*';
    }

    if (m_lastChar == '|') {
        advance();
        if (m_lastChar == '|') {
            advance();
            return static_cast<int>(TokenType::tok_logical_or);
        }
        return static_cast<int>(TokenType::tok_bitwise_or);
    }
    
    if (m_lastChar == '!') {
        advance();
        if (m_lastChar == '=') {
            advance();
            return static_cast<int>(TokenType::tok_not_equal);
        }
        return static_cast<int>(TokenType::tok_logical_not);
    }
    
    if (m_lastChar == '=') {
        advance();
        if (m_lastChar == '=') {
            advance();
            return static_cast<int>(TokenType::tok_equal);
        }
        return '='; // Assignment (not used in expressions currently)
    }
    
    if (m_lastChar == '<') {
        advance();
        if (m_lastChar == '=') {
            advance();
            return static_cast<int>(TokenType::tok_less_equal);
        }
        if (m_lastChar == '<') {
            advance();
            return static_cast<int>(TokenType::tok_bitwise_shl);
        }
        return '<';
    }
    
    if (m_lastChar == '>') {
        advance();
        if (m_lastChar == '=') {
            advance();
            return static_cast<int>(TokenType::tok_greater_equal);
        }
        if (m_lastChar == '>') {
            advance();
            return static_cast<int>(TokenType::tok_bitwise_shr);
        }
        return '>';
    }

    if (m_lastChar == '~') {
        advance();
        return static_cast<int>(TokenType::tok_bitwise_not);
    }

    // Handle transpose operator
    if (m_lastChar == '\'') {
        advance();
        return static_cast<int>(TokenType::tok_transpose);
    }

    // Handle braces for blocks
    if (m_lastChar == '{') {
        advance();
        return static_cast<int>(TokenType::tok_lbrace);
    }

    if (m_lastChar == '}') {
        advance();
        return static_cast<int>(TokenType::tok_rbrace);
    }

    // Check for end of file.  Don't eat the EOF.
    if (m_lastChar == EOF) {
        return static_cast<int>(TokenType::tok_eof);
    }

    // Otherwise, just return the character as its ascii value.
    int ThisChar = m_lastChar;
    advance();
    return ThisChar;
}

int Lexer::getNextToken() {
    CurTok = gettok();
    // std::cerr << "DEBUG getNextToken: returning " << CurTok << " '" << (char)CurTok << "'" << std::endl;
    return CurTok;
}

} // namespace Flux
