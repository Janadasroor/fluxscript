#include "flux/compiler/ast.h"
#include "flux/compiler/lexer.h"
#include <llvm/IR/Verifier.h>
#include <iostream>

namespace Flux {

llvm::Value* NumberExprAST::codegen(CodegenContext& context) {
    return llvm::ConstantFP::get(context.TheContext, llvm::APFloat(Val));
}

llvm::Value* ComplexExprAST::codegen(CodegenContext& context) {
    // Return real part for function return compatibility
    // Full complex struct support would require complex FFI handling
    llvm::Value* RealVal = llvm::ConstantFP::get(context.TheContext, llvm::APFloat(Real));
    return RealVal;
}

llvm::Value* StringExprAST::codegen(CodegenContext& context) {
    // Create a global constant string
    llvm::Constant* StrConst = llvm::ConstantDataArray::getString(context.TheContext, Val, true);
    llvm::GlobalVariable* GV = new llvm::GlobalVariable(
        *context.TheModule,
        StrConst->getType(),
        true,
        llvm::GlobalValue::PrivateLinkage,
        StrConst,
        ".str");
    
    // Cast to i8*
    return context.Builder.CreatePointerCast(GV, llvm::PointerType::get(llvm::Type::getInt8Ty(context.TheContext), 0), "strptr");
}

llvm::Value* VariableExprAST::codegen(CodegenContext& context) {
    llvm::Value* V = context.NamedValues[Name];
    if (!V) {
        std::cerr << "Unknown variable name: " << Name << std::endl;
        return nullptr;
    }
    // If it's an alloca, load it.
    if (auto* Alloca = llvm::dyn_cast<llvm::AllocaInst>(V)) {
        return context.Builder.CreateLoad(Alloca->getAllocatedType(), Alloca, Name.c_str());
    }
    return V;
}

llvm::Value* AssignExprAST::codegen(CodegenContext& context) {
    llvm::Value* ValV = Val->codegen(context);
    if (!ValV) return nullptr;

    llvm::Value* Variable = context.NamedValues[Name];
    if (!Variable) {
        std::cerr << "Unknown variable name: " << Name << std::endl;
        return nullptr;
    }

    if (!llvm::isa<llvm::AllocaInst>(Variable)) {
        std::cerr << "Cannot assign to read-only variable: " << Name << std::endl;
        return nullptr;
    }

    context.Builder.CreateStore(ValV, Variable);
    return ValV;
}

llvm::Value* BinaryExprAST::codegen(CodegenContext& context) {
    llvm::Value* L = LHS->codegen(context);
    llvm::Value* R = RHS->codegen(context);
    if (!L || !R) return nullptr;

    bool isIntOp = L->getType()->isIntegerTy();

    auto createBoolResult = [&](llvm::Value* cmp) -> llvm::Value* {
        return context.Builder.CreateUIToFP(cmp, llvm::Type::getDoubleTy(context.TheContext), "booltmp");
    };

    auto createIntResult = [&](llvm::Value* result) -> llvm::Value* {
        return context.Builder.CreateUIToFP(result, llvm::Type::getDoubleTy(context.TheContext), "booltmp");
    };

    switch (Op) {
    case '+':
        if (isIntOp) return context.Builder.CreateAdd(L, R, "addtmp");
        return context.Builder.CreateFAdd(L, R, "addtmp");
    case '-':
        if (isIntOp) return context.Builder.CreateSub(L, R, "subtmp");
        return context.Builder.CreateFSub(L, R, "subtmp");
    case '*':
        if (isIntOp) return context.Builder.CreateMul(L, R, "multmp");
        return context.Builder.CreateFMul(L, R, "multmp");
    case '/':
        if (isIntOp) return context.Builder.CreateSDiv(L, R, "divtmp");
        return context.Builder.CreateFDiv(L, R, "divtmp");
    
    // Bitwise operators
    case static_cast<int>(TokenType::tok_bitwise_and): {
        llvm::Value* LInt = context.Builder.CreateFPToSI(L, llvm::Type::getInt64Ty(context.TheContext), "andlhsint");
        llvm::Value* RInt = context.Builder.CreateFPToSI(R, llvm::Type::getInt64Ty(context.TheContext), "andrhsint");
        llvm::Value* Res = context.Builder.CreateAnd(LInt, RInt, "andtmp");
        return context.Builder.CreateSIToFP(Res, llvm::Type::getDoubleTy(context.TheContext), "andtmpfp");
    }
    case static_cast<int>(TokenType::tok_bitwise_or): {
        llvm::Value* LInt = context.Builder.CreateFPToSI(L, llvm::Type::getInt64Ty(context.TheContext), "orlhsint");
        llvm::Value* RInt = context.Builder.CreateFPToSI(R, llvm::Type::getInt64Ty(context.TheContext), "orrhsint");
        llvm::Value* Res = context.Builder.CreateOr(LInt, RInt, "ortmp");
        return context.Builder.CreateSIToFP(Res, llvm::Type::getDoubleTy(context.TheContext), "ortmpfp");
    }
    case static_cast<int>(TokenType::tok_bitwise_xor): {
        llvm::Value* LInt = context.Builder.CreateFPToSI(L, llvm::Type::getInt64Ty(context.TheContext), "xorlhsint");
        llvm::Value* RInt = context.Builder.CreateFPToSI(R, llvm::Type::getInt64Ty(context.TheContext), "xorrhsint");
        llvm::Value* Res = context.Builder.CreateXor(LInt, RInt, "xortmp");
        return context.Builder.CreateSIToFP(Res, llvm::Type::getDoubleTy(context.TheContext), "xortmpfp");
    }
    case static_cast<int>(TokenType::tok_bitwise_shl): {
        llvm::Value* LInt = context.Builder.CreateFPToSI(L, llvm::Type::getInt64Ty(context.TheContext), "shllhsint");
        llvm::Value* RInt = context.Builder.CreateFPToSI(R, llvm::Type::getInt64Ty(context.TheContext), "shlrhsint");
        llvm::Value* Res = context.Builder.CreateShl(LInt, RInt, "shltmp");
        return context.Builder.CreateSIToFP(Res, llvm::Type::getDoubleTy(context.TheContext), "shltmpfp");
    }
    case static_cast<int>(TokenType::tok_bitwise_shr): {
        llvm::Value* LInt = context.Builder.CreateFPToSI(L, llvm::Type::getInt64Ty(context.TheContext), "shrlhsint");
        llvm::Value* RInt = context.Builder.CreateFPToSI(R, llvm::Type::getInt64Ty(context.TheContext), "shrrhsint");
        llvm::Value* Res = context.Builder.CreateAShr(LInt, RInt, "shrtmp");
        return context.Builder.CreateSIToFP(Res, llvm::Type::getDoubleTy(context.TheContext), "shrtmpfp");
    }
    
    // Power operator - use llvm.pow
    case static_cast<int>(TokenType::tok_power): {
        llvm::Function* PowF = context.TheModule->getFunction("llvm.pow.f64");
        if (!PowF) {
            llvm::FunctionType* PowFTy = llvm::FunctionType::get(
                llvm::Type::getDoubleTy(context.TheContext),
                {llvm::Type::getDoubleTy(context.TheContext), 
                 llvm::Type::getDoubleTy(context.TheContext)}, false);
            PowF = llvm::Function::Create(PowFTy, llvm::Function::ExternalLinkage, "llvm.pow.f64", context.TheModule.get());
        }
        return context.Builder.CreateCall(PowF, {L, R}, "powtmp");
    }
    
    // Comparison operators
    case '<':
        return createBoolResult(context.Builder.CreateFCmpOLT(L, R, "cmptmp"));
    case '>':
        return createBoolResult(context.Builder.CreateFCmpOGT(L, R, "cmptmp"));
    case static_cast<int>(TokenType::tok_less_equal):
        return createBoolResult(context.Builder.CreateFCmpOLE(L, R, "cmptmp"));
    case static_cast<int>(TokenType::tok_greater_equal):
        return createBoolResult(context.Builder.CreateFCmpOGE(L, R, "cmptmp"));
    case static_cast<int>(TokenType::tok_equal):
        return createBoolResult(context.Builder.CreateFCmpOEQ(L, R, "cmptmp"));
    case static_cast<int>(TokenType::tok_not_equal):
        return createBoolResult(context.Builder.CreateFCmpUNE(L, R, "cmptmp"));
    
    // Logical operators (treat non-zero as true)
    case static_cast<int>(TokenType::tok_logical_and): {
        L = context.Builder.CreateFCmpONE(L, llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0)), "andlhs");
        R = context.Builder.CreateFCmpONE(R, llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0)), "andrhs");
        llvm::Value* And = context.Builder.CreateAnd(L, R, "andtmp");
        return createBoolResult(And);
    }
    case static_cast<int>(TokenType::tok_logical_or): {
        L = context.Builder.CreateFCmpONE(L, llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0)), "orlhs");
        R = context.Builder.CreateFCmpONE(R, llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0)), "orrhs");
        llvm::Value* Or = context.Builder.CreateOr(L, R, "ortmp");
        return createBoolResult(Or);
    }

    // Element-wise operators (MATLAB-style) - call Eigen helper functions
    case static_cast<int>(TokenType::tok_ew_mul): {
        llvm::Function* EwMulF = context.TheModule->getFunction("flux_vector_mul_elementwise");
        if (!EwMulF) {
            // For now, fall back to regular multiplication for scalars
            return context.Builder.CreateFMul(L, R, "ewmultmp");
        }
        return context.Builder.CreateCall(EwMulF, {L, R}, "ewmultmp");
    }
    case static_cast<int>(TokenType::tok_ew_div): {
        llvm::Function* EwDivF = context.TheModule->getFunction("flux_vector_div_elementwise");
        if (!EwDivF) {
            // For now, fall back to regular division for scalars
            return context.Builder.CreateFDiv(L, R, "ewdivtmp");
        }
        return context.Builder.CreateCall(EwDivF, {L, R}, "ewdivtmp");
    }
    case static_cast<int>(TokenType::tok_ew_power): {
        // Element-wise power - for now fall back to regular power for scalars
        llvm::Function* PowF = context.TheModule->getFunction("llvm.pow.f64");
        if (!PowF) {
            llvm::FunctionType* PowFTy = llvm::FunctionType::get(
                llvm::Type::getDoubleTy(context.TheContext),
                {llvm::Type::getDoubleTy(context.TheContext),
                 llvm::Type::getDoubleTy(context.TheContext)}, false);
            PowF = llvm::Function::Create(PowFTy, llvm::Function::ExternalLinkage, "llvm.pow.f64", context.TheModule.get());
        }
        return context.Builder.CreateCall(PowF, {L, R}, "ewpowtmp");
    }

    default:
        std::cerr << "Invalid binary operator: " << Op << std::endl;
        return nullptr;
    }
}

