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

#ifndef FLUX_COMPILER_PARSER_H
#define FLUX_COMPILER_PARSER_H

#include "flux/compiler/ast.h"
#include "flux/compiler/lexer.h"
#include <map>
#include <memory>
#include <unordered_set>

namespace Flux {

class Parser
{
public:
    explicit Parser(const std::string& input);

    std::unique_ptr<ExprAST> ParseExpression();
    std::unique_ptr<PrototypeAST> ParseExtern();
    std::unique_ptr<FunctionAST> ParseDefinition();
    std::unique_ptr<FunctionAST> ParseTopLevelExpr();
    std::unique_ptr<ExprAST> ParseImport();
    std::unique_ptr<ExprAST> ParseFromImport();
    std::unique_ptr<DebugStmtAST> ParseDebugStmt();
    std::unique_ptr<SensitivityStmtAST> ParseSensitivityStmt();
    std::unique_ptr<AskExprAST> ParseAskExpr();
    std::unique_ptr<ExplainExprAST> ParseExplainExpr();
    std::unique_ptr<SubstituteStmtAST> ParseSubstituteStmt();
    std::unique_ptr<ExprAST> ParseUpdateFunc();
    std::unique_ptr<FunctionAST> ParseAsyncDef();

    // Time-domain simulation and SPICE
    std::unique_ptr<AnalysisExprAST> ParseAnalysis();
    std::unique_ptr<MeasureExprAST> ParseMeasure();
    std::unique_ptr<SubcktAST> ParseSubckt();
    std::unique_ptr<ModelAST> ParseModel();

    // User-defined types
    std::unique_ptr<StructDeclAST> ParseStructDecl();
    std::unique_ptr<ExprAST> ParseStructConstructExpr(const std::string& TypeName,
                                                       const std::vector<FluxType>& GenericTypeArgs = {});
    std::unique_ptr<EnumDeclAST> ParseEnumDecl(
        std::vector<std::unique_ptr<StructDeclAST>>* anonStructs = nullptr);
    std::unique_ptr<ImplDeclAST> ParseImplDecl();
    std::unique_ptr<TraitDeclAST> ParseTraitDecl();
    bool ParseClassDecl(std::unique_ptr<StructDeclAST>* classStruct, std::unique_ptr<ImplDeclAST>* classImpl);

    int CurTok;
    int getNextToken();

    // Error recovery (public for use by JIT engine)
    void SkipToSynchronizationPoint();
    bool IsSynchronizationToken(int token);
    void ReportError(const std::string& message);
    bool hasError() const { return m_hasError; }
    void clearError() { m_hasError = false; }

    // Access parser errors (for LSP diagnostics)
    const std::vector<LexerDiagnostic>& getErrors() const { return m_lexer.getErrors(); }

private:
    std::unique_ptr<ExprAST> ParseNumberExpr();
    std::unique_ptr<ExprAST> ParseFixedExpr();
    std::unique_ptr<ExprAST> ParseImaginaryExpr();
    std::unique_ptr<ExprAST> ParseStringExpr();
    std::unique_ptr<ExprAST> ParseBoolExpr();
    std::unique_ptr<ExprAST> ParseParenExpr();
    std::unique_ptr<ExprAST> ParseIdentifierExpr();
    std::unique_ptr<ExprAST> ParseIfExpr();
    std::unique_ptr<ExprAST> ParseForExpr();
    std::unique_ptr<ExprAST> ParseWhileExpr();
    std::unique_ptr<ExprAST> ParseLetExpr();
    std::unique_ptr<ExprAST> ParseLambdaExpr();

    // Statement-based control flow parsers
    std::unique_ptr<ExprAST> ParseIfStmt();
    std::unique_ptr<ExprAST> ParseForStmt();
    std::unique_ptr<ExprAST> ParseWhileStmt();
    std::vector<std::unique_ptr<ExprAST>> ParseStmtBlock();
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

    // Additional keywords
    std::unique_ptr<ExprAST> ParseTryCatchExpr();
    std::unique_ptr<ExprAST> ParseThrowExpr();
    std::unique_ptr<ExprAST> ParseAssertExpr();
    std::unique_ptr<ExprAST> ParseYieldExpr();
    std::unique_ptr<ExprAST> ParseAwaitExpr();
    std::unique_ptr<ExprAST> ParseCornerExpr();
    std::unique_ptr<ExprAST> ParseMatchExpr();
    // Parse a match pattern: identifier, number, Enum.Variant, or Enum.Variant(binding_var)
    // Returns (pattern_expr, binding_var_name)
    std::pair<std::unique_ptr<ExprAST>, std::string> ParseMatchPattern();
    std::unique_ptr<ExprAST> ParseForeachExpr();
    std::unique_ptr<ExprAST> ParseRepeatUntil();
    std::unique_ptr<ExprAST> ParseDoWhile();
    std::unique_ptr<ExprAST> ParseParallelForExpr();

