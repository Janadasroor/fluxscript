#include "flux/compiler/ast.h"
#include "flux/compiler/lexer.h"
#include <llvm/IR/Verifier.h>
#include <iostream>

namespace Flux {

// Helper to check if a type is complex
static bool isComplexType(TypedValue V) {
    return V.Type.Kind == TypeKind::Complex;
}

static FluxType typeFromLLVM(llvm::Type* Ty) {
    if (Ty->isDoubleTy()) return FluxType(TypeKind::Double);
    if (Ty->isFloatTy()) return FluxType(TypeKind::Float);
    if (Ty->isIntegerTy(32)) return FluxType(TypeKind::Int);
    if (Ty->isVoidTy()) return FluxType(TypeKind::Void);
    if (Ty->isStructTy()) return FluxType(TypeKind::Complex);
    if (Ty->isPointerTy()) return FluxType(TypeKind::String);
    return FluxType(TypeKind::Double);
}

// Helper to promote double to complex
static TypedValue promoteToComplex(TypedValue V, CodegenContext& context) {
    if (isComplexType(V)) return V;
    
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
    llvm::StructType* ComplexTy = llvm::StructType::get(context.TheContext, {DoubleTy, DoubleTy});
    
    llvm::Value* Val = V.Val;
    if (V.Type.Kind == TypeKind::Int) {
        Val = context.Builder.CreateSIToFP(Val, DoubleTy, "inttodouble");
    } else if (V.Type.Kind == TypeKind::Float) {
        Val = context.Builder.CreateFPExt(Val, DoubleTy, "floattodouble");
    }

    llvm::Value* ComplexVal = llvm::UndefValue::get(ComplexTy);
    ComplexVal = context.Builder.CreateInsertValue(ComplexVal, Val, 0, "real");
    ComplexVal = context.Builder.CreateInsertValue(ComplexVal, llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0)), 1, "imag");
    return TypedValue(ComplexVal, TypeKind::Complex);
}

TypedValue NumberExprAST::codegen(CodegenContext& context) {
    return TypedValue(llvm::ConstantFP::get(context.TheContext, llvm::APFloat(Val)), TypeKind::Double);
}

TypedValue ComplexExprAST::codegen(CodegenContext& context) {
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
    llvm::StructType* ComplexTy = llvm::StructType::get(context.TheContext, {DoubleTy, DoubleTy});
    
    llvm::Value* ComplexVal = llvm::UndefValue::get(ComplexTy);
    llvm::Value* RealVal = llvm::ConstantFP::get(context.TheContext, llvm::APFloat(Real));
    llvm::Value* ImagVal = llvm::ConstantFP::get(context.TheContext, llvm::APFloat(Imag));
    
    ComplexVal = context.Builder.CreateInsertValue(ComplexVal, RealVal, 0, "real");
    ComplexVal = context.Builder.CreateInsertValue(ComplexVal, ImagVal, 1, "imag");
    
    return TypedValue(ComplexVal, TypeKind::Complex);
}

TypedValue StringExprAST::codegen(CodegenContext& context) {
    llvm::Constant* StrConst = llvm::ConstantDataArray::getString(context.TheContext, Val, true);
    llvm::GlobalVariable* GV = new llvm::GlobalVariable(
        *context.TheModule,
        StrConst->getType(),
        true,
        llvm::GlobalValue::PrivateLinkage,
        StrConst,
        ".str");
    
    llvm::Value* ValV = context.Builder.CreatePointerCast(GV, llvm::PointerType::get(llvm::Type::getInt8Ty(context.TheContext), 0), "strptr");
    return TypedValue(ValV, TypeKind::String);
}

TypedValue BlockExprAST::codegen(CodegenContext& context) {
    TypedValue LastVal(nullptr, TypeKind::Void);
    
    for (auto& Stmt : Statements) {
        LastVal = Stmt->codegen(context);
        if (!LastVal.Val && LastVal.Type.Kind != TypeKind::Void) return TypedValue(nullptr, TypeKind::Void);
    }
    
    return LastVal.Val ? LastVal : TypedValue(llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0)), TypeKind::Double);
}

TypedValue VariableExprAST::codegen(CodegenContext& context) {
    llvm::Value* V = context.NamedValues[Name];
    if (!V) {
        std::cerr << "Unknown variable name: " << Name << std::endl;
        return TypedValue();
    }
    if (auto* Alloca = llvm::dyn_cast<llvm::AllocaInst>(V)) {
        llvm::Type* Ty = Alloca->getAllocatedType();
        llvm::Value* LoadedV = context.Builder.CreateLoad(Ty, Alloca, Name.c_str());
        return TypedValue(LoadedV, typeFromLLVM(Ty));
    }
    return TypedValue(V, typeFromLLVM(V->getType()));
}

