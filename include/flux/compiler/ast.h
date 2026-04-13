/* Copyright 2026 Janada Sroor
 SPDX-License-Identifier: Apache-2.0
 http://www.apache.org/licenses/LICENSE-2.0
 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at
     http://www.apache.org/licenses/LICENSE-2.0
 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License. */

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

// Unit dimensions for dimensional analysis
struct UnitDimensions {
    int8_t mass = 0;           // kg
    int8_t length = 0;         // m
    int8_t time = 0;           // s
    int8_t current = 0;        // A
    int8_t temperature = 0;    // K
    int8_t amount = 0;         // mol
    int8_t luminous = 0;       // cd
    
    bool isDimensionless() const {
        return mass == 0 && length == 0 && time == 0 && 
               current == 0 && temperature == 0 && amount == 0 && luminous == 0;
    }
    
    bool operator==(const UnitDimensions& other) const {
        return mass == other.mass && length == other.length && 
               time == other.time && current == other.current &&
               temperature == other.temperature && amount == other.amount &&
               luminous == other.luminous;
    }
    
    bool operator!=(const UnitDimensions& other) const {
        return !(*this == other);
    }
    
    UnitDimensions operator*(const UnitDimensions& other) const {
        return {
            static_cast<int8_t>(mass + other.mass),
            static_cast<int8_t>(length + other.length),
            static_cast<int8_t>(time + other.time),
            static_cast<int8_t>(current + other.current),
            static_cast<int8_t>(temperature + other.temperature),
            static_cast<int8_t>(amount + other.amount),
            static_cast<int8_t>(luminous + other.luminous)
        };
    }
    
    UnitDimensions operator/(const UnitDimensions& other) const {
        return {
            static_cast<int8_t>(mass - other.mass),
            static_cast<int8_t>(length - other.length),
            static_cast<int8_t>(time - other.time),
            static_cast<int8_t>(current - other.current),
            static_cast<int8_t>(temperature - other.temperature),
            static_cast<int8_t>(amount - other.amount),
            static_cast<int8_t>(luminous - other.luminous)
        };
    }
    
    std::string toString() const;
};

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
    Vector,   // Vector { double*, i32 }
    Symbolic  // Symbolic expression handle (double/uintptr_t)
};

class FluxType {
public:
    TypeKind Kind;
    UnitDimensions Dimensions;
    
    FluxType(TypeKind K = TypeKind::Double, UnitDimensions D = {}) 
        : Kind(K), Dimensions(D) {}
    