    // Schematic generation
    std::unique_ptr<ExprAST> ParseSchematicExpr();
    std::unique_ptr<ExprAST> ParseExportSchematic();

    // Symbolic math
    std::unique_ptr<ExprAST> ParseSymDecl();
    std::unique_ptr<ExprAST> ParseSimplifyExpr();
    std::unique_ptr<ExprAST> ParseDifferentiateExpr();
    std::unique_ptr<ExprAST> ParseSubstituteExpr();
    std::unique_ptr<ExprAST> ParseEvaluateExpr();
    std::unique_ptr<ExprAST> ParseJacobianExpr();
    std::unique_ptr<ExprAST> ParsePDEExpr();
    std::unique_ptr<ExprAST> ParsePartialDiffExpr();
    std::unique_ptr<ExprAST> ParseIntegrateExpr();
    std::unique_ptr<ExprAST> ParseLaplaceExpr();
    std::unique_ptr<ExprAST> ParseInverseLaplaceExpr();
    std::unique_ptr<ExprAST> ParseExpandExpr();
    std::unique_ptr<ExprAST> ParseFactorExpr();
    std::unique_ptr<ExprAST> ParseCollectExpr();
    std::unique_ptr<ExprAST> ParseNumeratorExpr();
    std::unique_ptr<ExprAST> ParseDenominatorExpr();
    std::unique_ptr<ExprAST> ParsePolesExpr();
    std::unique_ptr<ExprAST> ParseZerosExpr();

    // AI / Neural Network
    std::unique_ptr<ExprAST> ParseTrainExpr();
    std::unique_ptr<ExprAST> ParsePredictExpr();
    std::unique_ptr<ExprAST> ParseGoalExpr();
    std::unique_ptr<ExprAST> ParseOptimizeExpr();
    std::unique_ptr<ExprAST> ParseBodeExpr();
    std::unique_ptr<ExprAST> ParsePlotExpr();
    std::unique_ptr<ExprAST> ParseThermalBlock();
    std::unique_ptr<ExprAST> ParseMonteCarlo();
    std::unique_ptr<ExprAST> ParseVirtualProbe();
    std::unique_ptr<ExprAST> ParseHotSwap();

    // Time-domain simulation
    std::unique_ptr<ExprAST> ParseBSourceDecl();
    std::unique_ptr<ExprAST> ParseTransientAnalysis();
    std::unique_ptr<ExprAST> ParseInitialCond();
    std::unique_ptr<ExprAST> ParseTimeVar();
    std::unique_ptr<ExprAST> ParseTimestepVar();
    std::unique_ptr<ExprAST> ParseTempVar();
    std::unique_ptr<ExprAST> ParseInputsExpr();
    std::unique_ptr<ExprAST> ParseOutputsExpr();

    // SPICE Time-Domain Simulation
    std::unique_ptr<ExprAST> ParseBuiltinVar();
    std::unique_ptr<ExprAST> ParseBSource();
    std::unique_ptr<ExprAST> ParseESource();
    std::unique_ptr<ExprAST> ParseFSource();
    std::unique_ptr<ExprAST> ParseGSource();
    std::unique_ptr<ExprAST> ParseHSource();
    std::unique_ptr<ExprAST> ParseWaveFile();
    std::unique_ptr<ExprAST> ParseMonteCarloAnalysis();
    std::unique_ptr<ExprAST> ParseWorstCaseAnalysis();
    std::unique_ptr<ExprAST> ParseStabilityAnalysis();
    std::unique_ptr<ExprAST> ParseSensitivityAnalysis();
    std::unique_ptr<ExprAST> ParseOptimization();

    std::unique_ptr<ExprAST> ParseProbe();
    std::unique_ptr<ExprAST> ParseSave();

    /* Hierarchical Design */
    std::unique_ptr<ExprAST> ParseSubcktInstance();

    /* Verilog-A Lite */
    std::unique_ptr<ExprAST> ParseAnalogBlock();
    std::unique_ptr<ExprAST> ParseDdtExpr();
    std::unique_ptr<ExprAST> ParseIdtExpr();

