#include "flux/compiler/ast.h"
#include "flux/compiler/lexer.h"
#include <llvm/IR/Verifier.h>
#include <llvm/BinaryFormat/Dwarf.h>
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
    if (Ty->isStructTy()) {
        if (Ty->getStructNumElements() == 3) return FluxType(TypeKind::Matrix);
        return FluxType(TypeKind::Complex);
    }
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

TypedValue ImportExprAST::codegen(CodegenContext& context) {
    // Import is handled at the JITEngine/ModuleLoader level
    // This codegen just marks the import as processed
    // The actual module loading happens before codegen
    
    // If we have an alias, register it in the context
    // This allows namespace::function syntax to be resolved
    if (!Alias.empty()) {
        // Store alias mapping for later resolution
        // This would be used by VariableExprAST to resolve namespaced symbols
    }
    
    return TypedValue(llvm::ConstantFP::get(context.TheContext, llvm::APFloat(1.0)), TypeKind::Double);
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

TypedValue MemberExprAST::codegen(CodegenContext& context) {
    if (auto* OBJ = dynamic_cast<VariableExprAST*>(Object.get())) {
        std::string full_name = OBJ->getName() + "." + MemberName;
        // Check if there is a local variable with this full name first (e.g. from an import)
        if (context.NamedValues.count(full_name)) {
            llvm::Value* V = context.NamedValues[full_name];
            if (auto* Alloca = llvm::dyn_cast<llvm::AllocaInst>(V)) {
                return TypedValue(context.Builder.CreateLoad(Alloca->getAllocatedType(), Alloca, full_name.c_str()), typeFromLLVM(Alloca->getAllocatedType()));
            }
            return TypedValue(V, typeFromLLVM(V->getType()));
        }
        
        // Treat as SPICE parameter probe: device.param
        llvm::Function* GetPF = context.TheModule->getFunction("flux_get_parameter");
        if (!GetPF) {
            llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
            llvm::Type* CharPtrTy = llvm::PointerType::get(llvm::Type::getInt8Ty(context.TheContext), 0);
            GetPF = llvm::Function::Create(llvm::FunctionType::get(DoubleTy, {CharPtrTy}, false), llvm::Function::ExternalLinkage, "flux_get_parameter", context.TheModule.get());
        }
        return TypedValue(context.Builder.CreateCall(GetPF, {context.Builder.CreateGlobalStringPtr(full_name)}, "getp"), TypeKind::Double);
    }
    std::cerr << "Member access only supported on top-level variables currently" << std::endl;
    return TypedValue();
}

TypedValue AssignExprAST::codegen(CodegenContext& context) {
    std::string TargetName;
    bool isSpice = false;
    llvm::Value* Variable = nullptr;

    if (auto* VAR = dynamic_cast<VariableExprAST*>(LHS.get())) {
        TargetName = VAR->getName();
        Variable = context.NamedValues[TargetName];
    } else if (auto* MEM = dynamic_cast<MemberExprAST*>(LHS.get())) {
        if (auto* OBJ = dynamic_cast<VariableExprAST*>(const_cast<ExprAST*>(MEM->getObject()))) {
            TargetName = OBJ->getName() + "." + MEM->getMemberName();
            isSpice = true;
        }
    } else if (auto* PAR = dynamic_cast<ParameterExprAST*>(LHS.get())) {
        TargetName = PAR->getParamName();
        isSpice = true;
    }

    if (isSpice) {
        llvm::Function* SetPF = context.TheModule->getFunction("flux_set_parameter");
        if (!SetPF) {
            llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
            llvm::Type* CharPtrTy = llvm::PointerType::get(llvm::Type::getInt8Ty(context.TheContext), 0);
            SetPF = llvm::Function::Create(llvm::FunctionType::get(DoubleTy, {CharPtrTy, DoubleTy}, false), llvm::Function::ExternalLinkage, "flux_set_parameter", context.TheModule.get());
        }
        TypedValue NewValTV = Val->codegen(context);
        if (!NewValTV.Val) return TypedValue();
        return TypedValue(context.Builder.CreateCall(SetPF, {context.Builder.CreateGlobalStringPtr(TargetName), NewValTV.Val}, "setp"), TypeKind::Double);
    }

    if (!Variable) {
        std::cerr << "Unknown variable name: " << TargetName << std::endl;
        return TypedValue();
    }

    if (!llvm::isa<llvm::AllocaInst>(Variable)) {
        std::cerr << "Cannot assign to read-only variable: " << TargetName << std::endl;
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

    if (L.Type.Kind == TypeKind::Matrix || R.Type.Kind == TypeKind::Matrix) {
        llvm::Type* Int32Ty = llvm::Type::getInt32Ty(context.TheContext);
        llvm::Type* VoidPtrTy = llvm::PointerType::get(context.TheContext, 0);
        llvm::StructType* MatStructTy = llvm::cast<llvm::StructType>(FluxType(TypeKind::Matrix).getLLVMType(context.TheContext));
        
        bool isMatMat = (L.Type.Kind == TypeKind::Matrix && R.Type.Kind == TypeKind::Matrix);
        bool isMatSca = (L.Type.Kind == TypeKind::Matrix && R.Type.Kind != TypeKind::Matrix);
        bool isScaMat = (L.Type.Kind != TypeKind::Matrix && R.Type.Kind == TypeKind::Matrix);
        
        llvm::Value* LPtr = nullptr;
        llvm::Value* RPtr = nullptr;
        llvm::Value* LRows = nullptr;
        llvm::Value* LCols = nullptr;
        llvm::Value* RRows = nullptr;
        llvm::Value* RCols = nullptr;
        
        if (L.Type.Kind == TypeKind::Matrix) {
            LPtr = context.Builder.CreateExtractValue(L.Val, 0, "l_mat_ptr");
            LRows = context.Builder.CreateExtractValue(L.Val, 1, "l_mat_rows");
            LCols = context.Builder.CreateExtractValue(L.Val, 2, "l_mat_cols");
        }
        if (R.Type.Kind == TypeKind::Matrix) {
            RPtr = context.Builder.CreateExtractValue(R.Val, 0, "r_mat_ptr");
            RRows = context.Builder.CreateExtractValue(R.Val, 1, "r_mat_rows");
            RCols = context.Builder.CreateExtractValue(R.Val, 2, "r_mat_cols");
        }
        
        llvm::Value* ResPtr = nullptr;
        llvm::Value* ResRows = nullptr;
        llvm::Value* ResCols = nullptr;
        
        switch (Op) {
        case '+': {
            if (isMatMat) {
                llvm::Function* F = context.TheModule->getFunction("flux_matrix_add");
                if (!F) F = llvm::Function::Create(llvm::FunctionType::get(VoidPtrTy, {VoidPtrTy, VoidPtrTy}, false), llvm::Function::ExternalLinkage, "flux_matrix_add", context.TheModule.get());
                ResPtr = context.Builder.CreateCall(F, {LPtr, RPtr}, "mat_add");
            } else if (isMatSca) {
                llvm::Function* F = context.TheModule->getFunction("flux_matrix_add_ms");
                if (!F) F = llvm::Function::Create(llvm::FunctionType::get(VoidPtrTy, {VoidPtrTy, llvm::Type::getDoubleTy(context.TheContext)}, false), llvm::Function::ExternalLinkage, "flux_matrix_add_ms", context.TheModule.get());
                ResPtr = context.Builder.CreateCall(F, {LPtr, R.Val}, "mat_add_ms");
            } else if (isScaMat) {
                llvm::Function* F = context.TheModule->getFunction("flux_matrix_add_ms");
                if (!F) F = llvm::Function::Create(llvm::FunctionType::get(VoidPtrTy, {VoidPtrTy, llvm::Type::getDoubleTy(context.TheContext)}, false), llvm::Function::ExternalLinkage, "flux_matrix_add_ms", context.TheModule.get());
                ResPtr = context.Builder.CreateCall(F, {RPtr, L.Val}, "mat_add_sm");
            }
            break;
        }
        case '-': {
            if (isMatMat) {
                llvm::Function* F = context.TheModule->getFunction("flux_matrix_sub");
                if (!F) F = llvm::Function::Create(llvm::FunctionType::get(VoidPtrTy, {VoidPtrTy, VoidPtrTy}, false), llvm::Function::ExternalLinkage, "flux_matrix_sub", context.TheModule.get());
                ResPtr = context.Builder.CreateCall(F, {LPtr, RPtr}, "mat_sub");
            } else if (isMatSca) {
                llvm::Function* F = context.TheModule->getFunction("flux_matrix_sub_ms");
                if (!F) F = llvm::Function::Create(llvm::FunctionType::get(VoidPtrTy, {VoidPtrTy, llvm::Type::getDoubleTy(context.TheContext)}, false), llvm::Function::ExternalLinkage, "flux_matrix_sub_ms", context.TheModule.get());
                ResPtr = context.Builder.CreateCall(F, {LPtr, R.Val}, "mat_sub_ms");
            } else if (isScaMat) {
                // Scalar - Matrix: need a special helper to do s - M
                llvm::Function* F = context.TheModule->getFunction("flux_matrix_sub_sm");
                if (!F) F = llvm::Function::Create(llvm::FunctionType::get(VoidPtrTy, {llvm::Type::getDoubleTy(context.TheContext), VoidPtrTy}, false), llvm::Function::ExternalLinkage, "flux_matrix_sub_sm", context.TheModule.get());
                ResPtr = context.Builder.CreateCall(F, {L.Val, RPtr}, "mat_sub_sm");
            }
            break;
        }
        case '*': {
            if (isMatMat) {
                llvm::Function* F = context.TheModule->getFunction("flux_matrix_mul");
                if (!F) F = llvm::Function::Create(llvm::FunctionType::get(VoidPtrTy, {VoidPtrTy, VoidPtrTy}, false), llvm::Function::ExternalLinkage, "flux_matrix_mul", context.TheModule.get());
                ResPtr = context.Builder.CreateCall(F, {LPtr, RPtr}, "mat_mul");
            } else if (isMatSca) {
                llvm::Function* F = context.TheModule->getFunction("flux_matrix_mul_ms");
                if (!F) F = llvm::Function::Create(llvm::FunctionType::get(VoidPtrTy, {VoidPtrTy, llvm::Type::getDoubleTy(context.TheContext)}, false), llvm::Function::ExternalLinkage, "flux_matrix_mul_ms", context.TheModule.get());
                ResPtr = context.Builder.CreateCall(F, {LPtr, R.Val}, "mat_mul_ms");
            } else if (isScaMat) {
                llvm::Function* F = context.TheModule->getFunction("flux_matrix_mul_ms");
                if (!F) F = llvm::Function::Create(llvm::FunctionType::get(VoidPtrTy, {VoidPtrTy, llvm::Type::getDoubleTy(context.TheContext)}, false), llvm::Function::ExternalLinkage, "flux_matrix_mul_ms", context.TheModule.get());
                ResPtr = context.Builder.CreateCall(F, {RPtr, L.Val}, "mat_mul_sm");
            }
            break;
        }
        case static_cast<int>(TokenType::tok_ew_mul): {
            if (isMatMat) {
                llvm::Function* F = context.TheModule->getFunction("flux_matrix_ew_mul");
                if (!F) F = llvm::Function::Create(llvm::FunctionType::get(VoidPtrTy, {VoidPtrTy, VoidPtrTy}, false), llvm::Function::ExternalLinkage, "flux_matrix_ew_mul", context.TheModule.get());
                ResPtr = context.Builder.CreateCall(F, {LPtr, RPtr}, "mat_ew_mul");
            } else if (isMatSca) {
                llvm::Function* F = context.TheModule->getFunction("flux_matrix_mul_ms");
                if (!F) F = llvm::Function::Create(llvm::FunctionType::get(VoidPtrTy, {VoidPtrTy, llvm::Type::getDoubleTy(context.TheContext)}, false), llvm::Function::ExternalLinkage, "flux_matrix_mul_ms", context.TheModule.get());
                ResPtr = context.Builder.CreateCall(F, {LPtr, R.Val}, "mat_ew_mul_ms");
            } else if (isScaMat) {
                llvm::Function* F = context.TheModule->getFunction("flux_matrix_mul_ms");
                if (!F) F = llvm::Function::Create(llvm::FunctionType::get(VoidPtrTy, {VoidPtrTy, llvm::Type::getDoubleTy(context.TheContext)}, false), llvm::Function::ExternalLinkage, "flux_matrix_mul_ms", context.TheModule.get());
                ResPtr = context.Builder.CreateCall(F, {RPtr, L.Val}, "mat_ew_mul_sm");
            }
            break;
        }
        case static_cast<int>(TokenType::tok_ew_div): {
            if (isMatMat) {
                llvm::Function* F = context.TheModule->getFunction("flux_matrix_ew_div");
                if (!F) F = llvm::Function::Create(llvm::FunctionType::get(VoidPtrTy, {VoidPtrTy, VoidPtrTy}, false), llvm::Function::ExternalLinkage, "flux_matrix_ew_div", context.TheModule.get());
                ResPtr = context.Builder.CreateCall(F, {LPtr, RPtr}, "mat_ew_div");
            }
            break;
        }
        default:
            std::cerr << "Binary operator not supported for matrices" << std::endl;
            return TypedValue();
        }
        
        if (ResPtr) {
            llvm::Function* RowsF = context.TheModule->getFunction("flux_matrix_rows");
            if (!RowsF) RowsF = llvm::Function::Create(llvm::FunctionType::get(Int32Ty, {VoidPtrTy}, false), llvm::Function::ExternalLinkage, "flux_matrix_rows", context.TheModule.get());
            llvm::Function* ColsF = context.TheModule->getFunction("flux_matrix_cols");
            if (!ColsF) ColsF = llvm::Function::Create(llvm::FunctionType::get(Int32Ty, {VoidPtrTy}, false), llvm::Function::ExternalLinkage, "flux_matrix_cols", context.TheModule.get());

            ResRows = context.Builder.CreateCall(RowsF, {ResPtr}, "res_rows");
            ResCols = context.Builder.CreateCall(ColsF, {ResPtr}, "res_cols");

            llvm::Value* MatVal = llvm::UndefValue::get(MatStructTy);
            MatVal = context.Builder.CreateInsertValue(MatVal, ResPtr, 0);
            MatVal = context.Builder.CreateInsertValue(MatVal, ResRows, 1);
            MatVal = context.Builder.CreateInsertValue(MatVal, ResCols, 2);
            return TypedValue(MatVal, TypeKind::Matrix);
        }
        return TypedValue();
    }

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
        return TypedValue(context.Builder.CreateFMul(LV, RV, "multmp"), TypeKind::Double);
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
    
    if (V.Type.Kind == TypeKind::Matrix) {
        llvm::Type* VoidPtrTy = llvm::PointerType::get(context.TheContext, 0);
        llvm::Function* TransposeF = context.TheModule->getFunction("flux_matrix_transpose");
        if (!TransposeF) TransposeF = llvm::Function::Create(llvm::FunctionType::get(VoidPtrTy, {VoidPtrTy}, false), llvm::Function::ExternalLinkage, "flux_matrix_transpose", context.TheModule.get());
        
        llvm::Value* MatPtr = context.Builder.CreateExtractValue(V.Val, 0, "mat_ptr");
        llvm::Value* Rows = context.Builder.CreateExtractValue(V.Val, 1, "mat_rows");
        llvm::Value* Cols = context.Builder.CreateExtractValue(V.Val, 2, "mat_cols");
        
        llvm::Value* ResPtr = context.Builder.CreateCall(TransposeF, {MatPtr}, "transtmp");
        
        llvm::StructType* MatStructTy = llvm::cast<llvm::StructType>(FluxType(TypeKind::Matrix).getLLVMType(context.TheContext));
        llvm::Value* MatVal = llvm::UndefValue::get(MatStructTy);
        MatVal = context.Builder.CreateInsertValue(MatVal, ResPtr, 0);
        MatVal = context.Builder.CreateInsertValue(MatVal, Cols, 1); // Swapped rows/cols
        MatVal = context.Builder.CreateInsertValue(MatVal, Rows, 2);
        
        return TypedValue(MatVal, TypeKind::Matrix);
    }
    
    // For scalars, transpose is just identity (or conjugate transpose for complex, but leaving it as is for now)
    return V;
}

TypedValue CallExprAST::codegen(CodegenContext& context) {
    // Handle built-in complex number functions
    if (Args.size() == 1) {
        TypedValue Arg = Args[0]->codegen(context);
        if (isComplexType(Arg)) {
            llvm::Value* Real = context.Builder.CreateExtractValue(Arg.Val, 0, "real");
            llvm::Value* Imag = context.Builder.CreateExtractValue(Arg.Val, 1, "imag");
            
            if (Callee == "real") return TypedValue(Real, TypeKind::Double);
            if (Callee == "imag") return TypedValue(Imag, TypeKind::Double);
            if (Callee == "conj") {
                llvm::Value* NegImag = context.Builder.CreateFNeg(Imag, "negimag");
                llvm::Value* Res = llvm::UndefValue::get(Arg.Val->getType());
                Res = context.Builder.CreateInsertValue(Res, Real, 0);
                return TypedValue(context.Builder.CreateInsertValue(Res, NegImag, 1), TypeKind::Complex);
            }
            if (Callee == "abs" || Callee == "mag") {
                // sqrt(re*re + im*im)
                llvm::Value* Re2 = context.Builder.CreateFMul(Real, Real, "re2");
                llvm::Value* Im2 = context.Builder.CreateFMul(Imag, Imag, "im2");
                llvm::Value* Sum = context.Builder.CreateFAdd(Re2, Im2, "sum2");
                
                llvm::Function* SqrtF = context.TheModule->getFunction("llvm.sqrt.f64");
                if (!SqrtF) {
                    SqrtF = llvm::Function::Create(
                        llvm::FunctionType::get(llvm::Type::getDoubleTy(context.TheContext), {llvm::Type::getDoubleTy(context.TheContext)}, false),
                        llvm::Function::ExternalLinkage, "llvm.sqrt.f64", context.TheModule.get());
                }
                return TypedValue(context.Builder.CreateCall(SqrtF, {Sum}, "abs"), TypeKind::Double);
            }
            if (Callee == "arg" || Callee == "phase") {
                llvm::Function* Atan2F = context.TheModule->getFunction("atan2");
                if (!Atan2F) {
                    Atan2F = llvm::Function::Create(
                        llvm::FunctionType::get(llvm::Type::getDoubleTy(context.TheContext), {llvm::Type::getDoubleTy(context.TheContext), llvm::Type::getDoubleTy(context.TheContext)}, false),
                        llvm::Function::ExternalLinkage, "atan2", context.TheModule.get());
                }
                return TypedValue(context.Builder.CreateCall(Atan2F, {Imag, Real}, "arg"), TypeKind::Double);
            }
        }
    }

    llvm::Function* CalleeF = context.TheModule->getFunction(Callee);
    if (!CalleeF) {
        llvm::Value* VarVal = context.NamedValues[Callee];
        if (VarVal) {
            llvm::Type* VarTy = nullptr;
            if (auto* Alloca = llvm::dyn_cast<llvm::AllocaInst>(VarVal)) {
                VarTy = Alloca->getAllocatedType();
            } else {
                VarTy = VarVal->getType();
            }
            
            FluxType FTy = typeFromLLVM(VarTy);
            if (FTy.Kind == TypeKind::Matrix) {
                if (Args.size() != 1 && Args.size() != 2) {
                    std::cerr << "Matrix indexing requires 1 or 2 arguments." << std::endl;
                    return TypedValue();
                }
                
                llvm::Value* MatV = nullptr;
                if (llvm::isa<llvm::AllocaInst>(VarVal)) {
                    MatV = context.Builder.CreateLoad(VarTy, VarVal, Callee.c_str());
                } else {
                    MatV = VarVal;
                }
                
                llvm::Value* MatPtr = context.Builder.CreateExtractValue(MatV, 0, "mat_ptr");
                
                TypedValue RowTV = Args[0]->codegen(context);
                if (!RowTV.Val) return TypedValue();
                llvm::Value* RowV = RowTV.Val;
                llvm::Type* Int32Ty = llvm::Type::getInt32Ty(context.TheContext);
                
                if (RowV->getType()->isFloatingPointTy()) RowV = context.Builder.CreateFPToSI(RowV, Int32Ty, "row_int");
                else if (RowV->getType()->isIntegerTy(64)) RowV = context.Builder.CreateTrunc(RowV, Int32Ty, "row_int");
                
                llvm::Value* ColV = llvm::ConstantInt::get(Int32Ty, 0); // Default to col 0 for vector
                if (Args.size() == 2) {
                    TypedValue ColTV = Args[1]->codegen(context);
                    if (!ColTV.Val) return TypedValue();
                    ColV = ColTV.Val;
                    if (ColV->getType()->isFloatingPointTy()) ColV = context.Builder.CreateFPToSI(ColV, Int32Ty, "col_int");
                    else if (ColV->getType()->isIntegerTy(64)) ColV = context.Builder.CreateTrunc(ColV, Int32Ty, "col_int");
                }
                
                llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
                llvm::Type* VoidPtrTy = llvm::PointerType::get(context.TheContext, 0);
                llvm::Function* GetElemF = context.TheModule->getFunction("flux_matrix_get");
                if (!GetElemF) GetElemF = llvm::Function::Create(llvm::FunctionType::get(DoubleTy, { VoidPtrTy, Int32Ty, Int32Ty }, false), llvm::Function::ExternalLinkage, "flux_matrix_get", context.TheModule.get());
                
                return TypedValue(context.Builder.CreateCall(GetElemF, {MatPtr, RowV, ColV}, "elem_val"), TypeKind::Double);
            }
        }
        return TypedValue();
    }
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
    llvm::BasicBlock* ThenBB = llvm::BasicBlock::Create(context.TheContext, "then");
    llvm::BasicBlock* ElseBB = llvm::BasicBlock::Create(context.TheContext, "else");
    llvm::BasicBlock* MergeBB = llvm::BasicBlock::Create(context.TheContext, "ifcont");
    context.Builder.CreateCondBr(CondV, ThenBB, ElseBB);
    
    // Generate Then block
    context.Builder.SetInsertPoint(ThenBB);
    TheFunction->insert(TheFunction->end(), ThenBB);
    TypedValue ThenTV = Then->codegen(context);
    if (!ThenTV.Val) return TypedValue();
    context.Builder.CreateBr(MergeBB);
    ThenBB = context.Builder.GetInsertBlock();
    
    // Generate Else block
    TheFunction->insert(TheFunction->end(), ElseBB);
    context.Builder.SetInsertPoint(ElseBB);
    TypedValue ElseTV = Else->codegen(context);
    if (!ElseTV.Val) return TypedValue();
    context.Builder.CreateBr(MergeBB);
    ElseBB = context.Builder.GetInsertBlock();
    
    // Merge block
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
    
    // Set up break/continue targets
    llvm::BasicBlock* OldLoopEnd = context.CurrentLoopEnd;
    llvm::BasicBlock* OldLoopCont = context.CurrentLoopCont;
    context.CurrentLoopEnd = AfterBB;
    context.CurrentLoopCont = LoopBB;  // Continue jumps to loop condition
    
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
    
    // Restore break/continue targets
    context.CurrentLoopEnd = OldLoopEnd;
    context.CurrentLoopCont = OldLoopCont;
    
    return BodyTV;
}

TypedValue WhileExprAST::codegen(CodegenContext& context) {
    llvm::Function* TheFunction = context.Builder.GetInsertBlock()->getParent();
    llvm::BasicBlock* CondBB = llvm::BasicBlock::Create(context.TheContext, "whilecond", TheFunction);
    llvm::BasicBlock* BodyBB = llvm::BasicBlock::Create(context.TheContext, "whilebody");
    llvm::BasicBlock* AfterBB = llvm::BasicBlock::Create(context.TheContext, "afterwhile");
    
    // Set up break/continue targets
    llvm::BasicBlock* OldLoopEnd = context.CurrentLoopEnd;
    llvm::BasicBlock* OldLoopCont = context.CurrentLoopCont;
    context.CurrentLoopEnd = AfterBB;
    context.CurrentLoopCont = CondBB;  // Continue jumps to condition
    
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
    
    // Restore break/continue targets
    context.CurrentLoopEnd = OldLoopEnd;
    context.CurrentLoopCont = OldLoopCont;
    
    return BodyTV;
}

TypedValue LetExprAST::codegen(CodegenContext& context) {
    TypedValue InitTV = Init->codegen(context);
    if (!InitTV.Val) return TypedValue();
    
    FluxType ActualType = (Type.Kind == TypeKind::Auto) ? InitTV.Type : Type;
    llvm::Type* VarTy = ActualType.getLLVMType(context.TheContext);
    
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
    if (!Body) return InitTV;
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
    if (!CreateMatF) CreateMatF = llvm::Function::Create(llvm::FunctionType::get(VoidPtrTy, { VoidPtrTy, Int32Ty, Int32Ty }, false), llvm::Function::ExternalLinkage, "flux_create_matrix", context.TheModule.get());
    
    llvm::Value* RowsVal = llvm::ConstantInt::get(Int32Ty, NumRows);
    llvm::Value* ColsVal = llvm::ConstantInt::get(Int32Ty, NumCols);
    llvm::Value* MatPtr = context.Builder.CreateCall(CreateMatF, {DataPtr, RowsVal, ColsVal}, "mat_ptr");
    
    llvm::StructType* MatStructTy = llvm::cast<llvm::StructType>(FluxType(TypeKind::Matrix).getLLVMType(context.TheContext));
    llvm::Value* MatVal = llvm::UndefValue::get(MatStructTy);
    MatVal = context.Builder.CreateInsertValue(MatVal, MatPtr, 0);
    MatVal = context.Builder.CreateInsertValue(MatVal, RowsVal, 1);
    MatVal = context.Builder.CreateInsertValue(MatVal, ColsVal, 2);
    
    return TypedValue(MatVal, TypeKind::Matrix);
}

TypedValue IndexExprAST::codegen(CodegenContext& context) {
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
    llvm::Type* Int32Ty = llvm::Type::getInt32Ty(context.TheContext);
    llvm::Type* VoidPtrTy = llvm::PointerType::get(context.TheContext, 0);

    TypedValue ArrayTV = Array->codegen(context);
    if (!ArrayTV.Val || ArrayTV.Type.Kind != TypeKind::Matrix) {
        std::cerr << "Can only index into matrices." << std::endl;
        return TypedValue();
    }

    llvm::Value* MatPtr = context.Builder.CreateExtractValue(ArrayTV.Val, 0, "mat_ptr");
    
    // Extract matrix dimensions for bounds checking
    llvm::Value* RowsVal = context.Builder.CreateExtractValue(ArrayTV.Val, 1, "mat_rows");
    llvm::Value* ColsVal = context.Builder.CreateExtractValue(ArrayTV.Val, 2, "mat_cols");

    if (IsMatrixIndex) {
        TypedValue RowTV = RowIndex->codegen(context);
        TypedValue ColTV = ColIndex ? ColIndex->codegen(context) : TypedValue();
        if (!RowTV.Val || !ColTV.Val) return TypedValue();

        llvm::Value* RowV = RowTV.Val;
        llvm::Value* ColV = ColTV.Val;

        if (RowV->getType()->isFloatingPointTy()) RowV = context.Builder.CreateFPToSI(RowV, Int32Ty, "row_int");
        else if (RowV->getType()->isIntegerTy(64)) RowV = context.Builder.CreateTrunc(RowV, Int32Ty, "row_int");

        if (ColV->getType()->isFloatingPointTy()) ColV = context.Builder.CreateFPToSI(ColV, Int32Ty, "col_int");
        else if (ColV->getType()->isIntegerTy(64)) ColV = context.Builder.CreateTrunc(ColV, Int32Ty, "col_int");

        // Bounds checking for row index
        llvm::Value* RowInBounds = context.Builder.CreateICmpULT(RowV, RowsVal, "row_in_bounds");
        llvm::Function* RowCheckF = context.TheModule->getFunction("flux_bounds_check_row");
        if (!RowCheckF) {
            llvm::FunctionType* CheckTy = llvm::FunctionType::get(llvm::Type::getVoidTy(context.TheContext), 
                {Int32Ty, Int32Ty, VoidPtrTy}, false);
            RowCheckF = llvm::Function::Create(CheckTy, llvm::Function::ExternalLinkage, 
                "flux_bounds_check_row", context.TheModule.get());
        }
        context.Builder.CreateCall(RowCheckF, {RowV, RowsVal, MatPtr}, "row_check");
        
        // Bounds checking for column index
        llvm::Value* ColInBounds = context.Builder.CreateICmpULT(ColV, ColsVal, "col_in_bounds");
        llvm::Function* ColCheckF = context.TheModule->getFunction("flux_bounds_check_col");
        if (!ColCheckF) {
            llvm::FunctionType* CheckTy = llvm::FunctionType::get(llvm::Type::getVoidTy(context.TheContext), 
                {Int32Ty, Int32Ty, VoidPtrTy}, false);
            ColCheckF = llvm::Function::Create(CheckTy, llvm::Function::ExternalLinkage, 
                "flux_bounds_check_col", context.TheModule.get());
        }
        context.Builder.CreateCall(ColCheckF, {ColV, ColsVal, MatPtr}, "col_check");

        llvm::Function* GetElemF = context.TheModule->getFunction("flux_matrix_get");
        if (!GetElemF) GetElemF = llvm::Function::Create(llvm::FunctionType::get(DoubleTy, { VoidPtrTy, Int32Ty, Int32Ty }, false), llvm::Function::ExternalLinkage, "flux_matrix_get", context.TheModule.get());

        return TypedValue(context.Builder.CreateCall(GetElemF, {MatPtr, RowV, ColV}, "elem_val"), TypeKind::Double);
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

    llvm::DISubprogram* subprogram = nullptr;
    if (context.DebugBuilder && context.DebugCompileUnit && context.DebugFile) {
        auto typeArray = context.DebugBuilder->getOrCreateTypeArray({});
        auto* subroutineType = context.DebugBuilder->createSubroutineType(typeArray);
        subprogram = context.DebugBuilder->createFunction(
            context.DebugFile,
            Proto->getName(),
            Proto->getName(),
            context.DebugFile,
            1,
            subroutineType,
            1,
            llvm::DINode::FlagZero,
            llvm::DISubprogram::SPFlagDefinition);
        TheFunction->setSubprogram(subprogram);
    }

    llvm::BasicBlock* BB = llvm::BasicBlock::Create(context.TheContext, "entry", TheFunction);
    context.Builder.SetInsertPoint(BB);
    if (subprogram) {
        context.Builder.SetCurrentDebugLocation(
            llvm::DILocation::get(context.TheContext, 1, 1, subprogram));
    }
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

TypedValue ParameterExprAST::codegen(CodegenContext& context) {
    llvm::Function* GetPF = context.TheModule->getFunction("flux_get_parameter");
    if (!GetPF) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        llvm::Type* CharPtrTy = llvm::PointerType::get(llvm::Type::getInt8Ty(context.TheContext), 0);
        GetPF = llvm::Function::Create(llvm::FunctionType::get(DoubleTy, {CharPtrTy}, false), llvm::Function::ExternalLinkage, "flux_get_parameter", context.TheModule.get());
    }
    return TypedValue(context.Builder.CreateCall(GetPF, {context.Builder.CreateGlobalStringPtr(ParamName)}, "p"), TypeKind::Double);
}

// ============================================================================
// SPICE Time-Domain Simulation Codegen
// ============================================================================

TypedValue BuiltinVarExprAST::codegen(CodegenContext& context) {
    // Generate calls for built-in variables: time, dt, temp
    std::string FuncName;
    if (Name == "time") FuncName = "flux_get_time";
    else if (Name == "dt") FuncName = "flux_get_dt";
    else if (Name == "temp") FuncName = "flux_get_temp";
    else {
        std::cerr << "Unknown built-in variable: " << Name << std::endl;
        return TypedValue();
    }

    llvm::Function* GetFunc = context.TheModule->getFunction(FuncName);
    if (!GetFunc) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        GetFunc = llvm::Function::Create(llvm::FunctionType::get(DoubleTy, {}, false),
                                         llvm::Function::ExternalLinkage,
                                         FuncName, context.TheModule.get());
    }
    return TypedValue(context.Builder.CreateCall(GetFunc, {}, Name.c_str()), TypeKind::Double);
}

TypedValue UpdateFuncAST::codegen(CodegenContext& context) {
    // Generate update(t, inputs) function
    // This creates a function that will be called by ngspice during simulation
    std::vector<llvm::Type*> ArgTypes;
    std::vector<std::string> ArgNames;
    
    // Time argument (double)
    ArgTypes.push_back(llvm::Type::getDoubleTy(context.TheContext));
    ArgNames.push_back(TimeVar);
    
    // Inputs argument (pointer to inputs array)
    llvm::Type* DoublePtrTy = llvm::PointerType::get(llvm::Type::getDoubleTy(context.TheContext), 0);
    ArgTypes.push_back(DoublePtrTy);
    ArgNames.push_back(InputsVar);
    
    // Return type is double (output value)
    llvm::Type* RetTy = llvm::Type::getDoubleTy(context.TheContext);
    
    llvm::FunctionType* FT = llvm::FunctionType::get(RetTy, ArgTypes, false);
    llvm::Function* TheFunction = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "update", context.TheModule.get());
    
    // Set argument names and allocate on stack
    unsigned Idx = 0;
    for (auto& Arg : TheFunction->args()) {
        Arg.setName(ArgNames[Idx]);
        llvm::IRBuilder<> TmpB(&TheFunction->getEntryBlock(), TheFunction->getEntryBlock().begin());
        llvm::AllocaInst* Alloca = TmpB.CreateAlloca(Arg.getType(), nullptr, ArgNames[Idx]);
        context.Builder.CreateStore(&Arg, Alloca);
        context.NamedValues[std::string(Arg.getName())] = Alloca;
        Idx++;
    }
    
    if (TypedValue RetTV = Body->codegen(context)) {
        llvm::Value* RetVal = RetTV.Val;
        if (RetVal->getType() != RetTy) {
            if (RetTy->isFloatingPointTy() && RetVal->getType()->isIntegerTy()) {
                RetVal = context.Builder.CreateSIToFP(RetVal, RetTy, "cast");
            }
        }
        context.Builder.CreateRet(RetVal);
        llvm::verifyFunction(*TheFunction);
        return TypedValue(TheFunction, TypeKind::Double);
    }
    
    TheFunction->eraseFromParent();
    return TypedValue();
}

// ============================================================================
// SPICE Behavioral Sources Codegen
// ============================================================================

TypedValue BSourceExprAST::codegen(CodegenContext& context) {
    // Generate B-source netlist entry
    // Bxxx n+ n- V=<expression> or I=<expression>
    std::string SourceType = IsCurrent ? "I" : "V";
    std::string NetlistEntry = "B" + Name + " " + PositiveNode + " " + NegativeNode + " " + SourceType + "=";
    
    // The expression will be evaluated and stored as a string
    // For now, we generate a placeholder and mark it for later netlist generation
    llvm::Function* RegisterBSource = context.TheModule->getFunction("flux_register_bsource");
    if (!RegisterBSource) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        llvm::Type* CharPtrTy = llvm::PointerType::get(llvm::Type::getInt8Ty(context.TheContext), 0);
        RegisterBSource = llvm::Function::Create(
            llvm::FunctionType::get(DoubleTy, {CharPtrTy, CharPtrTy, CharPtrTy, CharPtrTy}, false),
            llvm::Function::ExternalLinkage, "flux_register_bsource", context.TheModule.get());
    }
    
    return TypedValue(context.Builder.CreateCall(RegisterBSource, {
        context.Builder.CreateGlobalStringPtr(Name),
        context.Builder.CreateGlobalStringPtr(PositiveNode),
        context.Builder.CreateGlobalStringPtr(NegativeNode),
        context.Builder.CreateGlobalStringPtr(SourceType)
    }), TypeKind::Double);
}

