/* Copyright 2026 Janada Sroor
 SPDX-License-Identifier: Apache-2.0 */

#include "flux/runtime/runtime_helpers.h"
#include "flux/runtime/symbolic_helpers.h"
#include "flux/runtime/matrix_tracker.h"
#include <Eigen/Dense>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace Flux {

extern "C" double flux_sym_decl(const char* name)
{
    return make_sym_ptr(SymbolicEngine::instance().sym(name ? name : "x"));
}

extern "C" double flux_sym_number(double val)
{
    return make_sym_ptr(SymbolicExpr::makeNumber(val));
}
extern "C" double flux_sym_add(double a, double b)
{
    return make_sym_ptr(SymbolicEngine::instance().add(get_sym_ptr(a), get_sym_ptr(b)));
}
extern "C" double flux_sym_sub(double a, double b)
{
    auto sa = get_sym_ptr(a), sb = get_sym_ptr(b);
    auto neg_one = SymbolicExpr::makeNumber(-1.0);
    return make_sym_ptr(SymbolicEngine::instance().add(sa, SymbolicEngine::instance().mul(neg_one, sb)));
}
extern "C" double flux_sym_mul(double a, double b)
{
    return make_sym_ptr(SymbolicEngine::instance().mul(get_sym_ptr(a), get_sym_ptr(b)));
}
extern "C" double flux_sym_div(double a, double b)
{
    auto sa = get_sym_ptr(a), sb = get_sym_ptr(b);
    auto neg_one = SymbolicExpr::makeNumber(-1.0);
    return make_sym_ptr(SymbolicEngine::instance().mul(sa, SymbolicEngine::instance().power(sb, neg_one)));
}
extern "C" double flux_sym_pow(double a, double b)
{
    return make_sym_ptr(SymbolicEngine::instance().power(get_sym_ptr(a), get_sym_ptr(b)));
}
extern "C" double flux_sym_simplify(double e)
{
    return make_sym_ptr(SymbolicEngine::instance().simplify(get_sym_ptr(e)));
}
extern "C" double flux_sym_pdiff(double e, double v_dbl, int o)
{
    const char* v = reinterpret_cast<const char*>(static_cast<uintptr_t>(jit_bitcast<uint64_t>(v_dbl)));
    return make_sym_ptr(SymbolicEngine::instance().partial_differentiate(get_sym_ptr(e), v, o));
}
extern "C" double flux_sym_eq(double a, double b)
{
    return make_sym_ptr(SymbolicEngine::instance().eq(get_sym_ptr(a), get_sym_ptr(b)));
}
extern "C" double flux_sym_ne(double a, double b)
{
    return make_sym_ptr(SymbolicEngine::instance().ne(get_sym_ptr(a), get_sym_ptr(b)));
}

// Symbolic math operations (missing wrappers)
extern "C" double flux_sym_differentiate(double e, const char* var)
{
    return make_sym_ptr(SymbolicEngine::instance().differentiate(get_sym_ptr(e), var));
}

extern "C" double flux_sym_integrate(double e, const char* var)
{
    return make_sym_ptr(SymbolicEngine::instance().integrate(get_sym_ptr(e), var));
}

extern "C" double flux_sym_laplace(double e, const char* t, const char* s)
{
    return make_sym_ptr(SymbolicEngine::instance().laplace(get_sym_ptr(e), t, s));
}

extern "C" double flux_sym_inverse_laplace(double e, const char* s, const char* t)
{
    return make_sym_ptr(SymbolicEngine::instance().inverseLaplace(get_sym_ptr(e), s, t));
}

// Expand, factor, collect, numerator, denominator
extern "C" double flux_sym_expand(double e)
{
    return make_sym_ptr(SymbolicEngine::instance().expand(get_sym_ptr(e)));
}

extern "C" double flux_sym_factor(double e)
{
    return make_sym_ptr(SymbolicEngine::instance().factor(get_sym_ptr(e)));
}

extern "C" double flux_sym_collect(double e, const char* var)
{
    return make_sym_ptr(SymbolicEngine::instance().collect(get_sym_ptr(e), var));
}

extern "C" double flux_sym_numerator(double e)
{
    return make_sym_ptr(SymbolicEngine::instance().numerator(get_sym_ptr(e)));
}

extern "C" double flux_sym_denominator(double e)
{
    return make_sym_ptr(SymbolicEngine::instance().denominator(get_sym_ptr(e)));
}

extern "C" void* flux_sym_poles(double e)
{
    auto poles = SymbolicEngine::instance().poles(get_sym_ptr(e));
    size_t n = poles.size();
    auto mat = std::make_unique<Eigen::MatrixXd>(n, 2);
    for (size_t i = 0; i < n; ++i) {
        (*mat)(i, 0) = poles[i].real();
        (*mat)(i, 1) = poles[i].imag();
    }
    return g_matrix_tracker.register_matrix(std::move(mat));
}

extern "C" void* flux_sym_zeros(double e)
{
    auto zeros = SymbolicEngine::instance().zeros(get_sym_ptr(e));
    size_t n = zeros.size();
    auto mat = std::make_unique<Eigen::MatrixXd>(n, 2);
    for (size_t i = 0; i < n; ++i) {
        (*mat)(i, 0) = zeros[i].real();
        (*mat)(i, 1) = zeros[i].imag();
    }
    return g_matrix_tracker.register_matrix(std::move(mat));
}

// Stability analysis
extern "C" void* flux_sym_jacobian(void* e_p, int ec, void* v_p, int vc)
{
    auto* ptrs = static_cast<double*>(e_p);
    auto** names = static_cast<const char**>(v_p);
    std::vector<std::shared_ptr<SymbolicExpr>> exprs;
    for (int i = 0; i < ec; i++)
        exprs.push_back(get_sym_ptr(ptrs[i]));
    std::vector<std::string> vars;
    for (int i = 0; i < vc; i++)
        vars.push_back(names[i]);
    auto J = SymbolicEngine::instance().jacobian(exprs, vars);
    auto res = std::make_unique<Eigen::MatrixXd>(J.size(), vars.size());
    for (int r = 0; r < (int)J.size(); r++)
        for (int c = 0; c < (int)vars.size(); c++)
            (*res)(r, c) = make_sym_ptr(J[r][c]);
    return g_matrix_tracker.register_matrix(std::move(res));
}

extern "C" double flux_sym_pde_register(double eq, int vc, void* v_p)
{
    auto** names = static_cast<const char**>(v_p);
    std::vector<std::string> vars;
    for (int i = 0; i < vc; i++)
        vars.push_back(names[i]);
    return make_sym_ptr(SymbolicEngine::instance().pde_register(get_sym_ptr(eq), vars));
}

extern "C" double flux_string_concat(double a_dbl, double b_dbl);
extern "C" double flux_double_to_string(double val);
extern "C" double flux_set_diagnostic(double node_dbl, double type_dbl, double threshold);

} // namespace Flux
