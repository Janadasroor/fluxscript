#include "flux/compiler/parser.h"
#include <iostream>

namespace Flux {

Parser::Parser(const std::string& input) : m_lexer(input), m_hasError(false) {
    m_binopPrecedence['='] = 2;
    m_binopPrecedence[static_cast<int>(TokenType::tok_plus_equal)] = 2;
    m_binopPrecedence[static_cast<int>(TokenType::tok_minus_equal)] = 2;
    m_binopPrecedence[static_cast<int>(TokenType::tok_star_equal)] = 2;
    m_binopPrecedence[static_cast<int>(TokenType::tok_slash_equal)] = 2;
    m_binopPrecedence[static_cast<int>(TokenType::tok_logical_or)] = 5;
    m_binopPrecedence[static_cast<int>(TokenType::tok_logical_and)] = 10;
    m_binopPrecedence['|'] = 11;
    m_binopPrecedence[static_cast<int>(TokenType::tok_bitwise_or)] = 11;
    m_binopPrecedence[static_cast<int>(TokenType::tok_bitwise_xor)] = 12;
    m_binopPrecedence['&'] = 13;
    m_binopPrecedence[static_cast<int>(TokenType::tok_bitwise_and)] = 13;
    m_binopPrecedence[static_cast<int>(TokenType::tok_equal)] = 15;
    m_binopPrecedence[static_cast<int>(TokenType::tok_not_equal)] = 15;
    m_binopPrecedence['<'] = 20;
    m_binopPrecedence['>'] = 20;
    m_binopPrecedence[static_cast<int>(TokenType::tok_less_equal)] = 20;
    m_binopPrecedence[static_cast<int>(TokenType::tok_greater_equal)] = 20;
    m_binopPrecedence[static_cast<int>(TokenType::tok_bitwise_shl)] = 25;
    m_binopPrecedence[static_cast<int>(TokenType::tok_bitwise_shr)] = 25;
    m_binopPrecedence['+'] = 30;
    m_binopPrecedence['-'] = 30;
    m_binopPrecedence['*'] = 40;
    m_binopPrecedence['/'] = 40;
    m_binopPrecedence[static_cast<int>(TokenType::tok_ew_mul)] = 40;
    m_binopPrecedence[static_cast<int>(TokenType::tok_ew_div)] = 40;
    m_binopPrecedence[static_cast<int>(TokenType::tok_ew_power)] = 50;
    m_binopPrecedence[static_cast<int>(TokenType::tok_power)] = 50;
    getNextToken();
}

int Parser::getNextToken() { return CurTok = m_lexer.getNextToken(); }

int Parser::GetTokPrecedence() {
    int tokPrec = m_binopPrecedence[CurTok];
    if (tokPrec <= 0) return -1;
    return tokPrec;
}

void Parser::ReportError(const std::string& message) {
    std::cerr << "Error: " << message << " at line " << m_lexer.getCurrentLine() << ", col " << m_lexer.getCurrentColumn() << std::endl;
    m_hasError = true;
}

void Parser::SkipToSynchronizationPoint() {
    while (CurTok != static_cast<int>(TokenType::tok_eof) && !IsSynchronizationToken(CurTok)) getNextToken();
}

bool Parser::IsSynchronizationToken(int token) {
    switch (static_cast<TokenType>(token)) {
    case TokenType::tok_def:
    case TokenType::tok_extern:
    case TokenType::tok_var:
    case TokenType::tok_let:
    case TokenType::tok_return:
    case TokenType::tok_if:
    case TokenType::tok_for:
    case TokenType::tok_while:
    case TokenType::tok_rbrace:
    case TokenType::tok_semicolon: return true;
    default: return token == static_cast<int>(TokenType::tok_semicolon);
    }
}

std::unique_ptr<ExprAST> Parser::ParseNumberExpr() {
    auto Result = std::make_unique<NumberExprAST>(m_lexer.NumVal);
    getNextToken();
    return Result;
}

std::unique_ptr<ExprAST> Parser::ParseImaginaryExpr() {
    auto Result = std::make_unique<ComplexExprAST>(0.0, m_lexer.NumVal);
    getNextToken();
    return Result;
}

std::unique_ptr<ExprAST> Parser::ParseStringExpr() {
    auto Result = std::make_unique<StringExprAST>(m_lexer.StringVal);
    getNextToken();
    return Result;
}

std::unique_ptr<ExprAST> Parser::ParseParenExpr() {
    getNextToken();
    auto V = ParseExpression();
    if (!V) return nullptr;
    if (CurTok != ')') { ReportError("expected ')'"); return nullptr; }
    getNextToken();
    return V;
}

std::unique_ptr<ExprAST> Parser::ParseIdentifierExpr() {
    std::string IdName = m_lexer.IdentifierStr;
    getNextToken();
    
    // Handle V(node), I(branch), and P(param) as special expressions for SPICE support
    if (CurTok == '(' && (IdName == "V" || IdName == "I" || IdName == "P")) {
        getNextToken(); // eat (
        if (CurTok != static_cast<int>(TokenType::tok_identifier) && 
            CurTok != static_cast<int>(TokenType::tok_number)) {
            ReportError("expected name in V() / I() / P()");
            return nullptr;
        }

        std::string name;
        if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
            name = m_lexer.IdentifierStr;
        } else {
            name = std::to_string(static_cast<int>(m_lexer.NumVal));
        }
        getNextToken(); // eat name

        if (CurTok != ')') {
            ReportError("expected ')' after name");
            return nullptr;
        }
        getNextToken(); // eat )

        if (IdName == "V") {
            return std::make_unique<VoltageExprAST>(name);
        } else if (IdName == "I") {
            return std::make_unique<CurrentExprAST>(name);
        } else {
            return std::make_unique<ParameterExprAST>(name);
        }
    }

