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

#include "flux/compiler/component_modeling_ast.h"
#include "flux/compiler/parser.h"
#include <iostream>

namespace Flux {

// ============================================================================
// Hierarchical Design Parser
// ============================================================================

// Parse subcircuit instance: X1 n1 n2 subckt_name params: (R=1k)
std::unique_ptr<ExprAST> Parser::ParseSubcktInstance()
{
    getNextToken(); // eat instance or X-prefixed token

    // Instance name
    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected instance name");
        return nullptr;
    }
    std::string instanceName = m_lexer.IdentifierStr;
    getNextToken();

    // Subckt name
    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected subckt name");
        return nullptr;
    }
    std::string subcktName = m_lexer.IdentifierStr;
    getNextToken();

    auto instance = std::make_unique<SubcktInstanceAST>(instanceName, subcktName);

    // Parse nodes
    if (CurTok == '(') {
        getNextToken(); // eat (
        while (CurTok != ')' && CurTok != static_cast<int>(TokenType::tok_eof)) {
            if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
                instance->addNode(m_lexer.IdentifierStr);
                getNextToken();
            }
            if (CurTok == ',')
                getNextToken();
        }
        if (CurTok == ')')
            getNextToken(); // eat )
    }

    // Parse parameters
    if (CurTok == static_cast<int>(TokenType::tok_params) ||
        (CurTok == static_cast<int>(TokenType::tok_identifier) && m_lexer.IdentifierStr == "params")) {
        getNextToken(); // eat params
        if (CurTok == ':')
            getNextToken(); // eat :

        if (CurTok == '(' || CurTok == static_cast<int>(TokenType::tok_lbrace)) {
            getNextToken(); // eat ( or {
            while (CurTok != ')' && CurTok != static_cast<int>(TokenType::tok_rbrace) &&
                   CurTok != static_cast<int>(TokenType::tok_eof)) {
                if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
                    std::string paramName = m_lexer.IdentifierStr;
                    getNextToken();
                    if (CurTok == '=') {
                        getNextToken(); // eat =
                        if (CurTok == static_cast<int>(TokenType::tok_identifier) ||
                            CurTok == static_cast<int>(TokenType::tok_number)) {
                            std::string paramValue = (CurTok == static_cast<int>(TokenType::tok_number))
                                                         ? std::to_string(m_lexer.NumVal)
                                                         : m_lexer.IdentifierStr;
                            instance->addParam(paramName, paramValue);
                            getNextToken();
                        }
                    }
                }
                if (CurTok == ',')
                    getNextToken();
            }
            if (CurTok == ')' || CurTok == static_cast<int>(TokenType::tok_rbrace)) {
                getNextToken();
            }
        }
    }

    std::cout << "[Parser] Subckt instance: " << instanceName << " of " << subcktName << std::endl;

    return instance;
}

// ============================================================================
// Verilog-A Lite Parser
// ============================================================================

// Parse analog block: analog { ... }
std::unique_ptr<ExprAST> Parser::ParseAnalogBlock()
{
    getNextToken(); // eat analog

    auto analogBlock = std::make_unique<AnalogBlockAST>();

    if (CurTok == static_cast<int>(TokenType::tok_lbrace)) {
        getNextToken(); // eat {

        while (CurTok != static_cast<int>(TokenType::tok_rbrace) && CurTok != static_cast<int>(TokenType::tok_eof)) {

            // Parse contributor: V(node1, node2) <+ expression
            if (CurTok == static_cast<int>(TokenType::tok_identifier) &&
                (m_lexer.IdentifierStr == "V" || m_lexer.IdentifierStr == "I")) {
                std::string quantity = m_lexer.IdentifierStr;
                getNextToken(); // eat V or I

                if (CurTok != '(') {
                    ReportError("expected '(' after V or I");
                    return nullptr;
                }
                getNextToken(); // eat (

                std::string nodeP, nodeN = "0";
                if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
                    nodeP = m_lexer.IdentifierStr;
                    getNextToken();
                }

                if (CurTok == ',') {
                    getNextToken(); // eat ,
                    if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
                        nodeN = m_lexer.IdentifierStr;
                        getNextToken();
                    }
                }

                if (CurTok != ')') {
                    ReportError("expected ')'");
                    return nullptr;
                }
                getNextToken(); // eat )

                // Parse <+ contributor operator
                if (CurTok != static_cast<int>(TokenType::tok_contributor_op)) {
                    ReportError("expected <+ contributor operator");
                    return nullptr;
                }
                getNextToken(); // eat <+

                // Parse expression
                auto expr = ParseExpression();
                if (!expr)
                    return nullptr;

                auto contributor = std::make_unique<ContributorAST>(nodeP, nodeN, quantity);
                contributor->setExpression(std::move(expr));
                analogBlock->addContributor(std::move(contributor));
            } else if (CurTok == static_cast<int>(TokenType::tok_abstol) ||
                       CurTok == static_cast<int>(TokenType::tok_reltol)) {
                // Parse tolerance
                std::string tolParam = m_lexer.IdentifierStr;
                getNextToken();
                if (CurTok == '=') {
                    getNextToken();
                    if (CurTok == static_cast<int>(TokenType::tok_number)) {
                        analogBlock->setTolerance(tolParam, std::to_string(m_lexer.NumVal));
                        getNextToken();
                    }
                }
            }

            if (CurTok == static_cast<int>(TokenType::tok_semicolon)) {
                getNextToken();
            }
        }

        if (CurTok == static_cast<int>(TokenType::tok_rbrace)) {
            getNextToken(); // eat }
        }
    }

    std::cout << "[Parser] Analog block parsed with " << analogBlock->getContributors().size() << " contributors"
              << std::endl;

    return analogBlock;
}

