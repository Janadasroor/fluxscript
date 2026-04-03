// ============================================================================
// FluxScript Phase 4: Scaling & Security
// Safe hot-reload, timeout protection, multi-core JIT, sparse matrix support
// ============================================================================

#ifndef FLUX_SCALING_SECURITY_H
#define FLUX_SCALING_SECURITY_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <atomic>
#include <thread>
#include <future>
#include <chrono>
#include <functional>
#include <condition_variable>

#include "flux/jit/flux_jit.h"
#include "flux/jit/jit_manager.h"

// Eigen sparse matrix support
#ifdef HAVE_EIGEN
#include <Eigen/Sparse>
#endif

namespace Flux {

// ============================================================================
// Safe Hot-Reload Manager
// ============================================================================

// Hot-reload state machine
enum class HotReloadState {
    IDLE,           // No reload in progress
    PREPARING,      // Compiling new version
    READY,          // New version ready to swap
    SWAPPING,       // Currently swapping implementations
    SWAPPED,        // Swap complete, old version still active
    CLEANUP,        // Cleaning up old version
    FAILED          // Reload failed
};

// Hot-reload transaction
struct HotReloadTransaction {
    std::string component_name;
    std::string old_source;
    std::string new_source;
    std::unique_ptr<FluxJIT> new_jit;
    void* new_update_func = nullptr;
    ComponentState saved_state;
    HotReloadState state = HotReloadState::IDLE;
    std::chrono::steady_clock::time_point start_time;
    std::string error_message;
};

// Safe hot-reload manager
class HotReloadManager {
public:
    static HotReloadManager& instance();

    // Initialize/finalize
    void initialize();
    void finalize();

    // Hot-reload operations
    bool prepareReload(const std::string& component_name, const std::string& new_source,
                      std::string* error = nullptr);
    bool executeReload(std::string* error = nullptr);
    bool abortReload(std::string* error = nullptr);
    bool commitReload(std::string* error = nullptr);
    bool rollbackReload(std::string* error = nullptr);

    // Status
    HotReloadState getState(const std::string& component_name) const;
    bool isReloadInProgress() const;
    double getReloadProgress() const;  // 0.0 to 1.0

    // Configuration
    void setMaxReloadTimeMs(int timeout_ms) { m_max_reload_time_ms = timeout_ms; }
    int getMaxReloadTimeMs() const { return m_max_reload_time_ms; }

private:
    HotReloadManager() = default;
    ~HotReloadManager() = default;

    std::map<std::string, HotReloadTransaction> m_transactions;
    mutable std::mutex m_mutex;
    int m_max_reload_time_ms = 5000;  // 5 second default timeout
};

// ============================================================================
// Timeout Protection
// ============================================================================

// Timeout exception
struct TimeoutException : public std::exception {
    const char* what() const noexcept override {
        return "JIT evaluation timed out";
    }
};

// Timeout protector for JIT evaluation
class TimeoutProtector {
public:
    static TimeoutProtector& instance();

    // Initialize/finalize
    void initialize();
    void finalize();

    // Execute with timeout
    template<typename Func>
    auto executeWithTimeout(Func&& func, int timeout_ms, const std::string& context = "")
        -> decltype(func()) {
        
        std::promise<decltype(func())> promise;
        auto future = promise.get_future();

        std::thread exec_thread([this, &promise, func = std::forward<Func>(func), context]() {
            // Register thread for timeout monitoring
            registerThread(std::this_thread::get_id(), timeout_ms, context);
            
            try {
                auto result = func();
                promise.set_value(std::move(result));
            } catch (...) {
                promise.set_exception(std::current_exception());
            }
            
            unregisterThread(std::this_thread::get_id());
        });

        // Wait with timeout
        if (future.wait_for(std::chrono::milliseconds(timeout_ms)) == 
            std::future_status::timeout) {
            // Timeout occurred
            m_timeout_triggered.store(true);
            throw TimeoutException();
        }

        exec_thread.join();
        return future.get();
    }