llvm::Value* UnaryExprAST::codegen(CodegenContext& context) {
    llvm::Value* OperandV = Operand->codegen(context);
    if (!OperandV) return nullptr;

    switch (Op) {
    case '-':
        return context.Builder.CreateFNeg(OperandV, "unaryminus");
    case '+':
        return OperandV; // Unary + is a no-op
    case '~':
    case static_cast<int>(TokenType::tok_bitwise_not): {
        llvm::Value* OperandInt = context.Builder.CreateFPToSI(OperandV, llvm::Type::getInt64Ty(context.TheContext), "notint");
        llvm::Value* NotV = context.Builder.CreateNot(OperandInt, "nottmp");
        return context.Builder.CreateSIToFP(NotV, llvm::Type::getDoubleTy(context.TheContext), "nottmpfp");
    }
    case '!':
    case static_cast<int>(TokenType::tok_logical_not): {
        // Logical not: compare to 0.0, then negate
        llvm::Value* IsNonZero = context.Builder.CreateFCmpONE(
            OperandV, llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0)), "isnonzero");
        llvm::Value* Not = context.Builder.CreateNot(IsNonZero, "lognot");
        return context.Builder.CreateUIToFP(Not, llvm::Type::getDoubleTy(context.TheContext), "booltmp");
    }
    default:
        std::cerr << "Invalid unary operator: " << Op << std::endl;
        return nullptr;
    }
}