    llvm::Type* getLLVMType(llvm::LLVMContext& Context) const {
        switch (Kind) {
            case TypeKind::Float:
                return llvm::Type::getFloatTy(Context);
            case TypeKind::Int:
                return llvm::Type::getInt32Ty(Context);
            case TypeKind::Void:
                return llvm::Type::getVoidTy(Context);
            case TypeKind::Complex:
                // Complex numbers use native LLVM <2 x double> vector (SSE2 compatible)
                return llvm::VectorType::get(llvm::Type::getDoubleTy(Context), 2, false);
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
        // Complex is now <2 x double> (vector, not struct)
        if (T->isVectorTy()) {
            auto* VT = llvm::cast<llvm::VectorType>(T);
            if (VT->getElementCount().isScalar() &&
                VT->getElementCount().getKnownMinValue() == 2 &&
                VT->getElementType()->isDoubleTy())
                return FluxType(TypeKind::Complex);
        }
        if (T->isStructTy()) {
            if (T->getStructNumElements() == 3) return FluxType(TypeKind::Matrix);
            if (T->getStructNumElements() == 2) {
                // Vector is { double*, i32 }
                if (T->getStructElementType(0)->isPointerTy()) return FluxType(TypeKind::Vector);
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
    std::unique_ptr<llvm::Module> OwnedModule;
    llvm::Module* TheModule = nullptr;
    std::map<std::string, llvm::Value*> NamedValues;
    std::map<std::string, FluxType> NamedTypes;
    std::unique_ptr<llvm::DIBuilder> DebugBuilder;
    llvm::DICompileUnit* DebugCompileUnit = nullptr;
    llvm::DIFile* DebugFile = nullptr;
    bool DebugEnabled = false;
    std::vector<llvm::DIScope*> LexicalBlocks;
    
    // Control flow context for break/continue
    llvm::BasicBlock* CurrentLoopEnd = nullptr;      // Target for break in loops
    llvm::BasicBlock* CurrentLoopCont = nullptr;     // Target for continue in loops
    llvm::BasicBlock* CurrentSwitchEnd = nullptr;    // Target for break in switch

    // Exception handling context
    llvm::BasicBlock* CurrentCatchBB = nullptr;      // Target for throw
    llvm::Value* CurrentExceptionAlloca = nullptr;   // Where to store the thrown value
    
    // Generator context
    llvm::Value* GeneratorStateAlloca = nullptr;     // Struct containing state index and locals
    std::vector<llvm::BasicBlock*> YieldTargets;     // Blocks to resume from
    llvm::BasicBlock* GeneratorDispatcherBB = nullptr; // Entry dispatcher

    CodegenContext()
        : OwnedContext(std::make_unique<llvm::LLVMContext>()),
          TheContext(*OwnedContext),
          Builder(TheContext) {
        llvm::FastMathFlags FMF;
        FMF.setFast();
        Builder.setFastMathFlags(FMF);
    }

    CodegenContext(llvm::LLVMContext& Ctx, llvm::Module* Mod)
        : TheContext(Ctx),
          Builder(Ctx),
          TheModule(Mod) {
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
    int Line = 0;
    int Col = 0;
public:
    virtual ~ExprAST() = default;
    virtual TypedValue codegen(CodegenContext& context) = 0;
    void setLocation(int L, int C) { Line = L; Col = C; }
    int getLine() const { return Line; }
    int getCol() const { return Col; }
    virtual bool containsYield() const { return false; }
};

class NumberExprAST : public ExprAST {
    double Val;
    std::string Unit;
public:
    NumberExprAST(double Val, std::string Unit = "") : Val(Val), Unit(std::move(Unit)) {}
    TypedValue codegen(CodegenContext& context) override;
    double getValue() const { return Val; }
    const std::string& getUnit() const { return Unit; }
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

// Debug statement AST
class DebugStmtAST : public ExprAST {
    std::string CircuitFile;
    std::string Symptom;
    std::map<std::string, std::string> Options;
public:
    DebugStmtAST(const std::string& circuit, const std::string& symptom,
                 const std::map<std::string, std::string>& opts)
        : CircuitFile(circuit), Symptom(symptom), Options(opts) {}
    TypedValue codegen(CodegenContext& context) override;
};

// Sensitivity analysis AST
class SensitivityStmtAST : public ExprAST {
    std::string OutputVar;
    std::vector<std::string> Params;
    std::string CircuitFile;
public:
    SensitivityStmtAST(const std::string& output, const std::vector<std::string>& params,
                       const std::string& circuit = "")
        : OutputVar(output), Params(params), CircuitFile(circuit) {}
    TypedValue codegen(CodegenContext& context) override;
};

// Ask question AST
class AskExprAST : public ExprAST {
    std::string Question;
public:
    explicit AskExprAST(const std::string& question) : Question(question) {}
    TypedValue codegen(CodegenContext& context) override;
};

// Explain circuit AST
class ExplainExprAST : public ExprAST {
    std::string CircuitFile;
    std::string Aspect;
public:
    ExplainExprAST(const std::string& circuit, const std::string& aspect = "")
        : CircuitFile(circuit), Aspect(aspect) {}
    TypedValue codegen(CodegenContext& context) override;
};

// Component substitution AST
class SubstituteStmtAST : public ExprAST {
    std::string PartNumber;
    std::vector<std::string> Options;
public:
    SubstituteStmtAST(const std::string& part, const std::vector<std::string>& opts)
        : PartNumber(part), Options(opts) {}
    TypedValue codegen(CodegenContext& context) override;
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
    bool containsYield() const override {
        return Object->containsYield();
    }
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
    bool containsYield() const override {
        return LHS->containsYield() || RHS->containsYield();
    }
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
    bool containsYield() const override {
        return Operand->containsYield();
    }
};

class TransposeExprAST : public ExprAST {
    std::unique_ptr<ExprAST> Operand;
public:
    TransposeExprAST(std::unique_ptr<ExprAST> Operand)
        : Operand(std::move(Operand)) {}
    TypedValue codegen(CodegenContext& context) override;
    const ExprAST* getOperand() const { return Operand.get(); }
    bool containsYield() const override {
        return Operand->containsYield();
    }
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
    bool containsYield() const override {
        for (const auto& Arg : Args) {
            if (Arg->containsYield()) return true;
        }
        return false;
    }
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
    bool containsYield() const override {
        return LHS->containsYield() || Val->containsYield();
    }
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
    bool containsYield() const override {
        if (Cond->containsYield()) return true;
        if (Then->containsYield()) return true;
        if (Else && Else->containsYield()) return true;
        return false;
    }
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
    bool containsYield() const override {
        if (Start->containsYield()) return true;
        if (End->containsYield()) return true;
        if (Step && Step->containsYield()) return true;
        if (Body->containsYield()) return true;
        return false;
    }
};

class ArrayExprAST : public ExprAST {
    std::vector<std::unique_ptr<ExprAST>> Elements;
public:
    ArrayExprAST(std::vector<std::unique_ptr<ExprAST>> Elements)
        : Elements(std::move(Elements)) {}
    TypedValue codegen(CodegenContext& context) override;
    const std::vector<std::unique_ptr<ExprAST>>& getElements() const { return Elements; }
    bool containsYield() const override {
        for (const auto& Elem : Elements) {
            if (Elem->containsYield()) return true;
        }
        return false;
    }
};

class WhileExprAST : public ExprAST {
    std::unique_ptr<ExprAST> Cond, Body;
public:
    WhileExprAST(std::unique_ptr<ExprAST> Cond, std::unique_ptr<ExprAST> Body)
        : Cond(std::move(Cond)), Body(std::move(Body)) {}
    TypedValue codegen(CodegenContext& context) override;
    const ExprAST* getCond() const { return Cond.get(); }
    const ExprAST* getBody() const { return Body.get(); }
    bool containsYield() const override {
        return Cond->containsYield() || Body->containsYield();
    }
};

class DoWhileExprAST : public ExprAST {
    std::unique_ptr<ExprAST> Body, Cond;
public:
    DoWhileExprAST(std::unique_ptr<ExprAST> Body, std::unique_ptr<ExprAST> Cond)
        : Body(std::move(Body)), Cond(std::move(Cond)) {}
    TypedValue codegen(CodegenContext& context) override;
    const ExprAST* getBody() const { return Body.get(); }
    const ExprAST* getCond() const { return Cond.get(); }
    bool containsYield() const override {
        return Body->containsYield() || Cond->containsYield();
    }
};

// ============================================================================
// Statement-based Control Flow (void-returning, no PHI nodes)
// Syntax: if (cond) { stmts } else { stmts }
//         for (init; cond; step) { stmts }
//         while (cond) { stmts }
// ============================================================================

class IfStmtAST : public ExprAST {
    std::unique_ptr<ExprAST> Cond;
    std::vector<std::unique_ptr<ExprAST>> ThenBody;
    std::vector<std::unique_ptr<ExprAST>> ElseBody;
public:
    IfStmtAST(std::unique_ptr<ExprAST> Cond,
              std::vector<std::unique_ptr<ExprAST>> ThenBody,
              std::vector<std::unique_ptr<ExprAST>> ElseBody)
        : Cond(std::move(Cond)), ThenBody(std::move(ThenBody)), ElseBody(std::move(ElseBody)) {}
    TypedValue codegen(CodegenContext& context) override;
    const ExprAST* getCond() const { return Cond.get(); }
    const std::vector<std::unique_ptr<ExprAST>>& getThenBody() const { return ThenBody; }
    const std::vector<std::unique_ptr<ExprAST>>& getElseBody() const { return ElseBody; }
    bool containsYield() const override {
        if (Cond->containsYield()) return true;
        for (const auto& Stmt : ThenBody) {
            if (Stmt->containsYield()) return true;
        }
        for (const auto& Stmt : ElseBody) {
            if (Stmt->containsYield()) return true;
        }
        return false;
    }
};

class ForStmtAST : public ExprAST {
    std::unique_ptr<ExprAST> Init;
    std::unique_ptr<ExprAST> Cond;
    std::unique_ptr<ExprAST> Step;
    std::vector<std::unique_ptr<ExprAST>> Body;
public:
    ForStmtAST(std::unique_ptr<ExprAST> Init, std::unique_ptr<ExprAST> Cond,
               std::unique_ptr<ExprAST> Step, std::vector<std::unique_ptr<ExprAST>> Body)
        : Init(std::move(Init)), Cond(std::move(Cond)), Step(std::move(Step)), Body(std::move(Body)) {}
    TypedValue codegen(CodegenContext& context) override;
    const ExprAST* getInit() const { return Init.get(); }
    const ExprAST* getCond() const { return Cond.get(); }
    const ExprAST* getStep() const { return Step.get(); }
    const std::vector<std::unique_ptr<ExprAST>>& getBody() const { return Body; }
    bool containsYield() const override {
        if (Init && Init->containsYield()) return true;
        if (Cond && Cond->containsYield()) return true;
        if (Step && Step->containsYield()) return true;
        for (const auto& Stmt : Body) {
            if (Stmt->containsYield()) return true;
        }
        return false;
    }
};

class WhileStmtAST : public ExprAST {
    std::unique_ptr<ExprAST> Cond;
    std::vector<std::unique_ptr<ExprAST>> Body;
public:
    WhileStmtAST(std::unique_ptr<ExprAST> Cond, std::vector<std::unique_ptr<ExprAST>> Body)
        : Cond(std::move(Cond)), Body(std::move(Body)) {}
    TypedValue codegen(CodegenContext& context) override;
    const ExprAST* getCond() const { return Cond.get(); }
    const std::vector<std::unique_ptr<ExprAST>>& getBody() const { return Body; }
    bool containsYield() const override {
        if (Cond && Cond->containsYield()) return true;
        for (const auto& Stmt : Body) {
            if (Stmt->containsYield()) return true;
        }
        return false;
    }
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
    bool containsYield() const override {
        return (Init && Init->containsYield()) || (Body && Body->containsYield());
    }
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
    bool containsYield() const override {
        return Body && Body->containsYield();
    }
};

class VectorExprAST : public ExprAST {
    std::vector<std::unique_ptr<ExprAST>> Elements;
public:
    VectorExprAST(std::vector<std::unique_ptr<ExprAST>> Elements)
        : Elements(std::move(Elements)) {}
    TypedValue codegen(CodegenContext& context) override;
    const std::vector<std::unique_ptr<ExprAST>>& getElements() const { return Elements; }
    bool containsYield() const override {
        for (const auto& Elem : Elements) {
            if (Elem->containsYield()) return true;
        }
        return false;
    }
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
    bool containsYield() const override {
        for (const auto& Row : Rows) {
            for (const auto& Elem : Row) {
                if (Elem->containsYield()) return true;
            }
        }
        return false;
    }
};

class SymbolicExprAST : public ExprAST {
    std::unique_ptr<ExprAST> Expression;
public:
    SymbolicExprAST(std::unique_ptr<ExprAST> Expr) : Expression(std::move(Expr)) {}
    TypedValue codegen(CodegenContext& context) override;
    const ExprAST* getExpression() const { return Expression.get(); }
    bool containsYield() const override {
        return Expression && Expression->containsYield();
    }
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
    bool containsYield() const override { return false; }
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
    bool containsYield() const override {
        if (Array && Array->containsYield()) return true;
        if (RowIndex && RowIndex->containsYield()) return true;
        if (ColIndex && ColIndex->containsYield()) return true;
        return false;
    }
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
    bool containsYield() const override {
        if (Start && Start->containsYield()) return true;
        if (Step && Step->containsYield()) return true;
        if (End && End->containsYield()) return true;
        return false;
    }
};

class BlockExprAST : public ExprAST {
    std::vector<std::unique_ptr<ExprAST>> Statements;
public:
    BlockExprAST(std::vector<std::unique_ptr<ExprAST>> Statements)
        : Statements(std::move(Statements)) {}
    TypedValue codegen(CodegenContext& context) override;
    const std::vector<std::unique_ptr<ExprAST>>& getStatements() const { return Statements; }
    bool containsYield() const override {
        for (const auto& Stmt : Statements) {
            if (Stmt->containsYield()) return true;
        }
        return false;
    }
};

// Switch statement AST
class CaseClauseAST {
    std::unique_ptr<ExprAST> Value;
    std::vector<std::unique_ptr<ExprAST>> Body;
public:
    CaseClauseAST(std::unique_ptr<ExprAST> Value, std::vector<std::unique_ptr<ExprAST>> Body)
        : Value(std::move(Value)), Body(std::move(Body)) {}
    ExprAST* getValue() const { return Value.get(); }
    const std::vector<std::unique_ptr<ExprAST>>& getBody() const { return Body; }
};

class SwitchExprAST : public ExprAST {
    std::unique_ptr<ExprAST> Condition;
    std::vector<CaseClauseAST> Cases;
    std::vector<std::unique_ptr<ExprAST>> DefaultBody;
public:
    SwitchExprAST(std::unique_ptr<ExprAST> Cond, std::vector<CaseClauseAST> Cases,
                  std::vector<std::unique_ptr<ExprAST>> DefaultBody)
        : Condition(std::move(Cond)), Cases(std::move(Cases)), DefaultBody(std::move(DefaultBody)) {}
    TypedValue codegen(CodegenContext& context) override;
    ExprAST* getCondition() const { return Condition.get(); }
    const std::vector<CaseClauseAST>& getCases() const { return Cases; }
    const std::vector<std::unique_ptr<ExprAST>>& getDefaultBody() const { return DefaultBody; }
    bool containsYield() const override {
        if (Condition && Condition->containsYield()) return true;
        for (const auto& Clause : Cases) {
            if (Clause.getValue() && Clause.getValue()->containsYield()) return true;
            for (const auto& Stmt : Clause.getBody()) {
                if (Stmt->containsYield()) return true;
            }
        }
        for (const auto& Stmt : DefaultBody) {
            if (Stmt->containsYield()) return true;
        }
        return false;
    }
};

// Break and Continue statements
class BreakExprAST : public ExprAST {
public:
    BreakExprAST() {}
    TypedValue codegen(CodegenContext& context) override;
};

class ContinueExprAST : public ExprAST {
public:
    ContinueExprAST() {}
    TypedValue codegen(CodegenContext& context) override;
};

class ReturnExprAST : public ExprAST {
    std::unique_ptr<ExprAST> Val;
public:
    ReturnExprAST(std::unique_ptr<ExprAST> Val) : Val(std::move(Val)) {}
    TypedValue codegen(CodegenContext& context) override;
    const ExprAST* getVal() const { return Val.get(); }
    bool containsYield() const override {
        return Val && Val->containsYield();
    }
};

// Try/Catch/Finally
class TryCatchExprAST : public ExprAST {
    std::unique_ptr<ExprAST> TryBody;
    std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> CatchClauses;  // (exception_var, handler)
    std::unique_ptr<ExprAST> FinallyBody;
public:
    TryCatchExprAST(std::unique_ptr<ExprAST> TryBody)
        : TryBody(std::move(TryBody)) {}
    TypedValue codegen(CodegenContext& context) override;
    void addCatch(const std::string& var, std::unique_ptr<ExprAST> handler);
    void setFinally(std::unique_ptr<ExprAST> body);
    bool containsYield() const override {
        if (TryBody->containsYield()) return true;
        for (const auto& C : CatchClauses) if (C.second->containsYield()) return true;
        if (FinallyBody && FinallyBody->containsYield()) return true;
        return false;
    }
};

// Throw exception
class ThrowExprAST : public ExprAST {
    std::unique_ptr<ExprAST> Exception;
public:
    ThrowExprAST(std::unique_ptr<ExprAST> Exception)
        : Exception(std::move(Exception)) {}
    TypedValue codegen(CodegenContext& context) override;
    bool containsYield() const override { return Exception->containsYield(); }
};

// Assert
class AssertExprAST : public ExprAST {
    std::unique_ptr<ExprAST> Condition;
    std::string Message;
public:
    AssertExprAST(std::unique_ptr<ExprAST> Cond, std::string Msg)
        : Condition(std::move(Cond)), Message(std::move(Msg)) {}
    TypedValue codegen(CodegenContext& context) override;
};

// Yield (generator)
class YieldExprAST : public ExprAST {
    std::unique_ptr<ExprAST> Value;
public:
    YieldExprAST(std::unique_ptr<ExprAST> Value)
        : Value(std::move(Value)) {}
    TypedValue codegen(CodegenContext& context) override;
    bool containsYield() const override { return true; }
};

// Corner case analysis
class CornerExprAST : public ExprAST {
    std::string Variable;
    std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> Cases;
public:
    CornerExprAST(std::string Var)
        : Variable(std::move(Var)) {}
    TypedValue codegen(CodegenContext& context) override;
    void addCase(const std::string& name, std::unique_ptr<ExprAST> expr);
    bool containsYield() const override {
        for (const auto& C : Cases) if (C.second->containsYield()) return true;
        return false;
    }
};

// Pattern matching
class MatchExprAST : public ExprAST {
    std::unique_ptr<ExprAST> Value;
    std::vector<std::pair<std::unique_ptr<ExprAST>, std::unique_ptr<ExprAST>>> Arms;  // (pattern, result)
    std::unique_ptr<ExprAST> DefaultArm;
public:
    MatchExprAST(std::unique_ptr<ExprAST> Value)
        : Value(std::move(Value)) {}
    TypedValue codegen(CodegenContext& context) override;
    void addArm(std::unique_ptr<ExprAST> pattern, std::unique_ptr<ExprAST> result);
    void setDefault(std::unique_ptr<ExprAST> arm);
    bool containsYield() const override {
        if (Value->containsYield()) return true;
        for (const auto& A : Arms) if (A.second->containsYield()) return true;
        if (DefaultArm && DefaultArm->containsYield()) return true;
        return false;
    }
};

// Foreach loop
class ForeachExprAST : public ExprAST {
    std::string VarName;
    std::unique_ptr<ExprAST> Iterable;
    std::unique_ptr<ExprAST> Body;
public:
    ForeachExprAST(std::string Var, std::unique_ptr<ExprAST> Iterable, std::unique_ptr<ExprAST> Body)
        : VarName(std::move(Var)), Iterable(std::move(Iterable)), Body(std::move(Body)) {}
    TypedValue codegen(CodegenContext& context) override;
    bool containsYield() const override { return Body->containsYield(); }
};

// Repeat-Until loop
class RepeatUntilExprAST : public ExprAST {
    std::unique_ptr<ExprAST> Body;
    std::unique_ptr<ExprAST> Condition;
public:
    RepeatUntilExprAST(std::unique_ptr<ExprAST> Body, std::unique_ptr<ExprAST> Cond)
        : Body(std::move(Body)), Condition(std::move(Cond)) {}
    TypedValue codegen(CodegenContext& context) override;
    bool containsYield() const override { return Body->containsYield() || Condition->containsYield(); }
};

// Parallel for loop
class ParallelForExprAST : public ExprAST {
    std::string VarName;
    std::unique_ptr<ExprAST> Start;
    std::unique_ptr<ExprAST> End;
    std::unique_ptr<ExprAST> Body;
    int ChunkSize;  // Work chunk size for load balancing
public:
    ParallelForExprAST(std::string Var, std::unique_ptr<ExprAST> Start,
                       std::unique_ptr<ExprAST> End, std::unique_ptr<ExprAST> Body,
                       int ChunkSize = 1)
        : VarName(std::move(Var)), Start(std::move(Start)), End(std::move(End)),
          Body(std::move(Body)), ChunkSize(ChunkSize) {}
    TypedValue codegen(CodegenContext& context) override;
    bool containsYield() const override { return Body->containsYield(); }
};

class PrototypeAST {
    std::string Name;
    std::vector<std::pair<std::string, FluxType>> Args;  // Name-Type pairs
    FluxType ReturnType;
    int Line = 0;
    bool IsGenerator = false;
public:
    PrototypeAST(const std::string& Name, std::vector<std::pair<std::string, FluxType>> Args, FluxType RetType = FluxType(TypeKind::Double))
        : Name(Name), Args(std::move(Args)), ReturnType(RetType) {}
    llvm::Function* codegen(CodegenContext& context);
    const std::string& getName() const { return Name; }
    const std::vector<std::pair<std::string, FluxType>>& getArgs() const { return Args; }
    const FluxType& getReturnType() const { return ReturnType; }
    
    void setLocation(int L) { Line = L; }
    int getLine() const { return Line; }
    
    void setGenerator(bool G) { IsGenerator = G; }
    bool isGenerator() const { return IsGenerator; }
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

// Debug info helper
void emitLocation(ExprAST* ast, CodegenContext& context);

// ============================================================================
// SPICE Time-Domain Simulation AST Nodes
// ============================================================================

// Built-in variable: time, dt, temp
class BuiltinVarExprAST : public ExprAST {
    std::string Name;
public:
    explicit BuiltinVarExprAST(const std::string& Name) : Name(Name) {}
    TypedValue codegen(CodegenContext& context) override;
    const std::string& getName() const { return Name; }
};

// update(t, inputs) or update(t, inputs, outputs, state) function declaration
class UpdateFuncAST : public ExprAST {
    std::string TimeVar;
    std::string InputsVar;
    std::string OutputsVar;   // Optional: output array name
    std::string StateVar;     // Optional: state pointer name
    std::unique_ptr<ExprAST> Body;
public:
    UpdateFuncAST(const std::string& TimeVar, const std::string& InputsVar, std::unique_ptr<ExprAST> Body)
        : TimeVar(TimeVar), InputsVar(InputsVar), Body(std::move(Body)) {}
    UpdateFuncAST(const std::string& TimeVar, const std::string& InputsVar,
                  const std::string& OutputsVar, const std::string& StateVar,
                  std::unique_ptr<ExprAST> Body)
        : TimeVar(TimeVar), InputsVar(InputsVar), OutputsVar(OutputsVar),
          StateVar(StateVar), Body(std::move(Body)) {}
    TypedValue codegen(CodegenContext& context) override;
    const std::string& getTimeVar() const { return TimeVar; }
    const std::string& getInputsVar() const { return InputsVar; }
    const std::string& getOutputsVar() const { return OutputsVar; }
    const std::string& getStateVar() const { return StateVar; }
    bool hasOutputs() const { return !OutputsVar.empty(); }
    bool hasState() const { return !StateVar.empty(); }
    const ExprAST* getBody() const { return Body.get(); }
};

// ============================================================================
// SPICE Behavioral Source AST Nodes
// ============================================================================

// B-source (arbitrary behavioral voltage/current source)
class BSourceExprAST : public ExprAST {
    std::string Name;
    std::string PositiveNode;
    std::string NegativeNode;
    std::unique_ptr<ExprAST> Expression;  // The behavioral expression
    bool IsCurrent;  // true for I-source, false for V-source
public:
    BSourceExprAST(const std::string& Name, const std::string& PosNode, const std::string& NegNode,
                   std::unique_ptr<ExprAST> Expr, bool IsCurrent = false)
        : Name(Name), PositiveNode(PosNode), NegativeNode(NegNode),
          Expression(std::move(Expr)), IsCurrent(IsCurrent) {}
    TypedValue codegen(CodegenContext& context) override;
    const std::string& getName() const { return Name; }
    const std::string& getPositiveNode() const { return PositiveNode; }
    const std::string& getNegativeNode() const { return NegativeNode; }
    const ExprAST* getExpression() const { return Expression.get(); }
    bool isCurrentSource() const { return IsCurrent; }
};

// E-device (Voltage-Controlled Voltage Source)
class ESourceExprAST : public ExprAST {
    std::string Name;
    std::string PositiveNode;
    std::string NegativeNode;
    std::string ControlPosNode;
    std::string ControlNegNode;
    std::unique_ptr<ExprAST> Gain;  // Can be expression or constant
public:
    ESourceExprAST(const std::string& Name, const std::string& PosNode, const std::string& NegNode,
                   const std::string& CtrlPosNode, const std::string& CtrlNegNode,
                   std::unique_ptr<ExprAST> Gain)
        : Name(Name), PositiveNode(PosNode), NegativeNode(NegNode),
          ControlPosNode(CtrlPosNode), ControlNegNode(CtrlNegNode), Gain(std::move(Gain)) {}
    TypedValue codegen(CodegenContext& context) override;
    const std::string& getName() const { return Name; }
    const std::string& getPositiveNode() const { return PositiveNode; }
    const std::string& getNegativeNode() const { return NegativeNode; }
    const std::string& getControlPosNode() const { return ControlPosNode; }
    const std::string& getControlNegNode() const { return ControlNegNode; }
    const ExprAST* getGain() const { return Gain.get(); }
};

// F-device (Current-Controlled Current Source)
class FSourceExprAST : public ExprAST {
    std::string Name;
    std::string PositiveNode;
    std::string NegativeNode;
    std::string VoltageSourceName;  // Current measured through this V-source
    std::unique_ptr<ExprAST> Gain;
public:
    FSourceExprAST(const std::string& Name, const std::string& PosNode, const std::string& NegNode,
                   const std::string& VSourceName, std::unique_ptr<ExprAST> Gain)
        : Name(Name), PositiveNode(PosNode), NegativeNode(NegNode),
          VoltageSourceName(VSourceName), Gain(std::move(Gain)) {}
    TypedValue codegen(CodegenContext& context) override;
    const std::string& getName() const { return Name; }
    const std::string& getPositiveNode() const { return PositiveNode; }
    const std::string& getNegativeNode() const { return NegativeNode; }
    const std::string& getVoltageSourceName() const { return VoltageSourceName; }
    const ExprAST* getGain() const { return Gain.get(); }
};

// G-device (Voltage-Controlled Current Source)
class GSourceExprAST : public ExprAST {
    std::string Name;
    std::string PositiveNode;
    std::string NegativeNode;
    std::string ControlPosNode;
    std::string ControlNegNode;
    std::unique_ptr<ExprAST> Transconductance;
public:
    GSourceExprAST(const std::string& Name, const std::string& PosNode, const std::string& NegNode,
                   const std::string& CtrlPosNode, const std::string& CtrlNegNode,
                   std::unique_ptr<ExprAST> Transcond)
        : Name(Name), PositiveNode(PosNode), NegativeNode(NegNode),
          ControlPosNode(CtrlPosNode), ControlNegNode(CtrlNegNode), Transconductance(std::move(Transcond)) {}
    TypedValue codegen(CodegenContext& context) override;
    const std::string& getName() const { return Name; }
    const std::string& getPositiveNode() const { return PositiveNode; }
    const std::string& getNegativeNode() const { return NegativeNode; }
    const std::string& getControlPosNode() const { return ControlPosNode; }
    const std::string& getControlNegNode() const { return ControlNegNode; }
    const ExprAST* getTransconductance() const { return Transconductance.get(); }
};

// H-device (Current-Controlled Voltage Source)
class HSourceExprAST : public ExprAST {
    std::string Name;
    std::string PositiveNode;
    std::string NegativeNode;
    std::string VoltageSourceName;  // Current measured through this V-source
    std::unique_ptr<ExprAST> Transresistance;
public:
    HSourceExprAST(const std::string& Name, const std::string& PosNode, const std::string& NegNode,
                   const std::string& VSourceName, std::unique_ptr<ExprAST> Transres)
        : Name(Name), PositiveNode(PosNode), NegativeNode(NegNode),
          VoltageSourceName(VSourceName), Transresistance(std::move(Transres)) {}
    TypedValue codegen(CodegenContext& context) override;
    const std::string& getName() const { return Name; }
    const std::string& getPositiveNode() const { return PositiveNode; }
    const std::string& getNegativeNode() const { return NegativeNode; }
    const std::string& getVoltageSourceName() const { return VoltageSourceName; }
    const ExprAST* getTransresistance() const { return Transresistance.get(); }
};

// ============================================================================
// SPICE Analysis Control AST Nodes
// ============================================================================

// Analysis type enumeration
enum class AnalysisType {
    TRAN,    // Transient analysis
    DC,      // DC sweep
    AC,      // AC small-signal analysis
    NOISE,   // Noise analysis
    OP,      // Operating point
    TF,      // Transfer function
    SENS,    // Sensitivity analysis
    FOURIER  // Fourier analysis
};

// Analysis directive
class AnalysisExprAST : public ExprAST {
    AnalysisType Type;
    std::map<std::string, std::unique_ptr<ExprAST>> Parameters;
public:
    AnalysisExprAST(AnalysisType Type) : Type(Type) {}
    TypedValue codegen(CodegenContext& context) override;
    void addParameter(const std::string& Name, std::unique_ptr<ExprAST> Value);
    AnalysisType getAnalysisType() const { return Type; }
    const std::map<std::string, std::unique_ptr<ExprAST>>& getParameters() const { return Parameters; }
};

// Measurement type enumeration
enum class MeasureType {
    MAX,     // Maximum value
    MIN,     // Minimum value
    AVG,     // Average value
    RMS,     // Root mean square
    TRIG,    // Trigger (rise/fall time)
    TARG,    // Target (delay, etc.)
    WHEN,    // Time when condition is met
    FIND,    // Find value at specific time/condition
    DERIV,   // Derivative
    INTEG    // Integral
};

// Measurement directive
class MeasureExprAST : public ExprAST {
    std::string Name;
    MeasureType Type;
    std::unique_ptr<ExprAST> Expression;
    std::map<std::string, std::unique_ptr<ExprAST>> Parameters;
public:
    MeasureExprAST(const std::string& Name, MeasureType Type, std::unique_ptr<ExprAST> Expr)
        : Name(Name), Type(Type), Expression(std::move(Expr)) {}
    TypedValue codegen(CodegenContext& context) override;
    void addParameter(const std::string& Name, std::unique_ptr<ExprAST> Value);
    const std::string& getName() const { return Name; }
    MeasureType getMeasureType() const { return Type; }
    const ExprAST* getExpression() const { return Expression.get(); }
    const std::map<std::string, std::unique_ptr<ExprAST>>& getParameters() const { return Parameters; }
};

// Probe directive
class ProbeExprAST : public ExprAST {
    std::string VariableName;
    std::string OutputName;  // Optional custom name for output
public:
    ProbeExprAST(const std::string& VarName, const std::string& OutName = "")
        : VariableName(VarName), OutputName(OutName) {}
    TypedValue codegen(CodegenContext& context) override;
    const std::string& getVariableName() const { return VariableName; }
    const std::string& getOutputName() const { return OutputName; }
};

// Save directive
class SaveExprAST : public ExprAST {
    std::string VariableName;
public:
    explicit SaveExprAST(const std::string& VarName) : VariableName(VarName) {}
    TypedValue codegen(CodegenContext& context) override;
    const std::string& getVariableName() const { return VariableName; }
};

// ============================================================================
// SPICE Subcircuit and Model AST Nodes
// ============================================================================

// Subcircuit definition
class SubcktExprAST : public ExprAST {
    std::string Name;
    std::vector<std::string> Pins;
    std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> Parameters;
    std::vector<std::unique_ptr<ExprAST>> Body;
    std::vector<std::unique_ptr<FunctionAST>> Functions;
public:
    SubcktExprAST(const std::string& Name, std::vector<std::string> Pins)
        : Name(Name), Pins(std::move(Pins)) {}
    TypedValue codegen(CodegenContext& context) override;
    void addParameter(const std::string& Name, std::unique_ptr<ExprAST> Value);
    void addStatement(std::unique_ptr<ExprAST> Stmt);
    void addFunction(std::unique_ptr<FunctionAST> Func);
    const std::string& getName() const { return Name; }
    const std::vector<std::string>& getPins() const { return Pins; }
    const std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>>& getParameters() const { return Parameters; }
    const std::vector<std::unique_ptr<ExprAST>>& getBody() const { return Body; }
    const std::vector<std::unique_ptr<FunctionAST>>& getFunctions() const { return Functions; }
};

// Model declaration
class ModelExprAST : public ExprAST {
    std::string Name;
    std::string ModelType;  // D, NPN, PNP, NMOS, PMOS, R, L, C, SW, T, K
    std::map<std::string, std::unique_ptr<ExprAST>> Parameters;
public:
    ModelExprAST(const std::string& Name, const std::string& Type)
        : Name(Name), ModelType(Type) {}
    TypedValue codegen(CodegenContext& context) override;
    void addParameter(const std::string& Name, std::unique_ptr<ExprAST> Value);
    const std::string& getName() const { return Name; }
    const std::string& getModelType() const { return ModelType; }
    const std::map<std::string, std::unique_ptr<ExprAST>>& getParameters() const { return Parameters; }
};

// Parameter declaration
class ParamExprAST : public ExprAST {
    std::string Name;
    std::unique_ptr<ExprAST> Value;
public:
    ParamExprAST(const std::string& Name, std::unique_ptr<ExprAST> Value)
        : Name(Name), Value(std::move(Value)) {}
    TypedValue codegen(CodegenContext& context) override;
    const std::string& getName() const { return Name; }
    const ExprAST* getValue() const { return Value.get(); }
};

// Initial condition
class ICExprAST : public ExprAST {
    std::string NodeName;
    std::unique_ptr<ExprAST> Value;
public:
    ICExprAST(const std::string& NodeName, std::unique_ptr<ExprAST> Value)
        : NodeName(NodeName), Value(std::move(Value)) {}
    TypedValue codegen(CodegenContext& context) override;
    const std::string& getNodeName() const { return NodeName; }
    const ExprAST* getValue() const { return Value.get(); }
};

// Monte Carlo Analysis
class MonteCarloExprAST : public ExprAST {
    std::string OutputName;
    std::map<std::string, double> ComponentNominal;
    std::map<std::string, double> ComponentTolerance;
    int Iterations;
public:
    MonteCarloExprAST(const std::string& output, int iterations)
        : OutputName(output), Iterations(iterations) {}
    void addComponent(const std::string& name, double nominal, double tolerance) {
        ComponentNominal[name] = nominal;
        ComponentTolerance[name] = tolerance;
    }
    TypedValue codegen(CodegenContext& context) override;
    const std::string& getOutputName() const { return OutputName; }
    int getIterations() const { return Iterations; }
    const std::map<std::string, double>& getComponentNominal() const { return ComponentNominal; }
    const std::map<std::string, double>& getComponentTolerance() const { return ComponentTolerance; }
};

// Worst-Case Analysis
class WorstCaseExprAST : public ExprAST {
    std::string OutputName;
    std::map<std::string, double> ComponentNominal;
    std::map<std::string, double> ComponentTolerance;
public:
    WorstCaseExprAST(const std::string& output) : OutputName(output) {}
    void addComponent(const std::string& name, double nominal, double tolerance) {
        ComponentNominal[name] = nominal;
        ComponentTolerance[name] = tolerance;
    }
    TypedValue codegen(CodegenContext& context) override;
    const std::string& getOutputName() const { return OutputName; }
    const std::map<std::string, double>& getComponentNominal() const { return ComponentNominal; }
};

// Stability Analysis
class StabilityExprAST : public ExprAST {
    std::string OutputName;
public:
    StabilityExprAST(const std::string& output) : OutputName(output) {}
    TypedValue codegen(CodegenContext& context) override;
    const std::string& getOutputName() const { return OutputName; }
};

// Sensitivity Analysis
class SensitivityExprAST : public ExprAST {
    std::string OutputName;
public:
    SensitivityExprAST(const std::string& output) : OutputName(output) {}
    TypedValue codegen(CodegenContext& context) override;
    const std::string& getOutputName() const { return OutputName; }
};

// Optimization
class OptimizationExprAST : public ExprAST {
    std::string OutputName;
public:
    OptimizationExprAST(const std::string& output) : OutputName(output) {}
    TypedValue codegen(CodegenContext& context) override;
    const std::string& getOutputName() const { return OutputName; }
};

// FFT Analysis
class FFTExprAST : public ExprAST {
    std::string SignalName;
public:
    FFTExprAST(const std::string& signal) : SignalName(signal) {}
    TypedValue codegen(CodegenContext& context) override;
    const std::string& getSignalName() const { return SignalName; }
};

/* ========================================================================
 * Section 7.2: Mixed-Signal & Modeling Extensions
 * ======================================================================== */

// --- Event-driven constructs ---

// cross(expr, [rise_fall]) - Zero-crossing detection
// rise_fall: 0=any, 1=rising only, -1=falling only
class CrossExprAST : public ExprAST {
    std::unique_ptr<ExprAST> Expression;
    int RiseFall;  // 0=any edge, 1=rising, -1=falling
public:
    CrossExprAST(std::unique_ptr<ExprAST> Expr, int RiseFall = 0)
        : Expression(std::move(Expr)), RiseFall(RiseFall) {}
    TypedValue codegen(CodegenContext& context) override;
    const ExprAST* getExpression() const { return Expression.get(); }
    int getRiseFall() const { return RiseFall; }
};

// above(expr, threshold) - Threshold detection (returns 1 when expr > threshold)
class AboveExprAST : public ExprAST {
    std::unique_ptr<ExprAST> Expression;
    std::unique_ptr<ExprAST> Threshold;
public:
    AboveExprAST(std::unique_ptr<ExprAST> Expr, std::unique_ptr<ExprAST> Thresh)
        : Expression(std::move(Expr)), Threshold(std::move(Thresh)) {}
    TypedValue codegen(CodegenContext& context) override;
    const ExprAST* getExpression() const { return Expression.get(); }
    const ExprAST* getThreshold() const { return Threshold.get(); }
};

// timer() - Returns elapsed simulation time since last event
class TimerExprAST : public ExprAST {
public:
    TimerExprAST() {}
    TypedValue codegen(CodegenContext& context) override;
};

// --- Real-valued digital modeling ---

// FSM state transition table entry
struct FSMTransition {
    int CurrentState;
    int NextState;
    std::unique_ptr<ExprAST> Condition;  // Guard expression
    std::unique_ptr<ExprAST> Output;     // Output value (Moore/Mealy)
    FSMTransition(int cur, int next, std::unique_ptr<ExprAST> cond, std::unique_ptr<ExprAST> out)
        : CurrentState(cur), NextState(next), Condition(std::move(cond)), Output(std::move(out)) {}
};

// fsm(initial_state, transitions[], output_fn) - Finite State Machine
class FSMExprAST : public ExprAST {
    int InitialState;
    std::vector<FSMTransition> Transitions;
    std::string OutputFn;  // "moore" or "mealy"
public:
    FSMExprAST(int initState, std::string outputFn)
        : InitialState(initState), OutputFn(std::move(outputFn)) {}
    void addTransition(int cur, int next, std::unique_ptr<ExprAST> cond, std::unique_ptr<ExprAST> out);
    TypedValue codegen(CodegenContext& context) override;
    int getInitialState() const { return InitialState; }
    const std::vector<FSMTransition>& getTransitions() const { return Transitions; }
    const std::string& getOutputFn() const { return OutputFn; }
};

// edge(expr, type) - Edge trigger detection
// type: 1=posedge, -1=negedge, 0=any
class EdgeExprAST : public ExprAST {
    std::unique_ptr<ExprAST> Expression;
    int EdgeType;  // 1=posedge, -1=negedge, 0=any
    std::string EdgeStr;  // "posedge", "negedge", or ""
public:
    EdgeExprAST(std::unique_ptr<ExprAST> Expr, int type, std::string edgeStr)
        : Expression(std::move(Expr)), EdgeType(type), EdgeStr(std::move(edgeStr)) {}
    TypedValue codegen(CodegenContext& context) override;
    const ExprAST* getExpression() const { return Expression.get(); }
    int getEdgeType() const { return EdgeType; }
    const std::string& getEdgeStr() const { return EdgeStr; }
};

// triggered(event_expr, body) - Execute body only on event
class TriggeredExprAST : public ExprAST {
    std::unique_ptr<ExprAST> EventExpr;
    std::unique_ptr<ExprAST> Body;
public:
    TriggeredExprAST(std::unique_ptr<ExprAST> Event, std::unique_ptr<ExprAST> B)
        : EventExpr(std::move(Event)), Body(std::move(B)) {}
    TypedValue codegen(CodegenContext& context) override;
    const ExprAST* getEventExpr() const { return EventExpr.get(); }
    const ExprAST* getBody() const { return Body.get(); }
};

// --- Noise modeling primitives ---

// noise(type, amplitude, [freq]) - Noise generation function
// type: "white", "flicker", "thermal"
class NoiseExprAST : public ExprAST {
    std::string NoiseType;  // "white", "flicker", "thermal"
    std::unique_ptr<ExprAST> Amplitude;
    std::unique_ptr<ExprAST> Frequency;  // Optional, for flicker noise corner freq
public:
    NoiseExprAST(std::string type, std::unique_ptr<ExprAST> amp, std::unique_ptr<ExprAST> freq = nullptr)
        : NoiseType(std::move(type)), Amplitude(std::move(amp)), Frequency(std::move(freq)) {}
    TypedValue codegen(CodegenContext& context) override;
    const std::string& getNoiseType() const { return NoiseType; }
    const ExprAST* getAmplitude() const { return Amplitude.get(); }
    const ExprAST* getFrequency() const { return Frequency.get(); }
};

// white_noise(amplitude)
class WhiteNoiseExprAST : public ExprAST {
    std::unique_ptr<ExprAST> Amplitude;
public:
    WhiteNoiseExprAST(std::unique_ptr<ExprAST> amp) : Amplitude(std::move(amp)) {}
    TypedValue codegen(CodegenContext& context) override;
    const ExprAST* getAmplitude() const { return Amplitude.get(); }
};

// flicker_noise(amplitude, corner_freq)
class FlickerNoiseExprAST : public ExprAST {
    std::unique_ptr<ExprAST> Amplitude;
    std::unique_ptr<ExprAST> CornerFreq;
public:
    FlickerNoiseExprAST(std::unique_ptr<ExprAST> amp, std::unique_ptr<ExprAST> cf)
        : Amplitude(std::move(amp)), CornerFreq(std::move(cf)) {}
    TypedValue codegen(CodegenContext& context) override;
    const ExprAST* getAmplitude() const { return Amplitude.get(); }
    const ExprAST* getCornerFreq() const { return CornerFreq.get(); }
};

// thermal_noise(resistance, temperature)
class ThermalNoiseExprAST : public ExprAST {
    std::unique_ptr<ExprAST> Resistance;
    std::unique_ptr<ExprAST> Temperature;  // Kelvin, defaults to 300.15
public:
    ThermalNoiseExprAST(std::unique_ptr<ExprAST> res, std::unique_ptr<ExprAST> temp)
        : Resistance(std::move(res)), Temperature(std::move(temp)) {}
    TypedValue codegen(CodegenContext& context) override;
    const ExprAST* getResistance() const { return Resistance.get(); }
    const ExprAST* getTemperature() const { return Temperature.get(); }
};

// --- Piecewise and table-based models ---

// Piecewise data point
struct PiecewisePoint {
    std::unique_ptr<ExprAST> X;
    std::unique_ptr<ExprAST> Y;
    PiecewisePoint(std::unique_ptr<ExprAST> x, std::unique_ptr<ExprAST> y)
        : X(std::move(x)), Y(std::move(y)) {}
};

// piecewise(points..., [interpolation], x) - Piecewise function with optional interpolation
// interpolation: "linear", "cubic", "spline", "step"
class PiecewiseExprAST : public ExprAST {
    std::vector<PiecewisePoint> Points;
    std::string Interpolation;  // "linear", "cubic", "spline", "step"
    std::unique_ptr<ExprAST> QueryX;  // The x value to evaluate
public:
    PiecewiseExprAST(std::string interp) : Interpolation(std::move(interp)) {}
    void addPoint(std::unique_ptr<ExprAST> x, std::unique_ptr<ExprAST> y);
    TypedValue codegen(CodegenContext& context) override;
    const std::vector<PiecewisePoint>& getPoints() const { return Points; }
    const std::string& getInterpolation() const { return Interpolation; }
    const ExprAST* getQueryX() const { return QueryX.get(); }
    void setQueryX(std::unique_ptr<ExprAST> x) { QueryX = std::move(x); }
};

// Table entry
struct TableEntry {
    std::unique_ptr<ExprAST> Key;
    std::unique_ptr<ExprAST> Value;
    TableEntry(std::unique_ptr<ExprAST> k, std::unique_ptr<ExprAST> v)
        : Key(std::move(k)), Value(std::move(v)) {}
};

// table(entries..., default, key) - Table lookup with optional default
class TableExprAST : public ExprAST {
    std::vector<TableEntry> Entries;
    std::unique_ptr<ExprAST> DefaultValue;
    std::unique_ptr<ExprAST> QueryKey;
public:
    TableExprAST() {}
    void addEntry(std::unique_ptr<ExprAST> key, std::unique_ptr<ExprAST> val);
    TypedValue codegen(CodegenContext& context) override;
    const std::vector<TableEntry>& getEntries() const { return Entries; }
    const ExprAST* getDefaultValue() const { return DefaultValue.get(); }
    void setDefaultValue(std::unique_ptr<ExprAST> d) { DefaultValue = std::move(d); }
    const ExprAST* getQueryKey() const { return QueryKey.get(); }
    void setQueryKey(std::unique_ptr<ExprAST> k) { QueryKey = std::move(k); }
};

// csv_import(filename, [options]) - Import data from CSV
class CsvImportExprAST : public ExprAST {
    std::string Filename;
    std::map<std::string, std::string> Options;  // delimiter, header, skip_rows, etc.
public:
    CsvImportExprAST(std::string filename, std::map<std::string, std::string> opts)
        : Filename(std::move(filename)), Options(std::move(opts)) {}
    TypedValue codegen(CodegenContext& context) override;
    const std::string& getFilename() const { return Filename; }
    const std::map<std::string, std::string>& getOptions() const { return Options; }
};

// --- Units and dimensional analysis ---

// unit(value, unit_string) - Annotate value with unit
class UnitExprAST : public ExprAST {
    std::unique_ptr<ExprAST> Value;
    std::string UnitStr;  // e.g., "V", "mA", "k", "F"
public:
    UnitExprAST(std::unique_ptr<ExprAST> val, std::string unit)
        : Value(std::move(val)), UnitStr(std::move(unit)) {}
    TypedValue codegen(CodegenContext& context) override;
    const ExprAST* getValue() const { return Value.get(); }
    const std::string& getUnitStr() const { return UnitStr; }
};

// dimension(expr) - Return dimensional analysis string
class DimensionExprAST : public ExprAST {
    std::unique_ptr<ExprAST> Expression;
public:
    DimensionExprAST(std::unique_ptr<ExprAST> expr) : Expression(std::move(expr)) {}
    TypedValue codegen(CodegenContext& context) override;
    const ExprAST* getExpression() const { return Expression.get(); }
};

// convert(value, from_unit, to_unit) - Unit conversion
class ConvertExprAST : public ExprAST {
    std::unique_ptr<ExprAST> Value;
    std::string FromUnit;
    std::string ToUnit;
public:
    ConvertExprAST(std::unique_ptr<ExprAST> val, std::string from, std::string to)
        : Value(std::move(val)), FromUnit(std::move(from)), ToUnit(std::move(to)) {}
    TypedValue codegen(CodegenContext& context) override;
    const ExprAST* getValue() const { return Value.get(); }
    const std::string& getFromUnit() const { return FromUnit; }
    const std::string& getToUnit() const { return ToUnit; }
};

// has_unit(value, unit_string) - Check if value has specified unit
class HasUnitExprAST : public ExprAST {
    std::unique_ptr<ExprAST> Value;
    std::string UnitStr;
public:
    HasUnitExprAST(std::unique_ptr<ExprAST> val, std::string unit)
        : Value(std::move(val)), UnitStr(std::move(unit)) {}
    TypedValue codegen(CodegenContext& context) override;
    const ExprAST* getValue() const { return Value.get(); }
    const std::string& getUnitStr() const { return UnitStr; }
};

// ============================================================================
// Advanced Features AST Nodes
// ============================================================================

class PlotDeclAST : public ExprAST {
    std::vector<std::string> Signals;
    std::string Title;
    std::vector<std::string> Colors;
    bool GridEnabled = false;
    bool AutoScale = false;
public:
    PlotDeclAST(std::vector<std::string> signalNames) : Signals(std::move(signalNames)) {}
    TypedValue codegen(CodegenContext& context) override;
    void setTitle(const std::string& title) { Title = title; }
    void setColors(std::vector<std::string> colors) { Colors = std::move(colors); }
    void setGrid(bool enabled) { GridEnabled = enabled; }
    void setAutoScale(bool enabled) { AutoScale = enabled; }
};

class BenchmarkDeclAST : public ExprAST {
    std::string Circuit;
    std::vector<std::string> Comparators;
    std::vector<std::string> Metrics;
public:
    BenchmarkDeclAST(std::string circuit) : Circuit(std::move(circuit)) {}
    TypedValue codegen(CodegenContext& context) override;
    void setComparators(std::vector<std::string> comparators) { Comparators = std::move(comparators); }
    void setMetrics(std::vector<std::string> metrics) { Metrics = std::move(metrics); }
};

class OptimizeDeclAST : public ExprAST {
    std::string Circuit;
    std::map<std::string, std::string> Goals;
    std::vector<std::string> TuneParams;
    std::string Algorithm;
    int MaxIterations = 0;
    bool GPUAccelerated = false;
public:
    OptimizeDeclAST(std::string circuit) : Circuit(std::move(circuit)) {}
    TypedValue codegen(CodegenContext& context) override;
    void setGoals(std::map<std::string, std::string> goals) { Goals = std::move(goals); }
    void setTuneParams(std::vector<std::string> params) { TuneParams = std::move(params); }
    void setAlgorithm(const std::string& algo) { Algorithm = algo; }
    void setMaxIterations(int iter) { MaxIterations = iter; }
    void setGPUAccelerated(bool enabled) { GPUAccelerated = enabled; }
};

class SweepDeclAST : public ExprAST {
    std::string Signal;
    std::map<std::string, std::map<std::string, std::string>> Controls;
public:
    SweepDeclAST(std::string signal) : Signal(std::move(signal)) {}
    TypedValue codegen(CodegenContext& context) override;
    void addControl(const std::string& name, const std::string& type, const std::map<std::string, std::string>& params);
};

class ReportDeclAST : public ExprAST {
    std::string Filename;
    std::vector<std::string> Sections;
    bool IncludePNG = false;
    bool IncludeCSV = false;
public:
    ReportDeclAST(std::string filename) : Filename(std::move(filename)) {}
    TypedValue codegen(CodegenContext& context) override;
    void setSections(std::vector<std::string> sections) { Sections = std::move(sections); }
    void setIncludePNG(bool include) { IncludePNG = include; }
    void setIncludeCSV(bool include) { IncludeCSV = include; }
};

// ============================================================================
// Type Aliases for Netlist Generator Compatibility
// ============================================================================
using SubcktAST = SubcktExprAST;
using ModelAST = ModelExprAST;
using AnalysisDeclAST = AnalysisExprAST;
using MeasureDeclAST = MeasureExprAST;
using ProbeDeclAST = ProbeExprAST;

} // namespace Flux

#endif // FLUX_COMPILER_AST_H
