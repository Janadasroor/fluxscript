#ifndef FLUX_COMPILER_LIFETIME_INFERENCE_H
#define FLUX_COMPILER_LIFETIME_INFERENCE_H

#include "flux/compiler/ast.h"
#include <map>
#include <set>
#include <string>
#include <vector>

namespace Flux {

struct LifetimeOutlives {
    std::string smaller; // 'a: 'b  →  smaller='a, larger='b, meaning 'a <= 'b
    std::string larger;
};

class LifetimeInference {
public:
    // Run full inference and validation on a function
    // Returns true if no errors, false if constraints are unsatisfiable
    bool analyze(const FunctionAST* func);

    const std::vector<std::string>& getErrors() const { return m_errors; }

private:
    std::vector<std::string> m_errors;
    std::vector<LifetimeOutlives> m_constraints;

    // Collect constraints from the body expression
    void collectFromExpr(const ExprAST* expr,
                         const std::set<std::string>& lifetimeParamNames);

    // Collect constraints from type annotations (recursive)
    void collectFromType(const FluxType& type,
                         const std::string& containingLifetime,
                         const std::set<std::string>& lifetimeParamNames);

    // Check consistency: return true if valid
    bool checkConsistency(const std::set<std::string>& lifetimeParamNames,
                          const std::vector<LifetimeParam>& lifetimeParams,
                          const std::vector<std::pair<std::string, FluxType>>& args,
                          const FluxType& returnType);
};

} // namespace Flux

#endif