TypedValue ESourceExprAST::codegen(CodegenContext& context) {
    // Generate E-source (VCVS) netlist entry
    // Exxx n+ n- nc+ nc- <gain>
    llvm::Function* RegisterESource = context.TheModule->getFunction("flux_register_esource");
    if (!RegisterESource) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        llvm::Type* CharPtrTy = llvm::PointerType::get(llvm::Type::getInt8Ty(context.TheContext), 0);
        RegisterESource = llvm::Function::Create(
            llvm::FunctionType::get(DoubleTy, {CharPtrTy, CharPtrTy, CharPtrTy, CharPtrTy, CharPtrTy, DoubleTy}, false),
            llvm::Function::ExternalLinkage, "flux_register_esource", context.TheModule.get());
    }
    
    // Evaluate gain expression
    double GainVal = 1.0;
    if (Gain) {
        if (auto* NumGain = dynamic_cast<NumberExprAST*>(Gain.get())) {
            GainVal = NumGain->getValue();
        }
    }
    
    return TypedValue(context.Builder.CreateCall(RegisterESource, {
        context.Builder.CreateGlobalStringPtr(Name),
        context.Builder.CreateGlobalStringPtr(PositiveNode),
        context.Builder.CreateGlobalStringPtr(NegativeNode),
        context.Builder.CreateGlobalStringPtr(ControlPosNode),
        context.Builder.CreateGlobalStringPtr(ControlNegNode),
        llvm::ConstantFP::get(context.TheContext, llvm::APFloat(GainVal))
    }), TypeKind::Double);
}

