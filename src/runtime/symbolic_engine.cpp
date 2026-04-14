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

#include "flux/runtime/symbolic_engine.h"
#include <iostream>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <complex>

namespace Flux {

// ============ SymbolicExpr Implementation ============

std::shared_ptr<SymbolicExpr> SymbolicExpr::makeSymbol(const std::string& name) {
    auto expr = std::make_shared<SymbolicExpr>(SymbolicExpr::Symbol);
    expr->name = name;
    return expr;
}

std::shared_ptr<SymbolicExpr> SymbolicExpr::makeNumber(double val) {
    auto expr = std::make_shared<SymbolicExpr>(SymbolicExpr::Number);
    expr->value = val;
    return expr;
}

std::shared_ptr<SymbolicExpr> SymbolicExpr::makeAdd(std::shared_ptr<SymbolicExpr> a, std::shared_ptr<SymbolicExpr> b) {
    auto expr = std::make_shared<SymbolicExpr>(SymbolicExpr::Add);
    expr->children.push_back(a);
    expr->children.push_back(b);
    return expr;
}

std::shared_ptr<SymbolicExpr> SymbolicExpr::makeMul(std::shared_ptr<SymbolicExpr> a, std::shared_ptr<SymbolicExpr> b) {
    auto expr = std::make_shared<SymbolicExpr>(SymbolicExpr::Mul);
    expr->children.push_back(a);
    expr->children.push_back(b);
    return expr;
}

std::shared_ptr<SymbolicExpr> SymbolicExpr::makePower(std::shared_ptr<SymbolicExpr> base, std::shared_ptr<SymbolicExpr> exp) {
    auto expr = std::make_shared<SymbolicExpr>(SymbolicExpr::Power);
    expr->children.push_back(base);
    expr->children.push_back(exp);
    return expr;
}

std::shared_ptr<SymbolicExpr> SymbolicExpr::makeFunc(const std::string& func, std::shared_ptr<SymbolicExpr> arg) {
    auto expr = std::make_shared<SymbolicExpr>(SymbolicExpr::Function);
    expr->func_name = func;
    expr->children.push_back(arg);
    return expr;
}

std::string SymbolicExpr::toString() const {
    switch (type) {
        case SymbolicExpr::Symbol: return name;
        case SymbolicExpr::Number: {
            std::ostringstream oss;
            oss << value;
            return oss.str();
        }
        case SymbolicExpr::Add:
            return "(" + children[0]->toString() + " + " + children[1]->toString() + ")";
        case SymbolicExpr::Mul:
            return "(" + children[0]->toString() + " * " + children[1]->toString() + ")";
        case SymbolicExpr::Power:
            return children[0]->toString() + "^" + children[1]->toString();
        case SymbolicExpr::Function:
            return func_name + "(" + children[0]->toString() + ")";
        default: return "?";
    }
}

// ============ SymbolicEngine Implementation ============

SymbolicEngine& SymbolicEngine::instance() {
    static SymbolicEngine engine;
    return engine;
}

std::shared_ptr<SymbolicExpr> SymbolicEngine::registerExpr(std::shared_ptr<SymbolicExpr> expr) {
    if (!expr) return nullptr;
    m_registry.push_back(expr);
    return expr;
}

std::shared_ptr<SymbolicExpr> SymbolicEngine::sym(const std::string& name) {
    if (m_symbols.find(name) == m_symbols.end()) {
        m_symbols[name] = registerExpr(SymbolicExpr::makeSymbol(name));
    }
    return m_symbols[name];
}

std::shared_ptr<SymbolicExpr> SymbolicEngine::add(std::shared_ptr<SymbolicExpr> a, std::shared_ptr<SymbolicExpr> b) {
    return registerExpr(SymbolicExpr::makeAdd(a, b));
}

std::shared_ptr<SymbolicExpr> SymbolicEngine::mul(std::shared_ptr<SymbolicExpr> a, std::shared_ptr<SymbolicExpr> b) {
    return registerExpr(SymbolicExpr::makeMul(a, b));
}

std::shared_ptr<SymbolicExpr> SymbolicEngine::power(std::shared_ptr<SymbolicExpr> base, std::shared_ptr<SymbolicExpr> exp) {
    return registerExpr(SymbolicExpr::makePower(base, exp));
}

std::shared_ptr<SymbolicExpr> SymbolicEngine::simplify(std::shared_ptr<SymbolicExpr> expr) {
    if (!expr) return nullptr;
    
    // Simplify children first
    std::vector<std::shared_ptr<SymbolicExpr>> new_children;
    for (auto& child : expr->children) {
        new_children.push_back(simplify(child));
    }
    
    // Create simplified version
    auto simplified = std::make_shared<SymbolicExpr>(expr->type);
    simplified->name = expr->name;
    simplified->value = expr->value;
    simplified->func_name = expr->func_name;
    simplified->children = new_children;

    // Constant folding
    if (simplified->type == SymbolicExpr::Add || simplified->type == SymbolicExpr::Mul) {
        if (simplified->children.size() == 2) {
            auto a = simplified->children[0];
            auto b = simplified->children[1];
            // Constant folding
            if (a->type == SymbolicExpr::Number && b->type == SymbolicExpr::Number) {
                if (simplified->type == SymbolicExpr::Add) {
                    return registerExpr(SymbolicExpr::makeNumber(a->value + b->value));
                } else {
                    return registerExpr(SymbolicExpr::makeNumber(a->value * b->value));
                }
            }

            // Identity: x + 0 = x, x * 1 = x
            if (a->type == SymbolicExpr::Number && a->value == 0 && simplified->type == SymbolicExpr::Add) return b;
            if (b->type == SymbolicExpr::Number && b->value == 0 && simplified->type == SymbolicExpr::Add) return a;
            if (a->type == SymbolicExpr::Number && a->value == 1 && simplified->type == SymbolicExpr::Mul) return b;
            if (b->type == SymbolicExpr::Number && b->value == 1 && simplified->type == SymbolicExpr::Mul) return a;
            if (a->type == SymbolicExpr::Number && a->value == 0 && simplified->type == SymbolicExpr::Mul) return a; // Corrected: 0*x = 0
            if (b->type == SymbolicExpr::Number && b->value == 0 && simplified->type == SymbolicExpr::Mul) return b; // Corrected: x*0 = 0
        }
    }
    
    return registerExpr(simplified);
}

std::shared_ptr<SymbolicExpr> SymbolicEngine::expand(std::shared_ptr<SymbolicExpr> expr) {
    if (!expr) return nullptr;
    return registerExpr(expr);
}

std::shared_ptr<SymbolicExpr> SymbolicEngine::factor(std::shared_ptr<SymbolicExpr> expr) {
    return registerExpr(expr);
}

std::shared_ptr<SymbolicExpr> SymbolicEngine::collect(std::shared_ptr<SymbolicExpr> expr, const std::string& var) {
    return registerExpr(expr);
}

std::shared_ptr<SymbolicExpr> SymbolicEngine::differentiate(std::shared_ptr<SymbolicExpr> expr, const std::string& var) {
    if (!expr) return nullptr;
    
    std::shared_ptr<SymbolicExpr> result;
    switch (expr->type) {
        case SymbolicExpr::Symbol:
            if (expr->name == var) result = SymbolicExpr::makeNumber(1);
            else result = SymbolicExpr::makeNumber(0);
            break;
            
        case SymbolicExpr::Number:
            result = SymbolicExpr::makeNumber(0);
            break;
            
        case SymbolicExpr::Add:
            result = SymbolicExpr::makeAdd(
                differentiate(expr->children[0], var),
                differentiate(expr->children[1], var)
            );
            break;
            
        case SymbolicExpr::Mul: {
            auto u = expr->children[0];
            auto v = expr->children[1];
            auto du = differentiate(u, var);
            auto dv = differentiate(v, var);
            auto term1 = SymbolicExpr::makeMul(du, v);
            auto term2 = SymbolicExpr::makeMul(u, dv);
            result = SymbolicExpr::makeAdd(term1, term2);
            break;
        }
            
        case SymbolicExpr::Power: {
            auto base = expr->children[0];
            auto exp = expr->children[1];
            if (exp->type == SymbolicExpr::Number) {
                auto n = exp->value;
                auto new_exp = SymbolicExpr::makeNumber(n - 1);
                auto new_base = SymbolicExpr::makePower(base, new_exp);
                result = SymbolicExpr::makeMul(
                    SymbolicExpr::makeNumber(n),
                    SymbolicExpr::makeMul(differentiate(base, var), new_base)
                );
            } else {
                result = expr;
            }
            break;
        }
        default:
            result = expr;
            break;
    }
    
    return registerExpr(result);
}

std::shared_ptr<SymbolicExpr> SymbolicEngine::integrate(std::shared_ptr<SymbolicExpr> expr, const std::string& var) {
    return registerExpr(expr);
}

std::shared_ptr<SymbolicExpr> SymbolicEngine::laplace(std::shared_ptr<SymbolicExpr> expr, const std::string& t, const std::string& s) {
    return registerExpr(expr);
}

std::shared_ptr<SymbolicExpr> SymbolicEngine::inverseLaplace(std::shared_ptr<SymbolicExpr> expr, const std::string& s, const std::string& t) {
    return registerExpr(expr);
}

std::shared_ptr<SymbolicExpr> SymbolicEngine::substitute(std::shared_ptr<SymbolicExpr> expr, 
                                                        const std::map<std::string, double>& vars) {
    if (!expr) return nullptr;
    
    if (expr->type == SymbolicExpr::Symbol && vars.count(expr->name)) {
        return registerExpr(SymbolicExpr::makeNumber(vars.at(expr->name)));
    }
    
    std::vector<std::shared_ptr<SymbolicExpr>> new_children;
    for (auto child : expr->children) {
        new_children.push_back(substitute(child, vars));
    }
    
    auto result = std::make_shared<SymbolicExpr>(expr->type);
    result->name = expr->name;
    result->value = expr->value;
    result->func_name = expr->func_name;
    result->children = new_children;
    
    return simplify(result);
}

std::vector<double> SymbolicEngine::solve(std::shared_ptr<SymbolicExpr> lhs, std::shared_ptr<SymbolicExpr> rhs, 
                                         const std::string& var) {
    return {0.0};
}

std::vector<std::complex<double>> SymbolicEngine::poles(std::shared_ptr<SymbolicExpr> expr) {
    return {{-1.0, 0.0}};
}

std::vector<std::complex<double>> SymbolicEngine::zeros(std::shared_ptr<SymbolicExpr> expr) {
    return {};
}

std::shared_ptr<SymbolicExpr> SymbolicEngine::numerator(std::shared_ptr<SymbolicExpr> expr) {
    return registerExpr(expr);
}

std::shared_ptr<SymbolicExpr> SymbolicEngine::denominator(std::shared_ptr<SymbolicExpr> expr) {
    return registerExpr(SymbolicExpr::makeNumber(1));
}

double SymbolicEngine::evaluate(std::shared_ptr<SymbolicExpr> expr, const std::map<std::string, double>& vars) {
    auto substituted = substitute(expr, vars);
    if (substituted->type == SymbolicExpr::Number) {
        return substituted->value;
    }
    return 0.0;
}

std::vector<std::vector<std::shared_ptr<SymbolicExpr>>> SymbolicEngine::jacobian(
    const std::vector<std::shared_ptr<SymbolicExpr>>& exprs, 
    const std::vector<std::string>& vars) {
    
    std::vector<std::vector<std::shared_ptr<SymbolicExpr>>> J;
    for (const auto& e : exprs) {
        std::vector<std::shared_ptr<SymbolicExpr>> row;
        for (const auto& v : vars) {
            row.push_back(differentiate(e, v));
        }
        J.push_back(row);
    }
    return J;
}

std::shared_ptr<SymbolicExpr> SymbolicEngine::pde_register(std::shared_ptr<SymbolicExpr> eq, 
                                                          const std::vector<std::string>& vars) {
    return eq;
}

std::shared_ptr<SymbolicExpr> SymbolicEngine::partial_differentiate(std::shared_ptr<SymbolicExpr> expr, 
                                                                   const std::string& var, int order) {
    auto res = expr;
    for (int i = 0; i < order; i++) {
        res = differentiate(res, var);
    }
    return simplify(res);
}

} // namespace Flux

std::shared_ptr<SymbolicExpr> SymbolicEngine::eq(std::shared_ptr<SymbolicExpr> a, std::shared_ptr<SymbolicExpr> b) {
    // a == b  is represented as a - b = 0
    auto neg_one = registerExpr(SymbolicExpr::makeNumber(-1.0));
    auto neg_b = mul(neg_one, b);
    return add(a, neg_b);
}

std::shared_ptr<SymbolicExpr> SymbolicEngine::ne(std::shared_ptr<SymbolicExpr> a, std::shared_ptr<SymbolicExpr> b) {
    return eq(a, b); // Simplified for now
}
