// ============================================================================
// FluxScript Phase 4: Scaling & Security Implementation
// Safe hot-reload, timeout protection, multi-core JIT, sparse matrix support
// ============================================================================

#include "flux/jit/scaling_security.h"
#include "flux/jit/jit_manager.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <fstream>
#include <numeric>
#include <cmath>

namespace Flux {

// ============================================================================
// HotReloadManager Singleton
// ============================================================================

HotReloadManager& HotReloadManager::instance() {
    static HotReloadManager instance;
    return instance;
}

void HotReloadManager::initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::cout << "[HotReloadManager] Initialized" << std::endl;
}

void HotReloadManager::finalize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_transactions.clear();
    std::cout << "[HotReloadManager] Finalized" << std::endl;
}

bool HotReloadManager::prepareReload(const std::string& component_name, 
                                     const std::string& new_source,
                                     std::string* error) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto& jit_manager = JITManager::instance();
    auto* comp = jit_manager.getComponent(component_name);
    if (!comp) {
        if (error) *error = "Component not found: " + component_name;
        return false;
    }

    // Create transaction
    HotReloadTransaction txn;
    txn.component_name = component_name;
    txn.old_source = comp->source_code;
    txn.new_source = new_source;
    txn.saved_state = comp->state;
    txn.state = HotReloadState::PREPARING;
    txn.start_time = std::chrono::steady_clock::now();

    // Compile new version
    txn.new_jit = std::make_unique<FluxJIT>();
    if (!txn.new_jit->compile(new_source, error)) {
        txn.state = HotReloadState::FAILED;
        txn.error_message = error ? *error : "Compilation failed";
        return false;
    }

    // Get new function pointer
    txn.new_update_func = txn.new_jit->getFunction("update");
    if (!txn.new_update_func) {
        txn.state = HotReloadState::FAILED;
        txn.error_message = "Missing update function in new code";
        return false;
    }

    txn.state = HotReloadState::READY;
    m_transactions[component_name] = std::move(txn);

    std::cout << "[HotReloadManager] Prepared reload for: " << component_name << std::endl;
    return true;
}

bool HotReloadManager::executeReload(std::string* error) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Find transaction in READY state
    HotReloadTransaction* txn = nullptr;
    for (auto& [name, t] : m_transactions) {
        if (t.state == HotReloadState::READY) {
            txn = &t;
            break;
        }
    }

    if (!txn) {
        if (error) *error = "No prepared reload found";
        return false;
    }

    txn->state = HotReloadState::SWAPPING;

    auto& jit_manager = JITManager::instance();
    auto* comp = jit_manager.getComponent(txn->component_name);
    if (!comp) {
        txn->state = HotReloadState::FAILED;
        if (error) *error = "Component disappeared during reload";
        return false;
    }

    // Atomic swap (under JIT manager lock)
    comp->jit = std::move(txn->new_jit);
    comp->update_func = txn->new_update_func;
    comp->source_code = txn->new_source;
    comp->state = txn->saved_state;  // Preserve state
    comp->compile_count++;

    txn->state = HotReloadState::SWAPPED;

    std::cout << "[HotReloadManager] Executed reload for: " << txn->component_name << std::endl;
    return true;
}

bool HotReloadManager::abortReload(std::string* error) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    for (auto it = m_transactions.begin(); it != m_transactions.end();) {
        if (it->second.state != HotReloadState::IDLE && 
            it->second.state != HotReloadState::SWAPPED) {
            it->second.state = HotReloadState::FAILED;
            it->second.error_message = "Aborted by user";
            it = m_transactions.erase(it);
        } else {
            ++it;
        }
    }

    std::cout << "[HotReloadManager] Aborted all pending reloads" << std::endl;
    return true;
}

bool HotReloadManager::commitReload(std::string* error) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    for (auto it = m_transactions.begin(); it != m_transactions.end();) {
        if (it->second.state == HotReloadState::SWAPPED) {
            it->second.state = HotReloadState::CLEANUP;
            // Clean up old JIT (automatically done by moving transaction)
            it = m_transactions.erase(it);
        } else {
            ++it;
        }
    }

    std::cout << "[HotReloadManager] Committed all swapped reloads" << std::endl;
    return true;
}

