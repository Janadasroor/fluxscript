#include "flux/mlir/flux_mlir.h"

#include "flux/compiler/ast.h"
#include "flux/compiler/parser.h"

#include <mlir/Dialect/Arith/IR/Arith.h>
#include <mlir/Dialect/Func/IR/FuncOps.h>
#include <mlir/Dialect/Tensor/IR/Tensor.h>
#include <mlir/IR/BuiltinOps.h>
#include <mlir/IR/BuiltinTypes.h>
#include <mlir/IR/Builders.h>
#include <mlir/IR/DialectRegistry.h>
#include <mlir/IR/MLIRContext.h>

#include <llvm/ADT/StringMap.h>
#include <llvm/Support/raw_ostream.h>

#include <map>
#include <optional>

namespace Flux::MLIR {

namespace {

struct ParsedProgram {
    std::vector<std::unique_ptr<PrototypeAST>> externs;
    std::vector<std::unique_ptr<FunctionAST>> functions;
};

std::optional<ParsedProgram> parseProgram(const std::string& code, std::string& error) {
    Parser parser(code);
    ParsedProgram program;

    while (parser.CurTok != static_cast<int>(TokenType::tok_eof)) {
        if (parser.CurTok == static_cast<int>(TokenType::tok_def)) {
            auto function = parser.ParseDefinition();
            if (!function) {
                error = "failed to parse function definition";
                return std::nullopt;
            }
            program.functions.push_back(std::move(function));
            continue;
        }

        if (parser.CurTok == static_cast<int>(TokenType::tok_extern)) {
            auto proto = parser.ParseExtern();
            if (!proto) {
                error = "failed to parse extern declaration";
                return std::nullopt;
            }
            program.externs.push_back(std::move(proto));
            continue;
        }

        if (parser.CurTok == static_cast<int>(TokenType::tok_import)) {
            auto importAst = parser.ParseImport();
            if (!importAst) {
                error = "failed to parse import declaration";
                return std::nullopt;
            }
            error = "MLIR import lowering is not implemented yet";
            return std::nullopt;
        }

        if (parser.CurTok == static_cast<int>(TokenType::tok_semicolon)) {
            parser.getNextToken();
            continue;
        }

        auto topLevel = parser.ParseTopLevelExpr();
        if (!topLevel) {
            error = "failed to parse top-level expression";
            return std::nullopt;
        }
        program.functions.push_back(std::move(topLevel));
    }

    if (parser.hasError()) {
        error = "syntax validation failed";
        return std::nullopt;
    }

    return program;
}

class ModuleLowering {
public:
    explicit ModuleLowering(mlir::MLIRContext& context)
        : m_builder(&context),
          m_loc(m_builder.getUnknownLoc()),
          m_module(mlir::ModuleOp::create(m_loc)) {}

    EmitResult lower(const ParsedProgram& program) {
        declareBuiltins();

        for (const auto& proto : program.externs) {
            if (!declarePrototype(*proto))
                return failureResult();
        }

        for (const auto& function : program.functions) {
            if (!declarePrototype(*function->getProto()))
                return failureResult();
        }

        for (const auto& function : program.functions) {
            if (!lowerFunction(*function))
                return failureResult();
        }

        std::string printed;
        llvm::raw_string_ostream os(printed);
        m_module.print(os);
        os.flush();

        EmitResult result;
        result.ok = true;
        result.output = printed;
        return result;
    }

private:
    EmitResult failureResult() const {
        EmitResult result;
        result.error = m_error.empty() ? "MLIR lowering failed" : m_error;
        return result;
    }

    void setError(const std::string& error) {
        if (m_error.empty())
            m_error = error;
    }

    bool isScalarFloatType(mlir::Type type) const {
        return mlir::isa<mlir::FloatType>(type);
    }

    bool isFloatTensorType(mlir::Type type) const {
        auto shaped = mlir::dyn_cast<mlir::ShapedType>(type);
        return shaped && mlir::isa<mlir::FloatType>(shaped.getElementType());
    }

    mlir::Type lowerType(const FluxType& type) {
        switch (type.Kind) {
            case TypeKind::Float:
                return m_builder.getF32Type();
            case TypeKind::Int:
                return m_builder.getI32Type();
            case TypeKind::Void:
                return mlir::Type();
            case TypeKind::Vector:
                return mlir::UnrankedTensorType::get(m_builder.getF64Type());
            case TypeKind::Matrix:
                return mlir::UnrankedTensorType::get(m_builder.getF64Type());
            case TypeKind::Double:
            default:
                return m_builder.getF64Type();
        }
    }

