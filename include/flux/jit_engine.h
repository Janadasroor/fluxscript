#ifndef FLUX_JIT_ENGINE_H
#define FLUX_JIT_ENGINE_H

#include <memory>
#include <vector>
#include <variant>
#include <complex>

#include "flux/compiler/ast.h"

#include <QString>
#ifdef emit
#undef emit
#endif

namespace Flux {

class FluxJIT;
class Parser;

using FluxValue = std::variant<double, std::complex<double>, int>;

class JITEngine {
public:
    static JITEngine& instance();

    void initialize();
    void finalize();
    bool isInitialized() const;

    bool executeString(const QString& code, QString* error = nullptr);
    bool compileScript(const QString& code, QString* error = nullptr);

    CodegenContext& context() { return *m_codegenCtx; }

    // Call a compiled function.
    FluxValue callFunction(const std::string& name, const std::vector<double>& args, QString* error = nullptr);

private:
    JITEngine();
    ~JITEngine();

    void registerEigenFunctions();

    std::unique_ptr<FluxJIT> m_jit;
    std::unique_ptr<CodegenContext> m_codegenCtx;
    std::map<std::string, FluxType> m_functionReturnTypes;
    bool m_initialized = false;
};

} // namespace Flux

#endif // FLUX_JIT_ENGINE_H
