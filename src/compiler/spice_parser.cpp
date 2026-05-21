#if 0
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

// SPICE Parser Extension (NOT YET INTEGRATED - API OUT OF DATE)
// Implements parsing of .PARAM, .IC, .AC, .TRAN, .MEASURE, .PROBE, .SAVE,
// .SUBCKT, .MODEL, StateGet(@), StateSet($) statements

#include "flux/compiler/parser.h"

#include <algorithm>
#include <iostream>
#include <sstream>

namespace Flux {

// ============================================================================
// Parse .PARAM (Parameter) declaration
// ============================================================================
std::unique_ptr<ExprAST> Parser::ParseParamDecl() {
    getNextToken(); // eat param
    
    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected identifier after param");
        return nullptr;
    }
    
    std::string name = m_lexer.IdentifierStr;
    getNextToken(); // eat name
    
    if (CurTok != '=') {
        ReportError("expected '=' after parameter name");
        return nullptr;
    }
    
    getNextToken(); // eat =
    
    auto value = ParseExpression();
    if (!value) return nullptr;
    
    return std::make_unique<ParamDeclAST>(name, std::move(value));
}

std::unique_ptr<ExprAST> Parser::ParseICDecl() {
    getNextToken(); // eat ic
    
    if (CurTok != static_cast<int>(TokenType::tok_identifier) && 
        CurTok != static_cast<int>(TokenType::tok_number)) {
        ReportError("expected node name after ic");
        return nullptr;
    }
    
    std::string nodeName;
    if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
        nodeName = m_lexer.IdentifierStr;
    } else {
        nodeName = std::to_string(static_cast<int>(m_lexer.NumVal));
    }
    getNextToken(); // eat name
    
    if (CurTok != '=') {
        ReportError("expected '=' after node name");
        return nullptr;
    }
    
    getNextToken(); // eat =
    
    auto value = ParseExpression();
    if (!value) return nullptr;
    
    return std::make_unique<ICDeclAST>(nodeName, std::move(value));
}

std::unique_ptr<ExprAST> Parser::ParseAnalysisDecl() {
    getNextToken(); // eat analysis
    
    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected analysis type");
        return nullptr;
    }
    
    std::string type = m_lexer.IdentifierStr;
    getNextToken(); // eat type
    
    std::vector<std::unique_ptr<ExprAST>> params;
    
    // Parse analysis parameters (comma-separated or space-separated)
    while (CurTok != static_cast<int>(TokenType::tok_eof) &&
           CurTok != static_cast<int>(TokenType::tok_semicolon) &&
           CurTok != static_cast<int>(TokenType::tok_rbrace)) {
        
        if (CurTok == ',') {
            getNextToken(); // eat comma
            continue;
        }
        
        auto param = ParseExpression();
        if (!param) break;
        params.push_back(std::move(param));
    }
    
    return std::make_unique<AnalysisDeclAST>(type, std::move(params));
}

std::unique_ptr<ExprAST> Parser::ParseMeasureDecl() {
    getNextToken(); // eat measure
    
    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected measurement name");
        return nullptr;
    }
    
    std::string name = m_lexer.IdentifierStr;
    getNextToken(); // eat name
    
    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected measurement function (MAX, MIN, AVG, etc.)");
        return nullptr;
    }
    
    std::string func = m_lexer.IdentifierStr;
    getNextToken(); // eat function
    
    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected signal name");
        return nullptr;
    }
    
    std::string signal = m_lexer.IdentifierStr;
    getNextToken(); // eat signal
    
    auto measure = std::make_unique<MeasureDeclAST>(name, func, signal);
    
    // Parse optional parameters (WHEN, FROM, TO, etc.)
    while (CurTok != static_cast<int>(TokenType::tok_eof) &&
           CurTok != static_cast<int>(TokenType::tok_semicolon) &&
           CurTok != static_cast<int>(TokenType::tok_rbrace)) {
        
        if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
            std::string key = m_lexer.IdentifierStr;
            getNextToken(); // eat key
            
            if (CurTok == '=') {
                getNextToken(); // eat =
                if (CurTok == static_cast<int>(TokenType::tok_number)) {
                    measure->Options[key] = std::to_string(m_lexer.NumVal);
                    getNextToken(); // eat value
                }
            }
        } else {
            getNextToken();
        }
    }
    
    return measure;
}

std::unique_ptr<ExprAST> Parser::ParseProbeDecl() {
    getNextToken(); // eat probe
    
    std::vector<std::string> signals;
    
    while (CurTok != static_cast<int>(TokenType::tok_eof) &&
           CurTok != static_cast<int>(TokenType::tok_semicolon) &&
           CurTok != static_cast<int>(TokenType::tok_rbrace)) {
        
        if (CurTok == ',') {
            getNextToken(); // eat comma
            continue;
        }
        
        if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
            signals.push_back(m_lexer.IdentifierStr);
            getNextToken();
        } else {
            getNextToken();
        }
    }
    
    return std::make_unique<ProbeDeclAST>(std::move(signals));
}

