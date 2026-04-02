#include "flux/compiler/compiler_instance.h"

#include <llvm/IR/Verifier.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/Path.h>

#include "flux/compiler/lexer.h"
#include "flux/compiler/parser.h"

#include <sstream>

namespace Flux {

namespace {

std::string tokenSpelling(const Lexer& lexer, int token) {
    if (token == static_cast<int>(TokenType::tok_identifier))
        return lexer.IdentifierStr;
    if (token == static_cast<int>(TokenType::tok_string))
        return "\"" + lexer.StringVal + "\"";
    if (token == static_cast<int>(TokenType::tok_number) ||
        token == static_cast<int>(TokenType::tok_imaginary)) {
        std::ostringstream os;
        os << lexer.NumVal;
        if (token == static_cast<int>(TokenType::tok_imaginary))
            os << "j";
        return os.str();
    }
    if (token >= 0 && token < 128)
        return std::string(1, static_cast<char>(token));

    switch (static_cast<TokenType>(token)) {
        case TokenType::tok_eof: return "eof";
        case TokenType::tok_def: return "def";
        case TokenType::tok_extern: return "extern";
        case TokenType::tok_return: return "return";
        case TokenType::tok_var: return "var";
        case TokenType::tok_if: return "if";
        case TokenType::tok_then: return "then";
        case TokenType::tok_else: return "else";
        case TokenType::tok_for: return "for";
        case TokenType::tok_in: return "in";
        case TokenType::tok_do: return "do";
        case TokenType::tok_while: return "while";
        case TokenType::tok_let: return "let";
        case TokenType::tok_fn: return "fn";
        case TokenType::tok_import: return "import";
        case TokenType::tok_type_float: return "float";
        case TokenType::tok_type_double: return "double";
        case TokenType::tok_type_int: return "int";
        case TokenType::tok_type_void: return "void";
        case TokenType::tok_type_complex: return "complex";
        case TokenType::tok_type_string: return "string";
        case TokenType::tok_type_vector: return "vector";
        case TokenType::tok_type_matrix: return "matrix";
        default: return "token(" + std::to_string(token) + ")";
    }
}

} // namespace

CompilerInstance::CompilerInstance(CompilerOptions options)
    : m_options(std::move(options)) {}

std::unique_ptr<CodegenContext> CompilerInstance::createCodegenContext() const {
    auto context = std::make_unique<CodegenContext>();
    context->TheModule = std::make_unique<llvm::Module>(m_options.moduleName, context->TheContext);
    return context;
}

void CompilerInstance::injectStandardLibrary(CodegenContext& context,
                                             std::map<std::string, FluxType>& returnTypes) const {
    auto inject = [&](const std::string& name, int args) {
        std::vector<std::pair<std::string, FluxType>> params;
        for (int i = 0; i < args; ++i)
            params.push_back({"arg" + std::to_string(i), FluxType(TypeKind::Double)});

        PrototypeAST proto(name, params, FluxType(TypeKind::Double));
        proto.codegen(context);
        returnTypes[name] = FluxType(TypeKind::Double);
    };

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
    inject("sqrt", 1);
    inject("exp", 1);
    inject("log", 1);
    inject("log10", 1);
    inject("abs", 1);
    inject("floor", 1);
    inject("ceil", 1);
    inject("round", 1);
    inject("pow", 2);
    inject("pi", 0);
    inject("e", 0);
}

std::string CompilerInstance::resolveImportPath(const std::string& moduleName) const {
    namespace path = llvm::sys::path;

    llvm::SmallString<256> resolved;
    if (!m_options.inputName.empty() && m_options.inputName != "-") {
        resolved = m_options.inputName;
        path::remove_filename(resolved);
    }

    path::append(resolved, moduleName + ".flux");
    return std::string(resolved.str());
}

bool CompilerInstance::importModule(const std::string& moduleName,
                                    CodegenContext& context,
                                    std::map<std::string, FluxType>& returnTypes,
                                    std::string* error,
                                    std::map<std::string, bool>& importedModules) const {
    if (importedModules.find(moduleName) != importedModules.end())
        return true;
    importedModules[moduleName] = true;

    const auto fileName = resolveImportPath(moduleName);
    auto bufferOrErr = llvm::MemoryBuffer::getFile(fileName);
    if (!bufferOrErr) {
        if (error)
            *error = "Could not open imported file: " + fileName;
        return false;
    }

    Parser importedParser(std::string(bufferOrErr.get()->getBuffer()));
    return compileParser(importedParser, context, returnTypes, error, importedModules);
}

bool CompilerInstance::compileParser(Parser& parser,
                                     CodegenContext& context,
                                     std::map<std::string, FluxType>& returnTypes,
                                     std::string* error,
                                     std::map<std::string, bool>& importedModules) const {
    while (parser.CurTok != static_cast<int>(TokenType::tok_eof)) {
        std::unique_ptr<FunctionAST> functionAst;

        if (parser.CurTok == static_cast<int>(TokenType::tok_def)) {
            functionAst = parser.ParseDefinition();
        } else if (parser.CurTok == static_cast<int>(TokenType::tok_import)) {
            auto importAst = parser.ParseImport();
            if (importAst &&
                !importModule(importAst->getModuleName(), context, returnTypes, error, importedModules))
                return false;
            continue;
        } else if (parser.CurTok == static_cast<int>(TokenType::tok_extern)) {
            auto proto = parser.ParseExtern();
            if (proto) {
                returnTypes[proto->getName()] = proto->getReturnType();
                proto->codegen(context);
            }
            continue;
        } else if (parser.CurTok == static_cast<int>(TokenType::tok_semicolon)) {
            parser.getNextToken();
            continue;
        } else {
            functionAst = parser.ParseTopLevelExpr();
        }

        if (!functionAst) {
            if (parser.CurTok != static_cast<int>(TokenType::tok_eof))
                parser.SkipToSynchronizationPoint();
            continue;
        }

        const std::string functionName = functionAst->getProto()->getName();
        returnTypes[functionName] = functionAst->getProto()->getReturnType();
        if (!functionAst->codegen(context)) {
            if (error)
                *error = "Code generation failed.";
            return false;
        }
    }

    return !parser.hasError();
}

std::vector<TokenInfo> CompilerInstance::tokenize(const std::string& code, std::string* error) const {
    std::vector<TokenInfo> tokens;
    Lexer lexer(code);

    while (true) {
        const int token = lexer.getNextToken();
        tokens.push_back(TokenInfo{
            token,
            tokenSpelling(lexer, token),
            lexer.getCurrentLine(),
            lexer.getCurrentColumn()
        });

        if (token == static_cast<int>(TokenType::tok_eof))
            break;
    }

    if (error)
        error->clear();
    return tokens;
}

std::unique_ptr<CompileArtifacts> CompilerInstance::compileToIR(const std::string& code,
                                                                std::string* error) {
    auto artifacts = std::make_unique<CompileArtifacts>();
    artifacts->codegenContext = createCodegenContext();

    if (m_options.injectStdlib)
        injectStandardLibrary(*artifacts->codegenContext, artifacts->functionReturnTypes);

    Parser parser(code);
    std::map<std::string, bool> importedModules;
    if (!compileParser(parser, *artifacts->codegenContext, artifacts->functionReturnTypes, error, importedModules))
        return nullptr;

    if (llvm::verifyModule(*artifacts->codegenContext->TheModule, &llvm::errs())) {
        if (error)
            *error = "Generated LLVM IR is invalid.";
        return nullptr;
    }

    if (error)
        error->clear();
    return artifacts;
}

std::string CompilerInstance::emitLLVMIR(const std::string& code, std::string* error) {
    auto artifacts = compileToIR(code, error);
    if (!artifacts)
        return {};

    std::string ir;
    llvm::raw_string_ostream os(ir);
    artifacts->codegenContext->TheModule->print(os, nullptr);
    os.flush();
    return ir;
}

} // namespace Flux