    bool declarePrototype(const PrototypeAST& proto) {
        if (m_module.lookupSymbol<mlir::func::FuncOp>(proto.getName()))
            return true;

        std::vector<mlir::Type> argTypes;
        argTypes.reserve(proto.getArgs().size());
        for (const auto& arg : proto.getArgs()) {
            auto type = lowerType(arg.second);
            if (!type) {
                setError("unsupported MLIR function argument type in '" + proto.getName() + "'");
                return false;
            }
            argTypes.push_back(type);
        }

        std::vector<mlir::Type> resultTypes;
        if (auto resultType = lowerType(proto.getReturnType()))
            resultTypes.push_back(resultType);

        auto funcType = m_builder.getFunctionType(argTypes, resultTypes);
        auto function = mlir::func::FuncOp::create(m_loc, proto.getName(), funcType);
        m_module.push_back(function);
        return true;
    }

    void declareBuiltins() {
        auto declareBuiltin = [&](const std::string& name, int arity) {
            if (m_module.lookupSymbol<mlir::func::FuncOp>(name))
                return;

            std::vector<mlir::Type> args(static_cast<size_t>(arity), m_builder.getF64Type());
            auto type = m_builder.getFunctionType(args, {m_builder.getF64Type()});
            m_module.push_back(mlir::func::FuncOp::create(m_loc, name, type));
        };

        declareBuiltin("sin", 1);
        declareBuiltin("cos", 1);
        declareBuiltin("tan", 1);
        declareBuiltin("asin", 1);
        declareBuiltin("acos", 1);
        declareBuiltin("atan", 1);
        declareBuiltin("atan2", 2);
        declareBuiltin("sqrt", 1);
        declareBuiltin("exp", 1);
        declareBuiltin("log", 1);
        declareBuiltin("log10", 1);
        declareBuiltin("abs", 1);
        declareBuiltin("floor", 1);
        declareBuiltin("ceil", 1);
        declareBuiltin("round", 1);
        declareBuiltin("pow", 2);
        declareBuiltin("pi", 0);
        declareBuiltin("e", 0);
    }

    mlir::Value lookupValue(const std::string& name) const {
        for (auto it = m_scopes.rbegin(); it != m_scopes.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end())
                return found->second;
        }
        return {};
    }

    void pushScope() {
        m_scopes.emplace_back();
    }

    void popScope() {
        m_scopes.pop_back();
    }

    void bindValue(const std::string& name, mlir::Value value) {
        if (!m_scopes.empty())
            m_scopes.back()[name] = value;
    }

    mlir::Value castValue(mlir::Value value, mlir::Type targetType) {
        if (!value || !targetType)
            return {};
        if (value.getType() == targetType)
            return value;

        if (isScalarFloatType(targetType) && mlir::isa<mlir::IntegerType>(value.getType()))
            return m_builder.create<mlir::arith::SIToFPOp>(m_loc, targetType, value);

        if (mlir::isa<mlir::IntegerType>(targetType) && isScalarFloatType(value.getType()))
            return m_builder.create<mlir::arith::FPToSIOp>(m_loc, targetType, value);

        if (mlir::isa<mlir::TensorType>(targetType) && mlir::isa<mlir::TensorType>(value.getType()))
            return m_builder.create<mlir::tensor::CastOp>(m_loc, targetType, value);

        return {};
    }

    mlir::Value lowerNumericConstant(double value) {
        return m_builder.create<mlir::arith::ConstantFloatOp>(
            m_loc, llvm::APFloat(value), m_builder.getF64Type());
    }