TypedValue FSourceExprAST::codegen(CodegenContext& context) {
    // Generate F-source (CCCS) netlist entry
    // Fxxx n+ n- <vsource_name> <gain>
    llvm::Function* RegisterFSource = context.TheModule->getFunction("flux_register_fsource");
    if (!RegisterFSource) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        llvm::Type* CharPtrTy = llvm::PointerType::get(llvm::Type::getInt8Ty(context.TheContext), 0);
        RegisterFSource = llvm::Function::Create(
            llvm::FunctionType::get(DoubleTy, {CharPtrTy, CharPtrTy, CharPtrTy, CharPtrTy, DoubleTy}, false),
            llvm::Function::ExternalLinkage, "flux_register_fsource", context.TheModule.get());
    }
    
    double GainVal = 1.0;
    if (Gain) {
        if (auto* NumGain = dynamic_cast<NumberExprAST*>(Gain.get())) {
            GainVal = NumGain->getValue();
        }
    }
    
    return TypedValue(context.Builder.CreateCall(RegisterFSource, {
        context.Builder.CreateGlobalStringPtr(Name),
        context.Builder.CreateGlobalStringPtr(PositiveNode),
        context.Builder.CreateGlobalStringPtr(NegativeNode),
        context.Builder.CreateGlobalStringPtr(VoltageSourceName),
        llvm::ConstantFP::get(context.TheContext, llvm::APFloat(GainVal))
    }), TypeKind::Double);
}