llvm::Value* CallExprAST::codegen(CodegenContext& context) {
    llvm::Function* CalleeF = context.TheModule->getFunction(Callee);
    if (!CalleeF) {
        std::cerr << "Unknown function referenced: " << Callee << std::endl;
        return nullptr;
    }

    if (CalleeF->arg_size() != Args.size()) {
        std::cerr << "Incorrect # arguments passed. Expected " << CalleeF->arg_size() 
                  << ", got " << Args.size() << std::endl;
        return nullptr;
    }

    std::vector<llvm::Value*> ArgsV;
    for (unsigned i = 0, e = Args.size(); i != e; ++i) {
        llvm::Value* ArgV = Args[i]->codegen(context);
        if (!ArgV) return nullptr;

        llvm::Type* ExpectedTy = CalleeF->getArg(i)->getType();
        if (ArgV->getType() != ExpectedTy) {
            if (ExpectedTy->isIntegerTy() && ArgV->getType()->isFloatingPointTy()) {
                ArgV = context.Builder.CreateFPToSI(ArgV, ExpectedTy, "cast");
            } else if (ExpectedTy->isFloatingPointTy() && ArgV->getType()->isIntegerTy()) {
                ArgV = context.Builder.CreateSIToFP(ArgV, ExpectedTy, "cast");
            }
        }
        ArgsV.push_back(ArgV);
    }

    return context.Builder.CreateCall(CalleeF, ArgsV, "calltmp");
}