// Parse ddt() expression
std::unique_ptr<ExprAST> Parser::ParseDdtExpr()
{
    getNextToken(); // eat ddt

    if (CurTok != '(') {
        ReportError("expected '(' after ddt");
        return nullptr;
    }
    getNextToken(); // eat (

    auto operand = ParseExpression();
    if (!operand)
        return nullptr;

    if (CurTok != ')') {
        ReportError("expected ')'");
        return nullptr;
    }
    getNextToken(); // eat )

    return std::make_unique<DdtExprAST>(std::move(operand));
}

// Parse idt() expression
std::unique_ptr<ExprAST> Parser::ParseIdtExpr()
{
    getNextToken(); // eat idt

    if (CurTok != '(') {
        ReportError("expected '(' after idt");
        return nullptr;
    }
    getNextToken(); // eat (

    auto operand = ParseExpression();
    if (!operand)
        return nullptr;

    std::unique_ptr<ExprAST> ic = nullptr;
    if (CurTok == ',') {
        getNextToken(); // eat ,
        ic = ParseExpression();
        if (!ic)
            return nullptr;
    }

    if (CurTok != ')') {
        ReportError("expected ')'");
        return nullptr;
    }
    getNextToken(); // eat )

    return std::make_unique<IdtExprAST>(std::move(operand), std::move(ic));
}

// ============================================================================
// Symbol Pin Mapping Parser
// ============================================================================

// Parse symbol declaration
std::unique_ptr<ExprAST> Parser::ParseSymbolDecl()
{
    getNextToken(); // eat symbol

    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected symbol name");
        return nullptr;
    }
    std::string symbolName = m_lexer.IdentifierStr;
    getNextToken();

    auto symbol = std::make_unique<SymbolDeclAST>(symbolName);

    // Parse pins
    if (CurTok == '(') {
        getNextToken(); // eat (
        while (CurTok != ')' && CurTok != static_cast<int>(TokenType::tok_eof)) {
            if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
                symbol->addPin(m_lexer.IdentifierStr);
                getNextToken();
            }
            if (CurTok == ',')
                getNextToken();
        }
        if (CurTok == ')')
            getNextToken(); // eat )
    }

    std::cout << "[Parser] Symbol declaration: " << symbolName << std::endl;

    return symbol;
}

// Parse pin mapping
std::unique_ptr<ExprAST> Parser::ParsePinMap()
{
    getNextToken(); // eat pinmap

    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected symbol name");
        return nullptr;
    }
    std::string symbolName = m_lexer.IdentifierStr;
    getNextToken();

    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected subckt name");
        return nullptr;
    }
    std::string subcktName = m_lexer.IdentifierStr;
    getNextToken();

    auto pinMap = std::make_unique<PinMapAST>(symbolName, subcktName);

    // Parse mappings
    if (CurTok == static_cast<int>(TokenType::tok_lbrace) || CurTok == '(') {
        getNextToken(); // eat { or (

        while (CurTok != static_cast<int>(TokenType::tok_rbrace) && CurTok != ')' &&
               CurTok != static_cast<int>(TokenType::tok_eof)) {

            if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
                std::string symbolPin = m_lexer.IdentifierStr;
                getNextToken();

                if (CurTok == static_cast<int>(TokenType::tok_identifier) ||
                    CurTok == static_cast<int>(TokenType::tok_map)) {
                    if (CurTok == static_cast<int>(TokenType::tok_map))
                        getNextToken(); // eat map

                    if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
                        std::string subcktPin = m_lexer.IdentifierStr;
                        pinMap->addMapping(symbolPin, subcktPin);
                        getNextToken();
                    }
                }
            }

            if (CurTok == ',')
                getNextToken();
        }

        if (CurTok == static_cast<int>(TokenType::tok_rbrace) || CurTok == ')') {
            getNextToken();
        }
    }

    std::cout << "[Parser] Pin map: " << symbolName << " -> " << subcktName << " (" << pinMap->getMappings().size()
              << " mappings)" << std::endl;

    return pinMap;
}

} // namespace Flux