TypedValue GSourceExprAST::codegen(CodegenContext& context) {
    // Generate G-source (VCCS) netlist entry
    // Gxxx n+ n- nc+ nc- <transconductance>
    llvm::Function* RegisterGSource = context.TheModule->getFunction("flux_register_gsource");
    if (!RegisterGSource) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        llvm::Type* CharPtrTy = llvm::PointerType::get(llvm::Type::getInt8Ty(context.TheContext), 0);
        RegisterGSource = llvm::Function::Create(
            llvm::FunctionType::get(DoubleTy, {CharPtrTy, CharPtrTy, CharPtrTy, CharPtrTy, CharPtrTy, DoubleTy}, false),
            llvm::Function::ExternalLinkage, "flux_register_gsource", context.TheModule.get());
    }
    
    double TranscondVal = 1.0;
    if (Transconductance) {
        if (auto* NumVal = dynamic_cast<NumberExprAST*>(Transconductance.get())) {
            TranscondVal = NumVal->getValue();
        }
    }
    
    return TypedValue(context.Builder.CreateCall(RegisterGSource, {
        context.Builder.CreateGlobalStringPtr(Name),
        context.Builder.CreateGlobalStringPtr(PositiveNode),
        context.Builder.CreateGlobalStringPtr(NegativeNode),
        context.Builder.CreateGlobalStringPtr(ControlPosNode),
        context.Builder.CreateGlobalStringPtr(ControlNegNode),
        llvm::ConstantFP::get(context.TheContext, llvm::APFloat(TranscondVal))
    }), TypeKind::Double);
}

