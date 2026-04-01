#include "flux/compiler/parser.h"
#include <iostream>

namespace Flux {

Parser::Parser(const std::string& input) : m_lexer(input) {
    // Operator precedence (higher binds tighter)
    m_binopPrecedence['='] = 2;
    m_binopPrecedence[static_cast<int>(TokenType::tok_logical_or)] = 5;
    m_binopPrecedence[static_cast<int>(TokenType::tok_logical_and)] = 10;
    m_binopPrecedence['|'] = 11;  // bitwise OR (character)
    m_binopPrecedence[static_cast<int>(TokenType::tok_bitwise_or)] = 11;
    m_binopPrecedence[static_cast<int>(TokenType::tok_bitwise_xor)] = 12;
    m_binopPrecedence['&'] = 13;  // bitwise AND (character)
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
    // Element-wise operators have same precedence as their scalar counterparts
    m_binopPrecedence[static_cast<int>(TokenType::tok_ew_mul)] = 40;
    m_binopPrecedence[static_cast<int>(TokenType::tok_ew_div)] = 40;
    m_binopPrecedence[static_cast<int>(TokenType::tok_ew_power)] = 50;
    m_binopPrecedence[static_cast<int>(TokenType::tok_power)] = 50;

    getNextToken(); // prime the lexer
}

int Parser::getNextToken() {
    return CurTok = m_lexer.getNextToken();
}

int Parser::GetTokPrecedence() {
    int tokPrec = m_binopPrecedence[CurTok];
    if (tokPrec <= 0) return -1;
    return tokPrec;
}

std::unique_ptr<ExprAST> Parser::ParseNumberExpr() {
    auto Result = std::make_unique<NumberExprAST>(m_lexer.NumVal);
    getNextToken();
    return std::move(Result);
}

std::unique_ptr<ExprAST> Parser::ParseImaginaryExpr() {
    auto Result = std::make_unique<ComplexExprAST>(0.0, m_lexer.NumVal);
    getNextToken();
    return std::move(Result);
}

std::unique_ptr<ExprAST> Parser::ParseStringExpr() {
    auto Result = std::make_unique<StringExprAST>(m_lexer.StringVal);
    getNextToken();
    return std::move(Result);
}

std::unique_ptr<ExprAST> Parser::ParseParenExpr() {
    getNextToken(); // eat (.
    auto V = ParseExpression();
    if (!V) return nullptr;

    if (CurTok != ')') {
        std::cerr << "expected ')'" << std::endl;
        return nullptr;
    }
    getNextToken(); // eat ).
    return V;
}

std::unique_ptr<ExprAST> Parser::ParseIdentifierExpr() {
    std::string IdName = m_lexer.IdentifierStr;
    getNextToken(); // eat identifier.

    // TODO: Check for array indexing: identifier[expr]
    // Currently disabled - IndexExprAST not implemented
    // if (CurTok == '[') {
    //     getNextToken(); // eat [
    //     auto Index = ParseExpression();
    //     if (!Index) return nullptr;
    //     if (CurTok != ']') {
    //         std::cerr << "expected ']' in array indexing" << std::endl;
    //         return nullptr;
    //     }
    //     getNextToken(); // eat ]
    //     return std::make_unique<IndexExprAST>(std::make_unique<VariableExprAST>(IdName), std::move(Index));
    // }

    if (CurTok != '(') // Simple variable ref.
        return std::make_unique<VariableExprAST>(IdName);

    // Call.
    getNextToken(); // eat (
    std::vector<std::unique_ptr<ExprAST>> Args;
    if (CurTok != ')') {
        while (true) {
            if (auto Arg = ParseExpression())
                Args.push_back(std::move(Arg));
            else
                return nullptr;

            if (CurTok == ')') break;

            if (CurTok != ',') {
                std::cerr << "Expected ')' or ',' in argument list" << std::endl;
                return nullptr;
            }
            getNextToken();
        }
    }

    getNextToken(); // Eat the ')'.
    return std::make_unique<CallExprAST>(IdName, std::move(Args));
}

std::unique_ptr<ExprAST> Parser::ParseIfExpr() {
    getNextToken(); // eat the if.

    // condition.
    auto Cond = ParseExpression();
    if (!Cond) return nullptr;

    if (CurTok != static_cast<int>(TokenType::tok_then)) {
        std::cerr << "expected then at " << CurTok << std::endl;
        return nullptr;
    }
    getNextToken(); // eat the then

    auto Then = ParseExpression();
    if (!Then) return nullptr;

    if (CurTok != static_cast<int>(TokenType::tok_else)) {
        std::cerr << "expected else at " << CurTok << std::endl;
        return nullptr;
    }

    getNextToken(); // eat else

    auto Else = ParseExpression();
    if (!Else) return nullptr;

    return std::make_unique<IfExprAST>(std::move(Cond), std::move(Then), std::move(Else));
}

std::unique_ptr<ExprAST> Parser::ParseForExpr() {
    getNextToken(); // eat the for.

    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        std::cerr << "expected identifier after for" << std::endl;
        return nullptr;
    }

