#include "flux/compiler/parser.h"
#include "flux/compiler/model_quality_ast.h"
#include <iostream>

namespace Flux {

// ============================================================================
// Transient Assertion Parser
// ============================================================================

std::unique_ptr<ExprAST> Parser::ParseAssertDecl() {
    getNextToken(); // eat assert
    
    // Parse V(node) or I(branch)
    if (CurTok != static_cast<int>(TokenType::tok_identifier) || 
        (m_lexer.IdentifierStr != "V" && m_lexer.IdentifierStr != "I")) {
        ReportError("expected V() or I() after assert");
        return nullptr;
    }
    
    std::string quantity = m_lexer.IdentifierStr;
    getNextToken(); // eat V or I
    
    if (CurTok != '(') {
        ReportError("expected '(' after V or I");
        return nullptr;
    }
    getNextToken(); // eat (
    
    std::string node;
    if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
        node = m_lexer.IdentifierStr;
        getNextToken();
    }
    
    if (CurTok != ')') {
        ReportError("expected ')'");
        return nullptr;
    }
    getNextToken(); // eat )
    
    // Parse operator
    std::string op;
    if (CurTok == '<') op = "<=";
    else if (CurTok == '>') op = ">=";
    else if (CurTok == '<') op = "<";
    else if (CurTok == '>') op = ">";
    else if (CurTok == '=') op = "==";
    else {
        ReportError("expected comparison operator");
        return nullptr;
    }
    getNextToken();
    
    // Parse bound
    if (CurTok != static_cast<int>(TokenType::tok_number)) {
        ReportError("expected numeric bound");
        return nullptr;
    }
    double bound = m_lexer.NumVal;
    getNextToken();
    
    auto assertDecl = std::make_unique<AssertDeclAST>(node, op, bound);
    
    // Optional: within time
    if (CurTok == static_cast<int>(TokenType::tok_identifier) && 
        m_lexer.IdentifierStr == "within") {
        getNextToken(); // eat within
        if (CurTok != static_cast<int>(TokenType::tok_number)) {
            ReportError("expected time value after within");
            return nullptr;
        }
        assertDecl->setWithinTime(m_lexer.NumVal);
        getNextToken();
    }
    
    // Optional: message
    if (CurTok == static_cast<int>(TokenType::tok_identifier) && 
        m_lexer.IdentifierStr == "message") {
        getNextToken(); // eat message
        if (CurTok == static_cast<int>(TokenType::tok_string)) {
            assertDecl->setMessage(m_lexer.StringVal);
            getNextToken();
        }
    }
    
    std::cout << "[Parser] Assert: " << quantity << "(" << node << ") " 
              << op << " " << bound << std::endl;
    
    return assertDecl;
}

std::unique_ptr<ExprAST> Parser::ParseSettleDecl() {
    getNextToken(); // eat settle
    
    // Parse V(node)
    if (CurTok != static_cast<int>(TokenType::tok_V)) {
        ReportError("expected V() after settle");
        return nullptr;
    }
    getNextToken(); // eat V
    
    if (CurTok != '(') {
        ReportError("expected '('");
        return nullptr;
    }
    getNextToken();
    
    std::string node;
    if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
        node = m_lexer.IdentifierStr;
        getNextToken();
    }
    
    if (CurTok != ')') {
        ReportError("expected ')'");
        return nullptr;
    }
    getNextToken(); // eat )
    
    // Parse tolerance%
    if (CurTok != static_cast<int>(TokenType::tok_number)) {
        ReportError("expected tolerance value");
        return nullptr;
    }
    double tol = m_lexer.NumVal;
    getNextToken();
    
    // Parse after time
    if (CurTok != static_cast<int>(TokenType::tok_identifier) || 
        m_lexer.IdentifierStr != "after") {
        ReportError("expected 'after' keyword");
        return nullptr;
    }
    getNextToken(); // eat after
    
    if (CurTok != static_cast<int>(TokenType::tok_number)) {
        ReportError("expected time value");
        return nullptr;
    }
    double afterTime = m_lexer.NumVal;
    getNextToken();
    
    std::cout << "[Parser] Settle: V(" << node << ") within " << tol 
              << "% after " << afterTime << "s" << std::endl;
    
    return std::make_unique<SettleDeclAST>(node, tol, afterTime);
}

// ============================================================================
// Golden Waveform Parser
// ============================================================================

