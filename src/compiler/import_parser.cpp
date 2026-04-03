#include "flux/compiler/module_loader.h"
#include "flux/compiler/parser.h"
#include "flux/compiler/ast.h"
#include <iostream>

namespace Flux {

std::unique_ptr<ExprAST> Parser::ParseImport() {
    getNextToken(); // eat import

    std::string moduleName;
    std::string versionSpec;
    std::string alias;
    std::vector<std::string> symbols;

    // Parse module name
    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected module name after import");
        return nullptr;
    }

    moduleName = m_lexer.IdentifierStr;
    getNextToken();

    // Parse optional version constraint
    if (CurTok == '@') {
        getNextToken(); // eat @

        if (CurTok == static_cast<int>(TokenType::tok_number)) {
            // Simple version: @1.0
            versionSpec = std::to_string(static_cast<int>(m_lexer.NumVal));
            getNextToken();

            if (CurTok == '.') {
                getNextToken(); // eat .
                if (CurTok == static_cast<int>(TokenType::tok_number)) {
                    versionSpec += "." + std::to_string(static_cast<int>(m_lexer.NumVal));
                    getNextToken();

                    if (CurTok == '.') {
                        getNextToken(); // eat .
                        if (CurTok == static_cast<int>(TokenType::tok_number)) {
                            versionSpec += "." + std::to_string(static_cast<int>(m_lexer.NumVal));
                            getNextToken();
                        }
                    }
                }
            }
        }
    }

    // Parse optional alias
    if (CurTok == static_cast<int>(TokenType::tok_identifier) && m_lexer.IdentifierStr == "as") {
        getNextToken(); // eat as

        if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
            ReportError("expected alias name after 'as'");
            return nullptr;
        }

        alias = m_lexer.IdentifierStr;
        getNextToken();
    }

    // Parse optional symbol list
    if (CurTok == static_cast<int>(TokenType::tok_lbrace)) {
        getNextToken(); // eat {

        while (CurTok != static_cast<int>(TokenType::tok_rbrace) &&
               CurTok != static_cast<int>(TokenType::tok_eof)) {

            if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
                symbols.push_back(m_lexer.IdentifierStr);
                getNextToken();

                if (CurTok == ',') {
                    getNextToken();
                }
            } else {
                getNextToken();
            }
        }

        if (CurTok == static_cast<int>(TokenType::tok_rbrace)) {
            getNextToken(); // eat }
        }
    }

    return std::make_unique<ImportExprAST>(moduleName, versionSpec, alias, symbols);
}

} // namespace Flux