TypedValue AssignExprAST::codegen(CodegenContext& context) {
    llvm::Value* Variable = context.NamedValues[Name];
    if (!Variable) {
        std::cerr << "Unknown variable name: " << Name << std::endl;
        return TypedValue();
    }

    if (!llvm::isa<llvm::AllocaInst>(Variable)) {
        std::cerr << "Cannot assign to read-only variable: " << Name << std::endl;
        return TypedValue();
    }

    auto* Alloca = llvm::cast<llvm::AllocaInst>(Variable);
    llvm::Type* VarTy = Alloca->getAllocatedType();
    FluxType VarFluxTy = typeFromLLVM(VarTy);

    TypedValue CurrentTV(context.Builder.CreateLoad(VarTy, Variable, "current"), VarFluxTy);

    TypedValue ValTV = Val->codegen(context);
    if (!ValTV.Val) return TypedValue();

    if (Op != 0) {
        if (isComplexType(CurrentTV) || isComplexType(ValTV)) {
            TypedValue L = promoteToComplex(CurrentTV, context);
            TypedValue R = promoteToComplex(ValTV, context);

            llvm::Value* LReal = context.Builder.CreateExtractValue(L.Val, 0, "lreal");
            llvm::Value* LImag = context.Builder.CreateExtractValue(L.Val, 1, "limag");
            llvm::Value* RReal = context.Builder.CreateExtractValue(R.Val, 0, "rreal");
            llvm::Value* RImag = context.Builder.CreateExtractValue(R.Val, 1, "rimag");

            switch (Op) {
            case '+': {
                llvm::Value* ResReal = context.Builder.CreateFAdd(LReal, RReal, "addre");
                llvm::Value* ResImag = context.Builder.CreateFAdd(LImag, RImag, "addim");
                llvm::Value* Res = llvm::UndefValue::get(L.Val->getType());
                Res = context.Builder.CreateInsertValue(Res, ResReal, 0);
                Res = context.Builder.CreateInsertValue(Res, ResImag, 1);
                ValTV = TypedValue(Res, TypeKind::Complex);
                break;
            }
            case '-': {
                llvm::Value* ResReal = context.Builder.CreateFSub(LReal, RReal, "subre");
                llvm::Value* ResImag = context.Builder.CreateFSub(LImag, RImag, "subim");
                llvm::Value* Res = llvm::UndefValue::get(L.Val->getType());
                Res = context.Builder.CreateInsertValue(Res, ResReal, 0);
                Res = context.Builder.CreateInsertValue(Res, ResImag, 1);
                ValTV = TypedValue(Res, TypeKind::Complex);
                break;
            }
            case '*': {
                llvm::Value* ac = context.Builder.CreateFMul(LReal, RReal, "ac");
                llvm::Value* bd = context.Builder.CreateFMul(LImag, RImag, "bd");
                llvm::Value* ad = context.Builder.CreateFMul(LReal, RImag, "ad");
                llvm::Value* bc = context.Builder.CreateFMul(LImag, RReal, "bc");
                llvm::Value* ResReal = context.Builder.CreateFSub(ac, bd, "multre");
                llvm::Value* ResImag = context.Builder.CreateFAdd(ad, bc, "multim");
                llvm::Value* Res = llvm::UndefValue::get(L.Val->getType());
                Res = context.Builder.CreateInsertValue(Res, ResReal, 0);
                Res = context.Builder.CreateInsertValue(Res, ResImag, 1);
                ValTV = TypedValue(Res, TypeKind::Complex);
                break;
            }
            case '/': {
                llvm::Value* ac = context.Builder.CreateFMul(LReal, RReal, "ac");
                llvm::Value* bd = context.Builder.CreateFMul(LImag, RImag, "bd");
                llvm::Value* bc = context.Builder.CreateFMul(LImag, RReal, "bc");
                llvm::Value* ad = context.Builder.CreateFMul(LReal, RImag, "ad");
                llvm::Value* c2 = context.Builder.CreateFMul(RReal, RReal, "c2");
                llvm::Value* d2 = context.Builder.CreateFMul(RImag, RImag, "d2");
                llvm::Value* den = context.Builder.CreateFAdd(c2, d2, "den");
                llvm::Value* ResRealNum = context.Builder.CreateFAdd(ac, bd, "numre");
                llvm::Value* ResImagNum = context.Builder.CreateFSub(bc, ad, "numim");
                llvm::Value* ResReal = context.Builder.CreateFDiv(ResRealNum, den, "divre");
                llvm::Value* ResImag = context.Builder.CreateFDiv(ResImagNum, den, "divim");
                llvm::Value* Res = llvm::UndefValue::get(L.Val->getType());
                Res = context.Builder.CreateInsertValue(Res, ResReal, 0);
                Res = context.Builder.CreateInsertValue(Res, ResImag, 1);
                ValTV = TypedValue(Res, TypeKind::Complex);
                break;
            }
            }
        } else {
            switch (Op) {
            case '+':
                ValTV = TypedValue(context.Builder.CreateFAdd(CurrentTV.Val, ValTV.Val, "addtmp"), TypeKind::Double);
                break;
            case '-':
                ValTV = TypedValue(context.Builder.CreateFSub(CurrentTV.Val, ValTV.Val, "subtmp"), TypeKind::Double);
                break;
            case '*':
                ValTV = TypedValue(context.Builder.CreateFMul(CurrentTV.Val, ValTV.Val, "multmp"), TypeKind::Double);
                break;
            case '/':
                ValTV = TypedValue(context.Builder.CreateFDiv(CurrentTV.Val, ValTV.Val, "divtmp"), TypeKind::Double);
                break;
            }
        }
    }

    if (ValTV.Type.Kind != VarFluxTy.Kind) {
        if (VarFluxTy.Kind == TypeKind::Complex) {
            ValTV = promoteToComplex(ValTV, context);
        } else if (VarFluxTy.Kind == TypeKind::Double && ValTV.Type.Kind == TypeKind::Int) {
            ValTV = TypedValue(context.Builder.CreateSIToFP(ValTV.Val, VarTy, "cast"), TypeKind::Double);
        }
    }

    context.Builder.CreateStore(ValTV.Val, Variable);
    return ValTV;
}

