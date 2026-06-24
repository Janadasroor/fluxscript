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

std::unique_ptr<DebugStmtAST> Parser::ParseDebugStmt()
{
    getNextToken(); // eat debug

    std::string circuitFile;
    std::string symptom;
    std::map<std::string, std::string> options;

    if (CurTok == static_cast<int>(TokenType::tok_string)) {
        circuitFile = m_lexer.StringVal;
        getNextToken();
    }

    if (CurTok == '{') {
        getNextToken();
        while (CurTok != '}' && CurTok != static_cast<int>(TokenType::tok_eof)) {
            if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
                std::string key = m_lexer.IdentifierStr;
                getNextToken();
                if (CurTok == ':') {
                    getNextToken();
                    if (CurTok == static_cast<int>(TokenType::tok_string)) {
                        options[key] = m_lexer.StringVal;
                        if (key == "symptom")
                            symptom = m_lexer.StringVal;
                        getNextToken();
                    }
                }
            }
            if (CurTok == ',')
                getNextToken();
            else if (CurTok != '}')
                break;
        }
        if (CurTok == '}')
            getNextToken();
    }

    return std::make_unique<DebugStmtAST>(circuitFile, symptom, options);
}

std::unique_ptr<SensitivityStmtAST> Parser::ParseSensitivityStmt()
{
    getNextToken(); // eat sensitivity

    std::string output;
    std::vector<std::string> params;
    std::string circuit;

    if (CurTok == '(') {
        getNextToken();
        while (CurTok != ')' && CurTok != static_cast<int>(TokenType::tok_eof)) {
            if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
                std::string key = m_lexer.IdentifierStr;
                getNextToken();
                if (CurTok == '=') {
                    getNextToken();
                    if (CurTok == static_cast<int>(TokenType::tok_string)) {
                        if (key == "output")
                            output = m_lexer.StringVal;
                        else if (key == "circuit")
                            circuit = m_lexer.StringVal;
                        getNextToken();
                    } else if (CurTok == '[') {
                        getNextToken();
                        while (CurTok != ']') {
                            if (CurTok == static_cast<int>(TokenType::tok_string)) {
                                params.push_back(m_lexer.StringVal);
                                getNextToken();
                            }
                            if (CurTok == ',')
                                getNextToken();
                            else if (CurTok != ']')
                                break;
                        }
                        if (CurTok == ']')
                            getNextToken();
                    }
                }
            }
            if (CurTok == ',')
                getNextToken();
            else if (CurTok != ')')
                break;
        }
        if (CurTok == ')')
            getNextToken();
    }

    return std::make_unique<SensitivityStmtAST>(output, params, circuit);
}

std::unique_ptr<AskExprAST> Parser::ParseAskExpr()
{
    getNextToken(); // eat ask

    std::string question;
    if (CurTok == static_cast<int>(TokenType::tok_string)) {
        question = m_lexer.StringVal;
        getNextToken();
    }

    return std::make_unique<AskExprAST>(question);
}

std::unique_ptr<ExplainExprAST> Parser::ParseExplainExpr()
{
    getNextToken(); // eat explain

    std::string circuit;
    std::string aspect;

    if (CurTok == static_cast<int>(TokenType::tok_identifier) && m_lexer.IdentifierStr == "circuit") {
        getNextToken();
        if (CurTok == static_cast<int>(TokenType::tok_string)) {
            circuit = m_lexer.StringVal;
            getNextToken();
        }
    } else if (CurTok == static_cast<int>(TokenType::tok_string)) {
        aspect = m_lexer.StringVal;
        getNextToken();
    }

    return std::make_unique<ExplainExprAST>(circuit, aspect);
}

std::unique_ptr<SubstituteStmtAST> Parser::ParseSubstituteStmt()
{
    getNextToken(); // eat substitute

    std::string partNumber;
    std::vector<std::string> options;

    if (CurTok == static_cast<int>(TokenType::tok_string)) {
        partNumber = m_lexer.StringVal;
        getNextToken();
    }

    return std::make_unique<SubstituteStmtAST>(partNumber, options);
}

} // namespace Flux
