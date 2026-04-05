// Codegen for New Features (Debug, Sensitivity, Ask, Explain, Substitute)
#include "flux/compiler/ast.h"
#include "flux/runtime/flux_runtime.h"
#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>

namespace Flux {

// Debug statement codegen
TypedValue DebugStmtAST::codegen(CodegenContext& context) {
    llvm::Type* VoidPtrTy = llvm::PointerType::get(context.TheContext, 0);
    llvm::Type* Int32Ty = llvm::Type::getInt32Ty(context.TheContext);
    
    // Get or declare debug function
    llvm::Function* debugFunc = context.TheModule->getFunction("flux_debug_create");
    if (!debugFunc) {
        llvm::FunctionType* FT = llvm::FunctionType::get(VoidPtrTy, false);
        debugFunc = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "flux_debug_create", context.TheModule.get());
    }
    
    // Create debugger
    llvm::Value* debugger = context.Builder.CreateCall(debugFunc);
    
    // Load circuit
    if (!CircuitFile.empty()) {
        llvm::Function* loadFunc = context.TheModule->getFunction("flux_debug_load");
        if (!loadFunc) {
            llvm::FunctionType* FT = llvm::FunctionType::get(llvm::Type::getVoidTy(context.TheContext),
                                                              llvm::ArrayRef<llvm::Type*>{VoidPtrTy, VoidPtrTy}, false);
            loadFunc = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "flux_debug_load", context.TheModule.get());
        }
        
        llvm::Value* circuitStr = context.Builder.CreateGlobalStringPtr(CircuitFile);
        context.Builder.CreateCall(loadFunc, {debugger, circuitStr});
    }
    
    // Add symptom
    if (!Symptom.empty()) {
        llvm::Function* addSymptomFunc = context.TheModule->getFunction("flux_debug_add_symptom");
        if (!addSymptomFunc) {
            llvm::FunctionType* FT = llvm::FunctionType::get(llvm::Type::getVoidTy(context.TheContext),
                                                              llvm::ArrayRef<llvm::Type*>{VoidPtrTy, VoidPtrTy,
                                                               VoidPtrTy}, false);
            addSymptomFunc = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "flux_debug_add_symptom", context.TheModule.get());
        }
        
        llvm::Value* typeStr = context.Builder.CreateGlobalStringPtr("auto");
        llvm::Value* symptomStr = context.Builder.CreateGlobalStringPtr(Symptom);
        context.Builder.CreateCall(addSymptomFunc, {debugger, typeStr, symptomStr});
    }
    
    // Run diagnosis
    llvm::Function* diagnoseFunc = context.TheModule->getFunction("flux_debug_diagnose");
    if (!diagnoseFunc) {
        llvm::FunctionType* FT = llvm::FunctionType::get(VoidPtrTy,
                                                          llvm::ArrayRef<llvm::Type*>{VoidPtrTy}, false);
        diagnoseFunc = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "flux_debug_diagnose", context.TheModule.get());
    }
    
    llvm::Value* result = context.Builder.CreateCall(diagnoseFunc, {debugger});
    
    // Return result as string
    return TypedValue(result, TypeKind::String);
}

