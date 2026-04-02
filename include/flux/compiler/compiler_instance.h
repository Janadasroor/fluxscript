#ifndef FLUX_COMPILER_COMPILER_INSTANCE_H
#define FLUX_COMPILER_COMPILER_INSTANCE_H

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <llvm/ADT/StringRef.h>

#include "flux/compiler/ast.h"
#include "flux/jit/flux_jit.h"

namespace Flux {

class Parser;

struct TokenInfo {
    int token = 0;
    std::string spelling;
    int line = 0;
    int column = 0;
    size_t offset = 0;
    size_t length = 0;
    std::string rawText;
};

struct CompilerOptions {
    std::string inputName = "<stdin>";
    std::string moduleName = "Flux Module";
    OptimizationLevel optimizationLevel = OptimizationLevel::O2;
    bool injectStdlib = true;
    bool debugInfo = false;
};

struct CompileArtifacts {
    std::unique_ptr<CodegenContext> codegenContext;
    std::map<std::string, FluxType> functionReturnTypes;
};

class CompilerInstance {
public:
    explicit CompilerInstance(CompilerOptions options = {});

    std::vector<TokenInfo> tokenize(const std::string& code, std::string* error = nullptr) const;
    std::unique_ptr<CompileArtifacts> compileToIR(const std::string& code, std::string* error = nullptr);
    std::string emitLLVMIR(const std::string& code, std::string* error = nullptr);

private:
    std::unique_ptr<CodegenContext> createCodegenContext() const;
    void injectStandardLibrary(CodegenContext& context, std::map<std::string, FluxType>& returnTypes) const;
    bool compileParser(Parser& parser, CodegenContext& context,
                       std::map<std::string, FluxType>& returnTypes,
                       std::string* error,
                       std::map<std::string, bool>& importedModules) const;
    bool importModule(const std::string& moduleName, CodegenContext& context,
                      std::map<std::string, FluxType>& returnTypes,
                      std::string* error,
                      std::map<std::string, bool>& importedModules) const;
    std::string resolveImportPath(const std::string& moduleName) const;

    CompilerOptions m_options;
};

} // namespace Flux

#endif // FLUX_COMPILER_COMPILER_INSTANCE_H
