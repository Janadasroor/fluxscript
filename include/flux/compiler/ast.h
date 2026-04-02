#ifndef FLUX_COMPILER_AST_H
#define FLUX_COMPILER_AST_H

#include <string>
#include <memory>
#include <vector>
#include <map>
#include <memory>
#include <llvm/IR/Value.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include "flux/compiler/lexer.h"

namespace Flux {

// Type system for explicit typing
enum class TypeKind {
    Auto,     // Inferred type
    Double,   // Default type (double precision float)
    Float,    // Single precision float
    Int,      // Integer
    Void,     // Void (for return types)
    Complex,  // Complex number (double complex)
    String,   // String (i8*)
    Matrix,   // Matrix { double*, i32, i32 }
    Vector    // Vector { double*, i32 }
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
                {
                    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(Context);
                    return llvm::StructType::get(Context, {DoubleTy, DoubleTy});
                }
            case TypeKind::Matrix:
                // Matrix represented as { double*, i32, i32 }
                return llvm::StructType::get(Context, {
                    llvm::PointerType::get(Context, 0),
                    llvm::Type::getInt32Ty(Context),
                    llvm::Type::getInt32Ty(Context)
                });
            case TypeKind::Vector:
                // Vector represented as { double*, i32 }
                return llvm::StructType::get(Context, {
                    llvm::PointerType::get(Context, 0),
                    llvm::Type::getInt32Ty(Context)
                });
            case TypeKind::String:
                // Strings are represented as i8* (pointer to char)
                return llvm::PointerType::get(llvm::Type::getInt8Ty(Context), 0);
            case TypeKind::Double:
            default:
                return llvm::Type::getDoubleTy(Context);
        }
    }
    
    static FluxType fromLLVMType(llvm::Type* T) {
        if (T->isDoubleTy()) return FluxType(TypeKind::Double);
        if (T->isFloatTy()) return FluxType(TypeKind::Float);
        if (T->isIntegerTy(32)) return FluxType(TypeKind::Int);
        if (T->isVoidTy()) return FluxType(TypeKind::Void);
        if (T->isStructTy()) {
            if (T->getStructNumElements() == 3) return FluxType(TypeKind::Matrix);
            if (T->getStructNumElements() == 2) {
                // Could be Vector or Complex. For now, distinguish by field type if possible,
                // or default to Complex if both are double.
                // Vector is { double*, i32 }, Complex is { double, double }.
                if (T->getStructElementType(0)->isPointerTy()) return FluxType(TypeKind::Vector);
                return FluxType(TypeKind::Complex);
            }
        }
        if (T->isPointerTy()) return FluxType(TypeKind::String);
        return FluxType(TypeKind::Double);
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
            case static_cast<int>(TokenType::tok_type_string):
                return FluxType(TypeKind::String);
            case static_cast<int>(TokenType::tok_type_matrix):
                return FluxType(TypeKind::Matrix);
            case static_cast<int>(TokenType::tok_type_vector):
                return FluxType(TypeKind::Vector);
            case static_cast<int>(TokenType::tok_type_double):
            default:
                return FluxType(TypeKind::Double);
        }
    }
};

class CodegenContext {
public:
    std::unique_ptr<llvm::LLVMContext> OwnedContext;
    llvm::LLVMContext& TheContext;
    llvm::IRBuilder<> Builder;
    std::unique_ptr<llvm::Module> TheModule;
    std::map<std::string, llvm::Value*> NamedValues;
    std::unique_ptr<llvm::DIBuilder> DebugBuilder;
    llvm::DICompileUnit* DebugCompileUnit = nullptr;
    llvm::DIFile* DebugFile = nullptr;
    bool DebugEnabled = false;

    CodegenContext()
        : OwnedContext(std::make_unique<llvm::LLVMContext>()),
          TheContext(*OwnedContext),
          Builder(TheContext) {
        // Enable Fast-Math flags by default for performance in simulations
        llvm::FastMathFlags FMF;
        FMF.setFast();
        Builder.setFastMathFlags(FMF);
    }
};

struct TypedValue {
    llvm::Value* Val;
    FluxType Type;

    TypedValue(llvm::Value* V = nullptr, FluxType T = TypeKind::Double)
        : Val(V), Type(T) {}
    