llvm::Value* IfExprAST::codegen(CodegenContext& context) {
    llvm::Value* CondV = Cond->codegen(context);
    if (!CondV) return nullptr;

    // Convert condition to a bool by comparing non-equal to 0.0.
    CondV = context.Builder.CreateFCmpONE(
        CondV, llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0)), "ifcond");

    llvm::Function* TheFunction = context.Builder.GetInsertBlock()->getParent();

    // Create blocks for the then and else cases.  Insert the 'then' block at the
    // end of the function.
    llvm::BasicBlock* ThenBB = llvm::BasicBlock::Create(context.TheContext, "then", TheFunction);
    llvm::BasicBlock* ElseBB = llvm::BasicBlock::Create(context.TheContext, "else");
    llvm::BasicBlock* MergeBB = llvm::BasicBlock::Create(context.TheContext, "ifcont");

    context.Builder.CreateCondBr(CondV, ThenBB, ElseBB);

    // Emit then value.
    context.Builder.SetInsertPoint(ThenBB);

    llvm::Value* ThenV = Then->codegen(context);
    if (!ThenV) return nullptr;

    context.Builder.CreateBr(MergeBB);
    // Codegen of 'Then' can change the current block, update ThenBB for the PHI.
    ThenBB = context.Builder.GetInsertBlock();

    // Emit else block.
    TheFunction->insert(TheFunction->end(), ElseBB);
    context.Builder.SetInsertPoint(ElseBB);

    llvm::Value* ElseV = Else->codegen(context);
    if (!ElseV) return nullptr;

    context.Builder.CreateBr(MergeBB);
    // Codegen of 'Else' can change the current block, update ElseBB for the PHI.
    ElseBB = context.Builder.GetInsertBlock();

    // Emit merge block.
    TheFunction->insert(TheFunction->end(), MergeBB);
    context.Builder.SetInsertPoint(MergeBB);
    llvm::PHINode* PN = context.Builder.CreatePHI(llvm::Type::getDoubleTy(context.TheContext), 2, "iftmp");

    PN->addIncoming(ThenV, ThenBB);
    PN->addIncoming(ElseV, ElseBB);
    return PN;
}

llvm::Value* ForExprAST::codegen(CodegenContext& context) {
    // Generate code for start, end, and optional step
    llvm::Value* StartV = Start->codegen(context);
    if (!StartV) return nullptr;

    llvm::Value* EndV = End->codegen(context);
    if (!EndV) return nullptr;

    llvm::Value* StepV = Step ? Step->codegen(context) : nullptr;
    if (Step && !StepV) return nullptr;

    // Default step is 1.0
    if (!StepV) {
        StepV = llvm::ConstantFP::get(context.TheContext, llvm::APFloat(1.0));
    }

    llvm::Function* TheFunction = context.Builder.GetInsertBlock()->getParent();

    // Create blocks
    llvm::BasicBlock* PreheaderBB = context.Builder.GetInsertBlock();
    llvm::BasicBlock* LoopBB = llvm::BasicBlock::Create(context.TheContext, "loop", TheFunction);
    llvm::BasicBlock* AfterBB = llvm::BasicBlock::Create(context.TheContext, "afterloop");

    // Branch to loop
    context.Builder.CreateBr(LoopBB);

    // Emit loop body
    context.Builder.SetInsertPoint(LoopBB);

    // Create PHI node for the loop variable
    llvm::PHINode* Variable = context.Builder.CreatePHI(llvm::Type::getDoubleTy(context.TheContext), 2, VarName.c_str());
    Variable->addIncoming(StartV, PreheaderBB);

    // Save old value and set the loop variable
    llvm::Value* OldVal = context.NamedValues[VarName];
    context.NamedValues[VarName] = Variable;

    // Emit the body
    llvm::Value* BodyV = Body->codegen(context);
    if (!BodyV) return nullptr;

    // Increment the variable
    llvm::Value* NextVar = context.Builder.CreateFAdd(Variable, StepV, "nextvar");
    Variable->addIncoming(NextVar, context.Builder.GetInsertBlock());

    // End the loop variable scope
    context.NamedValues[VarName] = OldVal;

    // Check loop condition
    llvm::Value* EndCond = context.Builder.CreateFCmpOLT(NextVar, EndV, "loopcond");
    llvm::BasicBlock* LoopEndBB = context.Builder.GetInsertBlock();
    context.Builder.CreateCondBr(EndCond, LoopBB, AfterBB);

    // Emit after block
    TheFunction->insert(TheFunction->end(), AfterBB);
    context.Builder.SetInsertPoint(AfterBB);

    // Return the last value of the loop variable (or 0)
    return BodyV;
}