std::unique_ptr<ExprAST> Parser::ParseStateGet() {
    getNextToken(); // eat state_get
    
    if (CurTok != '(') {
        ReportError("expected '(' after state_get");
        return nullptr;
    }
    
    getNextToken(); // eat (
    
    if (CurTok != static_cast<int>(TokenType::tok_identifier) &&
        CurTok != static_cast<int>(TokenType::tok_string)) {
        ReportError("expected state variable name");
        return nullptr;
    }
    
    std::string name;
    if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
        name = m_lexer.IdentifierStr;
    } else {
        name = m_lexer.StringVal;
    }
    getNextToken(); // eat name
    
    std::unique_ptr<ExprAST> defaultVal;
    
    if (CurTok == ',') {
        getNextToken(); // eat comma
        defaultVal = ParseExpression();
        if (!defaultVal) return nullptr;
    }
    
    if (CurTok != ')') {
        ReportError("expected ')' after state_get arguments");
        return nullptr;
    }
    
    getNextToken(); // eat )
    
    return std::make_unique<StateGetExprAST>(name, std::move(defaultVal));
}

std::unique_ptr<ExprAST> Parser::ParseStateSet() {
    getNextToken(); // eat state_set
    
    if (CurTok != '(') {
        ReportError("expected '(' after state_set");
        return nullptr;
    }
    
    getNextToken(); // eat (
    
    if (CurTok != static_cast<int>(TokenType::tok_identifier) &&
        CurTok != static_cast<int>(TokenType::tok_string)) {
        ReportError("expected state variable name");
        return nullptr;
    }
    
    std::string name;
    if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
        name = m_lexer.IdentifierStr;
    } else {
        name = m_lexer.StringVal;
    }
    getNextToken(); // eat name
    
    if (CurTok != ',') {
        ReportError("expected ',' after state variable name");
        return nullptr;
    }
    
    getNextToken(); // eat comma
    
    auto value = ParseExpression();
    if (!value) return nullptr;
    
    if (CurTok != ')') {
        ReportError("expected ')' after state_set arguments");
        return nullptr;
    }
    
    getNextToken(); // eat )
    
    return std::make_unique<StateSetExprAST>(name, std::move(value));
}

std::unique_ptr<SubcktAST> Parser::ParseSubckt() {
    getNextToken(); // eat subckt
    
    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected subcircuit name");
        return nullptr;
    }
    
    std::string name = m_lexer.IdentifierStr;
    getNextToken(); // eat name
    
    std::vector<std::string> pins;
    
    // Parse pin names
    while (CurTok != static_cast<int>(TokenType::tok_eof) &&
           CurTok != static_cast<int>(TokenType::tok_semicolon) &&
           CurTok != static_cast<int>(TokenType::tok_lbrace)) {
        
        if (CurTok == ',') {
            getNextToken(); // eat comma
            continue;
        }
        
        if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
            pins.push_back(m_lexer.IdentifierStr);
            getNextToken();
        } else {
            getNextToken();
        }
    }
    
    std::vector<std::unique_ptr<ExprAST>> body;
    
    // Parse body if there's a brace
    if (CurTok == static_cast<int>(TokenType::tok_lbrace)) {
        getNextToken(); // eat {
        
        while (CurTok != static_cast<int>(TokenType::tok_eof) &&
               CurTok != static_cast<int>(TokenType::tok_rbrace)) {
            
            if (CurTok == static_cast<int>(TokenType::tok_ends)) {
                break;
            }
            
            // Skip to next statement
            while (CurTok != static_cast<int>(TokenType::tok_eof) &&
                   CurTok != static_cast<int>(TokenType::tok_semicolon)) {
                getNextToken();
            }
            
            if (CurTok == static_cast<int>(TokenType::tok_semicolon)) {
                getNextToken();
            }
        }
        
        if (CurTok == static_cast<int>(TokenType::tok_rbrace)) {
            getNextToken(); // eat }
        }
    }
    
    return std::make_unique<SubcktAST>(name, std::move(pins), std::move(body));
}

std::unique_ptr<ModelAST> Parser::ParseModel() {
    getNextToken(); // eat model
    
    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected model name");
        return nullptr;
    }
    
    std::string name = m_lexer.IdentifierStr;
    getNextToken(); // eat name
    
    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected model type");
        return nullptr;
    }
    
    std::string type = m_lexer.IdentifierStr;
    getNextToken(); // eat type
    
    std::map<std::string, double> params;
    
    // Parse model parameters (name=value pairs)
    if (CurTok == '(') {
        getNextToken(); // eat (
        
        while (CurTok != static_cast<int>(TokenType::tok_eof) &&
               CurTok != static_cast<int>(TokenType::tok_rbrace)) {
            
            if (CurTok == ')') {
                getNextToken(); // eat )
                break;
            }
            
            if (CurTok == ',') {
                getNextToken(); // eat comma
                continue;
            }
            
            if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
                std::string paramName = m_lexer.IdentifierStr;
                getNextToken(); // eat name
                
                if (CurTok == '=') {
                    getNextToken(); // eat =
                    
                    if (CurTok == static_cast<int>(TokenType::tok_number)) {
                        params[paramName] = m_lexer.NumVal;
                        getNextToken(); // eat value
                    }
                }
            } else {
                getNextToken();
            }
        }
    }
    
    return std::make_unique<ModelAST>(name, type, std::move(params));
}

} // namespace Flux
#endif