TypedValue HSourceExprAST::codegen(CodegenContext& context) {
    // Generate H-source (CCVS) netlist entry
    // Hxxx n+ n- <vsource_name> <transresistance>
    llvm::Function* RegisterHSource = context.TheModule->getFunction("flux_register_hsource");
    if (!RegisterHSource) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        llvm::Type* CharPtrTy = llvm::PointerType::get(llvm::Type::getInt8Ty(context.TheContext), 0);
        RegisterHSource = llvm::Function::Create(
            llvm::FunctionType::get(DoubleTy, {CharPtrTy, CharPtrTy, CharPtrTy, CharPtrTy, DoubleTy}, false),
            llvm::Function::ExternalLinkage, "flux_register_hsource", context.TheModule.get());
    }
    
    double TransresVal = 1.0;
    if (Transresistance) {
        if (auto* NumVal = dynamic_cast<NumberExprAST*>(Transresistance.get())) {
            TransresVal = NumVal->getValue();
        }
    }
    
    return TypedValue(context.Builder.CreateCall(RegisterHSource, {
        context.Builder.CreateGlobalStringPtr(Name),
        context.Builder.CreateGlobalStringPtr(PositiveNode),
        context.Builder.CreateGlobalStringPtr(NegativeNode),
        context.Builder.CreateGlobalStringPtr(VoltageSourceName),
        llvm::ConstantFP::get(context.TheContext, llvm::APFloat(TransresVal))
    }), TypeKind::Double);
}