    operator llvm::Value*() const { return Val; }
};

class ExprAST {
public:
    virtual ~ExprAST() = default;
    virtual TypedValue codegen(CodegenContext& context) = 0;
};

class NumberExprAST : public ExprAST {
    double Val;
public:
    NumberExprAST(double Val) : Val(Val) {}
    TypedValue codegen(CodegenContext& context) override;
    double getValue() const { return Val; }
};

class ComplexExprAST : public ExprAST {
    double Real;
    double Imag;
public:
    ComplexExprAST(double Real, double Imag) : Real(Real), Imag(Imag) {}
    TypedValue codegen(CodegenContext& context) override;
    double getReal() const { return Real; }
    double getImag() const { return Imag; }
};

class StringExprAST : public ExprAST {
    std::string Val;
public:
    StringExprAST(const std::string& Val) : Val(Val) {}
    TypedValue codegen(CodegenContext& context) override;
    const std::string& getValue() const { return Val; }
};

class ImportExprAST : public ExprAST {
    std::string ModuleName;
    std::string VersionSpec;      // Version specification (e.g., "1.0.0", ">=1.0.0")
    std::string Alias;            // Optional namespace alias
    std::vector<std::string> Symbols;  // Specific symbols to import
    
public:
    ImportExprAST(const std::string& ModuleName, 
                  const std::string& VersionSpec = "",
                  const std::string& Alias = "",
                  const std::vector<std::string>& Symbols = {})
        : ModuleName(ModuleName), VersionSpec(VersionSpec), Alias(Alias), Symbols(Symbols) {}
    TypedValue codegen(CodegenContext& context) override;
    const std::string& getModuleName() const { return ModuleName; }
    const std::string& getVersionSpec() const { return VersionSpec; }
    const std::string& getAlias() const { return Alias; }
    const std::vector<std::string>& getSymbols() const { return Symbols; }
};

class VariableExprAST : public ExprAST {
    std::string Name;
public:
    VariableExprAST(const std::string& Name) : Name(Name) {}
    TypedValue codegen(CodegenContext& context) override;
    const std::string& getName() const { return Name; }
};

class MemberExprAST : public ExprAST {
    std::unique_ptr<ExprAST> Object;
    std::string MemberName;
public:
    MemberExprAST(std::unique_ptr<ExprAST> Object, const std::string& MemberName)
        : Object(std::move(Object)), MemberName(MemberName) {}
    TypedValue codegen(CodegenContext& context) override;
    const ExprAST* getObject() const { return Object.get(); }
    const std::string& getMemberName() const { return MemberName; }
};

class BinaryExprAST : public ExprAST {
    int Op; // Changed to int to support multi-character operators
    std::unique_ptr<ExprAST> LHS, RHS;
public:
    BinaryExprAST(int Op, std::unique_ptr<ExprAST> LHS, std::unique_ptr<ExprAST> RHS)
        : Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}
    TypedValue codegen(CodegenContext& context) override;
    int getOp() const { return Op; }
    const ExprAST* getLHS() const { return LHS.get(); }
    const ExprAST* getRHS() const { return RHS.get(); }
};

class UnaryExprAST : public ExprAST {
    int Op; // Changed to int to support multi-character operators
    std::unique_ptr<ExprAST> Operand;
public:
    UnaryExprAST(int Op, std::unique_ptr<ExprAST> Operand)
        : Op(Op), Operand(std::move(Operand)) {}
    TypedValue codegen(CodegenContext& context) override;
    int getOp() const { return Op; }
    const ExprAST* getOperand() const { return Operand.get(); }
};

class TransposeExprAST : public ExprAST {
    std::unique_ptr<ExprAST> Operand;
public:
    TransposeExprAST(std::unique_ptr<ExprAST> Operand)
        : Operand(std::move(Operand)) {}
    TypedValue codegen(CodegenContext& context) override;
    const ExprAST* getOperand() const { return Operand.get(); }
};

class CallExprAST : public ExprAST {
    std::string Callee;
    std::vector<std::unique_ptr<ExprAST>> Args;
public:
    CallExprAST(const std::string& Callee, std::vector<std::unique_ptr<ExprAST>> Args)
        : Callee(Callee), Args(std::move(Args)) {}
    TypedValue codegen(CodegenContext& context) override;
    const std::string& getCallee() const { return Callee; }
    const std::vector<std::unique_ptr<ExprAST>>& getArgs() const { return Args; }
};

