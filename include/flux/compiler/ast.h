#ifndef FLUX_COMPILER_AST_H
#define FLUX_COMPILER_AST_H

#include <string>
#include <memory>
#include <vector>
#include <map>
#include <llvm/IR/Value.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include "flux/compiler/lexer.h"

namespace Flux {

// Type system for explicit typing
enum class TypeKind {
    Double,   // Default type (double precision float)
    Float,    // Single precision float
    Int,      // Integer
    Void,     // Void (for return types)
    Complex,  // Complex number (double complex)
    String    // String (i8*)
};

class FluxType {
public:
    TypeKind Kind;
    
    FluxType(TypeKind K = TypeKind::Double) : Kind(K) {}
    
    llvm::Type* getLLVMType(llvm::LLVMContext& Context) const {
        switch (Kind) {
            case TypeKind::Float:
                return llvm::Type::getFloatTy(Context);
            case TypeKind::Int:
                return llvm::Type::getInt32Ty(Context);
            case TypeKind::Void:
                return llvm::Type::getVoidTy(Context);
            case TypeKind::Complex:
                // Complex numbers are represented as { double, double } structs
                return llvm::StructType::get(Context,
                    llvm::Type::getDoubleTy(Context),
                    llvm::Type::getDoubleTy(Context));
            case TypeKind::String:
                // Strings are represented as i8* (pointer to char)
                return llvm::PointerType::get(llvm::Type::getInt8Ty(Context), 0);
            case TypeKind::Double:
            default:
                return llvm::Type::getDoubleTy(Context);
        }
    }
    
    static FluxType fromToken(int token) {
        switch (token) {
            case static_cast<int>(TokenType::tok_type_float):
                return FluxType(TypeKind::Float);
            case static_cast<int>(TokenType::tok_type_int):
                return FluxType(TypeKind::Int);
            case static_cast<int>(TokenType::tok_type_void):
                return FluxType(TypeKind::Void);
            case static_cast<int>(TokenType::tok_type_complex):
                return FluxType(TypeKind::Complex);
            case static_cast<int>(TokenType::tok_type_double):
            default:
                return FluxType(TypeKind::Double);
        }
    }
};

class CodegenContext {
public:
    llvm::LLVMContext TheContext;
    llvm::IRBuilder<> Builder;
    std::unique_ptr<llvm::Module> TheModule;
    std::map<std::string, llvm::Value*> NamedValues;

    CodegenContext() : Builder(TheContext) {}
};

class ExprAST {
public:
    virtual ~ExprAST() = default;
    virtual llvm::Value* codegen(CodegenContext& context) = 0;
};

class NumberExprAST : public ExprAST {
    double Val;
public:
    NumberExprAST(double Val) : Val(Val) {}
    llvm::Value* codegen(CodegenContext& context) override;
};

class ComplexExprAST : public ExprAST {
    double Real;
    double Imag;
public:
    ComplexExprAST(double Real, double Imag) : Real(Real), Imag(Imag) {}
    llvm::Value* codegen(CodegenContext& context) override;
};

class StringExprAST : public ExprAST {
    std::string Val;
public:
    StringExprAST(const std::string& Val) : Val(Val) {}
    llvm::Value* codegen(CodegenContext& context) override;
};

class VariableExprAST : public ExprAST {
    std::string Name;
public:
    VariableExprAST(const std::string& Name) : Name(Name) {}
    llvm::Value* codegen(CodegenContext& context) override;
    const std::string& getName() const { return Name; }
};

class BinaryExprAST : public ExprAST {
    int Op; // Changed to int to support multi-character operators
    std::unique_ptr<ExprAST> LHS, RHS;
public:
    BinaryExprAST(int Op, std::unique_ptr<ExprAST> LHS, std::unique_ptr<ExprAST> RHS)
        : Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}
    llvm::Value* codegen(CodegenContext& context) override;
};

class UnaryExprAST : public ExprAST {
    int Op; // Changed to int to support multi-character operators
    std::unique_ptr<ExprAST> Operand;
public:
    UnaryExprAST(int Op, std::unique_ptr<ExprAST> Operand)
        : Op(Op), Operand(std::move(Operand)) {}
    llvm::Value* codegen(CodegenContext& context) override;
};

class CallExprAST : public ExprAST {
    std::string Callee;
    std::vector<std::unique_ptr<ExprAST>> Args;
public:
    CallExprAST(const std::string& Callee, std::vector<std::unique_ptr<ExprAST>> Args)
        : Callee(Callee), Args(std::move(Args)) {}
    llvm::Value* codegen(CodegenContext& context) override;
};