    mlir::Value lowerExpr(const ExprAST* expr) {
        if (!expr)
            return {};

        if (const auto* number = dynamic_cast<const NumberExprAST*>(expr))
            return lowerNumericConstant(number->getValue());

        if (const auto* variable = dynamic_cast<const VariableExprAST*>(expr)) {
            auto value = lookupValue(variable->getName());
            if (!value)
                setError("unknown variable '" + variable->getName() + "'");
            return value;
        }

        if (const auto* unary = dynamic_cast<const UnaryExprAST*>(expr)) {
            auto operand = lowerExpr(unary->getOperand());
            if (!operand)
                return {};

            switch (unary->getOp()) {
                case '+':
                    return operand;
                case '-':
                    if (isScalarFloatType(operand.getType()) || isFloatTensorType(operand.getType()))
                        return m_builder.create<mlir::arith::NegFOp>(m_loc, operand);
                    setError("unsupported unary '-' operand in MLIR lowering");
                    return {};
                default:
                    setError("unsupported unary operator in MLIR lowering");
                    return {};
            }
        }

        if (const auto* binary = dynamic_cast<const BinaryExprAST*>(expr)) {
            auto lhs = lowerExpr(binary->getLHS());
            auto rhs = lowerExpr(binary->getRHS());
            if (!lhs || !rhs)
                return {};

            if (lhs.getType() != rhs.getType()) {
                rhs = castValue(rhs, lhs.getType());
                if (!rhs) {
                    setError("binary operands could not be cast to the same MLIR type");
                    return {};
                }
            }

            switch (binary->getOp()) {
                case '+':
                    return m_builder.create<mlir::arith::AddFOp>(m_loc, lhs, rhs);
                case '-':
                    return m_builder.create<mlir::arith::SubFOp>(m_loc, lhs, rhs);
                case '*':
                    return m_builder.create<mlir::arith::MulFOp>(m_loc, lhs, rhs);
                case '/':
                    return m_builder.create<mlir::arith::DivFOp>(m_loc, lhs, rhs);
                case static_cast<int>(TokenType::tok_power): {
                    auto callee = m_module.lookupSymbol<mlir::func::FuncOp>("pow");
                    auto call = m_builder.create<mlir::func::CallOp>(m_loc, callee, mlir::ValueRange{lhs, rhs});
                    return call.getNumResults() == 1 ? call.getResult(0) : mlir::Value();
                }
                case '<':
                case '>':
                case static_cast<int>(TokenType::tok_less_equal):
                case static_cast<int>(TokenType::tok_greater_equal):
                case static_cast<int>(TokenType::tok_equal):
                case static_cast<int>(TokenType::tok_not_equal): {
                    mlir::arith::CmpFPredicate predicate;
                    switch (binary->getOp()) {
                        case '<': predicate = mlir::arith::CmpFPredicate::OLT; break;
                        case '>': predicate = mlir::arith::CmpFPredicate::OGT; break;
                        case static_cast<int>(TokenType::tok_less_equal): predicate = mlir::arith::CmpFPredicate::OLE; break;
                        case static_cast<int>(TokenType::tok_greater_equal): predicate = mlir::arith::CmpFPredicate::OGE; break;
                        case static_cast<int>(TokenType::tok_equal): predicate = mlir::arith::CmpFPredicate::OEQ; break;
                        default: predicate = mlir::arith::CmpFPredicate::ONE; break;
                    }
                    auto cmp = m_builder.create<mlir::arith::CmpFOp>(m_loc, predicate, lhs, rhs);
                    return m_builder.create<mlir::arith::UIToFPOp>(m_loc, m_builder.getF64Type(), cmp.getResult());
                }
                default:
                    setError("unsupported binary operator in MLIR lowering");
                    return {};
            }
        }

        if (const auto* call = dynamic_cast<const CallExprAST*>(expr)) {
            std::vector<mlir::Value> args;
            args.reserve(call->getArgs().size());
            for (const auto& arg : call->getArgs()) {
                auto value = lowerExpr(arg.get());
                if (!value)
                    return {};
                args.push_back(value);
            }

            auto callee = m_module.lookupSymbol<mlir::func::FuncOp>(call->getCallee());
            if (!callee) {
                std::vector<mlir::Type> argTypes;
                argTypes.reserve(args.size());
                for (auto arg : args)
                    argTypes.push_back(arg.getType());
                auto functionType = m_builder.getFunctionType(argTypes, {m_builder.getF64Type()});
                callee = mlir::func::FuncOp::create(m_loc, call->getCallee(), functionType);
                m_module.push_back(callee);
            }

            auto callOp = m_builder.create<mlir::func::CallOp>(m_loc, callee, mlir::ValueRange(args));
            return callOp.getNumResults() == 1 ? callOp.getResult(0) : mlir::Value();
        }

        if (const auto* let = dynamic_cast<const LetExprAST*>(expr)) {
            auto init = lowerExpr(let->getInit());
            if (!init)
                return {};

            if (!let->getBody()) {
                bindValue(let->getVarName(), init);
                return init;
            }

            pushScope();
            bindValue(let->getVarName(), init);
            auto body = lowerExpr(let->getBody());
            popScope();
            return body;
        }

        if (const auto* block = dynamic_cast<const BlockExprAST*>(expr)) {
            pushScope();
            mlir::Value lastValue = lowerNumericConstant(0.0);
            for (const auto& statement : block->getStatements()) {
                auto value = lowerExpr(statement.get());
                if (!value) {
                    popScope();
                    return {};
                }
                lastValue = value;
            }
            popScope();
            return lastValue;
        }

        if (const auto* vector = dynamic_cast<const VectorExprAST*>(expr)) {
            std::vector<mlir::Value> elements;
            elements.reserve(vector->getElements().size());
            for (const auto& element : vector->getElements()) {
                auto value = castValue(lowerExpr(element.get()), m_builder.getF64Type());
                if (!value) {
                    setError("vector elements must lower to scalar floating-point values");
                    return {};
                }
                elements.push_back(value);
            }

            auto type = mlir::RankedTensorType::get(
                {static_cast<int64_t>(elements.size())}, m_builder.getF64Type());
            return m_builder.create<mlir::tensor::FromElementsOp>(m_loc, type, elements);
        }

        if (const auto* matrix = dynamic_cast<const MatrixExprAST*>(expr)) {
            std::vector<mlir::Value> elements;
            elements.reserve(static_cast<size_t>(matrix->getNumRows() * matrix->getNumCols()));
            for (const auto& row : matrix->getRows()) {
                for (const auto& element : row) {
                    auto value = castValue(lowerExpr(element.get()), m_builder.getF64Type());
                    if (!value) {
                        setError("matrix elements must lower to scalar floating-point values");
                        return {};
                    }
                    elements.push_back(value);
                }
            }

            auto type = mlir::RankedTensorType::get(
                {matrix->getNumRows(), matrix->getNumCols()}, m_builder.getF64Type());
            return m_builder.create<mlir::tensor::FromElementsOp>(m_loc, type, elements);
        }

        if (dynamic_cast<const StringExprAST*>(expr) || dynamic_cast<const ComplexExprAST*>(expr) ||
            dynamic_cast<const IfExprAST*>(expr) || dynamic_cast<const ForExprAST*>(expr) ||
            dynamic_cast<const WhileExprAST*>(expr) || dynamic_cast<const LambdaExprAST*>(expr) ||
            dynamic_cast<const AssignExprAST*>(expr) || dynamic_cast<const TransposeExprAST*>(expr) ||
            dynamic_cast<const RangeExprAST*>(expr) || dynamic_cast<const IndexExprAST*>(expr) ||
            dynamic_cast<const VoltageExprAST*>(expr) || dynamic_cast<const CurrentExprAST*>(expr) ||
            dynamic_cast<const ParameterExprAST*>(expr)) {
            setError("expression kind is not implemented in MLIR lowering yet");
            return {};
        }

        setError("unknown expression kind in MLIR lowering");
        return {};
    }

