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
#include "flux/compiler/time_domain_ast.h"
#include <iostream>

namespace Flux {

// ============ B-Source Parser ============

std::unique_ptr<ExprAST> Parser::ParseBSourceDecl()
{
    getNextToken(); // eat bsource

    // Parse name
    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected bsource name");
        return nullptr;
    }
    std::string name = m_lexer.IdentifierStr;
    getNextToken();

    // Parse output node
    if (CurTok != '(') {
        ReportError("expected '(' after bsource name");
        return nullptr;
    }
    getNextToken(); // eat (

    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected output node name");
        return nullptr;
    }
    std::string outNode = m_lexer.IdentifierStr;
    getNextToken();

    // Parse reference node (optional, default is ground)
    std::string refNode = "0";
    if (CurTok == ',') {
        getNextToken(); // eat ,
        if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
            refNode = m_lexer.IdentifierStr;
            getNextToken();
        }
    }

    if (CurTok != ')') {
        ReportError("expected ')' after bsource nodes");
        return nullptr;
    }
    getNextToken(); // eat )

    auto bsource = std::make_unique<BSourceDeclAST>(name, outNode, refNode);

    // Parse input nodes (optional)
    if (CurTok == '[') {
        getNextToken(); // eat [
        while (CurTok != ']' && CurTok != static_cast<int>(TokenType::tok_eof)) {
            if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
                bsource->addInputNode(m_lexer.IdentifierStr);
                getNextToken();
            }
            if (CurTok == ',')
                getNextToken();
        }
        if (CurTok == ']')
            getNextToken(); // eat ]
    }

    // Parse update function body
    if (CurTok == static_cast<int>(TokenType::tok_lbrace)) {
        getNextToken(); // eat {
        bsource->setBody(ParseBlockExpr());
    }

    std::cout << "[Parser] B-source '" << name << "' registered" << std::endl;

    return bsource;
}

// ============ Transient Analysis Parser ============

std::unique_ptr<ExprAST> Parser::ParseTransientAnalysis()
{
    getNextToken(); // eat transient

    if (CurTok != static_cast<int>(TokenType::tok_number)) {
        ReportError("expected timestep after transient");
        return nullptr;
    }
    double timestep = m_lexer.NumVal;
    getNextToken();

    if (CurTok != static_cast<int>(TokenType::tok_number)) {
        ReportError("expected stop time after timestep");
        return nullptr;
    }
    double stopTime = m_lexer.NumVal;
    getNextToken();

    double startTime = 0.0;
    double maxTimestep = 0.0;

    // Optional start time
    if (CurTok == static_cast<int>(TokenType::tok_number)) {
        startTime = m_lexer.NumVal;
        getNextToken();

        // Optional max timestep
        if (CurTok == static_cast<int>(TokenType::tok_number)) {
            maxTimestep = m_lexer.NumVal;
            getNextToken();
        }
    }

    auto transient = std::make_unique<TransientAnalysisAST>(timestep, stopTime);

    std::cout << "[Parser] Transient analysis: t=" << timestep << " to " << stopTime << std::endl;

    return transient;
}

// ============ Initial Condition Parser ============

std::unique_ptr<ExprAST> Parser::ParseInitialCond()
{
    getNextToken(); // eat initial

    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected node name after initial");
        return nullptr;
    }
    std::string node = m_lexer.IdentifierStr;
    getNextToken();

    if (CurTok != '=') {
        ReportError("expected '=' after node name");
        return nullptr;
    }
    getNextToken(); // eat =

    auto value = ParseExpression();
    if (!value)
        return nullptr;

    return std::make_unique<InitialCondAST>(node, std::move(value));
}

// ============ Time Variable Parser ============

std::unique_ptr<ExprAST> Parser::ParseTimeVar()
{
    getNextToken(); // eat time
    return std::make_unique<TimeExprAST>();
}

std::unique_ptr<ExprAST> Parser::ParseTimestepVar()
{
    getNextToken(); // eat dt
    return std::make_unique<TimestepExprAST>();
}

std::unique_ptr<ExprAST> Parser::ParseTempVar()
{
    getNextToken(); // eat temp
    return std::make_unique<TempExprAST>();
}

// ============ Inputs Dictionary Parser ============

std::unique_ptr<ExprAST> Parser::ParseInputsExpr()
{
    getNextToken(); // eat inputs

    if (CurTok != '[') {
        ReportError("expected '[' after inputs");
        return nullptr;
    }
    getNextToken(); // eat [

    std::string node;
    if (CurTok == static_cast<int>(TokenType::tok_string)) {
        node = m_lexer.StringVal;
        getNextToken();
    } else if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
        node = m_lexer.IdentifierStr;
        getNextToken();
    } else if (CurTok == static_cast<int>(TokenType::tok_number)) {
        node = std::to_string((int)m_lexer.NumVal);
        getNextToken();
    } else {
        ReportError("[ABI-SYNC] expected node name or index in inputs");
        return nullptr;
    }

    if (CurTok != ']') {
        ReportError("expected ']' after node name");
        return nullptr;
    }
    getNextToken(); // eat ]

    return std::make_unique<InputsExprAST>(node);
}

// ============ Outputs Dictionary Parser ============

std::unique_ptr<ExprAST> Parser::ParseOutputsExpr()
{
    getNextToken(); // eat outputs

    if (CurTok != '[') {
        ReportError("expected '[' after outputs");
        return nullptr;
    }
    getNextToken(); // eat [

    std::string node;
    if (CurTok == static_cast<int>(TokenType::tok_string)) {
        node = m_lexer.StringVal;
        getNextToken();
    } else if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
        node = m_lexer.IdentifierStr;
        getNextToken();
    } else if (CurTok == static_cast<int>(TokenType::tok_number)) {
        node = std::to_string((int)m_lexer.NumVal);
        getNextToken();
    } else {
        ReportError("[ABI-SYNC] expected node name or index in outputs");
        return nullptr;
    }

    if (CurTok != ']') {
        ReportError("expected ']' after node name");
        return nullptr;
    }
    getNextToken(); // eat ]

    if (CurTok != '=') {
        ReportError("expected '=' after outputs[node]");
        return nullptr;
    }
    getNextToken(); // eat =

    auto value = ParseExpression();
    if (!value)
        return nullptr;

    return std::make_unique<OutputsExprAST>(node, std::move(value));
}

} // namespace Flux