    std::string IdName = m_lexer.IdentifierStr;
    getNextToken();

    if (CurTok != static_cast<int>(TokenType::tok_in)) {
        std::cerr << "expected 'in' after for variable" << std::endl;
        return nullptr;
    }
    getNextToken(); // eat 'in'

    auto Start = ParseExpression();
    if (!Start) return nullptr;

    if (CurTok != ',') {
        std::cerr << "expected ',' after start expression in for loop" << std::endl;
        return nullptr;
    }
    getNextToken(); // eat ','

    auto End = ParseExpression();
    if (!End) return nullptr;

    // Optional step
    std::unique_ptr<ExprAST> Step;
    if (CurTok == ',') {
        getNextToken(); // eat ','
        Step = ParseExpression();
        if (!Step) return nullptr;
    }

    if (CurTok != static_cast<int>(TokenType::tok_do)) {
        std::cerr << "expected 'do' after for loop range" << std::endl;
        return nullptr;
    }
    getNextToken(); // eat 'do'

    auto Body = ParseExpression();
    if (!Body) return nullptr;

    return std::make_unique<ForExprAST>(IdName, std::move(Start), std::move(End),
                                         std::move(Step), std::move(Body));
}

std::unique_ptr<ExprAST> Parser::ParseWhileExpr() {
    getNextToken(); // eat the while.

    // condition.
    auto Cond = ParseExpression();
    if (!Cond) return nullptr;

    if (CurTok != static_cast<int>(TokenType::tok_do)) {
        std::cerr << "expected 'do' after while condition" << std::endl;
        return nullptr;
    }
    getNextToken(); // eat 'do'

    auto Body = ParseExpression();
    if (!Body) return nullptr;

    return std::make_unique<WhileExprAST>(std::move(Cond), std::move(Body));
}

std::unique_ptr<ExprAST> Parser::ParseLetExpr() {
    bool isVar = (CurTok == static_cast<int>(TokenType::tok_var));
    getNextToken(); // eat the let/var.

    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        std::cerr << "expected identifier after " << (isVar ? "var" : "let") << std::endl;
        return nullptr;
    }

    std::string VarName = m_lexer.IdentifierStr;
    getNextToken();

    FluxType VarType(TypeKind::Double);
    if (CurTok == ':') {
        getNextToken();
        if (CurTok == static_cast<int>(TokenType::tok_type_float) ||
            CurTok == static_cast<int>(TokenType::tok_type_double) ||
            CurTok == static_cast<int>(TokenType::tok_type_int)) {
            VarType = FluxType::fromToken(CurTok);
            getNextToken();
        } else {
            std::cerr << "Expected type after ':' in variable declaration" << std::endl;
            return nullptr;
        }
    }

    if (CurTok != '=') {
        std::cerr << "expected '=' after " << (isVar ? "var" : "let") << " identifier" << std::endl;
        return nullptr;
    }
    getNextToken(); // eat '='

    auto Init = ParseExpression();
    if (!Init) return nullptr;

    if (CurTok == static_cast<int>(TokenType::tok_in)) {
        getNextToken(); // eat 'in'
        auto Body = ParseExpression();
        if (!Body) return nullptr;
        return std::make_unique<LetExprAST>(VarName, FluxType(TypeKind::Double), std::move(Init), std::move(Body));
    } else {
        return Init;
    }
}

