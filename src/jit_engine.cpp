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

// External C function for printing strings
extern "C" double flux_print_string(const char* str) {
    printf("%s", str);
    fflush(stdout);
    return 0.0;
}

// SPICE-style node access
extern "C" double flux_get_voltage(const char* node) {
    if (std::string(node) == "0") return 0.0;
    return 5.0; 
}

extern "C" double flux_get_current(const char* branch) {
    return 0.001; 
}

extern "C" double flux_get_parameter(const char* name) {
    if (std::string(name) == "TEMP") return 27.0;
    return 1.0;
}

// Behavioral Modeling Functions
extern "C" double flux_uramp(double x) {
    return x > 0.0 ? x : 0.0;
}

extern "C" double flux_limit(double x, double low, double high) {
    if (x < low) return low;
    if (x > high) return high;
    return x;
}

extern "C" double flux_buf(double x) {
    return x > 0.5 ? 1.0 : 0.0;
}

extern "C" double flux_inv(double x) {
    return x > 0.5 ? 0.0 : 1.0;
}

// Mathematical library functions
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

#include <Eigen/Dense>

extern "C" void* flux_create_matrix(double* data, int rows, int cols) {
    Eigen::MatrixXd* mat = new Eigen::MatrixXd(rows, cols);
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            (*mat)(r, c) = data[r * cols + c];
        }
    }
    return mat;
}

extern "C" void* flux_matrix_mul(void* a_ptr, void* b_ptr) {
    Eigen::MatrixXd* A = static_cast<Eigen::MatrixXd*>(a_ptr);
    Eigen::MatrixXd* B = static_cast<Eigen::MatrixXd*>(b_ptr);
    Eigen::MatrixXd* res = new Eigen::MatrixXd((*A) * (*B));
    return res;
}

extern "C" void* flux_matrix_mul_ms(void* m_ptr, double s) {
    Eigen::MatrixXd* M = static_cast<Eigen::MatrixXd*>(m_ptr);
    Eigen::MatrixXd* res = new Eigen::MatrixXd((*M) * s);
    return res;
}

extern "C" void* flux_matrix_add(void* a_ptr, void* b_ptr) {
    Eigen::MatrixXd* A = static_cast<Eigen::MatrixXd*>(a_ptr);
    Eigen::MatrixXd* B = static_cast<Eigen::MatrixXd*>(b_ptr);
    Eigen::MatrixXd* res = new Eigen::MatrixXd((*A) + (*B));
    return res;
}

extern "C" void* flux_matrix_sub(void* a_ptr, void* b_ptr) {
    Eigen::MatrixXd* A = static_cast<Eigen::MatrixXd*>(a_ptr);
    Eigen::MatrixXd* B = static_cast<Eigen::MatrixXd*>(b_ptr);
    Eigen::MatrixXd* res = new Eigen::MatrixXd((*A) - (*B));
    return res;
}

extern "C" void* flux_matrix_transpose(void* m_ptr) {
    Eigen::MatrixXd* M = static_cast<Eigen::MatrixXd*>(m_ptr);
    Eigen::MatrixXd* res = new Eigen::MatrixXd(M->transpose());
    return res;
}

extern "C" int flux_matrix_rows(void* m_ptr) {
    if (!m_ptr) return 0;
    return static_cast<Eigen::MatrixXd*>(m_ptr)->rows();
}

extern "C" int flux_matrix_cols(void* m_ptr) {
    if (!m_ptr) return 0;
    return static_cast<Eigen::MatrixXd*>(m_ptr)->cols();
}

extern "C" double flux_matrix_get(void* m_ptr, int row, int col) {
    Eigen::MatrixXd* M = static_cast<Eigen::MatrixXd*>(m_ptr);
    return (*M)(row, col);
}

extern "C" double flux_create_vector_sum(double* data, int size) {
    double sum = 0.0;
    for(int i=0; i<size; ++i) sum += data[i];
    return sum;
}

extern "C" double flux_print_matrix(void* m_ptr) {
    Eigen::MatrixXd* M = static_cast<Eigen::MatrixXd*>(m_ptr);
    if (!M) return 0.0;
    std::cout << *M << std::endl;
    return 1.0;
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
    if (m_jit) m_jit->setOptimizationLevel(level);
}

OptimizationLevel JITEngine::getOptimizationLevel() const {
    if (m_jit) return m_jit->getOptimizationLevel();
    return OptimizationLevel::O2;
}

void JITEngine::initialize() {
    if (m_initialized) return;
    m_jit = std::make_unique<FluxJIT>();
    m_codegenCtx = std::make_unique<CodegenContext>();
    m_codegenCtx->TheModule = std::make_unique<llvm::Module>("Flux JIT Core", m_codegenCtx->TheContext);
    m_initialized = true;
    registerEigenFunctions();
    std::cout << "FluxScript C++ LLVM JIT Engine Initialized." << std::endl;
}

