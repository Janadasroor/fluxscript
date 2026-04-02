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
    std::unique_ptr<ImportExprAST> ParseImport();
    std::unique_ptr<DebugStmtAST> ParseDebugStmt();
    std::unique_ptr<SensitivityStmtAST> ParseSensitivityStmt();
    std::unique_ptr<AskExprAST> ParseAskExpr();
    std::unique_ptr<ExplainExprAST> ParseExplainExpr();
    std::unique_ptr<SubstituteStmtAST> ParseSubstituteStmt();

    int CurTok;
    int getNextToken();
    
    // Error recovery (public for use by JIT engine)
    void SkipToSynchronizationPoint();
    bool IsSynchronizationToken(int token);
    void ReportError(const std::string& message);
    bool hasError() const { return m_hasError; }
    void clearError() { m_hasError = false; }

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
    std::unique_ptr<ExprAST> ParseLambdaExpr();
    std::unique_ptr<ExprAST> ParseUnaryExpr();
    std::unique_ptr<ExprAST> ParseVectorExpr();
    std::unique_ptr<ExprAST> ParseRangeExpr();
    std::unique_ptr<ExprAST> ParseMatrixExpr();
    std::unique_ptr<ExprAST> ParseBlockExpr();
    std::unique_ptr<ExprAST> ParsePrimary();
    std::unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec, std::unique_ptr<ExprAST> LHS);
    std::unique_ptr<PrototypeAST> ParsePrototype();
    
    // Advanced control flow
    std::unique_ptr<ExprAST> ParseSwitchExpr();
    std::unique_ptr<ExprAST> ParseBreakExpr();
    std::unique_ptr<ExprAST> ParseContinueExpr();

    int GetTokPrecedence();

    Lexer m_lexer;
    std::map<int, int> m_binopPrecedence;
    bool m_hasError;
};

} // namespace Flux

#endif // FLUX_COMPILER_PARSER_H