std::unique_ptr<ExprAST> Parser::ParseVectorExpr() {
    getNextToken(); // eat [
    
    std::vector<std::unique_ptr<ExprAST>> Elements;
    
    // Empty vector case: []
    if (CurTok == ']') {
        getNextToken();
        return std::make_unique<VectorExprAST>(std::move(Elements));
    }
    
    // Parse elements
    while (true) {
        auto Elem = ParseExpression();
        if (!Elem) return nullptr;
        Elements.push_back(std::move(Elem));
        
        if (CurTok == ']') {
            getNextToken();
            break;
        }
        if (CurTok != ',') {
            std::cerr << "expected ',' or ']' in vector literal" << std::endl;
            return nullptr;
        }
        getNextToken(); // eat ,
    }
    
    return std::make_unique<VectorExprAST>(std::move(Elements));
}

std::unique_ptr<ExprAST> Parser::ParseBlockExpr() {
    getNextToken(); // eat {
    
    std::vector<std::unique_ptr<ExprAST>> Statements;
    
    while (CurTok != static_cast<int>(TokenType::tok_rbrace) && 
           CurTok != static_cast<int>(TokenType::tok_eof)) {
        auto Stmt = ParseExpression();
        if (!Stmt) return nullptr;
        Statements.push_back(std::move(Stmt));
    }
    
    if (CurTok != static_cast<int>(TokenType::tok_rbrace)) {
        std::cerr << "expected '}' to close block" << std::endl;
        return nullptr;
    }
    getNextToken(); // eat }
    
    return std::make_unique<BlockExprAST>(std::move(Statements));
}

std::unique_ptr<ExprAST> Parser::ParseUnaryExpr() {
    if (CurTok == '!' ||
        CurTok == static_cast<int>(TokenType::tok_logical_not) ||
        CurTok == '~' ||
        CurTok == static_cast<int>(TokenType::tok_bitwise_not)) {
        int Op = CurTok;
        getNextToken();
        auto Operand = ParseUnaryExpr();
        if (!Operand) return nullptr;
        return std::make_unique<UnaryExprAST>(Op, std::move(Operand));
    }

    if (CurTok == '-' || CurTok == '+') {
        int Op = CurTok;
        getNextToken();
        auto Operand = ParseUnaryExpr();
        if (!Operand) return nullptr;
        return std::make_unique<UnaryExprAST>(Op, std::move(Operand));
    }

    return ParsePrimary();
}

std::unique_ptr<ExprAST> Parser::ParsePrimary() {
    switch (CurTok) {
    default:
        std::cerr << "unknown token " << CurTok << " when expecting an expression" << std::endl;
        return nullptr;
    case static_cast<int>(TokenType::tok_identifier):
        return ParseIdentifierExpr();
    case static_cast<int>(TokenType::tok_number):
        return ParseNumberExpr();
    case static_cast<int>(TokenType::tok_imaginary):
        return ParseImaginaryExpr();
    case static_cast<int>(TokenType::tok_string):
        return ParseStringExpr();
    case '(':
        return ParseParenExpr();
    case '[':
        return ParseVectorExpr();
    case '{':
        return ParseBlockExpr();
    case static_cast<int>(TokenType::tok_if):
        return ParseIfExpr();
    case static_cast<int>(TokenType::tok_for):
        return ParseForExpr();
    case static_cast<int>(TokenType::tok_while):
        return ParseWhileExpr();
    case static_cast<int>(TokenType::tok_let):
    case static_cast<int>(TokenType::tok_var):
        return ParseLetExpr();
    }
}

std::unique_ptr<ExprAST> Parser::ParseExpression() {
    auto LHS = ParseUnaryExpr();
    if (!LHS) return nullptr;

    return ParseBinOpRHS(0, std::move(LHS));
}