// ============================================================================
// SPICE Analysis Control Codegen
// ============================================================================

void AnalysisExprAST::addParameter(const std::string& Name, std::unique_ptr<ExprAST> Value) {
    Parameters[Name] = std::move(Value);
}

TypedValue AnalysisExprAST::codegen(CodegenContext& context) {
    std::string AnalysisName;
    switch (Type) {
        case AnalysisType::TRAN: AnalysisName = "tran"; break;
        case AnalysisType::DC: AnalysisName = "dc"; break;
        case AnalysisType::AC: AnalysisName = "ac"; break;
        case AnalysisType::NOISE: AnalysisName = "noise"; break;
        case AnalysisType::OP: AnalysisName = "op"; break;
        case AnalysisType::TF: AnalysisName = "tf"; break;
        case AnalysisType::SENS: AnalysisName = "sens"; break;
        case AnalysisType::FOURIER: AnalysisName = "fourier"; break;
    }
    
    llvm::Function* RegisterAnalysis = context.TheModule->getFunction("flux_register_analysis");
    if (!RegisterAnalysis) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        llvm::Type* CharPtrTy = llvm::PointerType::get(llvm::Type::getInt8Ty(context.TheContext), 0);
        RegisterAnalysis = llvm::Function::Create(
            llvm::FunctionType::get(DoubleTy, {CharPtrTy}, false),
            llvm::Function::ExternalLinkage, "flux_register_analysis", context.TheModule.get());
    }
    
    return TypedValue(context.Builder.CreateCall(RegisterAnalysis, {
        context.Builder.CreateGlobalStringPtr(AnalysisName)
    }), TypeKind::Double);
}

