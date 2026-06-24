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

#include "flux/compiler/parser.h"

namespace Flux {

std::unique_ptr<ExprAST> Parser::ParseImport()
{
    getNextToken(); // eat import

    std::string moduleName;
    std::string versionSpec;
    std::string alias;
    std::vector<std::string> symbols;

    if (CurTok != static_cast<int>(TokenType::tok_identifier) && CurTok != static_cast<int>(TokenType::tok_analysis) &&
        CurTok != static_cast<int>(TokenType::tok_measure)) {
        ReportError("expected module name after import");
        return nullptr;
    }

    moduleName = m_lexer.IdentifierStr;
    getNextToken();

    if (CurTok == '@') {
        getNextToken();

        if (CurTok == static_cast<int>(TokenType::tok_number)) {
            versionSpec = std::to_string(static_cast<int>(m_lexer.NumVal));
            getNextToken();

            if (CurTok == '.') {
                getNextToken();
                if (CurTok == static_cast<int>(TokenType::tok_number)) {
                    versionSpec += "." + std::to_string(static_cast<int>(m_lexer.NumVal));
                    getNextToken();

                    if (CurTok == '.') {
                        getNextToken();
                        if (CurTok == static_cast<int>(TokenType::tok_number)) {
                            versionSpec += "." + std::to_string(static_cast<int>(m_lexer.NumVal));
                            getNextToken();
                        }
                    }
                }
            }
        }
    }

    if (CurTok == static_cast<int>(TokenType::tok_identifier) && m_lexer.IdentifierStr == "as") {
        getNextToken();

        if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
            ReportError("expected alias name after 'as'");
            return nullptr;
        }

        alias = m_lexer.IdentifierStr;
        getNextToken();
    }

    if (CurTok == static_cast<int>(TokenType::tok_lbrace)) {
        getNextToken();

        while (CurTok != static_cast<int>(TokenType::tok_rbrace) && CurTok != static_cast<int>(TokenType::tok_eof)) {
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
            getNextToken();
        }
    }

    return std::make_unique<ImportExprAST>(moduleName, versionSpec, alias, symbols);
}

std::unique_ptr<ExprAST> Parser::ParseFromImport()
{
    getNextToken(); // eat from

    std::string moduleName;

    if (CurTok == static_cast<int>(TokenType::tok_string)) {
        moduleName = m_lexer.StringVal;
        getNextToken();
    } else if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
        moduleName = m_lexer.IdentifierStr;
        getNextToken();
    } else {
        ReportError("expected module name after 'from'");
        return nullptr;
    }

    if (CurTok != static_cast<int>(TokenType::tok_import)) {
        ReportError("expected 'import' after module name in from-import");
        return nullptr;
    }
    getNextToken(); // eat import

    std::vector<std::string> symbols;
    while (CurTok == static_cast<int>(TokenType::tok_identifier)) {
        symbols.push_back(m_lexer.IdentifierStr);
        getNextToken();
        if (CurTok == ',') {
            getNextToken();
        } else {
            break;
        }
    }

    if (symbols.empty()) {
        ReportError("expected at least one symbol name after 'import'");
        return nullptr;
    }

    return std::make_unique<ImportExprAST>(moduleName, "", "", symbols);
}

} // namespace Flux