TypedValue BinaryExprAST::codegen(CodegenContext& context) {
    TypedValue L = LHS->codegen(context);
    TypedValue R = RHS->codegen(context);
    if (!L.Val || !R.Val) return TypedValue();

    if (isComplexType(L) || isComplexType(R)) {
        L = promoteToComplex(L, context);
        R = promoteToComplex(R, context);

        llvm::Value* LReal = context.Builder.CreateExtractValue(L.Val, 0, "lreal");
        llvm::Value* LImag = context.Builder.CreateExtractValue(L.Val, 1, "limag");
        llvm::Value* RReal = context.Builder.CreateExtractValue(R.Val, 0, "rreal");
        llvm::Value* RImag = context.Builder.CreateExtractValue(R.Val, 1, "rimag");

        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        llvm::StructType* ComplexTy = llvm::StructType::get(context.TheContext, {DoubleTy, DoubleTy});

        switch (Op) {
        case '+': {
            llvm::Value* ResReal = context.Builder.CreateFAdd(LReal, RReal, "addre");
            llvm::Value* ResImag = context.Builder.CreateFAdd(LImag, RImag, "addim");
            llvm::Value* Res = llvm::UndefValue::get(ComplexTy);
            Res = context.Builder.CreateInsertValue(Res, ResReal, 0);
            return TypedValue(context.Builder.CreateInsertValue(Res, ResImag, 1), TypeKind::Complex);
        }
        case '-': {
            llvm::Value* ResReal = context.Builder.CreateFSub(LReal, RReal, "subre");
            llvm::Value* ResImagSub = context.Builder.CreateFSub(LImag, RImag, "subim");
            llvm::Value* Res = llvm::UndefValue::get(ComplexTy);
            Res = context.Builder.CreateInsertValue(Res, ResReal, 0);
            return TypedValue(context.Builder.CreateInsertValue(Res, ResImagSub, 1), TypeKind::Complex);
        }
        case '*': {
            llvm::Value* ac = context.Builder.CreateFMul(LReal, RReal, "ac");
            llvm::Value* bd = context.Builder.CreateFMul(LImag, RImag, "bd");
            llvm::Value* ad = context.Builder.CreateFMul(LReal, RImag, "ad");
            llvm::Value* bc = context.Builder.CreateFMul(LImag, RReal, "bc");
            llvm::Value* ResReal = context.Builder.CreateFSub(ac, bd, "multre");
            llvm::Value* ResImag = context.Builder.CreateFAdd(ad, bc, "multim");
            llvm::Value* Res = llvm::UndefValue::get(ComplexTy);
            Res = context.Builder.CreateInsertValue(Res, ResReal, 0);
            return TypedValue(context.Builder.CreateInsertValue(Res, ResImag, 1), TypeKind::Complex);
        }
        case '/': {
            llvm::Value* ac = context.Builder.CreateFMul(LReal, RReal, "ac");
            llvm::Value* bd = context.Builder.CreateFMul(LImag, RImag, "bd");
            llvm::Value* bc = context.Builder.CreateFMul(LImag, RReal, "bc");
            llvm::Value* ad = context.Builder.CreateFMul(LReal, RImag, "ad");
            llvm::Value* c2 = context.Builder.CreateFMul(RReal, RReal, "c2");
            llvm::Value* d2 = context.Builder.CreateFMul(RImag, RImag, "d2");
            llvm::Value* den = context.Builder.CreateFAdd(c2, d2, "den");
            llvm::Value* ResRealNum = context.Builder.CreateFAdd(ac, bd, "numre");
            llvm::Value* ResImagNum = context.Builder.CreateFSub(bc, ad, "numim");
            llvm::Value* ResReal = context.Builder.CreateFDiv(ResRealNum, den, "divre");
            llvm::Value* ResImag = context.Builder.CreateFDiv(ResImagNum, den, "divim");
            llvm::Value* Res = llvm::UndefValue::get(ComplexTy);
            Res = context.Builder.CreateInsertValue(Res, ResReal, 0);
            return TypedValue(context.Builder.CreateInsertValue(Res, ResImag, 1), TypeKind::Complex);
        }
        case static_cast<int>(TokenType::tok_equal): {
            llvm::Value* RealEq = context.Builder.CreateFCmpOEQ(LReal, RReal, "realeq");
            llvm::Value* ImagEq = context.Builder.CreateFCmpOEQ(LImag, RImag, "imageq");
            llvm::Value* And = context.Builder.CreateAnd(RealEq, ImagEq, "compeq");
            return TypedValue(context.Builder.CreateUIToFP(And, llvm::Type::getDoubleTy(context.TheContext), "booltmp"), TypeKind::Double);
        }
        case static_cast<int>(TokenType::tok_not_equal): {
            llvm::Value* RealNe = context.Builder.CreateFCmpUNE(LReal, RReal, "realne");
            llvm::Value* ImagNe = context.Builder.CreateFCmpUNE(LImag, RImag, "imagne");
            llvm::Value* Or = context.Builder.CreateOr(RealNe, ImagNe, "compne");
            return TypedValue(context.Builder.CreateUIToFP(Or, llvm::Type::getDoubleTy(context.TheContext), "booltmp"), TypeKind::Double);
        }
        default:
            std::cerr << "Binary operator not supported for complex numbers" << std::endl;
            return TypedValue();
        }
    }

    llvm::Value* LV = L.Val;
    llvm::Value* RV = R.Val;
    bool isIntOp = LV->getType()->isIntegerTy() && RV->getType()->isIntegerTy();

    auto createBoolResult = [&](llvm::Value* cmp) -> TypedValue {
        return TypedValue(context.Builder.CreateUIToFP(cmp, llvm::Type::getDoubleTy(context.TheContext), "booltmp"), TypeKind::Double);
    };

    switch (Op) {
    case '+':
        if (isIntOp) return TypedValue(context.Builder.CreateAdd(LV, RV, "addtmp"), TypeKind::Int);
        return TypedValue(context.Builder.CreateFAdd(LV, RV, "addtmp"), TypeKind::Double);
    case '-':
        if (isIntOp) return TypedValue(context.Builder.CreateSub(LV, RV, "subtmp"), TypeKind::Int);
        return TypedValue(context.Builder.CreateFSub(LV, RV, "subtmp"), TypeKind::Double);
    case '*':
        if (isIntOp) return TypedValue(context.Builder.CreateMul(LV, RV, "multmp"), TypeKind::Int);
        {
            llvm::Function* MatrixMulF = context.TheModule->getFunction("flux_matrix_mul");
            if (!MatrixMulF) {
                llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
                llvm::FunctionType* MatrixMulFTy = llvm::FunctionType::get(DoubleTy, {DoubleTy, DoubleTy}, false);
                MatrixMulF = llvm::Function::Create(MatrixMulFTy, llvm::Function::ExternalLinkage,
                                                  "flux_matrix_mul", context.TheModule.get());
            }
            return TypedValue(context.Builder.CreateCall(MatrixMulF, {LV, RV}, "multmp"), TypeKind::Double);
        }
    case '/':
        if (isIntOp) return TypedValue(context.Builder.CreateSDiv(LV, RV, "divtmp"), TypeKind::Int);
        return TypedValue(context.Builder.CreateFDiv(LV, RV, "divtmp"), TypeKind::Double);
    case static_cast<int>(TokenType::tok_bitwise_and): {
        llvm::Value* LInt = context.Builder.CreateFPToSI(LV, llvm::Type::getInt64Ty(context.TheContext), "andlhsint");
        llvm::Value* RInt = context.Builder.CreateFPToSI(RV, llvm::Type::getInt64Ty(context.TheContext), "andrhsint");
        return TypedValue(context.Builder.CreateSIToFP(context.Builder.CreateAnd(LInt, RInt, "andtmp"), llvm::Type::getDoubleTy(context.TheContext), "andtmpfp"), TypeKind::Double);
    }
    case static_cast<int>(TokenType::tok_bitwise_or): {
        llvm::Value* LInt = context.Builder.CreateFPToSI(LV, llvm::Type::getInt64Ty(context.TheContext), "orlhsint");
        llvm::Value* RInt = context.Builder.CreateFPToSI(RV, llvm::Type::getInt64Ty(context.TheContext), "orrhsint");
        return TypedValue(context.Builder.CreateSIToFP(context.Builder.CreateOr(LInt, RInt, "ortmp"), llvm::Type::getDoubleTy(context.TheContext), "ortmpfp"), TypeKind::Double);
    }
    case static_cast<int>(TokenType::tok_bitwise_xor): {
        llvm::Value* LInt = context.Builder.CreateFPToSI(LV, llvm::Type::getInt64Ty(context.TheContext), "xorlhsint");
        llvm::Value* RInt = context.Builder.CreateFPToSI(RV, llvm::Type::getInt64Ty(context.TheContext), "xorrhsint");
        return TypedValue(context.Builder.CreateSIToFP(context.Builder.CreateXor(LInt, RInt, "xortmp"), llvm::Type::getDoubleTy(context.TheContext), "xortmpfp"), TypeKind::Double);
    }
    case static_cast<int>(TokenType::tok_bitwise_shl): {
        llvm::Value* LInt = context.Builder.CreateFPToSI(LV, llvm::Type::getInt64Ty(context.TheContext), "shllhsint");
        llvm::Value* RInt = context.Builder.CreateFPToSI(RV, llvm::Type::getInt64Ty(context.TheContext), "shlrhsint");
        return TypedValue(context.Builder.CreateSIToFP(context.Builder.CreateShl(LInt, RInt, "shltmp"), llvm::Type::getDoubleTy(context.TheContext), "shltmpfp"), TypeKind::Double);
    }
    case static_cast<int>(TokenType::tok_bitwise_shr): {
        llvm::Value* LInt = context.Builder.CreateFPToSI(LV, llvm::Type::getInt64Ty(context.TheContext), "shrlhsint");
        llvm::Value* RInt = context.Builder.CreateFPToSI(RV, llvm::Type::getInt64Ty(context.TheContext), "shrrhsint");
        return TypedValue(context.Builder.CreateSIToFP(context.Builder.CreateAShr(LInt, RInt, "shrtmp"), llvm::Type::getDoubleTy(context.TheContext), "shrtmpfp"), TypeKind::Double);
    }
    case static_cast<int>(TokenType::tok_power): {
        llvm::Function* PowF = context.TheModule->getFunction("llvm.pow.f64");
        if (!PowF) {
            llvm::FunctionType* PowFTy = llvm::FunctionType::get(llvm::Type::getDoubleTy(context.TheContext), {llvm::Type::getDoubleTy(context.TheContext), llvm::Type::getDoubleTy(context.TheContext)}, false);
            PowF = llvm::Function::Create(PowFTy, llvm::Function::ExternalLinkage, "llvm.pow.f64", context.TheModule.get());
        }
        return TypedValue(context.Builder.CreateCall(PowF, {LV, RV}, "powtmp"), TypeKind::Double);
    }
    case '<': return createBoolResult(context.Builder.CreateFCmpOLT(LV, RV, "cmptmp"));
    case '>': return createBoolResult(context.Builder.CreateFCmpOGT(LV, RV, "cmptmp"));
    case static_cast<int>(TokenType::tok_less_equal): return createBoolResult(context.Builder.CreateFCmpOLE(LV, RV, "cmptmp"));
    case static_cast<int>(TokenType::tok_greater_equal): return createBoolResult(context.Builder.CreateFCmpOGE(LV, RV, "cmptmp"));
    case static_cast<int>(TokenType::tok_equal): return createBoolResult(context.Builder.CreateFCmpOEQ(LV, RV, "cmptmp"));
    case static_cast<int>(TokenType::tok_not_equal): return createBoolResult(context.Builder.CreateFCmpUNE(LV, RV, "cmptmp"));
    case static_cast<int>(TokenType::tok_logical_and): {
        llvm::Value* LCmp = context.Builder.CreateFCmpONE(LV, llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0)), "andlhs");
        llvm::Value* RCmp = context.Builder.CreateFCmpONE(RV, llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0)), "andrhs");
        return createBoolResult(context.Builder.CreateAnd(LCmp, RCmp, "andtmp"));
    }
    case static_cast<int>(TokenType::tok_logical_or): {
        llvm::Value* LCmp = context.Builder.CreateFCmpONE(LV, llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0)), "orlhs");
        llvm::Value* RCmp = context.Builder.CreateFCmpONE(RV, llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0)), "orrhs");
        return createBoolResult(context.Builder.CreateOr(LCmp, RCmp, "ortmp"));
    }
    case static_cast<int>(TokenType::tok_ew_mul): {
        llvm::Function* EwMulF = context.TheModule->getFunction("flux_vector_mul_elementwise");
        if (!EwMulF) return TypedValue(context.Builder.CreateFMul(LV, RV, "ewmultmp"), TypeKind::Double);
        return TypedValue(context.Builder.CreateCall(EwMulF, {LV, RV}, "ewmultmp"), TypeKind::Double);
    }
    case static_cast<int>(TokenType::tok_ew_div): {
        llvm::Function* EwDivF = context.TheModule->getFunction("flux_vector_div_elementwise");
        if (!EwDivF) return TypedValue(context.Builder.CreateFDiv(LV, RV, "ewdivtmp"), TypeKind::Double);
        return TypedValue(context.Builder.CreateCall(EwDivF, {LV, RV}, "ewdivtmp"), TypeKind::Double);
    }
    case static_cast<int>(TokenType::tok_ew_power): {
        llvm::Function* PowF = context.TheModule->getFunction("llvm.pow.f64");
        if (!PowF) {
            llvm::FunctionType* PowFTy = llvm::FunctionType::get(llvm::Type::getDoubleTy(context.TheContext), {llvm::Type::getDoubleTy(context.TheContext), llvm::Type::getDoubleTy(context.TheContext)}, false);
            PowF = llvm::Function::Create(PowFTy, llvm::Function::ExternalLinkage, "llvm.pow.f64", context.TheModule.get());
        }
        return TypedValue(context.Builder.CreateCall(PowF, {LV, RV}, "ewpowtmp"), TypeKind::Double);
    }
    default:
        std::cerr << "Invalid binary operator: " << Op << std::endl;
        return TypedValue();
    }
}

