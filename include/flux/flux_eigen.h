#ifndef FLUX_EIGEN_WRAPPER_H
#define FLUX_EIGEN_WRAPPER_H

#include <Eigen/Dense>
#include <cstdint>

namespace Flux {

// Vector types using Eigen
using FluxVector = Eigen::VectorXd;      // Dynamic size vector
using FluxVector3d = Eigen::Vector3d;    // Fixed 3D vector for cross product
using FluxMatrix = Eigen::MatrixXd;      // Dynamic size matrix

// Helper functions for vector operations
inline double vector_dot(const FluxVector& a, const FluxVector& b) {
    if (a.size() != b.size()) return 0.0;
    return a.dot(b);
}

inline double vector_norm(const FluxVector& v) {
    return v.norm();
}

inline double vector_cross_3d(double ax, double ay, double az, 
                               double bx, double by, double bz) {
    FluxVector3d a(ax, ay, az);
    FluxVector3d b(bx, by, bz);
    FluxVector3d result = a.cross(b);
    return result.norm();  // Return magnitude of cross product
}

inline FluxVector vector_add(const FluxVector& a, const FluxVector& b) {
    if (a.size() != b.size()) return FluxVector();
    return a + b;
}

inline FluxVector vector_sub(const FluxVector& a, const FluxVector& b) {
    if (a.size() != b.size()) return FluxVector();
    return a - b;
}

inline FluxVector vector_mul_elementwise(const FluxVector& a, const FluxVector& b) {
    if (a.size() != b.size()) return FluxVector();
    return a.cwiseProduct(b);
}

inline FluxVector vector_div_elementwise(const FluxVector& a, const FluxVector& b) {
    if (a.size() != b.size()) return FluxVector();
    return a.cwiseQuotient(b);
}

inline FluxVector vector_mul_scalar(const FluxVector& v, double s) {
    return v * s;
}

inline FluxVector vector_div_scalar(const FluxVector& v, double s) {
    return v / s;
}

// Matrix operations
inline FluxMatrix matrix_mul(const FluxMatrix& a, const FluxMatrix& b) {
    return a * b;
}

inline FluxMatrix matrix_add(const FluxMatrix& a, const FluxMatrix& b) {
    if (a.rows() != b.rows() || a.cols() != b.cols()) return FluxMatrix();
    return a + b;
}

inline FluxMatrix matrix_sub(const FluxMatrix& a, const FluxMatrix& b) {
    if (a.rows() != b.rows() || a.cols() != b.cols()) return FluxMatrix();
    return a - b;
}

inline FluxMatrix matrix_transpose(const FluxMatrix& m) {
    return m.transpose();
}

inline double matrix_det(const FluxMatrix& m) {
    if (m.rows() != m.cols()) return 0.0;
    return m.determinant();
}

// Create vector from array
inline double create_vector_sum(const double* data, int size) {
    double sum = 0.0;
    for (int i = 0; i < size; ++i) {
        sum += data[i];
    }
    return sum;
}

// Get vector element at index
inline double vector_get(const double* data, int size, int index) {
    if (index < 0 || index >= size) return 0.0;
    return data[index];
}

// Vector size
inline int vector_size(int size) {
    return size;
}

} // namespace Flux

#endif // FLUX_EIGEN_WRAPPER_H