void JITEngine::registerEigenFunctions() {
    if (!m_jit || !m_codegenCtx) return;
    
    m_jit->registerFunction("flux_create_vector_sum", reinterpret_cast<void*>(&flux_create_vector_sum));
    m_jit->registerFunction("flux_matrix_mul", reinterpret_cast<void*>(&flux_matrix_mul));
    m_jit->registerFunction("flux_matrix_mul_ms", reinterpret_cast<void*>(&flux_matrix_mul_ms));
    m_jit->registerFunction("flux_matrix_add", reinterpret_cast<void*>(&flux_matrix_add));
    m_jit->registerFunction("flux_matrix_sub", reinterpret_cast<void*>(&flux_matrix_sub));
    m_jit->registerFunction("flux_matrix_transpose", reinterpret_cast<void*>(&flux_matrix_transpose));
    m_jit->registerFunction("flux_matrix_get", reinterpret_cast<void*>(&flux_matrix_get));
    m_jit->registerFunction("flux_create_matrix", reinterpret_cast<void*>(&flux_create_matrix));
    m_jit->registerFunction("print_matrix", reinterpret_cast<void*>(&flux_print_matrix));

    m_jit->registerFunction("print_string", reinterpret_cast<void*>(&flux_print_string));

    m_jit->registerFunction("flux_get_voltage", reinterpret_cast<void*>(&flux_get_voltage));
    m_jit->registerFunction("flux_get_current", reinterpret_cast<void*>(&flux_get_current));
    m_jit->registerFunction("flux_get_parameter", reinterpret_cast<void*>(&flux_get_parameter));

    // Behavioral Functions
    m_jit->registerFunction("uramp", reinterpret_cast<void*>(&flux_uramp));
    m_jit->registerFunction("limit", reinterpret_cast<void*>(&flux_limit));
    m_jit->registerFunction("buf", reinterpret_cast<void*>(&flux_buf));
    m_jit->registerFunction("inv", reinterpret_cast<void*>(&flux_inv));

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

    m_jit->registerFunction("pi", reinterpret_cast<void*>(&flux_pi));
    m_jit->registerFunction("e", reinterpret_cast<void*>(&flux_e));
}

void JITEngine::injectStandardLibrary() {
    if (!m_codegenCtx) return;
    
    auto inject = [&](const std::string& name, int args) {
        std::vector<std::pair<std::string, FluxType>> params;
        for (int i = 0; i < args; ++i) {
            params.push_back({"arg" + std::to_string(i), FluxType(TypeKind::Double)});
        }
        PrototypeAST proto(name, params, FluxType(TypeKind::Double));
        proto.codegen(*m_codegenCtx);
        m_functionReturnTypes[name] = FluxType(TypeKind::Double);
    };

    // Trigonometry
    inject("sin", 1);
    inject("cos", 1);
    inject("tan", 1);
    inject("asin", 1);
    inject("acos", 1);
    inject("atan", 1);
    inject("atan2", 2);
    inject("sinh", 1);
    inject("cosh", 1);
    inject("tanh", 1);
    
    // Core math
    inject("sqrt", 1);
    inject("exp", 1);
    inject("log", 1);
    inject("log10", 1);
    inject("abs", 1);
    inject("floor", 1);
    inject("ceil", 1);
    inject("round", 1);
    inject("pow", 2);

    // Constants
    inject("pi", 0);
    inject("e", 0);
}

void JITEngine::finalize() {
    m_initialized = false;
    m_jit.reset();
    m_codegenCtx.reset();
}

bool JITEngine::isInitialized() const {
    return m_initialized;
}