TypedValue UnaryExprAST::codegen(CodegenContext& context) {
    TypedValue OperandTV = Operand->codegen(context);
    if (!OperandTV.Val) return TypedValue();

    if (isComplexType(OperandTV)) {
        llvm::Value* Real = context.Builder.CreateExtractValue(OperandTV.Val, 0, "real");
        llvm::Value* Imag = context.Builder.CreateExtractValue(OperandTV.Val, 1, "imag");
        switch (Op) {
        case '-': {
            llvm::Value* ResReal = context.Builder.CreateFNeg(Real, "negre");
            llvm::Value* ResImag = context.Builder.CreateFNeg(Imag, "negim");
            llvm::Value* Res = llvm::UndefValue::get(OperandTV.Val->getType());
            Res = context.Builder.CreateInsertValue(Res, ResReal, 0);
            return TypedValue(context.Builder.CreateInsertValue(Res, ResImag, 1), TypeKind::Complex);
        }
        case '+': return OperandTV;
        default: return TypedValue();
        }
    }

    llvm::Value* V = OperandTV.Val;
    switch (Op) {
    case '-':
        if (V->getType()->isIntegerTy()) return TypedValue(context.Builder.CreateNeg(V, "unaryminus"), OperandTV.Type);
        return TypedValue(context.Builder.CreateFNeg(V, "unaryminus"), OperandTV.Type);
    case '+': return OperandTV;
    case '~':
    case static_cast<int>(TokenType::tok_bitwise_not): {
        llvm::Value* OperandInt = context.Builder.CreateFPToSI(V, llvm::Type::getInt64Ty(context.TheContext), "notint");
        return TypedValue(context.Builder.CreateSIToFP(context.Builder.CreateNot(OperandInt, "nottmp"), llvm::Type::getDoubleTy(context.TheContext), "nottmpfp"), TypeKind::Double);
    }
    case '!':
    case static_cast<int>(TokenType::tok_logical_not): {
        llvm::Value* IsNonZero = context.Builder.CreateFCmpONE(V, llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0)), "isnonzero");
        return TypedValue(context.Builder.CreateUIToFP(context.Builder.CreateNot(IsNonZero, "lognot"), llvm::Type::getDoubleTy(context.TheContext), "booltmp"), TypeKind::Double);
    }
    default: return TypedValue();
    }
}

