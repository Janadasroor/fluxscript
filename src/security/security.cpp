#include "flux/security/security.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <csignal>
#include <cstring>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Type.h>

namespace Flux {

// ============================================================================
// AuditLogger Implementation
// ============================================================================

thread_local bool TimeoutGuard::m_timeoutExpired = false;

AuditLogger& AuditLogger::instance() {
    static AuditLogger logger;
    return logger;
}

AuditLogger::AuditLogger() = default;
AuditLogger::~AuditLogger() = default;

void AuditLogger::log(const AuditEntry& entry) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    m_entries.push_back(entry);
    
    // Trim if too many entries
    if (m_entries.size() > MAX_ENTRIES) {
        m_entries.erase(m_entries.begin());
    }
    
    // Write to file if configured
    if (!m_logFile.empty()) {
        std::ofstream file(m_logFile, std::ios::app);
        if (file.is_open()) {
            auto time = std::chrono::system_clock::to_time_t(entry.timestamp);
            file << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S")
                 << " [" << static_cast<int>(entry.type) << "] "
                 << entry.source << ": " << entry.description
                 << (entry.success ? " [OK]" : " [FAIL]") << std::endl;
        }
    }
    
    // Console output based on log level
    if (m_logLevel >= 2) {
        std::cout << "[AUDIT] " << entry.source << ": " << entry.description << std::endl;
    }
}

void AuditLogger::setLogFile(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_logFile = path;
}

void AuditLogger::setLogLevel(int level) {
    m_logLevel = level;
}

std::vector<AuditEntry> AuditLogger::getRecentEntries(size_t count) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<AuditEntry> result;
    size_t start = m_entries.size() > count ? m_entries.size() - count : 0;
    
    for (size_t i = start; i < m_entries.size(); ++i) {
        result.push_back(m_entries[i]);
    }
    
    return result;
}

void AuditLogger::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_entries.clear();
}

// ============================================================================
// TimeoutGuard Implementation
// ============================================================================

TimeoutGuard::TimeoutGuard(std::chrono::milliseconds timeout)
    : m_timeout(timeout) {}

TimeoutGuard::~TimeoutGuard() {
    stop();
}

void TimeoutGuard::start() {
    m_startTime = std::chrono::steady_clock::now();
    m_running = true;
    m_timeoutExpired = false;
}

void TimeoutGuard::stop() {
    m_running = false;
}

bool TimeoutGuard::isExpired() const {
    if (!m_running) return false;
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - m_startTime);
    
    return elapsed > m_timeout;
}

void TimeoutGuard::check() {
    if (isExpired()) {
        throwTimeout();
    }
}

void TimeoutGuard::reset() {
    m_startTime = std::chrono::steady_clock::now();
}

bool TimeoutGuard::isTimeoutEnabled() {
    return !m_timeoutExpired;
}

void TimeoutGuard::throwTimeout() {
    m_timeoutExpired = true;
    throw FluxTimeoutException();
}

// ============================================================================
// StackProtector Implementation
// ============================================================================

thread_local size_t StackProtector::currentDepth = 0;
std::atomic<size_t> StackProtector::globalMaxDepth{1000};

StackProtector::StackProtector(size_t maxDepth) : m_maxDepth(maxDepth) {
    globalMaxDepth = maxDepth;
}

StackProtector::~StackProtector() = default;

void StackProtector::enterFunction() {
    ++currentDepth;
    if (currentDepth > m_maxDepth) {
        throw FluxStackOverflowException();
    }
}

void StackProtector::leaveFunction() {
    if (currentDepth > 0) {
        --currentDepth;
    }
}

size_t StackProtector::getCurrentDepth() const {
    return currentDepth;
}

size_t StackProtector::getMaxDepth() const {
    return m_maxDepth;
}

void StackProtector::setMaxDepth(size_t depth) {
    m_maxDepth = depth;
    globalMaxDepth = depth;
}

bool StackProtector::checkStack() {
    return currentDepth < m_maxDepth;
}

// ============================================================================
// BoundsChecker Implementation
// ============================================================================

BoundsChecker& BoundsChecker::instance() {
    static BoundsChecker checker;
    return checker;
}

void BoundsChecker::enable() {
    m_enabled = true;
}

void BoundsChecker::disable() {
    m_enabled = false;
}

bool BoundsChecker::isEnabled() const {
    return m_enabled;
}