// Sensitivity statement codegen
TypedValue SensitivityStmtAST::codegen(CodegenContext& context) {
    llvm::Type* VoidPtrTy = llvm::PointerType::get(context.TheContext, 0);
    
    // Create sensitivity analyzer
    llvm::Function* createFunc = context.TheModule->getFunction("flux_sens_create");
    if (!createFunc) {
        llvm::FunctionType* FT = llvm::FunctionType::get(VoidPtrTy, false);
        createFunc = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "flux_sens_create", context.TheModule.get());
    }
    
    llvm::Value* analyzer = context.Builder.CreateCall(createFunc);
    
    // Load circuit
    if (!CircuitFile.empty()) {
        llvm::Function* loadFunc = context.TheModule->getFunction("flux_sens_load");
        if (!loadFunc) {
            llvm::FunctionType* FT = llvm::FunctionType::get(llvm::Type::getVoidTy(context.TheContext),
                                                              llvm::ArrayRef<llvm::Type*>{VoidPtrTy, VoidPtrTy}, false);
            loadFunc = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "flux_sens_load", context.TheModule.get());
        }
        
        llvm::Value* circuitStr = context.Builder.CreateGlobalStringPtr(CircuitFile);
        context.Builder.CreateCall(loadFunc, {analyzer, circuitStr});
    }
    
    // Set output variable
    llvm::Function* setOutputFunc = context.TheModule->getFunction("flux_sens_set_output");
    if (!setOutputFunc) {
        llvm::FunctionType* FT = llvm::FunctionType::get(llvm::Type::getVoidTy(context.TheContext),
                                                          llvm::ArrayRef<llvm::Type*>{VoidPtrTy, VoidPtrTy}, false);
        setOutputFunc = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "flux_sens_set_output", context.TheModule.get());
    }
    
    llvm::Value* outputStr = context.Builder.CreateGlobalStringPtr(OutputVar);
    context.Builder.CreateCall(setOutputFunc, {analyzer, outputStr});
    
    // Set parameters
    llvm::Function* setParamFunc = context.TheModule->getFunction("flux_sens_set_parameter");
    if (!setParamFunc) {
        llvm::FunctionType* FT = llvm::FunctionType::get(llvm::Type::getVoidTy(context.TheContext),
                                                          llvm::ArrayRef<llvm::Type*>{VoidPtrTy, VoidPtrTy,
                                                           llvm::Type::getDoubleTy(context.TheContext),
                                                           llvm::Type::getDoubleTy(context.TheContext)}, false);
        setParamFunc = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "flux_sens_set_parameter", context.TheModule.get());
    }
    
    for (const auto& param : Params) {
        llvm::Value* paramStr = context.Builder.CreateGlobalStringPtr(param);
        context.Builder.CreateCall(setParamFunc, {analyzer, paramStr,
                                                   llvm::ConstantFP::get(context.TheContext, llvm::APFloat(10000.0)),
                                                   llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.01))});
    }
    
    // Run analysis
    llvm::Function* analyzeFunc = context.TheModule->getFunction("flux_sens_analyze");
    if (!analyzeFunc) {
        llvm::FunctionType* FT = llvm::FunctionType::get(llvm::Type::getVoidTy(context.TheContext),
                                                          llvm::ArrayRef<llvm::Type*>{VoidPtrTy}, false);
        analyzeFunc = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "flux_sens_analyze", context.TheModule.get());
    }
    
    context.Builder.CreateCall(analyzeFunc, {analyzer});
    
    // Get results
    llvm::Function* getResultsFunc = context.TheModule->getFunction("flux_sens_get_results");
    if (!getResultsFunc) {
        llvm::FunctionType* FT = llvm::FunctionType::get(VoidPtrTy,
                                                          llvm::ArrayRef<llvm::Type*>{VoidPtrTy}, false);
        getResultsFunc = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "flux_sens_get_results", context.TheModule.get());
    }
    
    llvm::Value* result = context.Builder.CreateCall(getResultsFunc, {analyzer});
    
    return TypedValue(result, TypeKind::String);
}

// Ask expression codegen
TypedValue AskExprAST::codegen(CodegenContext& context) {
    llvm::Type* VoidPtrTy = llvm::PointerType::get(context.TheContext, 0);
    
    // Create NLP processor
    llvm::Function* createFunc = context.TheModule->getFunction("flux_nlp_create");
    if (!createFunc) {
        llvm::FunctionType* FT = llvm::FunctionType::get(VoidPtrTy, false);
        createFunc = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "flux_nlp_create", context.TheModule.get());
    }
    
    llvm::Value* nlp = context.Builder.CreateCall(createFunc);
    
    // Initialize
    llvm::Function* initFunc = context.TheModule->getFunction("flux_nlp_initialize");
    if (!initFunc) {
        llvm::FunctionType* FT = llvm::FunctionType::get(llvm::Type::getVoidTy(context.TheContext),
                                                          llvm::ArrayRef<llvm::Type*>{VoidPtrTy, VoidPtrTy}, false);
        initFunc = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "flux_nlp_initialize", context.TheModule.get());
    }
    
    llvm::Value* domainStr = context.Builder.CreateGlobalStringPtr("electronics");
    context.Builder.CreateCall(initFunc, {nlp, domainStr});
    
    // Query
    llvm::Function* queryFunc = context.TheModule->getFunction("flux_nlp_query");
    if (!queryFunc) {
        llvm::FunctionType* FT = llvm::FunctionType::get(VoidPtrTy,
                                                          llvm::ArrayRef<llvm::Type*>{VoidPtrTy, VoidPtrTy}, false);
        queryFunc = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "flux_nlp_query", context.TheModule.get());
    }
    
    llvm::Value* questionStr = context.Builder.CreateGlobalStringPtr(Question);
    llvm::Value* result = context.Builder.CreateCall(queryFunc, {nlp, questionStr});
    
    return TypedValue(result, TypeKind::String);
}