std::unique_ptr<ExprAST> Parser::ParseBinOpRHS(int ExprPrec, std::unique_ptr<ExprAST> LHS) {
    while (true) {
        int TokPrec = GetTokPrecedence();

        if (TokPrec < ExprPrec)
            return LHS;

        int BinOp = CurTok;
        
        // Handle right-associative operators (power)
        bool isRightAssociative = (BinOp == static_cast<int>(TokenType::tok_power));
        
        getNextToken(); // eat binop

        auto RHS = ParseUnaryExpr();
        if (!RHS) return nullptr;

        int NextPrec = GetTokPrecedence();
        if (isRightAssociative && TokPrec <= NextPrec) {
            RHS = ParseBinOpRHS(TokPrec + 1, std::move(RHS));
            if (!RHS) return nullptr;
        } else if (!isRightAssociative && TokPrec < NextPrec) {
            RHS = ParseBinOpRHS(TokPrec + 1, std::move(RHS));
            if (!RHS) return nullptr;
        }

        // Handle assignment separately
        if (BinOp == '=') {
            VariableExprAST* LHSE = dynamic_cast<VariableExprAST*>(LHS.get());
            if (!LHSE) {
                std::cerr << "destination of '=' must be a variable" << std::endl;
                return nullptr;
            }
            LHS = std::make_unique<AssignExprAST>(LHSE->getName(), std::move(RHS));
        } else {
            LHS = std::make_unique<BinaryExprAST>(BinOp, std::move(LHS), std::move(RHS));
        }
    }
}

std::unique_ptr<PrototypeAST> Parser::ParsePrototype() {
    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        std::cerr << "Expected function name in prototype, got " << CurTok << std::endl;
        return nullptr;
    }

    std::string FnName = m_lexer.IdentifierStr;
    getNextToken();

    // Optional argument list with types
    std::vector<std::pair<std::string, FluxType>> Args;
    if (CurTok == '(') {
        getNextToken(); // eat '('
        while (CurTok == static_cast<int>(TokenType::tok_identifier)) {
            std::string ArgName = m_lexer.IdentifierStr;
            getNextToken();
            
            // Optional type annotation
            FluxType ArgType(TypeKind::Double); // Default to double
            if (CurTok == ':') {
                getNextToken(); // eat ':'
                if (CurTok == static_cast<int>(TokenType::tok_type_float) ||
                    CurTok == static_cast<int>(TokenType::tok_type_double) ||
                    CurTok == static_cast<int>(TokenType::tok_type_int)) {
                    ArgType = FluxType::fromToken(CurTok);
                    getNextToken();
                } else {
                    std::cerr << "Expected type after ':' in parameter" << std::endl;
                    return nullptr;
                }
            }
            
            Args.push_back({ArgName, ArgType});
            
            if (CurTok == ',') {
                getNextToken(); // eat ','
            }
        }
        if (CurTok != ')') {
            std::cerr << "Expected ')' in prototype, got " << (char)CurTok << " (" << CurTok << ")" << std::endl;
            return nullptr;
        }
        getNextToken(); // eat ')'.
    }

    // Optional return type
    FluxType ReturnType(TypeKind::Double); // Default to double
    if (CurTok == static_cast<int>(TokenType::tok_arrow)) {
        getNextToken(); // eat '->'
        if (CurTok == static_cast<int>(TokenType::tok_type_float) ||
            CurTok == static_cast<int>(TokenType::tok_type_double) ||
            CurTok == static_cast<int>(TokenType::tok_type_int) ||
            CurTok == static_cast<int>(TokenType::tok_type_void)) {
            ReturnType = FluxType::fromToken(CurTok);
            getNextToken();
        } else {
            std::cerr << "Expected return type after '->'" << std::endl;
            return nullptr;
        }
    }

    return std::make_unique<PrototypeAST>(FnName, std::move(Args), ReturnType);
}

std::unique_ptr<FunctionAST> Parser::ParseDefinition() {
    getNextToken(); // eat def.
    auto Proto = ParsePrototype();
    if (!Proto) return nullptr;

    if (auto E = ParseExpression())
        return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));
    return nullptr;
}

std::unique_ptr<FunctionAST> Parser::ParseTopLevelExpr() {
    if (auto E = ParseExpression()) {
        auto Proto = std::make_unique<PrototypeAST>("__anon_expr", std::vector<std::pair<std::string, FluxType>>());
        return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));
    }
    return nullptr;
}

std::unique_ptr<PrototypeAST> Parser::ParseExtern() {
    getNextToken(); // eat extern.
    return ParsePrototype();
}

} // namespace Flux