bool BoundsChecker::checkBounds(size_t index, size_t size, const std::string& arrayName) {
    if (!m_enabled) return true;
    
    if (index >= size) {
        throwBoundsError(index, size, arrayName);
        return false;
    }
    return true;
}

void BoundsChecker::throwBoundsError(size_t index, size_t size, const std::string& arrayName) {
    throw FluxBoundsException(index, size, arrayName);
}

void BoundsChecker::registerArray(void* ptr, size_t size, const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_arrays[ptr] = {size, name};
}

void BoundsChecker::unregisterArray(void* ptr) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_arrays.erase(ptr);
}

// ============================================================================
// MemoryLimiter Implementation
// ============================================================================

MemoryLimiter& MemoryLimiter::instance() {
    static MemoryLimiter limiter;
    return limiter;
}

void MemoryLimiter::setMaxMemory(size_t bytes) {
    m_maxMemory = bytes;
}

void MemoryLimiter::setMaxAllocation(size_t bytes) {
    m_maxAllocation = bytes;
}

size_t MemoryLimiter::getMaxMemory() const {
    return m_maxMemory;
}

size_t MemoryLimiter::getMaxAllocation() const {
    return m_maxAllocation;
}

bool MemoryLimiter::trackAllocation(size_t size) {
    if (size > m_maxAllocation) {
        return false;
    }
    
    size_t expected = m_currentUsage;
    while (expected + size <= m_maxMemory) {
        if (m_currentUsage.compare_exchange_weak(expected, expected + size)) {
            return true;
        }
    }
    return false;
}

void MemoryLimiter::trackDeallocation(size_t size) {
    m_currentUsage.fetch_sub(size);
}

size_t MemoryLimiter::getCurrentUsage() const {
    return m_currentUsage;
}

bool MemoryLimiter::canAllocate(size_t size) const {
    return size <= m_maxAllocation && 
           (m_currentUsage + size) <= m_maxMemory;
}

void MemoryLimiter::throwMemoryError(size_t requested) {
    throw FluxMemoryException("Memory allocation failed: requested " + 
                              std::to_string(requested) + " bytes");
}

// ============================================================================
// Sandbox Implementation
// ============================================================================

Sandbox::Sandbox(const SecurityConfig& config) : m_config(config) {}

Sandbox::~Sandbox() {
    exit();
}

void Sandbox::enter() {
    m_active = true;
}

void Sandbox::exit() {
    m_active = false;
}

bool Sandbox::isActive() const {
    return m_active;
}

bool Sandbox::canAccessFileSystem() const {
    return m_active ? m_config.allowFileSystem : true;
}

bool Sandbox::canAccessNetwork() const {
    return m_active ? m_config.allowNetwork : true;
}

