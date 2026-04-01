#ifndef FLUX_COMPILER_PARSER_H
#define FLUX_COMPILER_PARSER_H

#include "flux/compiler/ast.h"
#include "flux/compiler/lexer.h"
#include <memory>
#include <map>

namespace Flux {

class Parser {
public:
    explicit Parser(const std::string& input);

    std::unique_ptr<ExprAST> ParseExpression();
    std::unique_ptr<PrototypeAST> ParseExtern();
    std::unique_ptr<FunctionAST> ParseDefinition();
    std::unique_ptr<FunctionAST> ParseTopLevelExpr();

    int CurTok;
    int getNextToken();

private:
    std::unique_ptr<ExprAST> ParseNumberExpr();
    std::unique_ptr<ExprAST> ParseImaginaryExpr();
    std::unique_ptr<ExprAST> ParseStringExpr();
    std::unique_ptr<ExprAST> ParseParenExpr();
    std::unique_ptr<ExprAST> ParseIdentifierExpr();
    std::unique_ptr<ExprAST> ParseIfExpr();
    std::unique_ptr<ExprAST> ParseForExpr();
    std::unique_ptr<ExprAST> ParseWhileExpr();
    std::unique_ptr<ExprAST> ParseLetExpr();
    std::unique_ptr<ExprAST> ParseUnaryExpr();
    std::unique_ptr<ExprAST> ParseVectorExpr();
    std::unique_ptr<ExprAST> ParseBlockExpr();
    std::unique_ptr<ExprAST> ParsePrimary();
    std::unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec, std::unique_ptr<ExprAST> LHS);
    std::unique_ptr<PrototypeAST> ParsePrototype();

    int GetTokPrecedence();

    Lexer m_lexer;
    std::map<int, int> m_binopPrecedence;
};

} // namespace Flux

#endif // FLUX_COMPILER_PARSER_H