std::unique_ptr<ExprAST> Parser::ParseGoldenDecl() {
    getNextToken(); // eat golden
    
    std::string name;
    if (CurTok == static_cast<int>(TokenType::tok_string)) {
        name = m_lexer.StringVal;
        getNextToken();
    } else if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
        name = m_lexer.IdentifierStr;
        getNextToken();
    } else {
        ReportError("expected golden waveform name");
        return nullptr;
    }
    
    std::string filename;
    if (CurTok == static_cast<int>(TokenType::tok_identifier) && 
        m_lexer.IdentifierStr == "from") {
        getNextToken(); // eat from
        if (CurTok == static_cast<int>(TokenType::tok_string)) {
            filename = m_lexer.StringVal;
            getNextToken();
        }
    }
    
    std::cout << "[Parser] Golden waveform: " << name;
    if (!filename.empty()) std::cout << " from " << filename;
    std::cout << std::endl;
    
    return std::make_unique<GoldenDeclAST>(name, filename);
}

std::unique_ptr<ExprAST> Parser::ParseCompareDecl() {
    getNextToken(); // eat compare
    
    // Parse V(node)
    if (CurTok != static_cast<int>(TokenType::tok_V)) {
        ReportError("expected V() after compare");
        return nullptr;
    }
    getNextToken(); // eat V
    
    if (CurTok != '(') {
        ReportError("expected '('");
        return nullptr;
    }
    getNextToken();
    
    std::string node;
    if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
        node = m_lexer.IdentifierStr;
        getNextToken();
    }
    
    if (CurTok != ')') {
        ReportError("expected ')'");
        return nullptr;
    }
    getNextToken(); // eat )
    
    // Parse with golden "name"
    if (CurTok != static_cast<int>(TokenType::tok_identifier) || 
        m_lexer.IdentifierStr != "with") {
        ReportError("expected 'with' keyword");
        return nullptr;
    }
    getNextToken(); // eat with
    
    if (CurTok != static_cast<int>(TokenType::tok_identifier) || 
        m_lexer.IdentifierStr != "golden") {
        ReportError("expected 'golden' keyword");
        return nullptr;
    }
    getNextToken(); // eat golden
    
    std::string goldenName;
    if (CurTok == static_cast<int>(TokenType::tok_string)) {
        goldenName = m_lexer.StringVal;
        getNextToken();
    } else if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
        goldenName = m_lexer.IdentifierStr;
        getNextToken();
    }
    
    // Parse tolerance
    double tolerance = 1.0; // default 1%
    if (CurTok == static_cast<int>(TokenType::tok_identifier) && 
        m_lexer.IdentifierStr == "tolerance") {
        getNextToken(); // eat tolerance
        if (CurTok != static_cast<int>(TokenType::tok_number)) {
            ReportError("expected tolerance value");
            return nullptr;
        }
        tolerance = m_lexer.NumVal;
        getNextToken();
    }
    
    std::cout << "[Parser] Compare: V(" << node << ") with golden '" 
              << goldenName << "' tol=" << tolerance << "%" << std::endl;
    
    return std::make_unique<CompareDeclAST>(node, goldenName, tolerance);
}

// ============================================================================
// Convergence Diagnostics Parser
// ============================================================================

std::unique_ptr<ExprAST> Parser::ParseConvergeDecl() {
    getNextToken(); // eat converge
    
    // Parse V(node)
    if (CurTok != static_cast<int>(TokenType::tok_V)) {
        ReportError("expected V() after converge");
        return nullptr;
    }
    getNextToken();
    
    if (CurTok != '(') {
        ReportError("expected '('");
        return nullptr;
    }
    getNextToken();
    
    std::string node;
    if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
        node = m_lexer.IdentifierStr;
        getNextToken();
    }
    
    if (CurTok != ')') {
        ReportError("expected ')'");
        return nullptr;
    }
    getNextToken(); // eat )
    
    // Parse max iterations
    int maxIter = 100;
    if (CurTok == static_cast<int>(TokenType::tok_identifier) && 
        m_lexer.IdentifierStr == "max") {
        getNextToken(); // eat max
        if (CurTok != static_cast<int>(TokenType::tok_number)) {
            ReportError("expected iteration count");
            return nullptr;
        }
        maxIter = (int)m_lexer.NumVal;
        getNextToken();
    }
    
    // Parse epsilon
    double epsilon = 1e-6;
    if (CurTok == static_cast<int>(TokenType::tok_identifier) && 
        m_lexer.IdentifierStr == "epsilon") {
        getNextToken(); // eat epsilon
        if (CurTok != static_cast<int>(TokenType::tok_number)) {
            ReportError("expected epsilon value");
            return nullptr;
        }
        epsilon = m_lexer.NumVal;
        getNextToken();
    }
    
    std::cout << "[Parser] Converge: V(" << node << ") max_iter=" 
              << maxIter << " eps=" << epsilon << std::endl;
    
    return std::make_unique<ConvergeDeclAST>(node, maxIter, epsilon);
}