TypedValue TransposeExprAST::codegen(CodegenContext& context) {
    TypedValue V = Operand->codegen(context);
    if (!V.Val) return TypedValue();
    llvm::Function* TransposeF = context.TheModule->getFunction("flux_matrix_transpose");
    if (!TransposeF) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        llvm::FunctionType* TransposeFTy = llvm::FunctionType::get(DoubleTy, {DoubleTy}, false);
        TransposeF = llvm::Function::Create(TransposeFTy, llvm::Function::ExternalLinkage, "flux_matrix_transpose", context.TheModule.get());
    }
    return TypedValue(context.Builder.CreateCall(TransposeF, {V.Val}, "transtmp"), TypeKind::Double);
}

TypedValue CallExprAST::codegen(CodegenContext& context) {
    llvm::Function* CalleeF = context.TheModule->getFunction(Callee);
    if (!CalleeF) return TypedValue();
    if (CalleeF->arg_size() != Args.size()) return TypedValue();
    std::vector<llvm::Value*> ArgsV;
    for (unsigned i = 0, e = Args.size(); i != e; ++i) {
        TypedValue ArgTV = Args[i]->codegen(context);
        if (!ArgTV.Val) return TypedValue();
        llvm::Value* ArgV = ArgTV.Val;
        llvm::Type* ExpectedTy = CalleeF->getArg(i)->getType();
        if (ArgV->getType() != ExpectedTy) {
            if (ExpectedTy->isIntegerTy() && ArgV->getType()->isFloatingPointTy()) ArgV = context.Builder.CreateFPToSI(ArgV, ExpectedTy, "cast");
            else if (ExpectedTy->isFloatingPointTy() && ArgV->getType()->isIntegerTy()) ArgV = context.Builder.CreateSIToFP(ArgV, ExpectedTy, "cast");
            else if (ExpectedTy->isStructTy() && !ArgV->getType()->isStructTy()) ArgV = promoteToComplex(ArgTV, context).Val;
        }
        ArgsV.push_back(ArgV);
    }
    return TypedValue(context.Builder.CreateCall(CalleeF, ArgsV, "calltmp"), typeFromLLVM(CalleeF->getReturnType()));
}

