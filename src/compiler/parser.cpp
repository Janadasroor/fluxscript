#include "flux/compiler/parser.h"

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
    m_lexer.reportError(message);
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

std::unique_ptr<ImportExprAST> Parser::ParseImport() {
    getNextToken(); // eat import
    if (CurTok != static_cast<int>(TokenType::tok_string)) {
        ReportError("expected module name string after import");
        return nullptr;
    }
    std::string ModuleName = m_lexer.StringVal;
    getNextToken(); // eat string
    
    std::string VersionSpec;
    std::string Alias;
    std::vector<std::string> Symbols;
    
    // Check for version specification: import "module" version "1.0.0"
    if (CurTok == static_cast<int>(TokenType::tok_identifier) && 
        m_lexer.IdentifierStr == "version") {
        getNextToken(); // eat 'version'
        if (CurTok != static_cast<int>(TokenType::tok_string)) {
            ReportError("expected version string after 'version'");
            return nullptr;
        }
        VersionSpec = m_lexer.StringVal;
        getNextToken(); // eat version string
    }
    
    // Check for alias: import "module" as alias
    if (CurTok == static_cast<int>(TokenType::tok_identifier) && 
        m_lexer.IdentifierStr == "as") {
        getNextToken(); // eat 'as'
        if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
            ReportError("expected identifier for alias");
            return nullptr;
        }
        Alias = m_lexer.IdentifierStr;
        getNextToken(); // eat alias identifier
    }
    
    // Check for specific symbols: import "module" using {sym1, sym2}
    if (CurTok == static_cast<int>(TokenType::tok_identifier) && 
        m_lexer.IdentifierStr == "using") {
        getNextToken(); // eat 'using'
        if (CurTok != '{') {
            ReportError("expected '{' after 'using'");
            return nullptr;
        }
        getNextToken(); // eat '{'
        
        while (CurTok != '}' && CurTok != static_cast<int>(TokenType::tok_eof)) {
            if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
                ReportError("expected identifier in symbol list");
                return nullptr;
            }
            Symbols.push_back(m_lexer.IdentifierStr);
            getNextToken(); // eat identifier
            
            if (CurTok == ',') {
                getNextToken(); // eat ','
            } else if (CurTok != '}') {
                ReportError("expected ',' or '}' in symbol list");
                return nullptr;
            }
        }
        
        if (CurTok != '}') {
            ReportError("expected '}' to close symbol list");
            return nullptr;
        }
        getNextToken(); // eat '}'
    }
    
    return std::make_unique<ImportExprAST>(ModuleName, VersionSpec, Alias, Symbols);
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

    // Handle namespace qualifier: math::fft
    if (CurTok == static_cast<int>(TokenType::tok_namespace_sep)) {
        getNextToken(); // eat ::
        if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
            ReportError("expected identifier after ::");
            return nullptr;
        }
        std::string memberName = m_lexer.IdentifierStr;
        getNextToken(); // eat member identifier
        
        // Create namespace-qualified variable name
        std::string qualifiedName = IdName + "::" + memberName;
        
        // Check if followed by function call
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
            // Call with qualified name
            return std::make_unique<CallExprAST>(qualifiedName, std::move(Args));
        }
        
        return std::make_unique<VariableExprAST>(qualifiedName);
    }

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
    FluxType Type(TypeKind::Double);
    if (CurTok == static_cast<int>(TokenType::tok_colon)) {
        getNextToken(); // eat :
        Type = FluxType::fromToken(CurTok);
        getNextToken(); // eat type keyword
    }
    if (CurTok != '=') {
        ReportError("expected '=' after let/var");
        return nullptr;
    }
    getNextToken();
    auto Init = ParseExpression();
    if (!Init) return nullptr;

    // In block contexts, 'in' is optional. If not present, we assume it's part of a sequence.
    if (CurTok == static_cast<int>(TokenType::tok_in)) {
        getNextToken();
        auto Body = ParseExpression();
        if (!Body) return nullptr;
        return std::make_unique<LetExprAST>(IdName, Type, std::move(Init), std::move(Body));
    }

    return std::make_unique<LetExprAST>(IdName, Type, std::move(Init), nullptr);
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
        if (CurTok == '-' || CurTok == '+' || CurTok == static_cast<int>(TokenType::tok_logical_not) || CurTok == static_cast<int>(TokenType::tok_bitwise_not)) isUnary = true;
    } else {
        if (CurTok == static_cast<int>(TokenType::tok_bitwise_not) ||
            CurTok == static_cast<int>(TokenType::tok_logical_not)) isUnary = true;
    }

    if (!isUnary || CurTok == '(' || CurTok == ',' || CurTok == '[' || CurTok == static_cast<int>(TokenType::tok_lbrace)) return ParsePrimary();
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
    case static_cast<int>(TokenType::tok_import): Res = ParseImport(); break;
    case static_cast<int>(TokenType::tok_debug): Res = ParseDebugStmt(); break;
    case static_cast<int>(TokenType::tok_sensitivity): Res = ParseSensitivityStmt(); break;
    case static_cast<int>(TokenType::tok_ask): Res = ParseAskExpr(); break;
    case static_cast<int>(TokenType::tok_explain): Res = ParseExplainExpr(); break;
    case static_cast<int>(TokenType::tok_substitute): Res = ParseSubstituteStmt(); break;
    case static_cast<int>(TokenType::tok_return):
        getNextToken(); // eat return
        Res = ParseExpression();
        break;
    
    // Advanced control flow
    case static_cast<int>(TokenType::tok_switch): Res = ParseSwitchExpr(); break;
    case static_cast<int>(TokenType::tok_break): Res = ParseBreakExpr(); break;
    case static_cast<int>(TokenType::tok_continue): Res = ParseContinueExpr(); break;
    
    default: {
        std::string msg = "unknown token in expression: ";
        if (CurTok > 0 && CurTok < 128) msg += (char)CurTok;
        else msg += std::to_string(CurTok);
        ReportError(msg);
        return nullptr;
    }
    }
    if (!Res) return nullptr;
    while (true) {
        if (CurTok == static_cast<int>(TokenType::tok_transpose)) {
            Res = std::make_unique<TransposeExprAST>(std::move(Res));
            getNextToken();
        } else if (CurTok == static_cast<int>(TokenType::tok_dot)) {
            getNextToken(); // eat .
            if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
                ReportError("expected identifier after '.'");
                return nullptr;
            }
            std::string MemberName = m_lexer.IdentifierStr;
            getNextToken();
            Res = std::make_unique<MemberExprAST>(std::move(Res), MemberName);
        } else {
            break;
        }
    }
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
            LHS = std::make_unique<AssignExprAST>(std::move(LHS), std::move(RHS));
        } else {
            LHS = std::make_unique<BinaryExprAST>(BinOp, std::move(LHS), std::move(RHS));
        }
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
        if (CurTok == static_cast<int>(TokenType::tok_colon)) {
            getNextToken(); // eat :
            Type = FluxType::fromToken(CurTok);
            getNextToken(); // eat type keyword
        }
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