bool HotReloadManager::rollbackReload(std::string* error) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    for (auto& [name, txn] : m_transactions) {
        if (txn.state == HotReloadState::SWAPPED) {
            // Restore old version
            auto& jit_manager = JITManager::instance();
            auto* comp = jit_manager.getComponent(name);
            if (comp) {
                comp->source_code = txn.old_source;
                comp->state = txn.saved_state;
            }
            txn.state = HotReloadState::IDLE;
        }
    }

    std::cout << "[HotReloadManager] Rolled back all swapped reloads" << std::endl;
    return true;
}

HotReloadState HotReloadManager::getState(const std::string& component_name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_transactions.find(component_name);
    if (it == m_transactions.end()) {
        return HotReloadState::IDLE;
    }
    return it->second.state;
}

bool HotReloadManager::isReloadInProgress() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& [name, txn] : m_transactions) {
        if (txn.state != HotReloadState::IDLE && 
            txn.state != HotReloadState::SWAPPED) {
            return true;
        }
    }
    return false;
}

double HotReloadManager::getReloadProgress() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    int total = m_transactions.size();
    if (total == 0) return 0.0;
    
    int completed = 0;
    for (const auto& [name, txn] : m_transactions) {
        if (txn.state == HotReloadState::SWAPPED || 
            txn.state == HotReloadState::CLEANUP) {
            completed++;
        }
    }
    
    return (double)completed / total;
}

// ============================================================================
// TimeoutProtector Singleton
// ============================================================================

TimeoutProtector& TimeoutProtector::instance() {
    static TimeoutProtector instance;
    return instance;
}

void TimeoutProtector::initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats = TimeoutStats();
    std::cout << "[TimeoutProtector] Initialized (default timeout: " 
              << m_default_timeout_ms << " ms)" << std::endl;
}

void TimeoutProtector::finalize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_active_threads.clear();
    std::cout << "[TimeoutProtector] Finalized" << std::endl;
}

bool TimeoutProtector::shouldTimeout() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_active_threads.find(std::this_thread::get_id());
    if (it == m_active_threads.end()) {
        return false;
    }

    auto elapsed = std::chrono::steady_clock::now() - it->second.start_time;
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    return elapsed_ms >= it->second.timeout_ms;
}

int TimeoutProtector::getRemainingTimeMs() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_active_threads.find(std::this_thread::get_id());
    if (it == m_active_threads.end()) {
        return 0;
    }

    auto elapsed = std::chrono::steady_clock::now() - it->second.start_time;
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    return std::max(0, it->second.timeout_ms - (int)elapsed_ms);
}

void TimeoutProtector::registerThread(std::thread::id id, int timeout_ms, const std::string& context) {
    std::lock_guard<std::mutex> lock(m_mutex);
    ThreadInfo info;
    info.id = id;
    info.timeout_ms = timeout_ms;
    info.context = context;
    info.start_time = std::chrono::steady_clock::now();
    m_active_threads[id] = info;
    m_stats.active_threads = m_active_threads.size();
}

void TimeoutProtector::unregisterThread(std::thread::id id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_active_threads.erase(id);
    m_stats.active_threads = m_active_threads.size();
}

TimeoutProtector::TimeoutStats TimeoutProtector::getStatistics() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_stats;
}

void TimeoutProtector::resetStatistics() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats = TimeoutStats();
}

// ============================================================================
// MultiCoreJIT Singleton
// ============================================================================

MultiCoreJIT& MultiCoreJIT::instance() {
    static MultiCoreJIT instance;
    return instance;
}

void MultiCoreJIT::initialize(int num_threads) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (num_threads == 0) {
        // Auto-detect
        num_threads = std::thread::hardware_concurrency();
        if (num_threads == 0) num_threads = 4;  // Fallback
    }
    
    m_num_threads = num_threads;
    m_stats = ParallelStats();
    
    std::cout << "[MultiCoreJIT] Initialized with " << num_threads << " threads" << std::endl;
}