class AssignExprAST : public ExprAST {
    std::unique_ptr<ExprAST> LHS;
    std::unique_ptr<ExprAST> Val;
    int Op;
public:
    AssignExprAST(std::unique_ptr<ExprAST> LHS, std::unique_ptr<ExprAST> Val, int Op = 0)
        : LHS(std::move(LHS)), Val(std::move(Val)), Op(Op) {}
    TypedValue codegen(CodegenContext& context) override;
    const ExprAST* getLHS() const { return LHS.get(); }
    const ExprAST* getValueExpr() const { return Val.get(); }
    int getAssignmentOp() const { return Op; }
};

class IfExprAST : public ExprAST {
    std::unique_ptr<ExprAST> Cond, Then, Else;
public:
    IfExprAST(std::unique_ptr<ExprAST> Cond, std::unique_ptr<ExprAST> Then,
              std::unique_ptr<ExprAST> Else)
        : Cond(std::move(Cond)), Then(std::move(Then)), Else(std::move(Else)) {}
    TypedValue codegen(CodegenContext& context) override;
    const ExprAST* getCond() const { return Cond.get(); }
    const ExprAST* getThen() const { return Then.get(); }
    const ExprAST* getElse() const { return Else.get(); }
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
    TypedValue codegen(CodegenContext& context) override;
    const std::string& getVarName() const { return VarName; }
    const ExprAST* getStart() const { return Start.get(); }
    const ExprAST* getEnd() const { return End.get(); }
    const ExprAST* getStep() const { return Step.get(); }
    const ExprAST* getBody() const { return Body.get(); }
};

class ArrayExprAST : public ExprAST {
    std::vector<std::unique_ptr<ExprAST>> Elements;
public:
    ArrayExprAST(std::vector<std::unique_ptr<ExprAST>> Elements)
        : Elements(std::move(Elements)) {}
    TypedValue codegen(CodegenContext& context) override;
    const std::vector<std::unique_ptr<ExprAST>>& getElements() const { return Elements; }
};

class WhileExprAST : public ExprAST {
    std::unique_ptr<ExprAST> Cond, Body;
public:
    WhileExprAST(std::unique_ptr<ExprAST> Cond, std::unique_ptr<ExprAST> Body)
        : Cond(std::move(Cond)), Body(std::move(Body)) {}
    TypedValue codegen(CodegenContext& context) override;
    const ExprAST* getCond() const { return Cond.get(); }
    const ExprAST* getBody() const { return Body.get(); }
};

class LetExprAST : public ExprAST {
    std::string VarName;
    FluxType Type;
    std::unique_ptr<ExprAST> Init;
    std::unique_ptr<ExprAST> Body;
public:
    LetExprAST(const std::string& VarName, FluxType Type, std::unique_ptr<ExprAST> Init, std::unique_ptr<ExprAST> Body)
        : VarName(VarName), Type(Type), Init(std::move(Init)), Body(std::move(Body)) {}
    TypedValue codegen(CodegenContext& context) override;
    const std::string& getVarName() const { return VarName; }
    const FluxType& getDeclaredType() const { return Type; }
    const ExprAST* getInit() const { return Init.get(); }
    const ExprAST* getBody() const { return Body.get(); }
};

class LambdaExprAST : public ExprAST {
    std::vector<std::string> Args;
    std::unique_ptr<ExprAST> Body;
public:
    LambdaExprAST(std::vector<std::string> Args, std::unique_ptr<ExprAST> Body)
        : Args(std::move(Args)), Body(std::move(Body)) {}
    TypedValue codegen(CodegenContext& context) override;
    const std::vector<std::string>& getArgs() const { return Args; }
    const ExprAST* getBody() const { return Body.get(); }
};

class VectorExprAST : public ExprAST {
    std::vector<std::unique_ptr<ExprAST>> Elements;
public:
    VectorExprAST(std::vector<std::unique_ptr<ExprAST>> Elements)
        : Elements(std::move(Elements)) {}
    TypedValue codegen(CodegenContext& context) override;
    const std::vector<std::unique_ptr<ExprAST>>& getElements() const { return Elements; }
};