// Parse debug statement: debug circuit "file.flux" { symptom: "clipping" }
std::unique_ptr<DebugStmtAST> Parser::ParseDebugStmt() {
    getNextToken(); // eat debug
    
    std::string circuitFile;
    std::string symptom;
    std::map<std::string, std::string> options;
    
    // Parse circuit file
    if (CurTok == static_cast<int>(TokenType::tok_string)) {
        circuitFile = m_lexer.StringVal;
        getNextToken();
    }
    
    // Parse options block
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
                        if (key == "symptom") symptom = m_lexer.StringVal;
                        getNextToken();
                    }
                }
            }
            if (CurTok == ',') getNextToken();
            else if (CurTok != '}') break;
        }
        if (CurTok == '}') getNextToken();
    }
    
    return std::make_unique<DebugStmtAST>(circuitFile, symptom, options);
}

// Parse sensitivity statement: sensitivity(output="Vout", params=["R1", "C1"])
std::unique_ptr<SensitivityStmtAST> Parser::ParseSensitivityStmt() {
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
                        if (key == "output") output = m_lexer.StringVal;
                        else if (key == "circuit") circuit = m_lexer.StringVal;
                        getNextToken();
                    } else if (CurTok == '[') {
                        // Parse array
                        getNextToken();
                        while (CurTok != ']') {
                            if (CurTok == static_cast<int>(TokenType::tok_string)) {
                                params.push_back(m_lexer.StringVal);
                                getNextToken();
                            }
                            if (CurTok == ',') getNextToken();
                            else if (CurTok != ']') break;
                        }
                        if (CurTok == ']') getNextToken();
                    }
                }
            }
            if (CurTok == ',') getNextToken();
            else if (CurTok != ')') break;
        }
        if (CurTok == ')') getNextToken();
    }
    
    return std::make_unique<SensitivityStmtAST>(output, params, circuit);
}

// Parse ask expression: ask "Why is gain low?"
std::unique_ptr<AskExprAST> Parser::ParseAskExpr() {
    getNextToken(); // eat ask
    
    std::string question;
    if (CurTok == static_cast<int>(TokenType::tok_string)) {
        question = m_lexer.StringVal;
        getNextToken();
    }
    
    return std::make_unique<AskExprAST>(question);
}

// Parse explain expression: explain circuit "file.flux"
std::unique_ptr<ExplainExprAST> Parser::ParseExplainExpr() {
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

// Parse substitute statement: substitute "RC0603FR-0710KL"
std::unique_ptr<SubstituteStmtAST> Parser::ParseSubstituteStmt() {
    getNextToken(); // eat substitute
    
    std::string partNumber;
    std::vector<std::string> options;
    
    if (CurTok == static_cast<int>(TokenType::tok_string)) {
        partNumber = m_lexer.StringVal;
        getNextToken();
    }
    
    return std::make_unique<SubstituteStmtAST>(partNumber, options);
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