void MeasureExprAST::addParameter(const std::string& Name, std::unique_ptr<ExprAST> Value) {
    Parameters[Name] = std::move(Value);
}

TypedValue MeasureExprAST::codegen(CodegenContext& context) {
    std::string MeasureName;
    switch (Type) {
        case MeasureType::MAX: MeasureName = "MAX"; break;
        case MeasureType::MIN: MeasureName = "MIN"; break;
        case MeasureType::AVG: MeasureName = "AVG"; break;
        case MeasureType::RMS: MeasureName = "RMS"; break;
        case MeasureType::TRIG: MeasureName = "TRIG"; break;
        case MeasureType::TARG: MeasureName = "TARG"; break;
        case MeasureType::WHEN: MeasureName = "WHEN"; break;
        case MeasureType::FIND: MeasureName = "FIND"; break;
        case MeasureType::DERIV: MeasureName = "DERIV"; break;
        case MeasureType::INTEG: MeasureName = "INTEG"; break;
    }
    
    llvm::Function* RegisterMeasure = context.TheModule->getFunction("flux_register_measure");
    if (!RegisterMeasure) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        llvm::Type* CharPtrTy = llvm::PointerType::get(llvm::Type::getInt8Ty(context.TheContext), 0);
        RegisterMeasure = llvm::Function::Create(
            llvm::FunctionType::get(DoubleTy, {CharPtrTy, CharPtrTy}, false),
            llvm::Function::ExternalLinkage, "flux_register_measure", context.TheModule.get());
    }
    
    return TypedValue(context.Builder.CreateCall(RegisterMeasure, {
        context.Builder.CreateGlobalStringPtr(Name),
        context.Builder.CreateGlobalStringPtr(MeasureName)
    }), TypeKind::Double);
}