class AssignExprAST : public ExprAST {
    std::string Name;
    std::unique_ptr<ExprAST> Val;
public:
    AssignExprAST(const std::string& Name, std::unique_ptr<ExprAST> Val)
        : Name(Name), Val(std::move(Val)) {}
    llvm::Value* codegen(CodegenContext& context) override;
};

class IfExprAST : public ExprAST {
    std::unique_ptr<ExprAST> Cond, Then, Else;
public:
    IfExprAST(std::unique_ptr<ExprAST> Cond, std::unique_ptr<ExprAST> Then,
              std::unique_ptr<ExprAST> Else)
        : Cond(std::move(Cond)), Then(std::move(Then)), Else(std::move(Else)) {}
    llvm::Value* codegen(CodegenContext& context) override;
};

class ForExprAST : public ExprAST {
    std::string VarName;
    std::unique_ptr<ExprAST> Start, End, Step;
    std::unique_ptr<ExprAST> Body;
public:
    ForExprAST(const std::string& VarName, std::unique_ptr<ExprAST> Start,
               std::unique_ptr<ExprAST> End, std::unique_ptr<ExprAST> Step,
               std::unique_ptr<ExprAST> Body)
        : VarName(VarName), Start(std::move(Start)), End(std::move(End)),
          Step(std::move(Step)), Body(std::move(Body)) {}
    llvm::Value* codegen(CodegenContext& context) override;
};

class WhileExprAST : public ExprAST {
    std::unique_ptr<ExprAST> Cond, Body;
public:
    WhileExprAST(std::unique_ptr<ExprAST> Cond, std::unique_ptr<ExprAST> Body)
        : Cond(std::move(Cond)), Body(std::move(Body)) {}
    llvm::Value* codegen(CodegenContext& context) override;
};

class LetExprAST : public ExprAST {
    std::string VarName;
    FluxType Type;
    std::unique_ptr<ExprAST> Init;
    std::unique_ptr<ExprAST> Body;
public:
    LetExprAST(const std::string& VarName, FluxType Type, std::unique_ptr<ExprAST> Init, std::unique_ptr<ExprAST> Body)
        : VarName(VarName), Type(Type), Init(std::move(Init)), Body(std::move(Body)) {}
    llvm::Value* codegen(CodegenContext& context) override;
};

class VectorExprAST : public ExprAST {
    std::vector<std::unique_ptr<ExprAST>> Elements;
public:
    VectorExprAST(std::vector<std::unique_ptr<ExprAST>> Elements)
        : Elements(std::move(Elements)) {}
    llvm::Value* codegen(CodegenContext& context) override;
};

class IndexExprAST : public ExprAST {
    std::unique_ptr<ExprAST> Array, Index;
public:
    IndexExprAST(std::unique_ptr<ExprAST> Array, std::unique_ptr<ExprAST> Index)
        : Array(std::move(Array)), Index(std::move(Index)) {}
    llvm::Value* codegen(CodegenContext& context) override;
};

class BlockExprAST : public ExprAST {
    std::vector<std::unique_ptr<ExprAST>> Statements;
public:
    BlockExprAST(std::vector<std::unique_ptr<ExprAST>> Statements)
        : Statements(std::move(Statements)) {}
    llvm::Value* codegen(CodegenContext& context) override;
};

class PrototypeAST {
    std::string Name;
    std::vector<std::pair<std::string, FluxType>> Args;  // Name-Type pairs
    FluxType ReturnType;
public:
    PrototypeAST(const std::string& Name, std::vector<std::pair<std::string, FluxType>> Args, FluxType RetType = FluxType(TypeKind::Double))
        : Name(Name), Args(std::move(Args)), ReturnType(RetType) {}
    llvm::Function* codegen(CodegenContext& context);
    const std::string& getName() const { return Name; }
    const std::vector<std::pair<std::string, FluxType>>& getArgs() const { return Args; }
    const FluxType& getReturnType() const { return ReturnType; }
};

class FunctionAST {
    std::unique_ptr<PrototypeAST> Proto;
    std::unique_ptr<ExprAST> Body;
public:
    FunctionAST(std::unique_ptr<PrototypeAST> Proto, std::unique_ptr<ExprAST> Body)
        : Proto(std::move(Proto)), Body(std::move(Body)) {}
    llvm::Function* codegen(CodegenContext& context);
    PrototypeAST* getProto() const { return Proto.get(); }
};

} // namespace Flux

#endif // FLUX_COMPILER_AST_H
