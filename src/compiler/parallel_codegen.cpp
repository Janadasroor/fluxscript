// Parallel for loop codegen - Actual parallel execution support
#include "flux/compiler/ast.h"
#include "flux/runtime/parallel_runtime.h"
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>

namespace Flux {

TypedValue ParallelForExprAST::codegen(CodegenContext& context) {
    // Generate parallel for loop
    // For now, generate as sequential loop to ensure correct variable scoping
    // Parallel execution with closure capture requires more complex infrastructure

    llvm::Function* TheFunction = context.Builder.GetInsertBlock()->getParent();
    llvm::LLVMContext& Ctx = context.TheContext;

    // Generate start and end values
    TypedValue StartTV = Start->codegen(context);
    TypedValue EndTV = End->codegen(context);
    if (!StartTV.Val || !EndTV.Val) return TypedValue();

    // Create loop blocks
    llvm::BasicBlock* PreheaderBB = context.Builder.GetInsertBlock();
    llvm::BasicBlock* LoopBB = llvm::BasicBlock::Create(Ctx, "par_loop", TheFunction);
    llvm::BasicBlock* AfterBB = llvm::BasicBlock::Create(Ctx, "after_par_loop");

    // Save old variable
    llvm::Value* OldVal = context.NamedValues[VarName];

    // Branch to loop
    context.Builder.CreateBr(LoopBB);
    context.Builder.SetInsertPoint(LoopBB);

    // Create PHI for loop variable
    llvm::PHINode* Variable = context.Builder.CreatePHI(llvm::Type::getDoubleTy(Ctx), 2, VarName.c_str());
    Variable->addIncoming(StartTV.Val, PreheaderBB);

    // Set loop variable in scope
    context.NamedValues[VarName] = Variable;

    // Generate body
    context.CurrentLoopEnd = AfterBB;
    context.CurrentLoopCont = LoopBB;

    TypedValue BodyTV = Body->codegen(context);

    // Restore scope
    context.CurrentLoopEnd = nullptr;
    context.CurrentLoopCont = nullptr;

    // Increment
    llvm::Value* NextVar = context.Builder.CreateFAdd(Variable,
        llvm::ConstantFP::get(Ctx, llvm::APFloat(1.0)), "nextvar");
    Variable->addIncoming(NextVar, context.Builder.GetInsertBlock());

    // Restore old variable
    context.NamedValues[VarName] = OldVal;

    // Check condition
    llvm::Value* EndCond = context.Builder.CreateFCmpOLT(NextVar, EndTV.Val, "loopcond");
    context.Builder.CreateCondBr(EndCond, LoopBB, AfterBB);

    // After loop
    TheFunction->insert(TheFunction->end(), AfterBB);
    context.Builder.SetInsertPoint(AfterBB);

    std::cout << "[CodeGen] Generated parallel for loop (sequential): " << VarName << std::endl;

    return BodyTV ? BodyTV : TypedValue(llvm::ConstantFP::get(Ctx, llvm::APFloat(0.0)), TypeKind::Double);
}

} // namespace Flux
