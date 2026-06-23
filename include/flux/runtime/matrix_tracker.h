/* Copyright 2026 Janada Sroor
 SPDX-License-Identifier: Apache-2.0 */

#ifndef FLUX_MATRIX_TRACKER_H
#define FLUX_MATRIX_TRACKER_H

#include <Eigen/Dense>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace Flux {

struct MatrixTracker
{
    std::unordered_map<void*, std::unique_ptr<Eigen::MatrixXd>> matrices;
    std::unordered_map<void*, std::unique_ptr<Eigen::MatrixXcd>> complex_matrices;
    std::mutex mutex;
    bool use_gpu = false;

    MatrixTracker()
    {
        const char* gpu_env = std::getenv("FLUX_USE_GPU");
        if (gpu_env && std::string(gpu_env) == "1")
            use_gpu = true;
    }

    void* register_matrix(std::unique_ptr<Eigen::MatrixXd> mat)
    {
        std::lock_guard<std::mutex> lock(mutex);
        void* ptr = static_cast<void*>(mat->data());
        matrices[ptr] = std::move(mat);
        return ptr;
    }

    void* register_complex_matrix(std::unique_ptr<Eigen::MatrixXcd> mat)
    {
        std::lock_guard<std::mutex> lock(mutex);
        void* ptr = static_cast<void*>(mat->data());
        complex_matrices[ptr] = std::move(mat);
        return ptr;
    }

    Eigen::MatrixXd* get_matrix(void* ptr)
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = matrices.find(ptr);
        return it != matrices.end() ? it->second.get() : nullptr;
    }

    Eigen::MatrixXcd* get_complex_matrix(void* ptr)
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = complex_matrices.find(ptr);
        return it != complex_matrices.end() ? it->second.get() : nullptr;
    }

    void unregister_matrix(void* ptr)
    {
        std::lock_guard<std::mutex> lock(mutex);
        matrices.erase(ptr);
        complex_matrices.erase(ptr);
    }
};

extern MatrixTracker g_matrix_tracker;

} // namespace Flux

#endif // FLUX_MATRIX_TRACKER_H
