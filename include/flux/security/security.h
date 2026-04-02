#ifndef FLUX_SECURITY_H
#define FLUX_SECURITY_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <chrono>
#include <atomic>
#include <mutex>
#include <thread>
#include <functional>
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>

namespace Flux {

// Security configuration
struct SecurityConfig {
    bool enableBoundsChecking = true;
    bool enableStackProtection = true;
    bool enableSandboxMode = false;
    bool enableTimeoutGuards = true;
    bool enableMemoryLimits = true;
    bool enableTypeSafetyVerification = true;
    bool enableAuditLogging = true;
    bool enableDeterministicExecution = false;
    
    // Timeout settings
    std::chrono::milliseconds maxExecutionTime{5000};  // 5 seconds default
    std::chrono::milliseconds maxCompileTime{10000};   // 10 seconds default
    
    // Memory limits
    size_t maxMemoryMB = 512;
    size_t maxAllocationSize = 100 * 1024 * 1024;  // 100MB max single allocation
    
    // Stack protection
    size_t maxRecursionDepth = 1000;
    
    // Sandbox restrictions
    bool allowFileSystem = false;
    bool allowNetwork = false;
    bool allowExternalCalls = true;  // Allow registered extern functions
    std::vector<std::string> allowedModules;
    
    // Deterministic execution
    bool forceDeterministicMath = false;
    int floatingPointPrecision = 64;
};

// Audit log entry
struct AuditEntry {
    enum class EventType {
        Compile,
        Execute,
        Import,
        MemoryAllocation,
        Timeout,
        BoundsViolation,
        StackOverflow,
        TypeMismatch,
        SandboxViolation
    };
    
    EventType type;
    std::chrono::system_clock::time_point timestamp;
    std::string source;
    std::string description;
    std::string details;
    bool success;
};

// Audit logger
class AuditLogger {
public:
    static AuditLogger& instance();
    
    void log(const AuditEntry& entry);
    void setLogFile(const std::string& path);
    void setLogLevel(int level);  // 0=errors, 1=warnings, 2=info, 3=debug
    std::vector<AuditEntry> getRecentEntries(size_t count) const;
    void clear();
    
private:
    AuditLogger();
    ~AuditLogger();
    AuditLogger(const AuditLogger&) = delete;
    AuditLogger& operator=(const AuditLogger&) = delete;
    
    mutable std::mutex m_mutex;
    std::vector<AuditEntry> m_entries;
    std::string m_logFile;
    int m_logLevel = 2;
    static const size_t MAX_ENTRIES = 10000;
};

// Timeout guard
class TimeoutGuard {
public:
    TimeoutGuard(std::chrono::milliseconds timeout);
    ~TimeoutGuard();
    
    void start();
    void stop();
    bool isExpired() const;
    void check();  // Throws if expired
    void reset();
    
    // Static method to check current timeout
    static bool isTimeoutEnabled();
    static void throwTimeout();
    
private:
    std::chrono::steady_clock::time_point m_startTime;
    std::chrono::milliseconds m_timeout;
    std::atomic<bool> m_running{false};
    static thread_local bool m_timeoutExpired;
};

// Stack protector
class StackProtector {
public:
    StackProtector(size_t maxDepth = 1000);
    ~StackProtector();
    
    void enterFunction();
    void leaveFunction();
    size_t getCurrentDepth() const;
    size_t getMaxDepth() const;
    void setMaxDepth(size_t depth);
    bool checkStack();  // Returns false if overflow detected
    
    // Thread-local stack depth
    static thread_local size_t currentDepth;
    
private:
    size_t m_maxDepth;
    static std::atomic<size_t> globalMaxDepth;
};

// Bounds checker
class BoundsChecker {
public:
    static BoundsChecker& instance();
    
    void enable();
    void disable();
    bool isEnabled() const;
    
    // Check array bounds
    bool checkBounds(size_t index, size_t size, const std::string& arrayName = "");
    void throwBoundsError(size_t index, size_t size, const std::string& arrayName = "");
    
    // Register array for tracking
    void registerArray(void* ptr, size_t size, const std::string& name = "");
    void unregisterArray(void* ptr);
    
private:
    BoundsChecker() = default;
    std::atomic<bool> m_enabled{true};
    std::map<void*, std::pair<size_t, std::string>> m_arrays;
    mutable std::mutex m_mutex;
};

// Memory limiter
class MemoryLimiter {
public:
    static MemoryLimiter& instance();
    
    void setMaxMemory(size_t bytes);
    void setMaxAllocation(size_t bytes);
    size_t getMaxMemory() const;
    size_t getMaxAllocation() const;
    
    // Track allocations
    bool trackAllocation(size_t size);
    void trackDeallocation(size_t size);
    size_t getCurrentUsage() const;
    
    // Check if allocation is allowed
    bool canAllocate(size_t size) const;
    void throwMemoryError(size_t requested);
    
private:
    MemoryLimiter() = default;
    
    std::atomic<size_t> m_currentUsage{0};
    std::atomic<size_t> m_maxMemory{512 * 1024 * 1024};  // 512MB default
    std::atomic<size_t> m_maxAllocation{100 * 1024 * 1024};  // 100MB default
    mutable std::mutex m_mutex;
};

// Sandbox
class Sandbox {
public:
    Sandbox(const SecurityConfig& config);
    ~Sandbox();
    
    void enter();
    void exit();
    bool isActive() const;
    
