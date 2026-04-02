#ifndef FLUX_ADVANCED_FEATURES_H
#define FLUX_ADVANCED_FEATURES_H

#include "flux/compiler/ast.h"
#include <memory>

namespace Flux {

// Forward declarations for parser
class Parser;

// Parser method declarations
std::unique_ptr<ExprAST> ParsePlotDecl();
std::unique_ptr<ExprAST> ParseBenchmarkDecl();
std::unique_ptr<ExprAST> ParseOptimizeDecl();
std::unique_ptr<ExprAST> ParseSweepDecl();
std::unique_ptr<ExprAST> ParseReportDecl();

} // namespace Flux

#endif // FLUX_ADVANCED_FEATURES_H
