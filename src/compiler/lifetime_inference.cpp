#include "flux/compiler/lifetime_inference.h"

#include "flux/compiler/ast.h"

#include <iostream>
#include <queue>

namespace Flux {

void LifetimeInference::collectFromExpr(const ExprAST* expr,
                                        const std::set<std::string>& lifetimeParamNames)
{
    (void)expr;
    (void)lifetimeParamNames;
}

void LifetimeInference::collectFromType(const FluxType& type,
                                        const std::string& containingLifetime,
                                        const std::set<std::string>& lifetimeParamNames)
{
    if (type.isRef()) {
        FluxType innerType = type.getRefInnerType();
        if (innerType.isRef()) {
            std::string innerLt = innerType.Lifetime;
            if (!innerLt.empty() && innerLt[0] == '\'') innerLt = innerLt.substr(1);
            if (!innerLt.empty() && lifetimeParamNames.count(innerLt)) {
                m_constraints.push_back({innerLt, containingLifetime});
            }
        }
        collectFromType(innerType, containingLifetime, lifetimeParamNames);
    }
    if (type.isStruct() && type.StructTypeId >= 0) {
    }
}

bool LifetimeInference::checkConsistency(
    const std::set<std::string>& lifetimeParamNames,
    const std::vector<LifetimeParam>& lifetimeParams,
    const std::vector<std::pair<std::string, FluxType>>& args,
    const FluxType& returnType)
{
    if (lifetimeParamNames.empty() && m_constraints.empty())
        return true;

    std::map<std::string, std::set<std::string>> outlives;
    std::map<std::string, std::set<std::string>> inbound;

    for (auto& lt : lifetimeParamNames) {
        outlives[lt];
        inbound[lt];
    }

    for (auto& c : m_constraints) {
        if (lifetimeParamNames.count(c.smaller) && lifetimeParamNames.count(c.larger)) {
            outlives[c.smaller].insert(c.larger);
            inbound[c.larger].insert(c.smaller);
        }
    }

    if (returnType.isRef()) {
        std::string retLt = returnType.Lifetime;
        if (!retLt.empty() && retLt[0] == '\'') retLt = retLt.substr(1);
        if (!retLt.empty() && lifetimeParamNames.count(retLt)) {
            bool foundSource = false;
            for (auto& arg : args) {
                if (arg.second.isRef()) {
                    std::string argLt = arg.second.Lifetime;
                    if (!argLt.empty() && argLt[0] == '\'') argLt = argLt.substr(1);
                    if (argLt == retLt) { foundSource = true; break; }
                }
            }
            if (!foundSource && retLt != "static") {
                for (auto& arg : args) {
                    if (arg.second.isRef()) {
                        std::string argLt = arg.second.Lifetime;
                        if (!argLt.empty() && argLt[0] == '\'') argLt = argLt.substr(1);
                        if (!argLt.empty() && lifetimeParamNames.count(argLt)) {
                            std::set<std::string> visited;
                            std::queue<std::string> q;
                            q.push(argLt);
                            visited.insert(argLt);
                            while (!q.empty()) {
                                std::string cur = q.front(); q.pop();
                                if (cur == retLt) { foundSource = true; break; }
                                for (auto& next : outlives[cur]) {
                                    if (!visited.count(next)) { visited.insert(next); q.push(next); }
                                }
                            }
                            if (foundSource) break;
                        }
                    }
                }
            }
            if (!foundSource && retLt != "static") {
                m_errors.push_back("[FLUX ERROR] Lifetime " + returnType.Lifetime +
                                   " in return type not connected to any parameter lifetime");
                std::cerr << m_errors.back() << std::endl;
                return false;
            }
        }
    }

    std::map<std::string, int> visited;
    for (auto& [lt, _] : outlives) visited[lt] = 0;

    std::function<bool(const std::string&)> dfs = [&](const std::string& node) -> bool {
        visited[node] = 1;
        for (auto& next : outlives[node]) {
            if (visited[next] == 1) {
                m_errors.push_back("[FLUX ERROR] Cycle in lifetime constraints: '" + node +
                                   "' and '" + next + "' depend on each other");
                std::cerr << m_errors.back() << std::endl;
                return false;
            }
            if (visited[next] == 0 && !dfs(next)) return false;
        }
        visited[node] = 2;
        return true;
    };

    for (auto& [lt, _] : outlives) {
        if (visited[lt] == 0 && !dfs(lt)) return false;
    }
    return true;
}

} // namespace Flux
