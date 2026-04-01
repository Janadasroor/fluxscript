#include "flux/compiler/lexer.h"
#include <cctype>
#include <cstdlib>
#include <iostream>

namespace Flux {

Lexer::Lexer(const std::string& input) : m_input(input), m_pos(0), m_lastChar(' ') {}

int Lexer::advance() {
    if (m_pos < m_input.size()) {
        m_lastChar = m_input[m_pos++];
    } else {
        m_lastChar = EOF;
    }
    return m_lastChar;
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
        if (IdentifierStr == "xor") return static_cast<int>(TokenType::tok_bitwise_xor);

        return static_cast<int>(TokenType::tok_identifier);
    }

    if (isdigit(m_lastChar) || m_lastChar == '.') { // Number: [0-9.]+
        std::string NumStr;
        do {
            NumStr += m_lastChar;
            advance();
        } while (isdigit(m_lastChar) || m_lastChar == '.');

        // Check for imaginary suffix 'j' or 'J'
        if (m_lastChar == 'j' || m_lastChar == 'J') {
            advance(); // consume the 'j'
            NumVal = strtod(NumStr.c_str(), nullptr);
            return static_cast<int>(TokenType::tok_imaginary);
        }

        NumVal = strtod(NumStr.c_str(), nullptr);
        return static_cast<int>(TokenType::tok_number);
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

    // Skip semicolons as statement separators (MATLAB-style)
    if (m_lastChar == ';') {
        advance();
        return gettok();
    }

    // Handle multi-character operators
    // Element-wise operators (MATLAB-style): .*, ./, .^
    if (m_lastChar == '.') {
        advance();
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

    if (m_lastChar == '^') {
        advance();
        return static_cast<int>(TokenType::tok_power);
    }

    // Handle arrow operator (->)
    if (m_lastChar == '-') {
        advance();
        if (m_lastChar == '>') {
            advance();
            return static_cast<int>(TokenType::tok_arrow);
        }
        return '-'; // Just a single -, return as minus
    }

    if (m_lastChar == '&') {
        advance();
        if (m_lastChar == '&') {
            advance();
            return static_cast<int>(TokenType::tok_logical_and);
        }
        return static_cast<int>(TokenType::tok_bitwise_and);
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
    return CurTok;
}

} // namespace Flux