llvm::Value* WhileExprAST::codegen(CodegenContext& context) {
    llvm::Function* TheFunction = context.Builder.GetInsertBlock()->getParent();

    // Create blocks
    llvm::BasicBlock* CondBB = llvm::BasicBlock::Create(context.TheContext, "whilecond", TheFunction);
    llvm::BasicBlock* BodyBB = llvm::BasicBlock::Create(context.TheContext, "whilebody");
    llvm::BasicBlock* AfterBB = llvm::BasicBlock::Create(context.TheContext, "afterwhile");

    // Branch to condition check
    context.Builder.CreateBr(CondBB);

    // Emit condition block
    context.Builder.SetInsertPoint(CondBB);
    llvm::Value* CondV = Cond->codegen(context);
    if (!CondV) return nullptr;

    // Convert condition to bool
    CondV = context.Builder.CreateFCmpONE(
        CondV, llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0)), "whilecond");

    context.Builder.CreateCondBr(CondV, BodyBB, AfterBB);

    // Emit body block
    TheFunction->insert(TheFunction->end(), BodyBB);
    context.Builder.SetInsertPoint(BodyBB);

    llvm::Value* BodyV = Body->codegen(context);
    if (!BodyV) return nullptr;

    // Branch back to condition
    context.Builder.CreateBr(CondBB);

    // Emit after block
    TheFunction->insert(TheFunction->end(), AfterBB);
    context.Builder.SetInsertPoint(AfterBB);

    // Return the body's last value (or 0.0)
    return BodyV ? BodyV : llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0));
}

llvm::Value* LetExprAST::codegen(CodegenContext& context) {
    llvm::Value* InitV = Init->codegen(context);
    if (!InitV) return nullptr;

    llvm::Type* VarTy = Type.getLLVMType(context.TheContext);
    if (InitV->getType() != VarTy) {
        if (VarTy->isIntegerTy() && InitV->getType()->isFloatingPointTy()) {
            InitV = context.Builder.CreateFPToSI(InitV, VarTy, "cast");
        } else if (VarTy->isFloatingPointTy() && InitV->getType()->isIntegerTy()) {
            InitV = context.Builder.CreateSIToFP(InitV, VarTy, "cast");
        }
    }

    llvm::AllocaInst* Alloca = context.Builder.CreateAlloca(VarTy, nullptr, VarName.c_str());
    context.Builder.CreateStore(InitV, Alloca);

    llvm::Value* OldVal = context.NamedValues[VarName];
    context.NamedValues[VarName] = Alloca;

    llvm::Value* BodyV = Body->codegen(context);
    if (!BodyV) return nullptr;

    context.NamedValues[VarName] = OldVal;

    return BodyV;
}