TypedValue IfExprAST::codegen(CodegenContext& context) {
    TypedValue CondTV = Cond->codegen(context);
    if (!CondTV.Val) return TypedValue();
    llvm::Value* CondV = context.Builder.CreateFCmpONE(CondTV.Val, llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0)), "ifcond");
    llvm::Function* TheFunction = context.Builder.GetInsertBlock()->getParent();
    llvm::BasicBlock* ThenBB = llvm::BasicBlock::Create(context.TheContext, "then", TheFunction);
    llvm::BasicBlock* ElseBB = llvm::BasicBlock::Create(context.TheContext, "else");
    llvm::BasicBlock* MergeBB = llvm::BasicBlock::Create(context.TheContext, "ifcont");
    context.Builder.CreateCondBr(CondV, ThenBB, ElseBB);
    context.Builder.SetInsertPoint(ThenBB);
    TypedValue ThenTV = Then->codegen(context);
    if (!ThenTV.Val) return TypedValue();
    context.Builder.CreateBr(MergeBB);
    ThenBB = context.Builder.GetInsertBlock();
    TheFunction->insert(TheFunction->end(), ElseBB);
    context.Builder.SetInsertPoint(ElseBB);
    TypedValue ElseTV = Else->codegen(context);
    if (!ElseTV.Val) return TypedValue();
    context.Builder.CreateBr(MergeBB);
    ElseBB = context.Builder.GetInsertBlock();
    TheFunction->insert(TheFunction->end(), MergeBB);
    context.Builder.SetInsertPoint(MergeBB);
    llvm::PHINode* PN = context.Builder.CreatePHI(ThenTV.Val->getType(), 2, "iftmp");
    PN->addIncoming(ThenTV.Val, ThenBB);
    PN->addIncoming(ElseTV.Val, ElseBB);
    return TypedValue(PN, ThenTV.Type);
}

TypedValue ForExprAST::codegen(CodegenContext& context) {
    TypedValue StartTV = Start->codegen(context);
    TypedValue EndTV = End->codegen(context);
    TypedValue StepTV = Step ? Step->codegen(context) : TypedValue(llvm::ConstantFP::get(context.TheContext, llvm::APFloat(1.0)), TypeKind::Double);
    if (!StartTV.Val || !EndTV.Val || !StepTV.Val) return TypedValue();
    llvm::Function* TheFunction = context.Builder.GetInsertBlock()->getParent();
    llvm::BasicBlock* PreheaderBB = context.Builder.GetInsertBlock();
    llvm::BasicBlock* LoopBB = llvm::BasicBlock::Create(context.TheContext, "loop", TheFunction);
    llvm::BasicBlock* AfterBB = llvm::BasicBlock::Create(context.TheContext, "afterloop");
    context.Builder.CreateBr(LoopBB);
    context.Builder.SetInsertPoint(LoopBB);
    llvm::PHINode* Variable = context.Builder.CreatePHI(llvm::Type::getDoubleTy(context.TheContext), 2, VarName.c_str());
    Variable->addIncoming(StartTV.Val, PreheaderBB);
    llvm::Value* OldVal = context.NamedValues[VarName];
    context.NamedValues[VarName] = Variable;
    TypedValue BodyTV = Body->codegen(context);
    if (!BodyTV.Val) return TypedValue();
    llvm::Value* NextVar = context.Builder.CreateFAdd(Variable, StepTV.Val, "nextvar");
    Variable->addIncoming(NextVar, context.Builder.GetInsertBlock());
    context.NamedValues[VarName] = OldVal;
    llvm::Value* EndCond = context.Builder.CreateFCmpOLT(NextVar, EndTV.Val, "loopcond");
    context.Builder.CreateCondBr(EndCond, LoopBB, AfterBB);
    TheFunction->insert(TheFunction->end(), AfterBB);
    context.Builder.SetInsertPoint(AfterBB);
    return BodyTV;
}

TypedValue WhileExprAST::codegen(CodegenContext& context) {
    llvm::Function* TheFunction = context.Builder.GetInsertBlock()->getParent();
    llvm::BasicBlock* CondBB = llvm::BasicBlock::Create(context.TheContext, "whilecond", TheFunction);
    llvm::BasicBlock* BodyBB = llvm::BasicBlock::Create(context.TheContext, "whilebody");
    llvm::BasicBlock* AfterBB = llvm::BasicBlock::Create(context.TheContext, "afterwhile");
    context.Builder.CreateBr(CondBB);
    context.Builder.SetInsertPoint(CondBB);
    TypedValue CondTV = Cond->codegen(context);
    if (!CondTV.Val) return TypedValue();
    llvm::Value* CondV = context.Builder.CreateFCmpONE(CondTV.Val, llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0)), "whilecond");
    context.Builder.CreateCondBr(CondV, BodyBB, AfterBB);
    TheFunction->insert(TheFunction->end(), BodyBB);
    context.Builder.SetInsertPoint(BodyBB);
    TypedValue BodyTV = Body->codegen(context);
    if (!BodyTV.Val) return TypedValue();
    context.Builder.CreateBr(CondBB);
    TheFunction->insert(TheFunction->end(), AfterBB);
    context.Builder.SetInsertPoint(AfterBB);
    return BodyTV;
}

TypedValue LetExprAST::codegen(CodegenContext& context) {
    TypedValue InitTV = Init->codegen(context);
    if (!InitTV.Val) return TypedValue();
    llvm::Type* VarTy = Type.getLLVMType(context.TheContext);
    llvm::Value* InitV = InitTV.Val;
    if (InitV->getType() != VarTy) {
        if (VarTy->isIntegerTy() && InitV->getType()->isFloatingPointTy()) InitV = context.Builder.CreateFPToSI(InitV, VarTy, "cast");
        else if (VarTy->isFloatingPointTy() && InitV->getType()->isIntegerTy()) InitV = context.Builder.CreateSIToFP(InitV, VarTy, "cast");
        else if (VarTy->isStructTy() && !InitV->getType()->isStructTy()) InitV = promoteToComplex(InitTV, context).Val;
    }
    llvm::AllocaInst* Alloca = context.Builder.CreateAlloca(VarTy, nullptr, VarName.c_str());
    context.Builder.CreateStore(InitV, Alloca);
    llvm::Value* OldVal = context.NamedValues[VarName];
    context.NamedValues[VarName] = Alloca;
    TypedValue BodyTV = Body->codegen(context);
    if (!BodyTV.Val) return TypedValue();
    context.NamedValues[VarName] = OldVal;
    return BodyTV;
}

