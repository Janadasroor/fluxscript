#include "flux/compiler/lifetime_inference.h"
#include <algorithm>
#include <iostream>
#include <sstream>
#include <queue>

namespace Flux {

bool LifetimeInference::analyze(const FunctionAST* func)
{
    m_errors.clear();
    m_constraints.clear();

    const auto* proto = func->getProto();
    auto lifetimeParams = proto->getLifetimeParams();
    std::set<std::string> ltSet(lifetimeParams.begin(), lifetimeParams.end());

    // Collect constraints from the body
    if (auto* body = func->getBody()) {
        collectFromExpr(body, ltSet);
    }

    // Collect constraints from parameter and return type annotations
    for (auto& arg : proto->getArgs()) {
        if (arg.second.isRef() && !arg.second.Lifetime.empty()) {
            std::string lt = arg.second.Lifetime;
            if (!lt.empty() && lt[0] == '\'') lt = lt.substr(1);
            if (ltSet.count(lt)) {
                collectFromType(arg.second, lt, ltSet);
            }
        }
    }

    if (proto->getReturnType().isRef() && !proto->getReturnType().Lifetime.empty()) {
        std::string retLt = proto->getReturnType().Lifetime;
        if (!retLt.empty() && retLt[0] == '\'') retLt = retLt.substr(1);
        collectFromType(proto->getReturnType(), retLt, ltSet);
    }

    // Check consistency
    return checkConsistency(ltSet, proto->getArgs(), proto->getReturnType());
}

void LifetimeInference::collectFromExpr(const ExprAST* expr,
                                        const std::set<std::string>& lifetimeParams)
{
    if (!expr) return;

    // BorrowExprAST: &'a expr — the borrow result has lifetime 'a
    if (auto* borrow = dynamic_cast<const BorrowExprAST*>(expr)) {
        // Extract lifetime from the borrow result type from codegen
        // Since we're pre-codegen, we look at the borrow expression structure
        // The borrow creates a reference; the inner expression's lifetime (if any)
        // must outlive the borrow's lifetime
        collectFromExpr(borrow->getOperand(), lifetimeParams);
        return;
    }

    // DerefExprAST: *expr
    if (auto* deref = dynamic_cast<const DerefExprAST*>(expr)) {
        collectFromExpr(deref->getOperand(), lifetimeParams);
        return;
    }

    // VariableExprAST
    if (auto* var = dynamic_cast<const VariableExprAST*>(expr)) {
        (void)var;
        return;
    }

    // NumberExprAST, etc. — no lifetimes
    if (dynamic_cast<const NumberExprAST*>(expr)) return;
    if (dynamic_cast<const BoolExprAST*>(expr)) return;

    // LetExprAST: let x = init in body
    if (auto* let = dynamic_cast<const LetExprAST*>(expr)) {
        collectFromExpr(let->getInit(), lifetimeParams);
        collectFromExpr(let->getBody(), lifetimeParams);
        return;
    }

    // BinaryExprAST
    if (auto* bin = dynamic_cast<const BinaryExprAST*>(expr)) {
        collectFromExpr(bin->getLHS(), lifetimeParams);
        collectFromExpr(bin->getRHS(), lifetimeParams);
        return;
    }

    // UnaryExprAST
    if (auto* un = dynamic_cast<const UnaryExprAST*>(expr)) {
        collectFromExpr(un->getOperand(), lifetimeParams);
        return;
    }

    // CallExprAST — check param lifetime matching
    if (auto* call = dynamic_cast<const CallExprAST*>(expr)) {
        for (auto& arg : call->getArgs()) {
            collectFromExpr(arg.get(), lifetimeParams);
        }
        return;
    }

    // IfExprAST
    if (auto* ife = dynamic_cast<const IfExprAST*>(expr)) {
        collectFromExpr(ife->getCond(), lifetimeParams);
        collectFromExpr(ife->getThen(), lifetimeParams);
        collectFromExpr(ife->getElse(), lifetimeParams);
        return;
    }

    // ForExprAST
    if (auto* fe = dynamic_cast<const ForExprAST*>(expr)) {
        collectFromExpr(fe->getStart(), lifetimeParams);
        collectFromExpr(fe->getEnd(), lifetimeParams);
        if (fe->getStep()) collectFromExpr(fe->getStep(), lifetimeParams);
        collectFromExpr(fe->getBody(), lifetimeParams);
        return;
    }

    // BlockExprAST
    if (auto* block = dynamic_cast<const BlockExprAST*>(expr)) {
        for (auto& stmt : block->getStatements()) {
            collectFromExpr(stmt.get(), lifetimeParams);
        }
        return;
    }

    // MemberExprAST
    if (auto* mem = dynamic_cast<const MemberExprAST*>(expr)) {
        collectFromExpr(mem->getObject(), lifetimeParams);
        return;
    }

    // Array/Index Expr
    if (auto* idx = dynamic_cast<const VariableExprAST*>(expr)) {
        // already handled above
        return;
    }

    // ComprehensionExprAST
    if (auto* comp = dynamic_cast<const ComprehensionExprAST*>(expr)) {
        collectFromExpr(comp->getExpr(), lifetimeParams);
        collectFromExpr(comp->getIterable(), lifetimeParams);
        return;
    }

    // LambdaExprAST
    if (auto* lam = dynamic_cast<const LambdaExprAST*>(expr)) {
        for (auto& arg : lam->getArgs()) {
            (void)arg;
        }
        collectFromExpr(lam->getBody(), lifetimeParams);
        return;
    }
}

void LifetimeInference::collectFromType(const FluxType& type,
                                        const std::string& containingLifetime,
                                        const std::set<std::string>& lifetimeParams)
{
    if (type.isRef()) {
        // The referenced (inner) type may have its own lifetime
        // e.g., for &'a &'b T, the inner type is &'b T with lifetime 'b
        // Constraint: inner's lifetime must outlive outer's lifetime ('b: 'a)
        FluxType innerType = type.getReferencedType();
        if (innerType.isRef()) {
            std::string innerLt = innerType.Lifetime;
            if (!innerLt.empty() && innerLt[0] == '\'') innerLt = innerLt.substr(1);
            if (!innerLt.empty() && lifetimeParams.count(innerLt)) {
                m_constraints.push_back({innerLt, containingLifetime});
            }
        }
        // Recurse into the referenced type
        collectFromType(innerType, containingLifetime, lifetimeParams);
    }

    // Struct types may have ref fields with lifetimes
    if (type.isStruct() && type.StructTypeId >= 0) {
        // We don't have direct access to struct field types here without CodegenContext
        // This would need to be deferred to codegen time
    }
}

bool LifetimeInference::checkConsistency(
    const std::set<std::string>& lifetimeParams,
    const std::vector<std::pair<std::string, FluxType>>& args,
    const FluxType& returnType)
{
    if (lifetimeParams.empty() && m_constraints.empty())
        return true;

    // Build a directed graph: edge from 'a to 'b means 'a <= 'b
    std::map<std::string, std::set<std::string>> outlives; // 'a → set of lifetimes that 'a must outlive
    std::map<std::string, std::set<std::string>> inbound;  // reverse edges

    // Initialize all lifetime params
    for (auto& lt : lifetimeParams) {
        outlives[lt];
        inbound[lt];
    }

    // Add explicit lifetime bounds from function signature
    // e.g., fn foo<'a, 'b: 'a>(...) means 'b >= 'a, so 'b must outlive 'a
    for (auto& lt : lifetimeParams) {
        size_t colonPos = lt.find(':');
        if (colonPos != std::string::npos) {
            std::string smaller = lt.substr(0, colonPos);
            std::string larger = lt.substr(colonPos + 1);
            // Trim
            smaller.erase(0, smaller.find_first_not_of(" \t"));
            smaller.erase(smaller.find_last_not_of(" \t") + 1);
            larger.erase(0, larger.find_first_not_of(" \t"));
            larger.erase(larger.find_last_not_of(" \t") + 1);
            if (!smaller.empty() && !larger.empty()) {
                m_constraints.push_back({smaller, larger});
            }
        }
    }

    // Add collected constraints
    for (auto& c : m_constraints) {
        if (lifetimeParams.count(c.smaller) && lifetimeParams.count(c.larger)) {
            outlives[c.smaller].insert(c.larger);
            inbound[c.larger].insert(c.smaller);
        }
    }

    // Check return type lifetime
    if (returnType.isRef()) {
        std::string retLt = returnType.Lifetime;
        if (!retLt.empty() && retLt[0] == '\'') retLt = retLt.substr(1);

        if (!retLt.empty() && lifetimeParams.count(retLt)) {
            // Verify the return lifetime is connected to at least one param lifetime
            // (a return lifetime must be one of the param lifetimes or 'static)
            bool foundSource = false;
            for (auto& arg : args) {
                if (arg.second.isRef()) {
                    std::string argLt = arg.second.Lifetime;
                    if (!argLt.empty() && argLt[0] == '\'') argLt = argLt.substr(1);
                    if (argLt == retLt) {
                        foundSource = true;
                        break;
                    }
                }
            }

            if (!foundSource && retLt != "static") {
                // Check via constraint graph: can we reach retLt from any param lifetime?
                for (auto& arg : args) {
                    if (arg.second.isRef()) {
                        std::string argLt = arg.second.Lifetime;
                        if (!argLt.empty() && argLt[0] == '\'') argLt = argLt.substr(1);
                        if (!argLt.empty() && lifetimeParams.count(argLt)) {
                            // BFS from argLt to retLt through outlives graph
                            std::set<std::string> visited;
                            std::queue<std::string> q;
                            q.push(argLt);
                            visited.insert(argLt);
                            while (!q.empty()) {
                                std::string cur = q.front(); q.pop();
                                if (cur == retLt) {
                                    foundSource = true;
                                    break;
                                }
                                for (auto& next : outlives[cur]) {
                                    if (!visited.count(next)) {
                                        visited.insert(next);
                                        q.push(next);
                                    }
                                }
                            }
                            if (foundSource) break;
                        }
                    }
                }
            }

            if (!foundSource && retLt != "static") {
                std::string err = "[FLUX ERROR] Lifetime " + returnType.Lifetime +
                                  " in return type of function is not connected to any parameter lifetime";
                m_errors.push_back(err);
                std::cerr << err << std::endl;
                return false;
            }
        }
    }

    // Check for cycles in the constraint graph
    // Use DFS to find cycles
    std::map<std::string, int> visited; // 0=unvisited, 1=in-progress, 2=done
    for (auto& [lt, _] : outlives) {
        visited[lt] = 0;
    }

    std::function<bool(const std::string&)> dfs = [&](const std::string& node) -> bool {
        visited[node] = 1;
        for (auto& next : outlives[node]) {
            if (visited[next] == 1) {
                // Cycle detected
                std::string err = "[FLUX ERROR] Cycle in lifetime constraints: '" + node +
                                  "' and '" + next + "' depend on each other";
                m_errors.push_back(err);
                std::cerr << err << std::endl;
                return false;
            }
            if (visited[next] == 0) {
                if (!dfs(next)) return false;
            }
        }
        visited[node] = 2;
        return true;
    };

    for (auto& [lt, _] : outlives) {
        if (visited[lt] == 0) {
            if (!dfs(lt)) return false;
        }
    }

    return true;
}

} // namespace Flux