    if (CurTok == '(') {
        getNextToken();
        std::vector<std::unique_ptr<ExprAST>> Args;
        if (CurTok != ')') {
            while (true) {
                if (auto Arg = ParseExpression()) Args.push_back(std::move(Arg));
                else return nullptr;
                if (CurTok == ')') break;
                if (CurTok != ',') { ReportError("expected ')' or ',' in argument list"); return nullptr; }
                getNextToken();
            }
        }
        getNextToken();
        return std::make_unique<CallExprAST>(IdName, std::move(Args));
    }
    return std::make_unique<VariableExprAST>(IdName);
}

std::unique_ptr<ExprAST> Parser::ParseIfExpr() {
    getNextToken();
    auto Cond = ParseExpression();
    if (!Cond) return nullptr;
    if (CurTok != static_cast<int>(TokenType::tok_then)) { ReportError("expected then"); return nullptr; }
    getNextToken();
    auto Then = ParseExpression();
    if (!Then) return nullptr;
    if (CurTok != static_cast<int>(TokenType::tok_else)) { ReportError("expected else"); return nullptr; }
    getNextToken();
    auto Else = ParseExpression();
    if (!Else) return nullptr;
    return std::make_unique<IfExprAST>(std::move(Cond), std::move(Then), std::move(Else));
}

std::unique_ptr<ExprAST> Parser::ParseForExpr() {
    getNextToken();
    if (CurTok != static_cast<int>(TokenType::tok_identifier)) { ReportError("expected identifier after for"); return nullptr; }
    std::string IdName = m_lexer.IdentifierStr;
    getNextToken();
    if (CurTok != static_cast<int>(TokenType::tok_in)) { ReportError("expected 'in' after for"); return nullptr; }
    getNextToken();
    auto Start = ParseExpression();
    if (!Start) return nullptr;
    if (CurTok != ',') { ReportError("expected ',' after for start value"); return nullptr; }
    getNextToken();
    auto End = ParseExpression();
    if (!End) return nullptr;
    std::unique_ptr<ExprAST> Step;
    if (CurTok == ',') { getNextToken(); Step = ParseExpression(); if (!Step) return nullptr; }
    if (CurTok != static_cast<int>(TokenType::tok_do)) { ReportError("expected 'do' after for"); return nullptr; }
    getNextToken();
    auto Body = ParseExpression();
    if (!Body) return nullptr;
    return std::make_unique<ForExprAST>(IdName, std::move(Start), std::move(End), std::move(Step), std::move(Body));
}