#if defined(FLUX_HAS_QT)
bool JITEngine::compileScript(const QString& code, QString* error) {
    if (!m_initialized) initialize();
    Parser parser(code.toStdString());
    m_functionReturnTypes.clear();
    injectStandardLibrary();
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
            if (!fnAST->codegen(*m_codegenCtx)) {
                if (error) *error = "Code generation failed.";
                return false;
            }
        } else {
            if (parser.CurTok != static_cast<int>(TokenType::tok_eof)) parser.SkipToSynchronizationPoint();
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
    injectStandardLibrary();
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
            if (!fnAST->codegen(*m_codegenCtx)) {
                if (error) *error = "Code generation failed.";
                return false;
            }
        } else {
            if (parser.CurTok != static_cast<int>(TokenType::tok_eof)) parser.SkipToSynchronizationPoint();
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

#if defined(FLUX_HAS_QT)
FluxValue JITEngine::callFunction(const std::string& name, const std::vector<double>& args, QString* error) {
    if (!m_initialized) { if (error) *error = "JIT not initialized"; return 0.0; }
    void* fnPtr = m_jit->getPointerToFunction(name);
    if (!fnPtr) { if (error) *error = QString("Function not found: ") + QString::fromStdString(name); return 0.0; }
    FluxType retType = m_functionReturnTypes[name];

    if (retType.Kind == TypeKind::Matrix) {
        struct MatrixRet { void* ptr; int rows; int cols; };
        using MatrixFn = MatrixRet(*)();
        using MatrixFn1 = MatrixRet(*)(double);
        using MatrixFn2 = MatrixRet(*)(double, double);

        MatrixRet r = {nullptr, 0, 0};
        if (args.empty()) r = reinterpret_cast<MatrixFn>(fnPtr)();
        else if (args.size() == 1) r = reinterpret_cast<MatrixFn1>(fnPtr)(args[0]);
        else if (args.size() == 2) r = reinterpret_cast<MatrixFn2>(fnPtr)(args[0], args[1]);

        return MatrixResult{r.ptr, r.rows, r.cols};
    } else if (retType.Kind == TypeKind::Complex) {
        // Complex return: LLVM returns small structs in registers or via hidden pointer.
        // For {double, double}, it's usually returned as a pair of registers.
        // The safest way to handle this across different ABIs without a wrapper is to 
        // use a helper that returns by value and hope it matches, but for {double, double}
        // it's often more reliable to call a function that takes a pointer to the result.

        // However, since we are using JIT and casting fnPtr directly:
        struct ComplexRet { double real; double imag; };
        using ComplexFn = ComplexRet(*)();
        using ComplexFn1 = ComplexRet(*)(double);
        using ComplexFn2 = ComplexRet(*)(double, double);

        if (args.empty()) {
            ComplexRet r = reinterpret_cast<ComplexFn>(fnPtr)();
            return std::complex<double>(r.real, r.imag);
        }
        if (args.size() == 1) {
            ComplexRet r = reinterpret_cast<ComplexFn1>(fnPtr)(args[0]);
            return std::complex<double>(r.real, r.imag);
        }
        if (args.size() == 2) {
            ComplexRet r = reinterpret_cast<ComplexFn2>(fnPtr)(args[0], args[1]);
            return std::complex<double>(r.real, r.imag);
        }
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
    if (!m_initialized) { if (error) *error = "JIT not initialized"; return 0.0; }
    void* fnPtr = m_jit->getPointerToFunction(name);
    if (!fnPtr) { if (error) *error = "Function not found: " + name; return 0.0; }
    FluxType retType = m_functionReturnTypes[name];

    if (retType.Kind == TypeKind::Matrix) {
        struct MatrixRet { void* ptr; int rows; int cols; };
        using MatrixFn = MatrixRet(*)();
        using MatrixFn1 = MatrixRet(*)(double);
        using MatrixFn2 = MatrixRet(*)(double, double);

        MatrixRet r = {nullptr, 0, 0};
        if (args.empty()) r = reinterpret_cast<MatrixFn>(fnPtr)();
        else if (args.size() == 1) r = reinterpret_cast<MatrixFn1>(fnPtr)(args[0]);
        else if (args.size() == 2) r = reinterpret_cast<MatrixFn2>(fnPtr)(args[0], args[1]);

        return MatrixResult{r.ptr, r.rows, r.cols};
    } else if (retType.Kind == TypeKind::Complex) {
        // Complex return: LLVM returns small structs in registers or via hidden pointer.
        // For {double, double}, it's usually returned as a pair of registers.
        // The safest way to handle this across different ABIs without a wrapper is to 
        // use a helper that returns by value and hope it matches, but for {double, double}
        // it's often more reliable to call a function that takes a pointer to the result.

        // However, since we are using JIT and casting fnPtr directly:
        struct ComplexRet { double real; double imag; };
        using ComplexFn = ComplexRet(*)();
        using ComplexFn1 = ComplexRet(*)(double);
        using ComplexFn2 = ComplexRet(*)(double, double);

        if (args.empty()) {
            ComplexRet r = reinterpret_cast<ComplexFn>(fnPtr)();
            return std::complex<double>(r.real, r.imag);
        }
        if (args.size() == 1) {
            ComplexRet r = reinterpret_cast<ComplexFn1>(fnPtr)(args[0]);
            return std::complex<double>(r.real, r.imag);
        }
        if (args.size() == 2) {
            ComplexRet r = reinterpret_cast<ComplexFn2>(fnPtr)(args[0], args[1]);
            return std::complex<double>(r.real, r.imag);
        }
    } else {
        if (args.empty()) return reinterpret_cast<double(*)()>(fnPtr)();
        if (args.size() == 1) return reinterpret_cast<double(*)(double)>(fnPtr)(args[0]);
        if (args.size() == 2) return reinterpret_cast<double(*)(double, double)>(fnPtr)(args[0], args[1]);
    }
    if (error) *error = "Unsupported arguments or return type";
    return 0.0;
}

} // namespace Flux