    // Check if current thread should timeout
    bool shouldTimeout() const;
    int getRemainingTimeMs() const;

    // Configuration
    void setDefaultTimeoutMs(int timeout_ms) { m_default_timeout_ms = timeout_ms; }
    int getDefaultTimeoutMs() const { return m_default_timeout_ms; }

    // Statistics
    struct TimeoutStats {
        int total_evaluations = 0;
        int timeouts = 0;
        double avg_eval_time_ms = 0.0;
        double max_eval_time_ms = 0.0;
        int active_threads = 0;
    };
    TimeoutStats getStatistics() const;
    void resetStatistics();

private:
    struct ThreadInfo {
        std::thread::id id;
        int timeout_ms;
        std::string context;
        std::chrono::steady_clock::time_point start_time;
    };

    void registerThread(std::thread::id id, int timeout_ms, const std::string& context);
    void unregisterThread(std::thread::id id);

    std::map<std::thread::id, ThreadInfo> m_active_threads;
    mutable std::mutex m_mutex;
    std::atomic<bool> m_timeout_triggered{false};
    int m_default_timeout_ms = 1000;  // 1 second default
    TimeoutStats m_stats;
};

// ============================================================================
// Multi-Core JIT Execution
// ============================================================================

// Parallel execution context
struct ParallelExecutionContext {
    int thread_id;
    int total_threads;
    std::atomic<bool>* cancel_flag;
    std::mutex* output_mutex;
};

// Multi-core JIT manager
class MultiCoreJIT {
public:
    static MultiCoreJIT& instance();

    // Initialize/finalize
    void initialize(int num_threads = 0);  // 0 = auto-detect
    void finalize();
    int getNumThreads() const { return m_num_threads; }

    // Parallel execution
    template<typename Func>
    std::vector<decltype(Func()(ParallelExecutionContext{}))>
    executeParallel(Func&& func, int num_tasks) {
        using ResultType = decltype(func(ParallelExecutionContext{}));
        std::vector<ResultType> results(num_tasks);
        std::atomic<int> task_counter{0};
        std::atomic<bool> cancel_flag{false};
        std::mutex output_mutex;

        auto worker = [&, func = std::forward<Func>(func)](int thread_id) {
            ParallelExecutionContext ctx{
                thread_id,
                m_num_threads,
                &cancel_flag,
                &output_mutex
            };

            while (true) {
                int task_id = task_counter.fetch_add(1);
                if (task_id >= num_tasks || cancel_flag.load()) {
                    break;
                }

                try {
                    results[task_id] = func(ctx);
                } catch (...) {
                    cancel_flag.store(true);
                    throw;
                }
            }
        };

        // Launch worker threads
        std::vector<std::thread> threads;
        for (int i = 0; i < m_num_threads; i++) {
            threads.emplace_back(worker, i);
        }

        // Wait for completion
        for (auto& t : threads) {
            t.join();
        }

        return results;
    }

    // Parallel component evaluation
    bool evaluateComponentsParallel(const std::vector<std::string>& component_names,
                                   double time, double dt,
                                   std::vector<std::vector<double>>& outputs);

    // Parallel parameter sweep
    template<typename SweepFunc>
    std::vector<typename std::result_of<SweepFunc(double)>::type>
    parallelSweep(const std::vector<double>& param_values, SweepFunc&& func) {
        using ResultType = typename std::result_of<SweepFunc(double)>::type;
        
        return executeParallel(
            [func = std::forward<SweepFunc>(func)](ParallelExecutionContext& ctx) -> ResultType {
                // Get next parameter value
                static std::atomic<int> param_index{0};
                int idx = param_index.fetch_add(1);
                if (idx >= param_values.size()) {
                    return ResultType();
                }
                return func(param_values[idx]);
            },
            param_values.size()
        );
    }