    // Check if operation is allowed
    bool canAccessFileSystem() const;
    bool canAccessNetwork() const;
    bool canCallExternal(const std::string& functionName) const;
    bool canImportModule(const std::string& moduleName) const;
    
    // Violation handling
    void reportViolation(const std::string& operation);
    
    const SecurityConfig& getConfig() const { return m_config; }
    
private:
    SecurityConfig m_config;
    std::atomic<bool> m_active{false};
};

// Type safety verifier
class TypeSafetyVerifier {
public:
    static TypeSafetyVerifier& instance();
    
    bool verifyModule(llvm::Module* module, std::string* error = nullptr);
    bool verifyFunction(llvm::Function* func, std::string* error = nullptr);
    
    // Type checking utilities
    bool checkTypeCompatibility(const std::string& expected, const std::string& actual);
    bool verifyCallSignature(llvm::Function* callee, const std::vector<llvm::Value*>& args);
    
private:
    TypeSafetyVerifier() = default;
};

// Deterministic execution controller
class DeterministicController {
public:
    static DeterministicController& instance();
    
    void enableDeterministicMode();
    void disableDeterministicMode();
    bool isDeterministicMode() const;
    
    // Set deterministic math behavior
    void setFloatingPointMode(bool strict);
    bool isStrictFloatingPoint() const;
    
    // Seed control for reproducibility
    void setRandomSeed(uint64_t seed);
    uint64_t getRandomSeed() const;
    
private:
    DeterministicController() = default;
    
    std::atomic<bool> m_deterministicMode{false};
    std::atomic<bool> m_strictFP{false};
    std::atomic<uint64_t> m_randomSeed{12345};
};

// Security manager - main interface
class SecurityManager {
public:
    static SecurityManager& instance();
    
    // Initialize with configuration
    void initialize(const SecurityConfig& config);
    const SecurityConfig& getConfig() const { return m_config; }
    
    // Pre-compilation checks
    bool preCompileCheck(const std::string& source, std::string* error = nullptr);
    
    // Pre-execution checks
    bool preExecuteCheck(llvm::Module* module, std::string* error = nullptr);
    
    // Runtime checks
    void checkBounds(size_t index, size_t size, const std::string& arrayName = "");
    void checkStack();
    void checkTimeout();
    void checkMemory(size_t requestedSize);
    
    // Sandbox control
    void enterSandbox();
    void exitSandbox();
    bool isInSandbox() const;
    
    // Audit logging
    void logEvent(AuditEntry::EventType type, const std::string& source, 
                  const std::string& description, bool success = true);
    
    // Get security status
    std::string getStatusReport() const;
    
private:
    SecurityManager() = default;
    
    SecurityConfig m_config;
    std::unique_ptr<Sandbox> m_sandbox;
    std::unique_ptr<TimeoutGuard> m_timeoutGuard;
    std::unique_ptr<StackProtector> m_stackProtector;
    bool m_initialized{false};
};

// ABI stability layer
class ABIStability {
public:
    static ABIStability& instance();
    
    // Version information
    int getAPIVersion() const { return FLUX_API_VERSION; }
    const char* getVersionString() const;
    
    // Type wrappers for stable ABI
    struct StableValue {
        union {
            double floatValue;
            int64_t intValue;
            void* ptrValue;
        };
        int typeTag;  // 0=double, 1=int, 2=ptr
        uint32_t version;
    };
    
    // Stable function wrapper
    using StableFunction = StableValue(*)(StableValue*, int);
    
    // Register function with stable ABI
    void registerStableFunction(const std::string& name, StableFunction func);
    StableFunction getStableFunction(const std::string& name);
    
    // Convert between stable and native types
    static StableValue toStable(double value);
    static StableValue toStable(int64_t value);
    static StableValue toStable(void* value);
    static double toDouble(StableValue value);
    static int64_t toInt(StableValue value);
    static void* toPtr(StableValue value);
    
private:
    ABIStability() = default;
    
    static const int FLUX_API_VERSION = 1;
    std::map<std::string, StableFunction> m_stableFunctions;
    mutable std::mutex m_mutex;
};

// Exception types
class FluxSecurityException : public std::runtime_error {
public:
    explicit FluxSecurityException(const std::string& msg) : std::runtime_error(msg) {}
};

class FluxTimeoutException : public FluxSecurityException {
public:
    FluxTimeoutException() : FluxSecurityException("Execution timeout exceeded") {}
};

class FluxStackOverflowException : public FluxSecurityException {
public:
    FluxStackOverflowException() : FluxSecurityException("Stack overflow detected") {}
};

class FluxBoundsException : public FluxSecurityException {
public:
    FluxBoundsException(size_t index, size_t size, const std::string& array)
        : FluxSecurityException("Array bounds violation: index " + std::to_string(index) + 
                                " out of bounds [0, " + std::to_string(size) + ") for " + array) {}
};

class FluxMemoryException : public FluxSecurityException {
public:
    explicit FluxMemoryException(const std::string& msg) : FluxSecurityException(msg) {}
};

class FluxSandboxException : public FluxSecurityException {
public:
    explicit FluxSandboxException(const std::string& operation)
        : FluxSecurityException("Sandbox violation: " + operation + " is not allowed") {}
};

class FluxTypeException : public FluxSecurityException {
public:
    explicit FluxTypeException(const std::string& msg) : FluxSecurityException(msg) {}
};

} // namespace Flux

#endif // FLUX_SECURITY_H
