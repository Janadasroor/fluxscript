#include "flux/jit/flux_jit.h"
#include <llvm/Support/Error.h>

// Undefine Qt's emit macro which conflicts with LLVM's emit methods
#ifdef emit
#undef emit
#endif

#include "flux/jit_engine.h"
#include "flux/compiler/parser.h"
#include "flux/flux_eigen.h"
#include <iostream>
#include <cstdio>
#include <cmath>

namespace Flux {

// External C function for printing strings (declared in scripts for extern use)
extern "C" double flux_print_string(const char* str) {
    printf("%s", str);
    fflush(stdout);
    return 0.0;
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

JITEngine& JITEngine::instance() {
    static JITEngine engine;
    return engine;
}

JITEngine::JITEngine() = default;

JITEngine::~JITEngine() {
    finalize();
}

void JITEngine::initialize() {
    if (m_initialized) return;

    m_jit = std::make_unique<FluxJIT>();
    m_codegenCtx = std::make_unique<CodegenContext>();
    m_codegenCtx->TheModule = std::make_unique<llvm::Module>("Flux JIT Core", m_codegenCtx->TheContext);

    m_initialized = true;
    
    // Eigen helper functions registration disabled - build issues
    // registerEigenFunctions();
    
    std::cout << "FluxScript C++ LLVM JIT Engine Initialized." << std::endl;
}

void JITEngine::registerEigenFunctions() {
    if (!m_jit || !m_codegenCtx) return;
    
    // Register vector helper functions
    m_jit->registerFunction("flux_vector_dot", reinterpret_cast<void*>(&vector_dot));
    m_jit->registerFunction("flux_vector_norm", reinterpret_cast<void*>(&vector_norm));
    m_jit->registerFunction("flux_vector_cross_3d", reinterpret_cast<void*>(&vector_cross_3d));
    m_jit->registerFunction("flux_vector_add", reinterpret_cast<void*>(&vector_add));
    m_jit->registerFunction("flux_vector_sub", reinterpret_cast<void*>(&vector_sub));
    m_jit->registerFunction("flux_vector_mul_elementwise", reinterpret_cast<void*>(&vector_mul_elementwise));
    m_jit->registerFunction("flux_vector_div_elementwise", reinterpret_cast<void*>(&vector_div_elementwise));
    m_jit->registerFunction("flux_vector_mul_scalar", reinterpret_cast<void*>(&vector_mul_scalar));
    m_jit->registerFunction("flux_vector_div_scalar", reinterpret_cast<void*>(&vector_div_scalar));
    m_jit->registerFunction("flux_create_vector_sum", reinterpret_cast<void*>(&create_vector_sum));
    m_jit->registerFunction("flux_vector_get", reinterpret_cast<void*>(&vector_get));
    m_jit->registerFunction("flux_vector_size", reinterpret_cast<void*>(&vector_size));
    
    // Register matrix helper functions
    m_jit->registerFunction("flux_matrix_mul", reinterpret_cast<void*>(&matrix_mul));
    m_jit->registerFunction("flux_matrix_add", reinterpret_cast<void*>(&matrix_add));
    m_jit->registerFunction("flux_matrix_sub", reinterpret_cast<void*>(&matrix_sub));
    m_jit->registerFunction("flux_matrix_transpose", reinterpret_cast<void*>(&matrix_transpose));
    m_jit->registerFunction("flux_matrix_det", reinterpret_cast<void*>(&matrix_det));

    // Register string helper functions
    m_jit->registerFunction("print_string", reinterpret_cast<void*>(&flux_print_string));

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
        } else {
            fnAST = parser.ParseTopLevelExpr();
        }

        // Generate code for the function
        if (fnAST) {
            std::string fnName = fnAST->getProto()->getName();
            m_functionReturnTypes[fnName] = fnAST->getProto()->getReturnType();
            
            if (llvm::Function* F = fnAST->codegen(*m_codegenCtx)) {
                // Success - function added to module
            } else {
                if (error) *error = "Code generation failed.";
                return false;
            }
        } else {
            // If nothing was parsed, we must advance to avoid infinite loop
            parser.getNextToken();
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
        // Complex return
        using ComplexFn = std::complex<double>(*)();
        using ComplexFn1 = std::complex<double>(*)(double);
        using ComplexFn2 = std::complex<double>(*)(double, double);

        if (args.empty()) {
            return reinterpret_cast<ComplexFn>(fnPtr)();
        } else if (args.size() == 1) {
            return reinterpret_cast<ComplexFn1>(fnPtr)(args[0]);
        } else if (args.size() == 2) {
            return reinterpret_cast<ComplexFn2>(fnPtr)(args[0], args[1]);
        }
    } else {
        // Scalar return (assuming double for now)
        if (args.empty()) {
            auto fn = reinterpret_cast<double(*)()>(fnPtr);
            return fn();
        } else if (args.size() == 1) {
            auto fn = reinterpret_cast<double(*)(double)>(fnPtr);
            return fn(args[0]);
        } else if (args.size() == 2) {
            auto fn = reinterpret_cast<double(*)(double, double)>(fnPtr);
            return fn(args[0], args[1]);
        }
    }

    if (error) *error = "Unsupported number of arguments or return type for JIT call";
    return 0.0;
}

} // namespace Flux