TypedValue LambdaExprAST::codegen(CodegenContext& context) {
    std::vector<llvm::Type*> ArgTypes(Args.size(), llvm::Type::getDoubleTy(context.TheContext));
    llvm::FunctionType* LambdaTy = llvm::FunctionType::get(llvm::Type::getDoubleTy(context.TheContext), ArgTypes, false);
    llvm::Function* LambdaFn = llvm::Function::Create(LambdaTy, llvm::Function::InternalLinkage, "lambda", context.TheModule.get());
    llvm::BasicBlock* BB = llvm::BasicBlock::Create(context.TheContext, "entry", LambdaFn);
    llvm::BasicBlock* OldBB = context.Builder.GetInsertBlock();
    std::map<std::string, llvm::Value*> OldNamedValues = context.NamedValues;
    context.NamedValues.clear();
    {
        llvm::IRBuilder<> LambdaBuilder(BB);
        unsigned Idx = 0;
        for (auto& Arg : LambdaFn->args()) {
            Arg.setName(Args[Idx++]);
            llvm::AllocaInst* Alloca = LambdaBuilder.CreateAlloca(llvm::Type::getDoubleTy(context.TheContext), nullptr, Arg.getName());
            LambdaBuilder.CreateStore(&Arg, Alloca);
            context.NamedValues[std::string(Arg.getName())] = Alloca;
        }
        context.Builder.SetInsertPoint(BB);
        TypedValue BodyTV = Body->codegen(context);
        if (BodyTV.Val) context.Builder.CreateRet(BodyTV.Val);
    }
    if (OldBB) context.Builder.SetInsertPoint(OldBB);
    context.NamedValues = OldNamedValues;
    return TypedValue(llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0)), TypeKind::Double);
}

TypedValue VectorExprAST::codegen(CodegenContext& context) {
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
    llvm::Type* Int32Ty = llvm::Type::getInt32Ty(context.TheContext);
    llvm::Type* Int64Ty = llvm::Type::getInt64Ty(context.TheContext);
    llvm::Type* VoidPtrTy = llvm::PointerType::get(context.TheContext, 0);
    llvm::Function* MallocF = context.TheModule->getFunction("malloc");
    if (!MallocF) MallocF = llvm::Function::Create(llvm::FunctionType::get(VoidPtrTy, { Int32Ty }, false), llvm::Function::ExternalLinkage, "malloc", context.TheModule.get());
    llvm::Value* DataSize = llvm::ConstantInt::get(Int32Ty, Elements.size() * 8);
    llvm::Value* DataPtr = context.Builder.CreateCall(MallocF, {DataSize}, "vec_data");
    for (size_t i = 0; i < Elements.size(); ++i) {
        TypedValue ElemTV = Elements[i]->codegen(context);
        if (ElemTV.Val) {
            llvm::Value* ElemPtr = context.Builder.CreateInBoundsGEP(DoubleTy, DataPtr, {llvm::ConstantInt::get(Int64Ty, i)}, "elem_ptr");
            context.Builder.CreateStore(ElemTV.Val, ElemPtr);
        }
    }
    llvm::Function* CreateVecSumF = context.TheModule->getFunction("flux_create_vector_sum");
    if (!CreateVecSumF) CreateVecSumF = llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getDoubleTy(context.TheContext), { VoidPtrTy, Int32Ty }, false), llvm::Function::ExternalLinkage, "flux_create_vector_sum", context.TheModule.get());
    return TypedValue(context.Builder.CreateCall(CreateVecSumF, {DataPtr, llvm::ConstantInt::get(Int32Ty, Elements.size())}, "vec_sum"), TypeKind::Double);
}

TypedValue MatrixExprAST::codegen(CodegenContext& context) {
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
    llvm::Type* Int32Ty = llvm::Type::getInt32Ty(context.TheContext);
    llvm::Type* Int64Ty = llvm::Type::getInt64Ty(context.TheContext);
    llvm::Type* VoidPtrTy = llvm::PointerType::get(context.TheContext, 0);
    llvm::Function* MallocF = context.TheModule->getFunction("malloc");
    if (!MallocF) MallocF = llvm::Function::Create(llvm::FunctionType::get(VoidPtrTy, { Int32Ty }, false), llvm::Function::ExternalLinkage, "malloc", context.TheModule.get());
    llvm::Value* DataSize = llvm::ConstantInt::get(Int32Ty, NumRows * NumCols * 8);
    llvm::Value* DataPtr = context.Builder.CreateCall(MallocF, {DataSize}, "mat_data");
    for (int row = 0; row < NumRows; ++row) {
        for (int col = 0; col < NumCols; ++col) {
            TypedValue ElemTV = Rows[row][col]->codegen(context);
            if (ElemTV.Val) {
                llvm::Value* ElemPtr = context.Builder.CreateInBoundsGEP(DoubleTy, DataPtr, {llvm::ConstantInt::get(Int64Ty, row * NumCols + col)}, "elem_ptr");
                context.Builder.CreateStore(ElemTV.Val, ElemPtr);
            }
        }
    }
    llvm::Function* CreateMatF = context.TheModule->getFunction("flux_create_matrix");
    if (!CreateMatF) CreateMatF = llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getDoubleTy(context.TheContext), { VoidPtrTy, Int32Ty, Int32Ty }, false), llvm::Function::ExternalLinkage, "flux_create_matrix", context.TheModule.get());
    return TypedValue(context.Builder.CreateCall(CreateMatF, {DataPtr, llvm::ConstantInt::get(Int32Ty, NumRows), llvm::ConstantInt::get(Int32Ty, NumCols)}, "mat_result"), TypeKind::Double);
}