// Explain expression codegen
TypedValue ExplainExprAST::codegen(CodegenContext& context) {
    llvm::Type* VoidPtrTy = llvm::PointerType::get(context.TheContext, 0);
    
    // Create explainer
    llvm::Function* createFunc = context.TheModule->getFunction("flux_explainer_create");
    if (!createFunc) {
        llvm::FunctionType* FT = llvm::FunctionType::get(VoidPtrTy, false);
        createFunc = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "flux_explainer_create", context.TheModule.get());
    }
    
    llvm::Value* explainer = context.Builder.CreateCall(createFunc);
    
    // Load circuit
    if (!CircuitFile.empty()) {
        llvm::Function* loadFunc = context.TheModule->getFunction("flux_explainer_load");
        if (!loadFunc) {
            llvm::FunctionType* FT = llvm::FunctionType::get(llvm::Type::getVoidTy(context.TheContext),
                                                              llvm::ArrayRef<llvm::Type*>{VoidPtrTy, VoidPtrTy}, false);
            loadFunc = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "flux_explainer_load", context.TheModule.get());
        }
        
        llvm::Value* circuitStr = context.Builder.CreateGlobalStringPtr(CircuitFile);
        context.Builder.CreateCall(loadFunc, {explainer, circuitStr});
    }
    
    // Get explanation
    llvm::Function* explainFunc = context.TheModule->getFunction("flux_explainer_explain");
    if (!explainFunc) {
        llvm::FunctionType* FT = llvm::FunctionType::get(VoidPtrTy,
                                                          llvm::ArrayRef<llvm::Type*>{VoidPtrTy}, false);
        explainFunc = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "flux_explainer_explain", context.TheModule.get());
    }
    
    llvm::Value* result = context.Builder.CreateCall(explainFunc, {explainer});
    
    return TypedValue(result, TypeKind::String);
}

// Substitute statement codegen
TypedValue SubstituteStmtAST::codegen(CodegenContext& context) {
    llvm::Type* VoidPtrTy = llvm::PointerType::get(context.TheContext, 0);
    
    // Create substitution engine
    llvm::Function* createFunc = context.TheModule->getFunction("flux_sub_engine_create");
    if (!createFunc) {
        llvm::FunctionType* FT = llvm::FunctionType::get(VoidPtrTy, false);
        createFunc = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "flux_sub_engine_create", context.TheModule.get());
    }
    
    llvm::Value* engine = context.Builder.CreateCall(createFunc);
    
    // Find substitutes
    llvm::Function* findFunc = context.TheModule->getFunction("flux_sub_find");
    if (!findFunc) {
        llvm::FunctionType* FT = llvm::FunctionType::get(VoidPtrTy,
                                                          llvm::ArrayRef<llvm::Type*>{VoidPtrTy, VoidPtrTy}, false);
        findFunc = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "flux_sub_find", context.TheModule.get());
    }
    
    llvm::Value* partStr = context.Builder.CreateGlobalStringPtr(PartNumber);
    llvm::Value* result = context.Builder.CreateCall(findFunc, {engine, partStr});
    
    return TypedValue(result, TypeKind::String);
}

} // namespace Flux