bool Sandbox::canCallExternal(const std::string& functionName) const {
    if (!m_active) return true;
    if (!m_config.allowExternalCalls) return false;
    
    // Check if function is in allowed list
    if (m_config.allowedModules.empty()) return true;
    
    for (const auto& mod : m_config.allowedModules) {
        if (functionName.find(mod) != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool Sandbox::canImportModule(const std::string& moduleName) const {
    if (!m_active) return true;
    
    if (m_config.allowedModules.empty()) return true;
    
    for (const auto& mod : m_config.allowedModules) {
        if (moduleName == mod) {
            return true;
        }
    }
    return false;
}

void Sandbox::reportViolation(const std::string& operation) {
    AuditLogger::instance().log({
        AuditEntry::EventType::SandboxViolation,
        std::chrono::system_clock::now(),
        "Sandbox",
        "Sandbox violation detected",
        operation,
        false
    });
    throw FluxSandboxException(operation);
}

// ============================================================================
// TypeSafetyVerifier Implementation
// ============================================================================

TypeSafetyVerifier& TypeSafetyVerifier::instance() {
    static TypeSafetyVerifier verifier;
    return verifier;
}

bool TypeSafetyVerifier::verifyModule(llvm::Module* module, std::string* error) {
    if (!module) {
        if (error) *error = "Null module";
        return false;
    }
    
    // Use LLVM's built-in verifier
    std::string errorMsg;
    llvm::raw_string_ostream errorStream(errorMsg);
    
    if (llvm::verifyModule(*module, &errorStream)) {
        if (error) *error = errorMsg;
        return false;
    }
    
    return true;
}

bool TypeSafetyVerifier::verifyFunction(llvm::Function* func, std::string* error) {
    if (!func) {
        if (error) *error = "Null function";
        return false;
    }
    
    std::string errorMsg;
    llvm::raw_string_ostream errorStream(errorMsg);
    
    if (llvm::verifyFunction(*func, &errorStream)) {
        if (error) *error = errorMsg;
        return false;
    }
    
    return true;
}

bool TypeSafetyVerifier::checkTypeCompatibility(const std::string& expected, 
                                                  const std::string& actual) {
    // Simple type compatibility check
    // In a full implementation, this would check LLVM types
    return expected == actual;
}

bool TypeSafetyVerifier::verifyCallSignature(llvm::Function* callee, 
                                              const std::vector<llvm::Value*>& args) {
    if (!callee) return false;
    
    auto funcType = callee->getFunctionType();
    if (!funcType) return false;
    
    // Check argument count
    if (funcType->getNumParams() != args.size()) {
        return false;
    }
    
    // Check argument types
    for (size_t i = 0; i < args.size(); ++i) {
        if (funcType->getParamType(i) != args[i]->getType()) {
            return false;
        }
    }
    
    return true;
}

// ============================================================================
// DeterministicController Implementation
// ============================================================================

DeterministicController& DeterministicController::instance() {
    static DeterministicController controller;
    return controller;
}

void DeterministicController::enableDeterministicMode() {
    m_deterministicMode = true;
    m_strictFP = true;
}

void DeterministicController::disableDeterministicMode() {
    m_deterministicMode = false;
    m_strictFP = false;
}

bool DeterministicController::isDeterministicMode() const {
    return m_deterministicMode;
}

void DeterministicController::setFloatingPointMode(bool strict) {
    m_strictFP = strict;
}

bool DeterministicController::isStrictFloatingPoint() const {
    return m_strictFP;
}

void DeterministicController::setRandomSeed(uint64_t seed) {
    m_randomSeed = seed;
}

uint64_t DeterministicController::getRandomSeed() const {
    return m_randomSeed;
}

// ============================================================================
// SecurityManager Implementation
// ============================================================================

SecurityManager& SecurityManager::instance() {
    static SecurityManager manager;
    return manager;
}

void SecurityManager::initialize(const SecurityConfig& config) {
    m_config = config;
    
    if (config.enableSandboxMode) {
        m_sandbox = std::make_unique<Sandbox>(config);
    }
    
    if (config.enableTimeoutGuards) {
        m_timeoutGuard = std::make_unique<TimeoutGuard>(config.maxExecutionTime);
    }
    
    if (config.enableStackProtection) {
        m_stackProtector = std::make_unique<StackProtector>(config.maxRecursionDepth);
    }
    
    if (config.enableBoundsChecking) {
        BoundsChecker::instance().enable();
    } else {
        BoundsChecker::instance().disable();
    }
    
    if (config.enableMemoryLimits) {
        MemoryLimiter::instance().setMaxMemory(config.maxMemoryMB * 1024 * 1024);
        MemoryLimiter::instance().setMaxAllocation(config.maxAllocationSize);
    }
    
    if (config.enableDeterministicExecution) {
        DeterministicController::instance().enableDeterministicMode();
    }
    
    m_initialized = true;
    
    AuditLogger::instance().log({
        AuditEntry::EventType::Compile,
        std::chrono::system_clock::now(),
        "SecurityManager",
        "Security manager initialized",
        "Config: bounds=" + std::to_string(config.enableBoundsChecking) +
        " stack=" + std::to_string(config.enableStackProtection) +
        " sandbox=" + std::to_string(config.enableSandboxMode),
        true
    });
}

bool SecurityManager::preCompileCheck(const std::string& source, std::string* error) {
    if (!m_initialized) return true;
    
    // Check for suspicious patterns
    if (m_config.enableSandboxMode && m_sandbox) {
        // Could check for disallowed patterns here
    }
    
    return true;
}

bool SecurityManager::preExecuteCheck(llvm::Module* module, std::string* error) {
    if (!m_initialized) return true;
    
    // Type safety verification
    if (m_config.enableTypeSafetyVerification) {
        if (!TypeSafetyVerifier::instance().verifyModule(module, error)) {
            AuditLogger::instance().log({
                AuditEntry::EventType::TypeMismatch,
                std::chrono::system_clock::now(),
                "SecurityManager",
                "Type safety verification failed",
                error ? *error : "",
                false
            });
            return false;
        }
    }
    
    return true;
}

void SecurityManager::checkBounds(size_t index, size_t size, const std::string& arrayName) {
    if (m_config.enableBoundsChecking) {
        BoundsChecker::instance().checkBounds(index, size, arrayName);
    }
}

void SecurityManager::checkStack() {
    if (m_config.enableStackProtection && m_stackProtector) {
        m_stackProtector->enterFunction();
    }
}

void SecurityManager::checkTimeout() {
    if (m_config.enableTimeoutGuards && m_timeoutGuard) {
        m_timeoutGuard->check();
    }
}

void SecurityManager::checkMemory(size_t requestedSize) {
    if (m_config.enableMemoryLimits) {
        if (!MemoryLimiter::instance().canAllocate(requestedSize)) {
            MemoryLimiter::instance().throwMemoryError(requestedSize);
        }
        MemoryLimiter::instance().trackAllocation(requestedSize);
    }
}

void SecurityManager::enterSandbox() {
    if (m_sandbox) {
        m_sandbox->enter();
    }
}

void SecurityManager::exitSandbox() {
    if (m_sandbox) {
        m_sandbox->exit();
    }
}

bool SecurityManager::isInSandbox() const {
    return m_sandbox && m_sandbox->isActive();
}

void SecurityManager::logEvent(AuditEntry::EventType type, const std::string& source,
                               const std::string& description, bool success) {
    if (m_config.enableAuditLogging) {
        AuditLogger::instance().log({
            type,
            std::chrono::system_clock::now(),
            source,
            description,
            "",
            success
        });
    }
}

std::string SecurityManager::getStatusReport() const {
    std::ostringstream oss;
    oss << "FluxScript Security Status\n";
    oss << "=========================\n";
    oss << "Initialized: " << (m_initialized ? "Yes" : "No") << "\n";
    oss << "Bounds Checking: " << (m_config.enableBoundsChecking ? "Enabled" : "Disabled") << "\n";
    oss << "Stack Protection: " << (m_config.enableStackProtection ? "Enabled" : "Disabled") << "\n";
    oss << "Sandbox Mode: " << (m_config.enableSandboxMode ? "Enabled" : "Disabled") << "\n";
    oss << "Timeout Guards: " << (m_config.enableTimeoutGuards ? "Enabled" : "Disabled") << "\n";
    oss << "Memory Limits: " << (m_config.enableMemoryLimits ? "Enabled" : "Disabled") << "\n";
    oss << "Type Safety: " << (m_config.enableTypeSafetyVerification ? "Enabled" : "Disabled") << "\n";
    oss << "Audit Logging: " << (m_config.enableAuditLogging ? "Enabled" : "Disabled") << "\n";
    oss << "Deterministic: " << (m_config.enableDeterministicExecution ? "Enabled" : "Disabled") << "\n";
    oss << "Max Memory: " << m_config.maxMemoryMB << "MB\n";
    oss << "Max Recursion: " << m_config.maxRecursionDepth << "\n";
    oss << "Max Execution: " << m_config.maxExecutionTime.count() << "ms\n";
    
    return oss.str();
}

// ============================================================================
// ABIStability Implementation
// ============================================================================

ABIStability& ABIStability::instance() {
    static ABIStability stability;
    return stability;
}

const char* ABIStability::getVersionString() const {
    return "1.0.0";
}

void ABIStability::registerStableFunction(const std::string& name, StableFunction func) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stableFunctions[name] = func;
}

ABIStability::StableFunction ABIStability::getStableFunction(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_stableFunctions.find(name);
    return (it != m_stableFunctions.end()) ? it->second : nullptr;
}

ABIStability::StableValue ABIStability::toStable(double value) {
    StableValue sv;
    sv.floatValue = value;
    sv.typeTag = 0;
    sv.version = FLUX_API_VERSION;
    return sv;
}

ABIStability::StableValue ABIStability::toStable(int64_t value) {
    StableValue sv;
    sv.intValue = value;
    sv.typeTag = 1;
    sv.version = FLUX_API_VERSION;
    return sv;
}

ABIStability::StableValue ABIStability::toStable(void* value) {
    StableValue sv;
    sv.ptrValue = value;
    sv.typeTag = 2;
    sv.version = FLUX_API_VERSION;
    return sv;
}

double ABIStability::toDouble(StableValue value) {
    return value.floatValue;
}

int64_t ABIStability::toInt(StableValue value) {
    return value.intValue;
}

void* ABIStability::toPtr(StableValue value) {
    return value.ptrValue;
}

} // namespace Flux
