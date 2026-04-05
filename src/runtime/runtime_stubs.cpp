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

// Stub implementations for missing runtime functions
// These allow the build to complete until full implementations are ready

#include <iostream>
#include <vector>
#include <complex>
#include <string>
#include "flux/runtime/flux_sim_service.h"

extern "C" {

FluxSimulationService* g_flux_sim_service = nullptr;

// ============ Matrix Stubs ============

struct MatrixData {
    std::vector<double> data;
    int rows;
    int cols;
};

int flux_matrix_rows(void* m_ptr) {
    if (!m_ptr) return 0;
    MatrixData* mat = static_cast<MatrixData*>(m_ptr);
    return mat->rows;
}

int flux_matrix_cols(void* m_ptr) {
    if (!m_ptr) return 0;
    MatrixData* mat = static_cast<MatrixData*>(m_ptr);
    return mat->cols;
}

double flux_print_matrix(void* m_ptr) {
    if (!m_ptr) return 0.0;
    MatrixData* mat = static_cast<MatrixData*>(m_ptr);
    std::cout << "Matrix [" << mat->rows << "x" << mat->cols << "]:" << std::endl;
    for (int i = 0; i < mat->rows; ++i) {
        for (int j = 0; j < mat->cols; ++j) {
            std::cout << mat->data[i * mat->cols + j] << "\t";
        }
        std::cout << std::endl;
    }
    return 0.0;
}

// ============ Runtime Function Registration Stub ============

} // extern "C"

namespace Flux {

class FluxJIT;

void registerRuntimeFunctions(FluxJIT& jit) {
    // Stub - in full implementation this registers all runtime functions
    std::cout << "[Runtime] Registered runtime functions (stub)" << std::endl;
}

} // namespace Flux