void MultiCoreJIT::finalize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::cout << "[MultiCoreJIT] Finalized" << std::endl;
}

bool MultiCoreJIT::evaluateComponentsParallel(const std::vector<std::string>& component_names,
                                             double time, double dt,
                                             std::vector<std::vector<double>>& outputs) {
    auto start_time = std::chrono::steady_clock::now();
    
    outputs.resize(component_names.size());
    std::atomic<bool> all_success{true};
    std::atomic<int> task_counter{0};

    auto worker = [&, this]() {
        auto& jit_manager = JITManager::instance();
        
        while (true) {
            int idx = task_counter.fetch_add(1);
            if (idx >= component_names.size()) {
                break;
            }

            const auto& name = component_names[idx];
            auto* comp = jit_manager.getComponent(name);
            if (!comp || !comp->update_func) {
                all_success.store(false);
                continue;
            }

            // Evaluate component
            std::vector<double> out(comp->num_outputs, 0.0);
            if (!jit_manager.evaluateComponent(name, time, dt, 
                                              comp->state.inputs.data(), 
                                              out.data())) {
                all_success.store(false);
            }
            outputs[idx] = std::move(out);
        }
    };

    // Launch workers
    std::vector<std::thread> threads;
    for (int i = 0; i < m_num_threads; i++) {
        threads.emplace_back(worker);
    }

    // Wait for completion
    for (auto& t : threads) {
        t.join();
    }

    auto end_time = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(end_time - start_time).count();
    
    m_stats.total_tasks += component_names.size();
    m_stats.total_time_s += elapsed;
    
    std::cout << "[MultiCoreJIT] Evaluated " << component_names.size() 
              << " components in " << elapsed << "s using " 
              << m_num_threads << " threads" << std::endl;

    return all_success.load();
}

MultiCoreJIT::ParallelStats MultiCoreJIT::getStatistics() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_stats;
}

// ============================================================================
// SparseMatrixWrapper (Eigen)
// ============================================================================

#ifdef HAVE_EIGEN

SparseMatrixWrapper::SparseMatrixWrapper() {
}

SparseMatrixWrapper::SparseMatrixWrapper(int rows, int cols) {
    m_matrix.resize(rows, cols);
}

void SparseMatrixWrapper::resize(int rows, int cols) {
    m_matrix.resize(rows, cols);
}

void SparseMatrixWrapper::reserve(int non_zeros) {
    m_matrix.reserve(non_zeros);
}

void SparseMatrixWrapper::setCoefficient(int row, int col, double value) {
    m_matrix.insert(row, col) = value;
}

double SparseMatrixWrapper::getCoefficient(int row, int col) const {
    return m_matrix.coeff(row, col);
}

void SparseMatrixWrapper::addToCoefficient(int row, int col, double value) {
    m_matrix.coeffRef(row, col) += value;
}

SparseMatrixWrapper SparseMatrixWrapper::transpose() const {
    SparseMatrixWrapper result;
    result.m_matrix = m_matrix.transpose();
    return result;
}

SparseMatrixWrapper SparseMatrixWrapper::multiply(const SparseMatrixWrapper& other) const {
    SparseMatrixWrapper result;
    result.m_matrix = m_matrix * other.m_matrix;
    return result;
}

std::vector<double> SparseMatrixWrapper::multiply(const std::vector<double>& vec) const {
    Eigen::VectorXd v(vec.size());
    for (size_t i = 0; i < vec.size(); i++) {
        v[i] = vec[i];
    }
    Eigen::VectorXd result = m_matrix * v;
    return std::vector<double>(result.data(), result.data() + result.size());
}