llvm::Value* VectorExprAST::codegen(CodegenContext& context) {
    // Create Eigen::VectorXd using helper function
    // We'll create the vector on the heap and return a pointer
    
    // First, allocate space for the vector data
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
    llvm::Type* Int32Ty = llvm::Type::getInt32Ty(context.TheContext);
    llvm::Type* Int64Ty = llvm::Type::getInt64Ty(context.TheContext);
    llvm::Type* VoidPtrTy = llvm::PointerType::get(context.TheContext, 0);
    
    // Get malloc function
    llvm::Function* MallocF = context.TheModule->getFunction("malloc");
    if (!MallocF) {
        llvm::FunctionType* MallocFTy = llvm::FunctionType::get(
            VoidPtrTy, { Int32Ty }, false);
        MallocF = llvm::Function::Create(MallocFTy, llvm::Function::ExternalLinkage, "malloc", context.TheModule.get());
    }
    
    // Allocate memory for the data array (size * 8 bytes per double)
    llvm::Value* DataSize = llvm::ConstantInt::get(Int32Ty, Elements.size() * 8);
    llvm::Value* DataPtr = context.Builder.CreateCall(MallocF, {DataSize}, "vec_data");
    
    // Store each element
    for (size_t i = 0; i < Elements.size(); ++i) {
        llvm::Value* ElemVal = Elements[i]->codegen(context);
        if (!ElemVal) return nullptr;
        
        llvm::Value* Index = llvm::ConstantInt::get(Int64Ty, i);
        llvm::Value* ElemPtr = context.Builder.CreateInBoundsGEP(DoubleTy, DataPtr, {Index}, "elem_ptr");
        context.Builder.CreateStore(ElemVal, ElemPtr);
    }
    
    // Get create_vector_sum helper function
    llvm::Function* CreateVecSumF = context.TheModule->getFunction("flux_create_vector_sum");
    if (!CreateVecSumF) {
        llvm::FunctionType* CreateVecSumFTy = llvm::FunctionType::get(
            llvm::Type::getDoubleTy(context.TheContext),
            { VoidPtrTy, Int32Ty }, false);
        CreateVecSumF = llvm::Function::Create(CreateVecSumFTy, llvm::Function::ExternalLinkage,
            "flux_create_vector_sum", context.TheModule.get());
    }

    // Call the helper function to compute the sum
    llvm::Value* SizeVal = llvm::ConstantInt::get(Int32Ty, Elements.size());
    return context.Builder.CreateCall(CreateVecSumF, {DataPtr, SizeVal}, "vec_sum");
}

llvm::Value* IndexExprAST::codegen(CodegenContext& context) {
    // For now, indexing returns the indexed element value directly
    // This is a simplified implementation that works with scalar values
    llvm::Value* IndexVal = Index->codegen(context);
    if (!IndexVal) return nullptr;
    
    // For scalar "arrays", just return the element at the given index
    // This is a placeholder until full vector support is implemented
    std::cerr << "Warning: Array indexing is experimental and may not work as expected" << std::endl;
    return IndexVal;  // Placeholder: just return the index value
}

llvm::Function* PrototypeAST::codegen(CodegenContext& context) {
    // Build argument types from typed parameters
    std::vector<llvm::Type*> ArgTypes;
    for (const auto& Arg : Args) {
        ArgTypes.push_back(Arg.second.getLLVMType(context.TheContext));
    }
    
    llvm::FunctionType* FT = llvm::FunctionType::get(ReturnType.getLLVMType(context.TheContext), ArgTypes, false);
    llvm::Function* F = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, Name, context.TheModule.get());

    unsigned Idx = 0;
    for (auto& Arg : F->args())
        Arg.setName(Args[Idx++].first);

    return F;
}

llvm::Function* FunctionAST::codegen(CodegenContext& context) {
    llvm::Function* TheFunction = context.TheModule->getFunction(Proto->getName());

    if (!TheFunction)
        TheFunction = Proto->codegen(context);

    if (!TheFunction)
        return nullptr;

    llvm::BasicBlock* BB = llvm::BasicBlock::Create(context.TheContext, "entry", TheFunction);
    context.Builder.SetInsertPoint(BB);

    context.NamedValues.clear();
    
    // Get the argument types from the prototype
    const auto& ArgTypes = Proto->getArgs();
    unsigned Idx = 0;
    for (auto& Arg : TheFunction->args()) {
        // Get the type for this argument
        llvm::Type* ArgTy = ArgTypes[Idx].second.getLLVMType(context.TheContext);
        
        // Create an alloca for this variable.
        llvm::AllocaInst* Alloca = context.Builder.CreateAlloca(ArgTy, nullptr, std::string(Arg.getName()));

        // Store the initial value into the alloca.
        context.Builder.CreateStore(&Arg, Alloca);

        // Add arguments to variable symbol table.
        context.NamedValues[std::string(Arg.getName())] = Alloca;
        Idx++;
    }

    if (llvm::Value* RetVal = Body->codegen(context)) {
        llvm::Type* RetTy = Proto->getReturnType().getLLVMType(context.TheContext);
        if (RetVal->getType() != RetTy) {
            if (RetTy->isIntegerTy() && RetVal->getType()->isFloatingPointTy()) {
                RetVal = context.Builder.CreateFPToSI(RetVal, RetTy, "cast");
            } else if (RetTy->isFloatingPointTy() && RetVal->getType()->isIntegerTy()) {
                RetVal = context.Builder.CreateSIToFP(RetVal, RetTy, "cast");
            }
        }
        context.Builder.CreateRet(RetVal);
        llvm::verifyFunction(*TheFunction);
        return TheFunction;
    }

    TheFunction->eraseFromParent();
    return nullptr;
}

} // namespace Flux