class MatrixExprAST : public ExprAST {
    std::vector<std::vector<std::unique_ptr<ExprAST>>> Rows;
    int NumRows;
    int NumCols;
public:
    MatrixExprAST(std::vector<std::vector<std::unique_ptr<ExprAST>>> Rows, int rows, int cols)
        : Rows(std::move(Rows)), NumRows(rows), NumCols(cols) {}
    TypedValue codegen(CodegenContext& context) override;
    const std::vector<std::vector<std::unique_ptr<ExprAST>>>& getRows() const { return Rows; }
    int getNumRows() const { return NumRows; }
    int getNumCols() const { return NumCols; }
};

class VoltageExprAST : public ExprAST {
    std::string NodeName;
public:
    VoltageExprAST(const std::string& NodeName) : NodeName(NodeName) {}
    TypedValue codegen(CodegenContext& context) override;
    const std::string& getNodeName() const { return NodeName; }
};

class CurrentExprAST : public ExprAST {
    std::string BranchName;
public:
    CurrentExprAST(const std::string& BranchName) : BranchName(BranchName) {}
    TypedValue codegen(CodegenContext& context) override;
    const std::string& getBranchName() const { return BranchName; }
};

class ParameterExprAST : public ExprAST {
    std::string ParamName;
public:
    ParameterExprAST(const std::string& ParamName) : ParamName(ParamName) {}
    TypedValue codegen(CodegenContext& context) override;
    const std::string& getParamName() const { return ParamName; }
};

class IndexExprAST : public ExprAST {
    std::unique_ptr<ExprAST> Array;
    std::unique_ptr<ExprAST> RowIndex;  // Can be a RangeExprAST for slicing
    std::unique_ptr<ExprAST> ColIndex;  // Optional, for matrix column indexing
    bool IsMatrixIndex;                 // True if accessing m(row, col)
public:
    IndexExprAST(std::unique_ptr<ExprAST> Array, std::unique_ptr<ExprAST> RowIdx)
        : Array(std::move(Array)), RowIndex(std::move(RowIdx)), ColIndex(nullptr), IsMatrixIndex(false) {}

    IndexExprAST(std::unique_ptr<ExprAST> Array, std::unique_ptr<ExprAST> RowIndex,
                 std::unique_ptr<ExprAST> ColIndex)
        : Array(std::move(Array)), RowIndex(std::move(RowIndex)), ColIndex(std::move(ColIndex)),
          IsMatrixIndex(true) {}

    TypedValue codegen(CodegenContext& context) override;
    const ExprAST* getArray() const { return Array.get(); }
    const ExprAST* getRowIndex() const { return RowIndex.get(); }
    const ExprAST* getColIndex() const { return ColIndex.get(); }
    bool isMatrixIndex() const { return IsMatrixIndex; }
};

// Range expression for slicing: start:end or start:step:end
class RangeExprAST : public ExprAST {
    std::unique_ptr<ExprAST> Start;
    std::unique_ptr<ExprAST> Step;   // Optional (defaults to 1)
    std::unique_ptr<ExprAST> End;
public:
    RangeExprAST(std::unique_ptr<ExprAST> Start, std::unique_ptr<ExprAST> End)
        : Start(std::move(Start)), Step(nullptr), End(std::move(End)) {}
    
    RangeExprAST(std::unique_ptr<ExprAST> Start, std::unique_ptr<ExprAST> Step, std::unique_ptr<ExprAST> End)
        : Start(std::move(Start)), Step(std::move(Step)), End(std::move(End)) {}
    
    TypedValue codegen(CodegenContext& context) override;
    const ExprAST* getStart() const { return Start.get(); }
    const ExprAST* getStep() const { return Step.get(); }
    const ExprAST* getEnd() const { return End.get(); }
};

class BlockExprAST : public ExprAST {
    std::vector<std::unique_ptr<ExprAST>> Statements;
public:
    BlockExprAST(std::vector<std::unique_ptr<ExprAST>> Statements)
        : Statements(std::move(Statements)) {}
    TypedValue codegen(CodegenContext& context) override;
    const std::vector<std::unique_ptr<ExprAST>>& getStatements() const { return Statements; }
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
    const ExprAST* getBody() const { return Body.get(); }
};

} // namespace Flux

#endif // FLUX_COMPILER_AST_H