std::unique_ptr<ExprAST> Parser::ParseWhileExpr() {
    getNextToken();
    auto Cond = ParseExpression();
    if (!Cond) return nullptr;
    if (CurTok != static_cast<int>(TokenType::tok_do)) { ReportError("expected 'do' after while"); return nullptr; }
    getNextToken();
    auto Body = ParseExpression();
    if (!Body) return nullptr;
    return std::make_unique<WhileExprAST>(std::move(Cond), std::move(Body));
}

std::unique_ptr<ExprAST> Parser::ParseLetExpr() {
    bool isLet = (CurTok == static_cast<int>(TokenType::tok_let));
    getNextToken();
    if (CurTok != static_cast<int>(TokenType::tok_identifier)) { ReportError("expected identifier after let/var"); return nullptr; }
    std::string IdName = m_lexer.IdentifierStr;
    getNextToken();
    FluxType Type(TypeKind::Auto);
    if (CurTok == static_cast<int>(TokenType::tok_colon)) { getNextToken(); Type = FluxType::fromToken(CurTok); getNextToken(); }
    if (CurTok != '=') { ReportError("expected '=' after let/var"); return nullptr; }
    getNextToken();
    auto Init = ParseExpression();
    if (!Init) return nullptr;
    if (CurTok != static_cast<int>(TokenType::tok_in)) { ReportError("expected 'in' after let/var"); return nullptr; }
    getNextToken();
    auto Body = ParseExpression();
    if (!Body) return nullptr;
    return std::make_unique<LetExprAST>(IdName, Type, std::move(Init), std::move(Body));
}

std::unique_ptr<ExprAST> Parser::ParseLambdaExpr() {
    getNextToken();
    if (CurTok != '(') { ReportError("expected '(' in lambda"); return nullptr; }
    getNextToken();
    std::vector<std::string> Args;
    if (CurTok != ')') {
        while (true) {
            if (CurTok != static_cast<int>(TokenType::tok_identifier)) { ReportError("expected identifier in lambda args"); return nullptr; }
            Args.push_back(m_lexer.IdentifierStr);
            getNextToken();
            if (CurTok == ')') break;
            if (CurTok != ',') { ReportError("expected ')' or ',' in lambda args"); return nullptr; }
            getNextToken();
        }
    }
    getNextToken();
    auto Body = ParseExpression();
    if (!Body) return nullptr;
    return std::make_unique<LambdaExprAST>(std::move(Args), std::move(Body));
}

std::unique_ptr<ExprAST> Parser::ParseUnaryExpr() {
    bool isUnary = false;
    if (isascii(CurTok)) {
        if (CurTok == '-' || CurTok == '+' || CurTok == '!' || CurTok == '~') isUnary = true;
    } else {
        if (CurTok == static_cast<int>(TokenType::tok_bitwise_not) ||
            CurTok == static_cast<int>(TokenType::tok_logical_not)) isUnary = true;
    }

    if (!isUnary || CurTok == '(' || CurTok == ',' || CurTok == '[' || CurTok == '{') return ParsePrimary();
    int Opc = CurTok;
    getNextToken();
    if (auto Operand = ParseUnaryExpr()) return std::make_unique<UnaryExprAST>(Opc, std::move(Operand));
    return nullptr;
}

