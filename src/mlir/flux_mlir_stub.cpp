#include "flux/mlir/flux_mlir.h"

namespace Flux::MLIR {

bool isAvailable() {
    return false;
}

EmitResult emitModule(const std::string&) {
    EmitResult result;
    result.error =
        "MLIR support is not available in this build. Reconfigure with "
        "-DFLUX_ENABLE_MLIR=ON and an installed MLIR package.";
    return result;
}

} // namespace Flux::MLIR