TypedValue ProbeExprAST::codegen(CodegenContext& context) {
    llvm::Function* RegisterProbe = context.TheModule->getFunction("flux_register_probe");
    if (!RegisterProbe) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        llvm::Type* CharPtrTy = llvm::PointerType::get(llvm::Type::getInt8Ty(context.TheContext), 0);
        RegisterProbe = llvm::Function::Create(
            llvm::FunctionType::get(DoubleTy, {CharPtrTy, CharPtrTy}, false),
            llvm::Function::ExternalLinkage, "flux_register_probe", context.TheModule.get());
    }
    
    return TypedValue(context.Builder.CreateCall(RegisterProbe, {
        context.Builder.CreateGlobalStringPtr(VariableName),
        context.Builder.CreateGlobalStringPtr(OutputName.empty() ? VariableName : OutputName)
    }), TypeKind::Double);
}

TypedValue SaveExprAST::codegen(CodegenContext& context) {
    llvm::Function* RegisterSave = context.TheModule->getFunction("flux_register_save");
    if (!RegisterSave) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        llvm::Type* CharPtrTy = llvm::PointerType::get(llvm::Type::getInt8Ty(context.TheContext), 0);
        RegisterSave = llvm::Function::Create(
            llvm::FunctionType::get(DoubleTy, {CharPtrTy}, false),
            llvm::Function::ExternalLinkage, "flux_register_save", context.TheModule.get());
    }
    
    return TypedValue(context.Builder.CreateCall(RegisterSave, {
        context.Builder.CreateGlobalStringPtr(VariableName)
    }), TypeKind::Double);
}

// ============================================================================
// SPICE Subcircuit and Model Codegen
// ============================================================================

void SubcktExprAST::addParameter(const std::string& Name, std::unique_ptr<ExprAST> Value) {
    Parameters.push_back({Name, std::move(Value)});
}

void SubcktExprAST::addStatement(std::unique_ptr<ExprAST> Stmt) {
    Body.push_back(std::move(Stmt));
}

TypedValue SubcktExprAST::codegen(CodegenContext& context) {
    // Build pin list string
    std::string PinsStr;
    for (size_t i = 0; i < Pins.size(); ++i) {
        if (i > 0) PinsStr += " ";
        PinsStr += Pins[i];
    }
    
    llvm::Function* RegisterSubckt = context.TheModule->getFunction("flux_register_subckt");
    if (!RegisterSubckt) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        llvm::Type* CharPtrTy = llvm::PointerType::get(llvm::Type::getInt8Ty(context.TheContext), 0);
        RegisterSubckt = llvm::Function::Create(
            llvm::FunctionType::get(DoubleTy, {CharPtrTy, CharPtrTy}, false),
            llvm::Function::ExternalLinkage, "flux_register_subckt", context.TheModule.get());
    }
    
    return TypedValue(context.Builder.CreateCall(RegisterSubckt, {
        context.Builder.CreateGlobalStringPtr(Name),
        context.Builder.CreateGlobalStringPtr(PinsStr)
    }), TypeKind::Double);
}

void ModelExprAST::addParameter(const std::string& Name, std::unique_ptr<ExprAST> Value) {
    Parameters[Name] = std::move(Value);
}

TypedValue ModelExprAST::codegen(CodegenContext& context) {
    llvm::Function* RegisterModel = context.TheModule->getFunction("flux_register_model");
    if (!RegisterModel) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        llvm::Type* CharPtrTy = llvm::PointerType::get(llvm::Type::getInt8Ty(context.TheContext), 0);
        RegisterModel = llvm::Function::Create(
            llvm::FunctionType::get(DoubleTy, {CharPtrTy, CharPtrTy}, false),
            llvm::Function::ExternalLinkage, "flux_register_model", context.TheModule.get());
    }
    
    return TypedValue(context.Builder.CreateCall(RegisterModel, {
        context.Builder.CreateGlobalStringPtr(Name),
        context.Builder.CreateGlobalStringPtr(ModelType)
    }), TypeKind::Double);
}

TypedValue ParamExprAST::codegen(CodegenContext& context) {
    // Parameters are handled at the simulation setup level
    // This just marks the parameter as declared
    llvm::Function* RegisterParam = context.TheModule->getFunction("flux_register_param");
    if (!RegisterParam) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        llvm::Type* CharPtrTy = llvm::PointerType::get(llvm::Type::getInt8Ty(context.TheContext), 0);
        RegisterParam = llvm::Function::Create(
            llvm::FunctionType::get(DoubleTy, {CharPtrTy, DoubleTy}, false),
            llvm::Function::ExternalLinkage, "flux_register_param", context.TheModule.get());
    }
    
    double Val = 0.0;
    if (auto* NumVal = dynamic_cast<NumberExprAST*>(Value.get())) {
        Val = NumVal->getValue();
    }
    
    return TypedValue(context.Builder.CreateCall(RegisterParam, {
        context.Builder.CreateGlobalStringPtr(Name),
        llvm::ConstantFP::get(context.TheContext, llvm::APFloat(Val))
    }), TypeKind::Double);
}

TypedValue ICExprAST::codegen(CodegenContext& context) {
    llvm::Function* RegisterIC = context.TheModule->getFunction("flux_register_ic");
    if (!RegisterIC) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        llvm::Type* CharPtrTy = llvm::PointerType::get(llvm::Type::getInt8Ty(context.TheContext), 0);
        RegisterIC = llvm::Function::Create(
            llvm::FunctionType::get(DoubleTy, {CharPtrTy, DoubleTy}, false),
            llvm::Function::ExternalLinkage, "flux_register_ic", context.TheModule.get());
    }
    
    double Val = 0.0;
    if (auto* NumVal = dynamic_cast<NumberExprAST*>(Value.get())) {
        Val = NumVal->getValue();
    }
    
    return TypedValue(context.Builder.CreateCall(RegisterIC, {
        context.Builder.CreateGlobalStringPtr(NodeName),
        llvm::ConstantFP::get(context.TheContext, llvm::APFloat(Val))
    }), TypeKind::Double);
}

} // namespace Flux