    bool lowerFunction(const FunctionAST& functionAst) {
        auto proto = functionAst.getProto();
        auto function = m_module.lookupSymbol<mlir::func::FuncOp>(proto->getName());
        if (!function) {
            setError("missing MLIR function declaration for '" + proto->getName() + "'");
            return false;
        }

        if (!function.empty()) {
            setError("duplicate function definition for '" + proto->getName() + "'");
            return false;
        }

        auto* block = function.addEntryBlock();
        m_builder.setInsertionPointToStart(block);
        pushScope();
        for (const auto& [index, arg] : llvm::enumerate(function.getArguments()))
            bindValue(proto->getArgs()[index].first, arg);

        auto value = lowerExpr(functionAst.getBody());
        if (!value) {
            popScope();
            return false;
        }

        if (function.getFunctionType().getNumResults() == 0) {
            m_builder.create<mlir::func::ReturnOp>(m_loc);
            popScope();
            return true;
        }

        auto targetType = function.getFunctionType().getResult(0);
        value = castValue(value, targetType);
        if (!value) {
            setError("could not cast function result for '" + proto->getName() + "'");
            popScope();
            return false;
        }

        m_builder.create<mlir::func::ReturnOp>(m_loc, value);
        popScope();
        return true;
    }

    mlir::OpBuilder m_builder;
    mlir::Location m_loc;
    mlir::ModuleOp m_module;
    std::vector<std::map<std::string, mlir::Value>> m_scopes;
    std::string m_error;
};

} // namespace

bool isAvailable() {
    return true;
}

EmitResult emitModule(const std::string& code) {
    std::string parseError;
    auto program = parseProgram(code, parseError);
    if (!program) {
        EmitResult result;
        result.error = parseError.empty() ? "syntax validation failed" : parseError;
        return result;
    }

    mlir::DialectRegistry registry;
    registry.insert<mlir::arith::ArithDialect, mlir::func::FuncDialect, mlir::tensor::TensorDialect>();

    mlir::MLIRContext context(registry);
    context.loadDialect<mlir::arith::ArithDialect, mlir::func::FuncDialect, mlir::tensor::TensorDialect>();

    ModuleLowering lowering(context);
    return lowering.lower(*program);
}

} // namespace Flux::MLIR
