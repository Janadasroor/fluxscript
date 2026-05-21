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

#include "flux/mlir/flux_mlir.h"

namespace Flux::MLIR {

bool isAvailable()
{
    return false;
}

EmitResult emitModule(const std::string&)
{
    EmitResult result;
    result.error = "MLIR support is not available in this build. Reconfigure with "
                   "-DFLUX_ENABLE_MLIR=ON and an installed MLIR package.";
    return result;
}

} // namespace Flux::MLIR