TypedValue IndexExprAST::codegen(CodegenContext& context) {
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
    llvm::Type* Int32Ty = llvm::Type::getInt32Ty(context.TheContext);
    llvm::Type* VoidPtrTy = llvm::PointerType::get(context.TheContext, 0);
    if (IsMatrixIndex) {
        llvm::Function* GetElemF = context.TheModule->getFunction("flux_matrix_get");
        if (!GetElemF) GetElemF = llvm::Function::Create(llvm::FunctionType::get(DoubleTy, { VoidPtrTy, Int32Ty, Int32Ty, Int32Ty, Int32Ty }, false), llvm::Function::ExternalLinkage, "flux_matrix_get", context.TheModule.get());
        return TypedValue(llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0)), TypeKind::Double);
    }
    return TypedValue(llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0)), TypeKind::Double);
}

TypedValue RangeExprAST::codegen(CodegenContext& context) {
    TypedValue StartTV = Start->codegen(context);
    TypedValue EndTV = End->codegen(context);
    TypedValue StepTV = Step ? Step->codegen(context) : TypedValue(llvm::ConstantFP::get(context.TheContext, llvm::APFloat(1.0)), TypeKind::Double);
    llvm::Function* RangeF = context.TheModule->getFunction("flux_create_range_sum");
    if (!RangeF) RangeF = llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getDoubleTy(context.TheContext), { llvm::Type::getDoubleTy(context.TheContext), llvm::Type::getDoubleTy(context.TheContext), llvm::Type::getDoubleTy(context.TheContext) }, false), llvm::Function::ExternalLinkage, "flux_create_range_sum", context.TheModule.get());
    return TypedValue(context.Builder.CreateCall(RangeF, {StartTV.Val, StepTV.Val, EndTV.Val}, "range_sum"), TypeKind::Double);
}

llvm::Function* PrototypeAST::codegen(CodegenContext& context) {
    std::vector<llvm::Type*> ArgTypes;
    for (const auto& Arg : Args) ArgTypes.push_back(Arg.second.getLLVMType(context.TheContext));
    llvm::FunctionType* FT = llvm::FunctionType::get(ReturnType.getLLVMType(context.TheContext), ArgTypes, false);
    llvm::Function* F = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, Name, context.TheModule.get());
    unsigned Idx = 0;
    for (auto& Arg : F->args()) Arg.setName(Args[Idx++].first);
    return F;
}

llvm::Function* FunctionAST::codegen(CodegenContext& context) {
    llvm::Function* TheFunction = context.TheModule->getFunction(Proto->getName());
    if (!TheFunction) TheFunction = Proto->codegen(context);
    if (!TheFunction) return nullptr;
    llvm::BasicBlock* BB = llvm::BasicBlock::Create(context.TheContext, "entry", TheFunction);
    context.Builder.SetInsertPoint(BB);
    context.NamedValues.clear();
    const auto& ArgTypes = Proto->getArgs();
    unsigned Idx = 0;
    for (auto& Arg : TheFunction->args()) {
        llvm::Type* ArgTy = ArgTypes[Idx].second.getLLVMType(context.TheContext);
        llvm::AllocaInst* Alloca = context.Builder.CreateAlloca(ArgTy, nullptr, std::string(Arg.getName()));
        context.Builder.CreateStore(&Arg, Alloca);
        context.NamedValues[std::string(Arg.getName())] = Alloca;
        Idx++;
    }
    if (TypedValue RetTV = Body->codegen(context)) {
        llvm::Value* RetVal = RetTV.Val;
        llvm::Type* RetTy = Proto->getReturnType().getLLVMType(context.TheContext);
        if (RetVal->getType() != RetTy) {
            if (RetTy->isIntegerTy() && RetVal->getType()->isFloatingPointTy()) RetVal = context.Builder.CreateFPToSI(RetVal, RetTy, "cast");
            else if (RetTy->isFloatingPointTy() && RetVal->getType()->isIntegerTy()) RetVal = context.Builder.CreateSIToFP(RetVal, RetTy, "cast");
            else if (RetTy->isStructTy() && !RetVal->getType()->isStructTy()) RetVal = promoteToComplex(RetTV, context).Val;
        }
        context.Builder.CreateRet(RetVal);
        llvm::verifyFunction(*TheFunction);
        return TheFunction;
    }
    TheFunction->eraseFromParent();
    return nullptr;
}

TypedValue VoltageExprAST::codegen(CodegenContext& context) {
    llvm::Function* GetVF = context.TheModule->getFunction("flux_get_voltage");
    if (!GetVF) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        llvm::Type* CharPtrTy = llvm::PointerType::get(llvm::Type::getInt8Ty(context.TheContext), 0);
        GetVF = llvm::Function::Create(llvm::FunctionType::get(DoubleTy, {CharPtrTy}, false), llvm::Function::ExternalLinkage, "flux_get_voltage", context.TheModule.get());
    }
    return TypedValue(context.Builder.CreateCall(GetVF, {context.Builder.CreateGlobalStringPtr(NodeName)}, "v"), TypeKind::Double);
}

TypedValue CurrentExprAST::codegen(CodegenContext& context) {
    llvm::Function* GetIF = context.TheModule->getFunction("flux_get_current");
    if (!GetIF) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        llvm::Type* CharPtrTy = llvm::PointerType::get(llvm::Type::getInt8Ty(context.TheContext), 0);
        GetIF = llvm::Function::Create(llvm::FunctionType::get(DoubleTy, {CharPtrTy}, false), llvm::Function::ExternalLinkage, "flux_get_current", context.TheModule.get());
    }
    return TypedValue(context.Builder.CreateCall(GetIF, {context.Builder.CreateGlobalStringPtr(BranchName)}, "i"), TypeKind::Double);
}

} // namespace Flux