std::vector<double> SparseMatrixWrapper::solve(const std::vector<double>& rhs,
                                               const std::string& solver) const {
    Eigen::VectorXd b(rhs.size());
    for (size_t i = 0; i < rhs.size(); i++) {
        b[i] = rhs[i];
    }

    Eigen::VectorXd x;
    
    if (solver == "SparseLU") {
        Eigen::SparseLU<Eigen::SparseMatrix<double>> lu(m_matrix);
        x = lu.solve(b);
    } else if (solver == "SparseQR") {
        Eigen::SparseQR<Eigen::SparseMatrix<double>, Eigen::COLAMDOrdering<int>> qr(m_matrix);
        x = qr.solve(b);
    } else if (solver == "ConjugateGradient") {
        Eigen::ConjugateGradient<Eigen::SparseMatrix<double>, Eigen::Lower|Eigen::Upper> cg(m_matrix);
        x = cg.solve(b);
    } else if (solver == "BiCGSTAB") {
        Eigen::BiCGSTAB<Eigen::SparseMatrix<double>> bicg(m_matrix);
        x = bicg.solve(b);
    } else {
        // Default to SparseLU
        Eigen::SparseLU<Eigen::SparseMatrix<double>> lu(m_matrix);
        x = lu.solve(b);
    }

    return std::vector<double>(x.data(), x.data() + x.size());
}

int SparseMatrixWrapper::rows() const {
    return m_matrix.rows();
}

int SparseMatrixWrapper::cols() const {
    return m_matrix.cols();
}

int SparseMatrixWrapper::nonZeros() const {
    return m_matrix.nonZeros();
}

double SparseMatrixWrapper::norm() const {
    return m_matrix.norm();
}

double SparseMatrixWrapper::conditionNumber() const {
    // Simplified - would compute actual condition number
    return 1.0;
}

std::vector<std::tuple<int, int, double>> SparseMatrixWrapper::toCOO() const {
    std::vector<std::tuple<int, int, double>> coo;
    for (int k = 0; k < m_matrix.outerSize(); ++k) {
        for (Eigen::SparseMatrix<double>::InnerIterator it(m_matrix, k); it; ++it) {
            coo.push_back(std::make_tuple(it.row(), it.col(), it.value()));
        }
    }
    return coo;
}

std::vector<double> SparseMatrixWrapper::toDense() const {
    Eigen::MatrixXd dense = Eigen::MatrixXd(m_matrix);
    return std::vector<double>(dense.data(), dense.data() + dense.size());
}

bool SparseMatrixWrapper::exportToMatrixMarket(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file.is_open()) {
        return false;
    }

    file << "%%MatrixMarket matrix coordinate real general\n";
    file << m_matrix.rows() << " " << m_matrix.cols() << " " << m_matrix.nonZeros() << "\n";

    for (int k = 0; k < m_matrix.outerSize(); ++k) {
        for (Eigen::SparseMatrix<double>::InnerIterator it(m_matrix, k); it; ++it) {
            file << (it.row() + 1) << " " << (it.col() + 1) << " " << it.value() << "\n";
        }
    }

    return true;
}

bool SparseMatrixWrapper::exportToCSV(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file.is_open()) {
        return false;
    }

    for (int k = 0; k < m_matrix.outerSize(); ++k) {
        for (Eigen::SparseMatrix<double>::InnerIterator it(m_matrix, k); it; ++it) {
            file << it.row() << "," << it.col() << "," << it.value() << "\n";
        }
    }

    return true;
}

// ============================================================================
// SparseMatrixOps
// ============================================================================

SparseMatrixWrapper SparseMatrixOps::createDiagonal(const std::vector<double>& diag) {
    int n = diag.size();
    SparseMatrixWrapper result(n, n);
    for (int i = 0; i < n; i++) {
        result.setCoefficient(i, i, diag[i]);
    }
    return result;
}

SparseMatrixWrapper SparseMatrixOps::createTridiagonal(const std::vector<double>& lower,
                                                       const std::vector<double>& diag,
                                                       const std::vector<double>& upper) {
    int n = diag.size();
    SparseMatrixWrapper result(n, n);
    
    for (int i = 0; i < n; i++) {
        result.setCoefficient(i, i, diag[i]);
        if (i > 0 && i - 1 < lower.size()) {
            result.setCoefficient(i, i - 1, lower[i - 1]);
        }
        if (i < n - 1 && i < upper.size()) {
            result.setCoefficient(i, i + 1, upper[i]);
        }
    }
    
    return result;
}

