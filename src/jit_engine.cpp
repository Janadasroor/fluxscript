#include "flux/jit/flux_jit.h"
#include <llvm/Support/Error.h>

#ifdef emit
#undef emit
#endif

#include "flux/jit_engine.h"
#include "flux/compiler/parser.h"
#include "flux/flux_eigen.h"
#include <iostream>
#include <cstdio>
#include <cmath>
#include <cstdint>

namespace Flux {

// External C function for printing strings (declared in scripts for extern use)
extern "C" double flux_print_string(const char* str) {
    printf("%s", str);
    fflush(stdout);
    return 0.0;
}

// SPICE-style node access (Dummies for standalone use)
extern "C" double flux_get_voltage(const char* node) {
    // In a real VioSpice integration, this would look up node voltage in the solver state
    if (std::string(node) == "0") return 0.0;
    return 5.0; // Return a default value for testing
}

extern "C" double flux_get_current(const char* branch) {
    // In a real VioSpice integration, this would look up branch current
    return 0.001; // Return 1mA default
}

// Mathematical library functions (extern "C" for JIT binding)
extern "C" double flux_sin(double x) { return std::sin(x); }
extern "C" double flux_cos(double x) { return std::cos(x); }
extern "C" double flux_tan(double x) { return std::tan(x); }
extern "C" double flux_asin(double x) { return std::asin(x); }
extern "C" double flux_acos(double x) { return std::acos(x); }
extern "C" double flux_atan(double x) { return std::atan(x); }
extern "C" double flux_atan2(double y, double x) { return std::atan2(y, x); }
extern "C" double flux_sqrt(double x) { return std::sqrt(x); }
extern "C" double flux_exp(double x) { return std::exp(x); }
extern "C" double flux_log(double x) { return std::log(x); }
extern "C" double flux_log10(double x) { return std::log10(x); }
extern "C" double flux_abs(double x) { return std::fabs(x); }
extern "C" double flux_floor(double x) { return std::floor(x); }
extern "C" double flux_ceil(double x) { return std::ceil(x); }
extern "C" double flux_round(double x) { return std::round(x); }
extern "C" double flux_pow(double base, double exp) { return std::pow(base, exp); }
extern "C" double flux_sinh(double x) { return std::sinh(x); }
extern "C" double flux_cosh(double x) { return std::cosh(x); }
extern "C" double flux_tanh(double x) { return std::tanh(x); }

// Mathematical constants
extern "C" double flux_pi() { return 3.14159265358979323846; }
extern "C" double flux_e() { return 2.71828182845904523536; }

// Matrix and Vector JIT Implementations
// Note: We use (double)(uintptr_t) to pass pointers as doubles in the JIT
extern "C" double flux_create_matrix(double* data, int rows, int cols) {
    // MatrixManager implementation is expected to be available
    // return (double)(uintptr_t)data;
    return 0.0; // Placeholder if MatrixManager is missing
}

extern "C" double flux_matrix_mul(double a_val, double b_val) {
    return a_val * b_val; // Placeholder
}

extern "C" double flux_matrix_transpose(double m_val) {
    return m_val; // Placeholder
}

extern "C" double flux_create_vector_sum(double* data, int size) {
    double sum = 0.0;
    for(int i=0; i<size; ++i) sum += data[i];
    return sum;
}

JITEngine& JITEngine::instance() {
    static JITEngine engine;
    return engine;
}

JITEngine::JITEngine() = default;

JITEngine::~JITEngine() {
    finalize();
}

void JITEngine::setOptimizationLevel(OptimizationLevel level) {
    if (m_jit) {
        m_jit->setOptimizationLevel(level);
    }
}

OptimizationLevel JITEngine::getOptimizationLevel() const {
    if (m_jit) {
        return m_jit->getOptimizationLevel();
    }
    return OptimizationLevel::O2;  // Default
}

void JITEngine::initialize() {
    if (m_initialized) return;

    m_jit = std::make_unique<FluxJIT>();
    m_codegenCtx = std::make_unique<CodegenContext>();
    m_codegenCtx->TheModule = std::make_unique<llvm::Module>("Flux JIT Core", m_codegenCtx->TheContext);

    m_initialized = true;

    // Register helper functions with JIT
    registerEigenFunctions();

    std::cout << "FluxScript C++ LLVM JIT Engine Initialized." << std::endl;
}

void JITEngine::registerEigenFunctions() {
    if (!m_jit || !m_codegenCtx) return;
    
    // Register vector helper functions
    m_jit->registerFunction("flux_create_vector_sum", reinterpret_cast<void*>(&flux_create_vector_sum));
    
    // Register matrix helper functions
    m_jit->registerFunction("flux_matrix_mul", reinterpret_cast<void*>(&flux_matrix_mul));
    m_jit->registerFunction("flux_matrix_transpose", reinterpret_cast<void*>(&flux_matrix_transpose));
    m_jit->registerFunction("flux_create_matrix", reinterpret_cast<void*>(&flux_create_matrix));

    // Register string helper functions
    m_jit->registerFunction("print_string", reinterpret_cast<void*>(&flux_print_string));

    // Register SPICE-style node access functions
    m_jit->registerFunction("flux_get_voltage", reinterpret_cast<void*>(&flux_get_voltage));
    m_jit->registerFunction("flux_get_current", reinterpret_cast<void*>(&flux_get_current));

    // Register mathematical library functions
    m_jit->registerFunction("sin", reinterpret_cast<void*>(&flux_sin));
    m_jit->registerFunction("cos", reinterpret_cast<void*>(&flux_cos));
    m_jit->registerFunction("tan", reinterpret_cast<void*>(&flux_tan));
    m_jit->registerFunction("asin", reinterpret_cast<void*>(&flux_asin));
    m_jit->registerFunction("acos", reinterpret_cast<void*>(&flux_acos));
    m_jit->registerFunction("atan", reinterpret_cast<void*>(&flux_atan));
    m_jit->registerFunction("atan2", reinterpret_cast<void*>(&flux_atan2));
    m_jit->registerFunction("sqrt", reinterpret_cast<void*>(&flux_sqrt));
    m_jit->registerFunction("exp", reinterpret_cast<void*>(&flux_exp));
    m_jit->registerFunction("log", reinterpret_cast<void*>(&flux_log));
    m_jit->registerFunction("log10", reinterpret_cast<void*>(&flux_log10));
    m_jit->registerFunction("abs", reinterpret_cast<void*>(&flux_abs));
    m_jit->registerFunction("floor", reinterpret_cast<void*>(&flux_floor));
    m_jit->registerFunction("ceil", reinterpret_cast<void*>(&flux_ceil));
    m_jit->registerFunction("round", reinterpret_cast<void*>(&flux_round));
    m_jit->registerFunction("pow", reinterpret_cast<void*>(&flux_pow));
    m_jit->registerFunction("sinh", reinterpret_cast<void*>(&flux_sinh));
    m_jit->registerFunction("cosh", reinterpret_cast<void*>(&flux_cosh));
    m_jit->registerFunction("tanh", reinterpret_cast<void*>(&flux_tanh));

    // Register mathematical constants (as zero-argument functions)
    m_jit->registerFunction("pi", reinterpret_cast<void*>(&flux_pi));
    m_jit->registerFunction("e", reinterpret_cast<void*>(&flux_e));
}

void JITEngine::finalize() {
    m_initialized = false;
    m_jit.reset();
    m_codegenCtx.reset();
}

bool JITEngine::isInitialized() const {
    return m_initialized;
}

#if FLUX_HAS_QT
bool JITEngine::compileScript(const QString& code, QString* error) {
    if (!m_initialized) initialize();

    Parser parser(code.toStdString());
    m_functionReturnTypes.clear();

    while (parser.CurTok != static_cast<int>(TokenType::tok_eof)) {
        std::unique_ptr<FunctionAST> fnAST = nullptr;

        if (parser.CurTok == static_cast<int>(TokenType::tok_def)) {
            fnAST = parser.ParseDefinition();
        } else if (parser.CurTok == static_cast<int>(TokenType::tok_extern)) {
            // Extern declarations create function prototypes in the module
            auto proto = parser.ParseExtern();
            if (proto) {
                m_functionReturnTypes[proto->getName()] = proto->getReturnType();
                proto->codegen(*m_codegenCtx);
            }
            continue;  // Don't try to generate code for extern declarations
        } else if (parser.CurTok == static_cast<int>(TokenType::tok_semicolon)) {
            // Skip semicolons as statement separators
            parser.getNextToken();
            continue;
        } else {
            fnAST = parser.ParseTopLevelExpr();
        }

        // Generate code for the function
        if (fnAST) {
            std::string fnName = fnAST->getProto()->getName();
            m_functionReturnTypes[fnName] = fnAST->getProto()->getReturnType();

            if (llvm::Function* F = fnAST->codegen(*m_codegenCtx)) {
                // Success
            } else {
                if (error) *error = "Code generation failed.";
                return false;
            }
        } else {
            if (parser.CurTok != static_cast<int>(TokenType::tok_eof)) {
                parser.SkipToSynchronizationPoint();
            }
        }
    }

    m_jit->addModule(std::move(m_codegenCtx->TheModule));
    m_codegenCtx->TheModule = std::make_unique<llvm::Module>("Flux JIT Core", m_codegenCtx->TheContext);

    return true;
}

bool JITEngine::executeString(const QString& code, QString* error) {
    if (!compileScript(code, error)) return false;
    return true;
}
#endif

bool JITEngine::compileScript(const std::string& code, std::string* error) {
    if (!m_initialized) initialize();

    Parser parser(code);
    m_functionReturnTypes.clear();
    m_overloadedFunctions.clear();

    while (parser.CurTok != static_cast<int>(TokenType::tok_eof)) {
        std::unique_ptr<FunctionAST> fnAST = nullptr;

        if (parser.CurTok == static_cast<int>(TokenType::tok_def)) {
            fnAST = parser.ParseDefinition();
        } else if (parser.CurTok == static_cast<int>(TokenType::tok_extern)) {
            auto proto = parser.ParseExtern();
            if (proto) {
                m_functionReturnTypes[proto->getName()] = proto->getReturnType();
                proto->codegen(*m_codegenCtx);
            }
            continue;
        } else if (parser.CurTok == static_cast<int>(TokenType::tok_semicolon)) {
            parser.getNextToken();
            continue;
        } else {
            fnAST = parser.ParseTopLevelExpr();
        }

        if (fnAST) {
            std::string fnName = fnAST->getProto()->getName();
            m_functionReturnTypes[fnName] = fnAST->getProto()->getReturnType();

            if (fnAST->codegen(*m_codegenCtx)) {
                // Success
            } else {
                if (error) *error = "Code generation failed.";
                return false;
            }
        } else {
            if (parser.CurTok != static_cast<int>(TokenType::tok_eof)) {
                parser.SkipToSynchronizationPoint();
            }
        }
    }

    m_jit->addModule(std::move(m_codegenCtx->TheModule));
    m_codegenCtx->TheModule = std::make_unique<llvm::Module>("Flux JIT Core", m_codegenCtx->TheContext);

    return true;
}

bool JITEngine::executeString(const std::string& code, std::string* error) {
    if (!compileScript(code, error)) return false;
    return true;
}

#if FLUX_HAS_QT
FluxValue JITEngine::callFunction(const std::string& name, const std::vector<double>& args, QString* error) {
    if (!m_initialized) {
        if (error) *error = "JIT not initialized";
        return 0.0;
    }

    void* fnPtr = m_jit->getPointerToFunction(name);
    if (!fnPtr) {
        if (error) *error = QString("Function not found: ") + QString::fromStdString(name);
        return 0.0;
    }

    FluxType retType = m_functionReturnTypes[name];

    if (retType.Kind == TypeKind::Complex) {
        using ComplexFn = std::complex<double>(*)();
        using ComplexFn1 = std::complex<double>(*)(double);
        using ComplexFn2 = std::complex<double>(*)(double, double);

        if (args.empty()) return reinterpret_cast<ComplexFn>(fnPtr)();
        if (args.size() == 1) return reinterpret_cast<ComplexFn1>(fnPtr)(args[0]);
        if (args.size() == 2) return reinterpret_cast<ComplexFn2>(fnPtr)(args[0], args[1]);
    } else {
        if (args.empty()) return reinterpret_cast<double(*)()>(fnPtr)();
        if (args.size() == 1) return reinterpret_cast<double(*)(double)>(fnPtr)(args[0]);
        if (args.size() == 2) return reinterpret_cast<double(*)(double, double)>(fnPtr)(args[0], args[1]);
    }

    if (error) *error = "Unsupported arguments or return type";
    return 0.0;
}
#endif

FluxValue JITEngine::callFunction(const std::string& name, const std::vector<double>& args, std::string* error) {
    if (!m_initialized) {
        if (error) *error = "JIT not initialized";
        return 0.0;
    }

    void* fnPtr = m_jit->getPointerToFunction(name);
    if (!fnPtr) {
        if (error) *error = "Function not found: " + name;
        return 0.0;
    }

    FluxType retType = m_functionReturnTypes[name];

    if (retType.Kind == TypeKind::Complex) {
        using ComplexFn = std::complex<double>(*)();
        using ComplexFn1 = std::complex<double>(*)(double);
        using ComplexFn2 = std::complex<double>(*)(double, double);

        if (args.empty()) return reinterpret_cast<ComplexFn>(fnPtr)();
        if (args.size() == 1) return reinterpret_cast<ComplexFn1>(fnPtr)(args[0]);
        if (args.size() == 2) return reinterpret_cast<ComplexFn2>(fnPtr)(args[0], args[1]);
    } else {
        if (args.empty()) return reinterpret_cast<double(*)()>(fnPtr)();
        if (args.size() == 1) return reinterpret_cast<double(*)(double)>(fnPtr)(args[0]);
        if (args.size() == 2) return reinterpret_cast<double(*)(double, double)>(fnPtr)(args[0], args[1]);
    }

    if (error) *error = "Unsupported arguments or return type";
    return 0.0;
}

} // namespace Flux
