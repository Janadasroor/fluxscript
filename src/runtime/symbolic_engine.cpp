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

            // Nested constant folding: k1 * (k2 * x) -> (k1*k2) * x
            if (simplified->type == SymbolicExpr::Mul && a->type == SymbolicExpr::Number && b->type == SymbolicExpr::Mul) {
                if (b->children[0]->type == SymbolicExpr::Number) {
                    auto new_k = a->value * b->children[0]->value;
                    return simplify(mul(SymbolicExpr::makeNumber(new_k), b->children[1]));
                }
            }
            
            // Identity: x + 0 = x, x * 1 = x
            if (a->type == SymbolicExpr::Number && a->value == 0 && simplified->type == SymbolicExpr::Add) return b;
            if (b->type == SymbolicExpr::Number && b->value == 0 && simplified->type == SymbolicExpr::Add) return a;
            if (a->type == SymbolicExpr::Number && a->value == 1 && simplified->type == SymbolicExpr::Mul) return b;
            if (b->type == SymbolicExpr::Number && b->value == 1 && simplified->type == SymbolicExpr::Mul) return a;
            if (a->type == SymbolicExpr::Number && a->value == 0 && simplified->type == SymbolicExpr::Mul) return a;
            if (b->type == SymbolicExpr::Number && b->value == 0 && simplified->type == SymbolicExpr::Mul) return b;
        }
    }
    
    return registerExpr(simplified);
}

std::shared_ptr<SymbolicExpr> SymbolicEngine::expand(std::shared_ptr<SymbolicExpr> expr) {
    if (!expr) return nullptr;
    
    // Recursive expansion
    if (expr->type == SymbolicExpr::Mul && expr->children.size() == 2) {
        auto a = expand(expr->children[0]);
        auto b = expand(expr->children[1]);
        
        if (a->type == SymbolicExpr::Add) {
            auto term1 = mul(a->children[0], b);
            auto term2 = mul(a->children[1], b);
            return expand(SymbolicExpr::makeAdd(term1, term2));
        }
        
        if (b->type == SymbolicExpr::Add) {
            auto term1 = mul(a, b->children[0]);
            auto term2 = mul(a, b->children[1]);
            return expand(SymbolicExpr::makeAdd(term1, term2));
        }
        
        return registerExpr(SymbolicExpr::makeMul(a, b));
    }
    
    std::vector<std::shared_ptr<SymbolicExpr>> new_children;
    for (auto& child : expr->children) {
        new_children.push_back(expand(child));
    }
    auto result = std::make_shared<SymbolicExpr>(expr->type);
    result->name = expr->name;
    result->value = expr->value;
    result->func_name = expr->func_name;
    result->children = new_children;
    return registerExpr(result);
}

std::shared_ptr<SymbolicExpr> SymbolicEngine::factor(std::shared_ptr<SymbolicExpr> expr) {
    if (!expr || expr->type != SymbolicExpr::Add) return expr;
    
    auto a = expr->children[0];
    auto b = expr->children[1];
    
    if (a->type == SymbolicExpr::Mul && b->type == SymbolicExpr::Mul && a->children.size() == 2 && b->children.size() == 2) {
        if (a->children[1]->toString() == b->children[1]->toString()) {
            auto common = a->children[1];
            auto sum = registerExpr(SymbolicExpr::makeAdd(a->children[0], b->children[0]));
            return registerExpr(SymbolicExpr::makeMul(sum, common));
        }
    }
    
    return expr;
}

std::shared_ptr<SymbolicExpr> SymbolicEngine::collect(std::shared_ptr<SymbolicExpr> expr, const std::string& var) {
    if (!expr) return nullptr;
    return expr;
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
                result = expr; // Fallback
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
    // Move everything to one side: lhs - rhs = 0
    auto eq = simplify(SymbolicExpr::makeAdd(lhs, SymbolicExpr::makeMul(SymbolicExpr::makeNumber(-1), rhs)));
    
    // Basic solver for ax^2 + bx + c = 0
    double a = 0, b = 0, c = 0;
    
    auto collect_coeffs = [&](auto self, std::shared_ptr<SymbolicExpr> e) -> void {
        if (!e) return;
        if (e->type == SymbolicExpr::Number) {
            c += e->value;
        } else if (e->type == SymbolicExpr::Symbol) {
            if (e->name == var) b += 1.0;
        } else if (e->type == SymbolicExpr::Mul) {
            auto op1 = e->children[0];
            auto op2 = e->children[1];
            if (op1->type == SymbolicExpr::Number && op2->type == SymbolicExpr::Symbol && op2->name == var) {
                b += op1->value;
            } else if (op1->type == SymbolicExpr::Symbol && op1->name == var && op2->type == SymbolicExpr::Symbol && op2->name == var) {
                a += 1.0;
            } else if (op1->type == SymbolicExpr::Number && op2->type == SymbolicExpr::Mul) {
                // handle k*x*x
                auto m = op2;
                if (m->children[0]->type == SymbolicExpr::Symbol && m->children[0]->name == var &&
                    m->children[1]->type == SymbolicExpr::Symbol && m->children[1]->name == var) {
                    a += op1->value;
                }
            }
        } else if (e->type == SymbolicExpr::Add) {
            self(self, e->children[0]);
            self(self, e->children[1]);
        }
    };
    
    collect_coeffs(collect_coeffs, eq);
    
    std::vector<double> solutions;
    if (a == 0) {
        if (b != 0) solutions.push_back(-c / b);
    } else {
        double disc = b*b - 4*a*c;
        if (disc >= 0) {
            solutions.push_back((-b + sqrt(disc)) / (2*a));
            if (disc > 0) solutions.push_back((-b - sqrt(disc)) / (2*a));
        }
    }
    
    return solutions;
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

} // namespace Flux