    /* Symbol Pin Mapping */
    std::unique_ptr<ExprAST> ParseSymbolDecl();
    std::unique_ptr<ExprAST> ParsePinMap();

    /* Section 7.1: Model Quality & Verification */
    std::unique_ptr<ExprAST> ParseAssertDecl();
    std::unique_ptr<ExprAST> ParseSettleDecl();
    std::unique_ptr<ExprAST> ParseGoldenDecl();
    std::unique_ptr<ExprAST> ParseCompareDecl();
    std::unique_ptr<ExprAST> ParseConvergeDecl();
    std::unique_ptr<ExprAST> ParseDiscontinuityDecl();
    std::unique_ptr<ExprAST> ParseStateDecl();
    std::unique_ptr<ExprAST> ParseDiagnosticDecl();
    std::unique_ptr<ExprAST> ParseToleranceDecl();
    std::unique_ptr<ExprAST> ParseVerifyBlock();
    std::unique_ptr<ExprAST> ParseParam();
    std::unique_ptr<ExprAST> ParseIC();

    /* Section 7.2: Mixed-Signal & Modeling Extensions */
    // Event-driven constructs
    std::unique_ptr<ExprAST> ParseCrossExpr();
    std::unique_ptr<ExprAST> ParseAboveExpr();
    std::unique_ptr<ExprAST> ParseTimerExpr();
    std::unique_ptr<ExprAST> ParseToleranceExpr();

    // Real-valued digital modeling
    std::unique_ptr<ExprAST> ParseFSMExpr();
    std::unique_ptr<ExprAST> ParseEdgeExpr();
    std::unique_ptr<ExprAST> ParseTriggeredExpr();

    // Noise modeling primitives
    std::unique_ptr<ExprAST> ParseNoiseExpr();
    std::unique_ptr<ExprAST> ParseWhiteNoiseExpr();
    std::unique_ptr<ExprAST> ParseFlickerNoiseExpr();
    std::unique_ptr<ExprAST> ParseThermalNoiseExpr();

    // Piecewise and table-based models
    std::unique_ptr<ExprAST> ParsePiecewiseExpr();
    std::unique_ptr<ExprAST> ParseTableExpr();
    std::unique_ptr<ExprAST> ParseCsvImportExpr();

    // Units and dimensional analysis
    std::unique_ptr<ExprAST> ParseUnitExpr();
    std::unique_ptr<ExprAST> ParseDimensionExpr();
    std::unique_ptr<ExprAST> ParseConvertExpr();
    std::unique_ptr<ExprAST> ParseHasUnitExpr();

    // Advanced features
    std::unique_ptr<ExprAST> ParsePlotDecl();
    std::unique_ptr<ExprAST> ParseBenchmarkDecl();
    std::unique_ptr<ExprAST> ParseOptimizeDecl();
    std::unique_ptr<ExprAST> ParseSweepDecl();
    std::unique_ptr<ExprAST> ParseADeviceExpr();
    std::unique_ptr<ExprAST> ParseReportDecl();

    int GetTokPrecedence();

    // Generics support
    std::vector<std::string> ParseGenericParams(
        std::map<std::string, std::vector<std::string>>* outBounds = nullptr);
    std::vector<FluxType> ParseGenericTypeArgs();

    // Helper: parse a type name from the current token
    FluxType parseTypeName(const std::vector<std::string>& genericParams = {},
                           const std::vector<std::string>& lifetimeParams = {});

    Lexer m_lexer;
    std::map<int, int> m_binopPrecedence;
    bool m_hasError;
    // Active generic type parameter names during prototype parsing.
    // Populated by ParsePrototype() so ParseIdentifierExpr() and type
    // parsing can detect generic type references.
    std::vector<std::string> m_activeGenericParams;
    // Active lifetime parameter names during impl block method parsing.
    // Populated by ParseImplDecl() so parseTypeName() can validate lifetimes
    // in method signatures without requiring each method to redeclare them.
    std::vector<std::string> m_activeLifetimeParams;
    // User-defined struct and enum type names known to the parser.
    // Populated by ParseStructDecl() / ParseEnumDecl() so parseTypeName()
    // can recognize them and produce the correct FluxType for resolution at codegen time.
    std::unordered_set<std::string> m_knownStructTypeNames;
    std::unordered_set<std::string> m_knownEnumTypeNames;
    // Trait type names known to the parser.
    std::unordered_set<std::string> m_knownTraitNames;
};

} // namespace Flux

#endif // FLUX_COMPILER_PARSER_H