std::unique_ptr<ExprAST> Parser::ParseDiscontinuityDecl() {
    getNextToken(); // eat discontinuity
    
    if (CurTok != static_cast<int>(TokenType::tok_V)) {
        ReportError("expected V() after discontinuity");
        return nullptr;
    }
    getNextToken();
    
    if (CurTok != '(') {
        ReportError("expected '('");
        return nullptr;
    }
    getNextToken();
    
    std::string node;
    if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
        node = m_lexer.IdentifierStr;
        getNextToken();
    }
    
    if (CurTok != ')') {
        ReportError("expected ')'");
        return nullptr;
    }
    getNextToken();
    
    double threshold = 0.1;
    if (CurTok == static_cast<int>(TokenType::tok_number)) {
        threshold = m_lexer.NumVal;
        getNextToken();
    }
    
    std::cout << "[Parser] Discontinuity: V(" << node << ") threshold=" 
              << threshold << std::endl;
    
    return std::make_unique<DiscontinuityDeclAST>(node, threshold);
}

std::unique_ptr<ExprAST> Parser::ParseStateDecl() {
    getNextToken(); // eat state
    
    if (CurTok != static_cast<int>(TokenType::tok_V)) {
        ReportError("expected V() after state");
        return nullptr;
    }
    getNextToken();
    
    if (CurTok != '(') {
        ReportError("expected '('");
        return nullptr;
    }
    getNextToken();
    
    std::string node;
    if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
        node = m_lexer.IdentifierStr;
        getNextToken();
    }
    
    if (CurTok != ')') {
        ReportError("expected ')'");
        return nullptr;
    }
    getNextToken();
    
    int historyDepth = 10;
    if (CurTok == static_cast<int>(TokenType::tok_number)) {
        historyDepth = (int)m_lexer.NumVal;
        getNextToken();
    }
    
    std::cout << "[Parser] State: V(" << node << ") depth=" << historyDepth << std::endl;
    
    return std::make_unique<StateDeclAST>(node, historyDepth);
}

std::unique_ptr<ExprAST> Parser::ParseVerifyBlock() {
    getNextToken(); // eat verify
    
    auto verifyBlock = std::make_unique<VerifyBlockAST>();
    
    if (CurTok == static_cast<int>(TokenType::tok_lbrace)) {
        getNextToken(); // eat {
        
        while (CurTok != static_cast<int>(TokenType::tok_rbrace) && 
               CurTok != static_cast<int>(TokenType::tok_eof)) {
            
            if (CurTok == static_cast<int>(TokenType::tok_assert)) {
                verifyBlock->addCheck(ParseAssertDecl());
            } else if (CurTok == static_cast<int>(TokenType::tok_settle)) {
                verifyBlock->addCheck(ParseSettleDecl());
            } else if (CurTok == static_cast<int>(TokenType::tok_converge)) {
                verifyBlock->addDiagnostic(ParseConvergeDecl());
            } else if (CurTok == static_cast<int>(TokenType::tok_discontinuity)) {
                verifyBlock->addDiagnostic(ParseDiscontinuityDecl());
            } else if (CurTok == static_cast<int>(TokenType::tok_state)) {
                verifyBlock->addDiagnostic(ParseStateDecl());
            } else if (CurTok == static_cast<int>(TokenType::tok_compare)) {
                verifyBlock->addComparison(ParseCompareDecl());
            } else if (CurTok == static_cast<int>(TokenType::tok_tolerance)) {
                // Parse tolerance
                double absTol = 1e-6, relTol = 1.0;
                if (CurTok == static_cast<int>(TokenType::tok_identifier) && 
                    m_lexer.IdentifierStr == "abs") {
                    getNextToken();
                    if (CurTok == '=') { getNextToken(); }
                    if (CurTok == static_cast<int>(TokenType::tok_number)) {
                        absTol = m_lexer.NumVal;
                        getNextToken();
                    }
                }
                if (CurTok == static_cast<int>(TokenType::tok_identifier) && 
                    m_lexer.IdentifierStr == "rel") {
                    getNextToken();
                    if (CurTok == '=') { getNextToken(); }
                    if (CurTok == static_cast<int>(TokenType::tok_number)) {
                        relTol = m_lexer.NumVal;
                        getNextToken();
                    }
                }
                verifyBlock->addCheck(std::make_unique<ToleranceDeclAST>(absTol, relTol));
            }
            
            if (CurTok == ';') getNextToken();
        }
        
        if (CurTok == static_cast<int>(TokenType::tok_rbrace)) {
            getNextToken(); // eat }
        }
    }
    
    std::cout << "[Parser] Verify block parsed" << std::endl;
    
    return verifyBlock;
}

} // namespace Flux
