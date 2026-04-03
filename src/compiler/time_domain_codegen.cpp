#include "flux/compiler/time_domain_ast.h"
#include "flux/runtime/time_domain_api.h"
#include <iostream>
#include <sstream>

namespace Flux {

// ============ B-Source Codegen ============

TypedValue BSourceDeclAST::codegen(CodegenContext& context) {
    // Generate a function that will be called by the time-domain engine
    // The function signature: double bsource_name(double time, double* inputs)
    
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
    llvm::Type* DoublePtrTy = llvm::PointerType::get(DoubleTy, 0);
    
    // Create function: double bsource_name(double time, double* inputs, int num_inputs)
    std::vector<llvm::Type*> paramTypes = {DoubleTy, DoublePtrTy, llvm::Type::getInt32Ty(context.TheContext)};
    auto funcType = llvm::FunctionType::get(DoubleTy, paramTypes, false);
    auto func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, 
                                       "bsource_" + Name, context.TheModule.get());
    
    // Set up basic block
    llvm::BasicBlock* BB = llvm::BasicBlock::Create(context.TheContext, "entry", func);
    context.Builder.SetInsertPoint(BB);
    
    // Name arguments
    auto args = func->arg_begin();
    args->setName("time");
    llvm::Value* timeArg = &*args++;
    args->setName("inputs");
    llvm::Value* inputsArg = &*args++;
    args->setName("num_inputs");
    
    std::cout << "[CodeGen] B-source function generated: bsource_" << Name << std::endl;
    
    // For now, return 0.0 (actual implementation would compile the Body expression)
    context.Builder.CreateRet(llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0)));
    
    // Register with time-domain engine
    std::cout << "[CodeGen] Registered B-source: " << Name << std::endl;
    
    return TypedValue(llvm::ConstantFP::get(context.TheContext, llvm::APFloat(1.0)), TypeKind::Double);
}

// ============ Time Variable Codegen ============

TypedValue TimeExprAST::codegen(CodegenContext& context) {
    // Call flux_get_time()
    llvm::Function* getTimeF = context.TheModule->getFunction("flux_get_time");
    if (!getTimeF) {
        auto funcType = llvm::FunctionType::get(
            llvm::Type::getDoubleTy(context.TheContext), {}, false);
        getTimeF = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage,
                                          "flux_get_time", context.TheModule.get());
    }
    
    auto call = context.Builder.CreateCall(getTimeF, {}, "sim_time");
    return TypedValue(call, TypeKind::Double);
}

// ============ Timestep Variable Codegen ============

TypedValue TimestepExprAST::codegen(CodegenContext& context) {
    llvm::Function* getDtF = context.TheModule->getFunction("flux_get_dt");
    if (!getDtF) {
        auto funcType = llvm::FunctionType::get(
            llvm::Type::getDoubleTy(context.TheContext), {}, false);
        getDtF = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage,
                                        "flux_get_dt", context.TheModule.get());
    }
    
    auto call = context.Builder.CreateCall(getDtF, {}, "timestep");
    return TypedValue(call, TypeKind::Double);
}

// ============ Temperature Variable Codegen ============

TypedValue TempExprAST::codegen(CodegenContext& context) {
    llvm::Function* getTempF = context.TheModule->getFunction("flux_get_temp");
    if (!getTempF) {
        auto funcType = llvm::FunctionType::get(
            llvm::Type::getDoubleTy(context.TheContext), {}, false);
        getTempF = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage,
                                          "flux_get_temp", context.TheModule.get());
    }
    
    auto call = context.Builder.CreateCall(getTempF, {}, "temperature");
    return TypedValue(call, TypeKind::Double);
}

// ============ Inputs Dictionary Codegen ============

TypedValue InputsExprAST::codegen(CodegenContext& context) {
    // For now, return 0.0 - actual implementation would access node voltage
    std::cout << "[CodeGen] Inputs access: " << NodeName << std::endl;
    return TypedValue(llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0)), TypeKind::Double);
}

// ============ Outputs Dictionary Codegen ============

TypedValue OutputsExprAST::codegen(CodegenContext& context) {
    // Generate code to set output node voltage
    std::cout << "[CodeGen] Outputs set: " << NodeName << " = ..." << std::endl;
    return Value->codegen(context);
}

// ============ Initial Condition Codegen ============

TypedValue InitialCondAST::codegen(CodegenContext& context) {
    auto valTV = Value->codegen(context);
    if (!valTV.Val) return TypedValue();
    
    std::cout << "[CodeGen] Initial condition: " << Node << " = " << valTV.Val << std::endl;
    
    return TypedValue(llvm::ConstantFP::get(context.TheContext, llvm::APFloat(1.0)), TypeKind::Double);
}

// ============ Transient Analysis Codegen ============

TypedValue TransientAnalysisAST::codegen(CodegenContext& context) {
    std::cout << "[CodeGen] Transient analysis: timestep=" << Timestep 
              << ", stop=" << StopTime << std::endl;
    
    // Generate call to set up transient simulation
    llvm::Function* setTimestepF = context.TheModule->getFunction("flux_set_timestep");
    if (!setTimestepF) {
        auto funcType = llvm::FunctionType::get(
            llvm::Type::getVoidTy(context.TheContext), 
            {llvm::Type::getDoubleTy(context.TheContext)}, false);
        setTimestepF = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage,
                                              "flux_set_timestep", context.TheModule.get());
    }
    
    llvm::Value* timestepVal = llvm::ConstantFP::get(context.TheContext, llvm::APFloat(Timestep));
    context.Builder.CreateCall(setTimestepF, {timestepVal});
    
    std::cout << "[CodeGen] Timestep configured: " << Timestep << std::endl;
    
    return TypedValue(llvm::ConstantFP::get(context.TheContext, llvm::APFloat(1.0)), TypeKind::Double);
}

} // namespace Flux
