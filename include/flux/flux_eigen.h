#ifndef FLUX_EIGEN_WRAPPER_H
#define FLUX_EIGEN_WRAPPER_H

#include <Eigen/Dense>
#include <cstdint>
#include <vector>
#include <map>
#include <iostream>

namespace Flux {

// Vector types using Eigen
using FluxVector = Eigen::VectorXd;      // Dynamic size vector
using FluxVector3d = Eigen::Vector3d;    // Fixed 3D vector for cross product
using FluxMatrix = Eigen::MatrixXd;      // Dynamic size matrix

struct MatrixInfo {
    int rows;
    int cols;
    double* data;
};

// Global manager to track matrices created on the heap
class MatrixManager {
public:
    static MatrixManager& instance() {
        static MatrixManager manager;
        return manager;
    }

    void registerMatrix(double* data, int rows, int cols) {
        m_matrices[data] = {rows, cols, data};
    }

    bool isMatrix(double* data) const {
        return m_matrices.find(data) != m_matrices.end();
    }

    MatrixInfo getInfo(double* data) const {
        auto it = m_matrices.find(data);
        if (it != m_matrices.end()) return it->second;
        return {0, 0, nullptr};
    }

    void unregisterMatrix(double* data) {
        m_matrices.erase(data);
    }

private:
    std::map<double*, MatrixInfo> m_matrices;
};

} // namespace Flux

// External C interface for JIT
extern "C" {
    // Matrix multiplication
    double flux_matrix_mul(double a, double b);
    
    // Matrix transpose
    double flux_matrix_transpose(double m);

    // Create matrix from array
    double flux_create_matrix(double* data, int rows, int cols);

    // Helper functions for vector operations
    double flux_vector_dot(double* a_data, int a_size, double* b_data, int b_size);
    double flux_vector_norm(double* data, int size);
    
    // Create vector
    double flux_create_vector_sum(double* data, int size);
}

#endif // FLUX_EIGEN_WRAPPER_H
