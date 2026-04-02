#ifndef FLUX_MLIR_H
#define FLUX_MLIR_H

#include <string>

namespace Flux::MLIR {

struct EmitResult {
    bool ok = false;
    std::string output;
    std::string error;
};

bool isAvailable();
EmitResult emitModule(const std::string& code);

} // namespace Flux::MLIR

#endif // FLUX_MLIR_H
