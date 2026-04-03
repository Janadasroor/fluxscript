#include "flux/compiler/ast.h"
#include "flux/runtime/parallel_runtime.h"
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>

namespace Flux {

TypedValue ParallelForExprAST::codegen(CodegenContext& context) {
    // Generate parallel for loop
    // This creates a call to the parallel runtime
    
    llvm::Function* TheFunction = context.Builder.GetInsertBlock()->getParent();
    
    // Generate start and end values
    TypedValue StartTV = Start->codegen(context);
    TypedValue EndTV = End->codegen(context);
    if (!StartTV.Val || !EndTV.Val) return TypedValue();
    
    // Convert to integers
    llvm::Value* StartInt = context.Builder.CreateFPToSI(StartTV.Val, llvm::Type::getInt32Ty(context.TheContext));
    llvm::Value* EndInt = context.Builder.CreateFPToSI(EndTV.Val, llvm::Type::getInt32Ty(context.TheContext));
    
    // Generate call to parallel runtime
    // flux_parallel_for(start, end, chunk_size, body_function)
    
    llvm::Type* Int32Ty = llvm::Type::getInt32Ty(context.TheContext);
    llvm::Type* VoidTy = llvm::Type::getVoidTy(context.TheContext);
    llvm::Type* Int32PtrTy = llvm::PointerType::get(Int32Ty, 0);
    
    // Get or create the parallel_for function
    llvm::Function* ParallelForF = context.TheModule->getFunction("flux_parallel_for");
    if (!ParallelForF) {
        // Create stub that calls the runtime
        ParallelForF = llvm::Function::Create(
            llvm::FunctionType::get(VoidTy, {Int32Ty, Int32Ty, Int32Ty}, false),
            llvm::Function::ExternalLinkage,
            "flux_parallel_for",
            context.TheModule.get());
    }
    
    // For now, generate a sequential loop as placeholder
    // The full implementation would create thread pool work distribution
    
    llvm::BasicBlock* PreheaderBB = context.Builder.GetInsertBlock();
    llvm::BasicBlock* LoopBB = llvm::BasicBlock::Create(context.TheContext, "parallel_loop", TheFunction);
    llvm::BasicBlock* AfterBB = llvm::BasicBlock::Create(context.TheContext, "after_parallel_loop");
    
    context.Builder.CreateBr(LoopBB);
    context.Builder.SetInsertPoint(LoopBB);
    
    llvm::PHINode* Variable = context.Builder.CreatePHI(llvm::Type::getDoubleTy(context.TheContext), 2, VarName.c_str());
    Variable->addIncoming(StartTV.Val, PreheaderBB);
    
    llvm::Value* OldVal = context.NamedValues[VarName];
    context.NamedValues[VarName] = Variable;
    
    TypedValue BodyTV = Body->codegen(context);
    if (!BodyTV.Val) return TypedValue();
    
    llvm::Value* NextVar = context.Builder.CreateFAdd(Variable, llvm::ConstantFP::get(context.TheContext, llvm::APFloat(1.0)), "nextvar");
    Variable->addIncoming(NextVar, context.Builder.GetInsertBlock());
    context.NamedValues[VarName] = OldVal;
    
    llvm::Value* EndCond = context.Builder.CreateFCmpOLT(NextVar, EndTV.Val, "loopcond");
    context.Builder.CreateCondBr(EndCond, LoopBB, AfterBB);
    
    TheFunction->insert(TheFunction->end(), AfterBB);
    context.Builder.SetInsertPoint(AfterBB);
    
    std::cout << "[CodeGen] Generated parallel for loop (sequential placeholder)" << std::endl;
    
    return TypedValue(llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0)), TypeKind::Double);
}

} // namespace Flux