std::unique_ptr<ExprAST> Parser::ParseMatrixExpr() {
    getNextToken();
    std::vector<std::vector<std::unique_ptr<ExprAST>>> Rows;
    if (CurTok == ']') { getNextToken(); return std::make_unique<MatrixExprAST>(std::move(Rows), 0, 0); }
    int maxCols = 0;
    while (true) {
        std::vector<std::unique_ptr<ExprAST>> CurrentRow;
        while (true) {
            auto Elem = ParseExpression();
            if (!Elem) return nullptr;
            CurrentRow.push_back(std::move(Elem));
            if (CurTok == ']' || CurTok == static_cast<int>(TokenType::tok_semicolon)) break;
            if (CurTok == ',') getNextToken();
            else { ReportError("expected ',', ';', or ']' in matrix"); return nullptr; }
        }
        if ((int)CurrentRow.size() > maxCols) maxCols = CurrentRow.size();
        Rows.push_back(std::move(CurrentRow));
        if (CurTok == ']') { getNextToken(); break; }
        getNextToken();
    }
    for (auto& row : Rows) while (row.size() < (size_t)maxCols) row.push_back(std::make_unique<NumberExprAST>(0.0));
    return std::make_unique<MatrixExprAST>(std::move(Rows), Rows.size(), maxCols);
}

std::unique_ptr<ExprAST> Parser::ParseRangeExpr() {
    auto Start = ParseExpression();
    if (!Start) return nullptr;
    if (CurTok != static_cast<int>(TokenType::tok_colon)) return Start;
    getNextToken();
    auto End = ParseExpression();
    if (!End) return nullptr;
    return std::make_unique<RangeExprAST>(std::move(Start), std::move(End));
}

std::unique_ptr<ExprAST> Parser::ParseBlockExpr() {
    getNextToken();
    std::vector<std::unique_ptr<ExprAST>> Stmts;
    while (CurTok != static_cast<int>(TokenType::tok_rbrace) && CurTok != static_cast<int>(TokenType::tok_eof)) {
        if (auto Expr = ParseExpression()) Stmts.push_back(std::move(Expr));
        else return nullptr;
        if (CurTok == static_cast<int>(TokenType::tok_semicolon) || CurTok == static_cast<int>(TokenType::tok_semicolon)) getNextToken();
    }
    if (CurTok != static_cast<int>(TokenType::tok_rbrace)) { ReportError("expected '}'"); return nullptr; }
    getNextToken();
    return std::make_unique<BlockExprAST>(std::move(Stmts));
}

std::unique_ptr<ExprAST> Parser::ParsePrimary() {
    std::unique_ptr<ExprAST> Res;
    switch (CurTok) {
    case static_cast<int>(TokenType::tok_identifier): Res = ParseIdentifierExpr(); break;
    case static_cast<int>(TokenType::tok_number): Res = ParseNumberExpr(); break;
    case static_cast<int>(TokenType::tok_imaginary): Res = ParseImaginaryExpr(); break;
    case static_cast<int>(TokenType::tok_string): Res = ParseStringExpr(); break;
    case '(': Res = ParseParenExpr(); break;
    case '[': Res = ParseMatrixExpr(); break;
    case static_cast<int>(TokenType::tok_lbrace): Res = ParseBlockExpr(); break;
    case static_cast<int>(TokenType::tok_if): Res = ParseIfExpr(); break;
    case static_cast<int>(TokenType::tok_for): Res = ParseForExpr(); break;
    case static_cast<int>(TokenType::tok_while): Res = ParseWhileExpr(); break;
    case static_cast<int>(TokenType::tok_let):
    case static_cast<int>(TokenType::tok_var): Res = ParseLetExpr(); break;
    case static_cast<int>(TokenType::tok_fn): Res = ParseLambdaExpr(); break;
    default: {
        std::string msg = "unknown token in expression: ";
        if (CurTok > 0 && CurTok < 128) msg += (char)CurTok;
        else msg += std::to_string(CurTok);
        ReportError(msg);
        return nullptr;
    }
    }
    if (!Res) return nullptr;
    while (CurTok == static_cast<int>(TokenType::tok_transpose)) { Res = std::make_unique<TransposeExprAST>(std::move(Res)); getNextToken(); }
    return Res;
}

