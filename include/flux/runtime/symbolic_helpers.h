/* Copyright 2026 Janada Sroor
 SPDX-License-Identifier: Apache-2.0 */

#ifndef FLUX_SYMBOLIC_HELPERS_H
#define FLUX_SYMBOLIC_HELPERS_H

#include "flux/runtime/symbolic_engine.h"
#include <memory>

namespace Flux {

inline std::shared_ptr<SymbolicExpr> get_sym_ptr(double ptr)
{
    if (ptr == 0)
        return nullptr;
    return std::shared_ptr<SymbolicExpr>((SymbolicExpr*)(uintptr_t)ptr, [](SymbolicExpr*) {});
}

inline double make_sym_ptr(std::shared_ptr<SymbolicExpr> expr)
{
    if (!expr)
        return 0.0;
    auto& engine = SymbolicEngine::instance();
    return (double)(uintptr_t)engine.registerExpr(expr).get();
}

} // namespace Flux

#endif // FLUX_SYMBOLIC_HELPERS_H
