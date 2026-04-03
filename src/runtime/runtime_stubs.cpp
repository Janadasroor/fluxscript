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