std::unique_ptr<ExprAST> Parser::ParseBinOpRHS(int ExprPrec, std::unique_ptr<ExprAST> LHS) {
    while (true) {
        int TokPrec = GetTokPrecedence();
        if (TokPrec < ExprPrec) return LHS;
        int BinOp = CurTok;
        getNextToken();
        auto RHS = ParseUnaryExpr();
        if (!RHS) return nullptr;
        int NextPrec = GetTokPrecedence();
        if (TokPrec < NextPrec) { RHS = ParseBinOpRHS(TokPrec + 1, std::move(RHS)); if (!RHS) return nullptr; }
        if (BinOp == '=') {
            if (auto* LHSE = dynamic_cast<VariableExprAST*>(LHS.get())) LHS = std::make_unique<AssignExprAST>(LHSE->getName(), std::move(RHS));
            else return nullptr;
        } else LHS = std::make_unique<BinaryExprAST>(BinOp, std::move(LHS), std::move(RHS));
    }
}

std::unique_ptr<ExprAST> Parser::ParseExpression() {
    auto LHS = ParseUnaryExpr();
    if (!LHS) return nullptr;
    return ParseBinOpRHS(0, std::move(LHS));
}

std::unique_ptr<PrototypeAST> Parser::ParsePrototype() {
    if (CurTok != static_cast<int>(TokenType::tok_identifier)) return nullptr;
    std::string FnName = m_lexer.IdentifierStr;
    getNextToken();
    if (CurTok != '(') return nullptr;
    getNextToken();
    std::vector<std::pair<std::string, FluxType>> Args;
    while (CurTok == static_cast<int>(TokenType::tok_identifier)) {
        std::string Name = m_lexer.IdentifierStr;
        getNextToken();
        FluxType Type(TypeKind::Double);
        if (CurTok == static_cast<int>(TokenType::tok_colon)) { getNextToken(); Type = FluxType::fromToken(CurTok); getNextToken(); }
        Args.push_back({Name, Type});
        if (CurTok == ')') break;
        if (CurTok != ',') return nullptr;
        getNextToken();
    }
    if (CurTok != ')') return nullptr;
    getNextToken();
    FluxType RetType(TypeKind::Double);
    if (CurTok == static_cast<int>(TokenType::tok_arrow)) { getNextToken(); RetType = FluxType::fromToken(CurTok); getNextToken(); }
    return std::make_unique<PrototypeAST>(FnName, std::move(Args), RetType);
}

std::unique_ptr<PrototypeAST> Parser::ParseExtern() { getNextToken(); return ParsePrototype(); }

std::unique_ptr<FunctionAST> Parser::ParseDefinition() {
    getNextToken();
    auto Proto = ParsePrototype();
    if (!Proto) return nullptr;
    if (auto Body = ParseExpression()) return std::make_unique<FunctionAST>(std::move(Proto), std::move(Body));
    return nullptr;
}

std::unique_ptr<FunctionAST> Parser::ParseTopLevelExpr() {
    if (auto Body = ParseExpression()) {
        FluxType RetType(TypeKind::Double);
        if (dynamic_cast<ComplexExprAST*>(Body.get())) {
            RetType = FluxType(TypeKind::Complex);
        } else if (dynamic_cast<MatrixExprAST*>(Body.get())) {
            RetType = FluxType(TypeKind::Matrix);
        }
        return std::make_unique<FunctionAST>(std::make_unique<PrototypeAST>("__anon_expr", std::vector<std::pair<std::string, FluxType>>(), RetType), std::move(Body));
    }
    return nullptr;
}

} // namespace Flux