    // Statistics
    struct ParallelStats {
        int total_tasks = 0;
        double total_time_s = 0.0;
        double speedup = 1.0;
        double efficiency = 0.0;
    };
    ParallelStats getStatistics() const;

private:
    MultiCoreJIT() = default;
    ~MultiCoreJIT() = default;

    int m_num_threads = 1;
    mutable std::mutex m_mutex;
    ParallelStats m_stats;
};

// ============================================================================
// Sparse Matrix Support (Eigen)
// ============================================================================

#ifdef HAVE_EIGEN

// Sparse matrix wrapper for large netlist post-processing
class SparseMatrixWrapper {
public:
    // Constructors
    SparseMatrixWrapper();
    SparseMatrixWrapper(int rows, int cols);
    
    // Initialize
    void resize(int rows, int cols);
    void reserve(int non_zeros);

    // Element access
    void setCoefficient(int row, int col, double value);
    double getCoefficient(int row, int col) const;
    void addToCoefficient(int row, int col, double value);

    // Matrix operations
    SparseMatrixWrapper transpose() const;
    SparseMatrixWrapper multiply(const SparseMatrixWrapper& other) const;
    std::vector<double> multiply(const std::vector<double>& vec) const;
    
    // Solvers
    std::vector<double> solve(const std::vector<double>& rhs, 
                             const std::string& solver = "SparseLU") const;
    
    // Properties
    int rows() const;
    int cols() const;
    int nonZeros() const;
    double norm() const;
    double conditionNumber() const;

    // Conversion
    std::vector<std::tuple<int, int, double>> toCOO() const;
    std::vector<double> toDense() const;

    // Export
    bool exportToMatrixMarket(const std::string& filename) const;
    bool exportToCSV(const std::string& filename) const;

private:
    Eigen::SparseMatrix<double> m_matrix;
};

// Sparse matrix operations
class SparseMatrixOps {
public:
    // Create common matrix types
    static SparseMatrixWrapper createDiagonal(const std::vector<double>& diag);
    static SparseMatrixWrapper createTridiagonal(const std::vector<double>& lower,
                                                 const std::vector<double>& diag,
                                                 const std::vector<double>& upper);
    static SparseMatrixWrapper createLaplacian1D(int n, double h);
    static SparseMatrixWrapper createLaplacian2D(int nx, int ny, double hx, double hy);

    // Matrix analysis
    static double computeConditionNumber(const SparseMatrixWrapper& matrix);
    static std::vector<double> computeEigenvalues(const SparseMatrixWrapper& matrix, int num_eigenvalues);
    static SparseMatrixWrapper incompleteLU(const SparseMatrixWrapper& matrix);
    static SparseMatrixWrapper incompleteCholesky(const SparseMatrixWrapper& matrix);
};

#endif // HAVE_EIGEN

// ============================================================================
// Security & Sandboxing
// ============================================================================

// JIT sandbox configuration
struct SandboxConfig {
    bool allow_file_io = false;
    bool allow_network = false;
    bool allow_system_calls = false;
    int max_memory_mb = 256;
    int max_execution_time_ms = 5000;
    int max_loop_iterations = 1000000;
    bool allow_recursion = true;
    int max_recursion_depth = 100;
};

// JIT sandbox manager
class JITSandbox {
public:
    static JITSandbox& instance();

    // Initialize/finalize
    void initialize(const SandboxConfig& config = SandboxConfig());
    void finalize();

    // Execute code in sandbox
    bool executeSandboxed(const std::string& source_code, 
                         std::string* output = nullptr,
                         std::string* error = nullptr);

    // Validation
    bool validateSource(const std::string& source_code, std::string* error = nullptr) const;

    // Configuration
    void setConfig(const SandboxConfig& config);
    SandboxConfig getConfig() const;

private:
    JITSandbox() = default;
    ~JITSandbox() = default;

    SandboxConfig m_config;
    mutable std::mutex m_mutex;
};

} // namespace Flux

#endif // FLUX_SCALING_SECURITY_H