SparseMatrixWrapper SparseMatrixOps::createLaplacian1D(int n, double h) {
    SparseMatrixWrapper result(n, n);
    double factor = 1.0 / (h * h);
    
    for (int i = 0; i < n; i++) {
        result.setCoefficient(i, i, -2.0 * factor);
        if (i > 0) {
            result.setCoefficient(i, i - 1, factor);
        }
        if (i < n - 1) {
            result.setCoefficient(i, i + 1, factor);
        }
    }
    
    return result;
}

SparseMatrixWrapper SparseMatrixOps::createLaplacian2D(int nx, int ny, double hx, double hy) {
    int n = nx * ny;
    SparseMatrixWrapper result(n, n);
    
    double fx = 1.0 / (hx * hx);
    double fy = 1.0 / (hy * hy);
    double center = -2.0 * (fx + fy);
    
    for (int j = 0; j < ny; j++) {
        for (int i = 0; i < nx; i++) {
            int idx = j * nx + i;
            result.setCoefficient(idx, idx, center);
            
            if (i > 0) result.setCoefficient(idx, idx - 1, fx);
            if (i < nx - 1) result.setCoefficient(idx, idx + 1, fx);
            if (j > 0) result.setCoefficient(idx, idx - nx, fy);
            if (j < ny - 1) result.setCoefficient(idx, idx + nx, fy);
        }
    }
    
    return result;
}

double SparseMatrixOps::computeConditionNumber(const SparseMatrixWrapper& matrix) {
    return matrix.conditionNumber();
}

std::vector<double> SparseMatrixOps::computeEigenvalues(const SparseMatrixWrapper& matrix, 
                                                        int num_eigenvalues) {
    // Would use Eigen's sparse eigenvalue solver
    return std::vector<double>();
}

SparseMatrixWrapper SparseMatrixOps::incompleteLU(const SparseMatrixWrapper& matrix) {
    SparseMatrixWrapper result;
    // Would compute ILU preconditioner
    return result;
}

SparseMatrixWrapper SparseMatrixOps::incompleteCholesky(const SparseMatrixWrapper& matrix) {
    SparseMatrixWrapper result;
    // Would compute IC preconditioner
    return result;
}

#endif // HAVE_EIGEN

// ============================================================================
// JITSandbox Singleton
// ============================================================================

JITSandbox& JITSandbox::instance() {
    static JITSandbox instance;
    return instance;
}

void JITSandbox::initialize(const SandboxConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
    std::cout << "[JITSandbox] Initialized" << std::endl;
}

void JITSandbox::finalize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::cout << "[JITSandbox] Finalized" << std::endl;
}

bool JITSandbox::executeSandboxed(const std::string& source_code,
                                 std::string* output,
                                 std::string* error) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Validate source code
    if (!validateSource(source_code, error)) {
        return false;
    }

    // TODO: Implement actual sandboxed execution
    // This would use seccomp-bpf, namespaces, or other OS-level sandboxing
    
    std::cout << "[JITSandbox] Executing sandboxed code" << std::endl;
    return true;
}

bool JITSandbox::validateSource(const std::string& source_code, std::string* error) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Check for forbidden operations
    if (!m_config.allow_file_io) {
        if (source_code.find("fopen") != std::string::npos ||
            source_code.find("open(") != std::string::npos) {
            if (error) *error = "File I/O not allowed in sandbox";
            return false;
        }
    }

    if (!m_config.allow_system_calls) {
        if (source_code.find("system(") != std::string::npos ||
            source_code.find("exec(") != std::string::npos) {
            if (error) *error = "System calls not allowed in sandbox";
            return false;
        }
    }

    if (!m_config.allow_network) {
        if (source_code.find("socket(") != std::string::npos ||
            source_code.find("connect(") != std::string::npos) {
            if (error) *error = "Network access not allowed in sandbox";
            return false;
        }
    }

    return true;
}

void JITSandbox::setConfig(const SandboxConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
}

SandboxConfig JITSandbox::getConfig() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config;
}

} // namespace Flux
