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

#include "flux/compiler/time_domain_ast.h"
#include "flux/runtime/time_domain_api.h"
#include <algorithm>
#include <iostream>
#include <sstream>

namespace Flux {

// ============ B-Source Codegen ============

TypedValue BSourceDeclAST::codegen(CodegenContext& context)
{
    // Generate a function that will be called by the time-domain engine
    // The function signature: double bsource_name(double time, double* inputs, int num_inputs)

    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
    llvm::Type* DoublePtrTy = llvm::PointerType::get(DoubleTy->getContext(), 0);

    // Create function: double bsource_name(double time, double* inputs, int num_inputs)
    std::vector<llvm::Type*> paramTypes = {DoubleTy, DoublePtrTy, llvm::Type::getInt32Ty(context.TheContext)};
    auto funcType = llvm::FunctionType::get(DoubleTy, paramTypes, false);
    auto func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, "bsource_" + Name, context.TheModule);

    // Set up basic block
    llvm::BasicBlock* BB = llvm::BasicBlock::Create(context.TheContext, "entry", func);
    context.Builder.SetInsertPoint(BB);

    // Name arguments and add to NamedValues so body can reference them
    auto args = func->arg_begin();
    args->setName("time");
    llvm::Value* timeArg = &*args++;
    args->setName("inputs");
    llvm::Value* inputsArg = &*args++;
    args->setName("num_inputs");
    llvm::Value* numInputsArg = &*args++;

    // Save existing NamedValues
    auto SavedNamedValues = context.NamedValues;

    // Add function arguments to NamedValues so the body expression can use them
    context.NamedValues["time"] = timeArg;
    context.NamedValues["inputs"] = inputsArg;
    context.NamedValues["num_inputs"] = numInputsArg;

    // Compile the body expression
    TypedValue BodyTV = Body->codegen(context);
    if (!BodyTV.Val) {
        std::cerr << "[CodeGen] B-source body codegen failed: " << Name << std::endl;
        context.NamedValues = SavedNamedValues;
        return TypedValue();
    }

    // Return the result of the body expression
    llvm::Value* ResultVal = BodyTV.Val;

    // Convert to double if needed
    if (BodyTV.Type.Kind == TypeKind::Int) {
        ResultVal = context.Builder.CreateSIToFP(ResultVal, DoubleTy, "int_to_double");
    }

    context.Builder.CreateRet(ResultVal);

    // Restore NamedValues
    context.NamedValues = SavedNamedValues;

    std::cout << "[CodeGen] B-source compiled successfully: bsource_" << Name << std::endl;

    // Register with time-domain engine
    std::cout << "[CodeGen] Registered B-source: " << Name << std::endl;

    return TypedValue(llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), 1.0), TypeKind::Double);
}

// ============ Time Variable Codegen ============

TypedValue TimeExprAST::codegen(CodegenContext& context)
{
    // Call flux_get_time()
    llvm::Function* getTimeF = context.TheModule->getFunction("flux_get_time");
    if (!getTimeF) {
        auto funcType = llvm::FunctionType::get(llvm::Type::getDoubleTy(context.TheContext), {}, false);
        getTimeF =
            llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, "flux_get_time", context.TheModule);
    }

    auto call = context.Builder.CreateCall(getTimeF, {}, "sim_time");
    return TypedValue(call, TypeKind::Double);
}

// ============ Timestep Variable Codegen ============

TypedValue TimestepExprAST::codegen(CodegenContext& context)
{
    llvm::Function* getDtF = context.TheModule->getFunction("flux_get_dt");
    if (!getDtF) {
        auto funcType = llvm::FunctionType::get(llvm::Type::getDoubleTy(context.TheContext), {}, false);
        getDtF = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, "flux_get_dt", context.TheModule);
    }

    auto call = context.Builder.CreateCall(getDtF, {}, "timestep");
    return TypedValue(call, TypeKind::Double);
}

// ============ Temperature Variable Codegen ============

TypedValue TempExprAST::codegen(CodegenContext& context)
{
    llvm::Function* getTempF = context.TheModule->getFunction("flux_get_temp");
    if (!getTempF) {
        auto funcType = llvm::FunctionType::get(llvm::Type::getDoubleTy(context.TheContext), {}, false);
        getTempF =
            llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, "flux_get_temp", context.TheModule);
    }

    auto call = context.Builder.CreateCall(getTempF, {}, "temperature");
    return TypedValue(call, TypeKind::Double);
}

// ============ Inputs Dictionary Codegen ============

TypedValue InputsExprAST::codegen(CodegenContext& context)
{
    // Access input node voltage from the inputs array
    // The inputs parameter is a double* passed as the second argument
    // NodeName would be something like "in[0]" or we use a runtime lookup

    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);

    // Get the inputs pointer from NamedValues.
    // UpdateFuncAST stores function arguments in allocas, while some older
    // time-domain paths keep the raw argument value directly. Accept both so
    // native SmartSignal JIT code always indexes the actual input buffer.
    llvm::Value* InputsValue = context.NamedValues["inputs"];
    if (!InputsValue) {
        std::cerr << "[CodeGen] Error: 'inputs' variable not available in context" << std::endl;
        return TypedValue(llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), 0.0), TypeKind::Double);
    }

    llvm::Value* InputsPtr = InputsValue;
    if (auto* Alloca = llvm::dyn_cast<llvm::AllocaInst>(InputsValue)) {
        llvm::Type* StoredTy = Alloca->getAllocatedType();
        if (StoredTy->isPointerTy()) {
            InputsPtr = context.Builder.CreateLoad(StoredTy, Alloca, "inputs_raw_ptr");
        }
    }

    // If NodeName is a pure number, use it as direct index.
    // Otherwise, try to extract index from node name (e.g. "in0" -> 0)
    int nodeIndex = 0;
    bool isAllDigits = !NodeName.empty() && std::all_of(NodeName.begin(), NodeName.end(), ::isdigit);
    if (isAllDigits) {
        nodeIndex = std::stoi(NodeName);
    } else {
        std::string idxStr;
        for (char c : NodeName) {
            if (std::isdigit(c)) {
                idxStr += c;
            }
        }
        if (!idxStr.empty()) {
            nodeIndex = std::stoi(idxStr);
        }
    }

    // Create GEP to access inputs[nodeIndex]
    llvm::Value* IndexVal = llvm::ConstantInt::get(context.TheContext, llvm::APInt(32, nodeIndex));
    llvm::Value* ElemPtr = context.Builder.CreateGEP(DoubleTy, InputsPtr, IndexVal, "input_ptr");

    // Load the voltage value
    llvm::Value* VoltageVal = context.Builder.CreateLoad(DoubleTy, ElemPtr, "input_voltage");

    std::cout << "[CodeGen] Inputs access: node '" << NodeName << "' at index " << nodeIndex << std::endl;

    return TypedValue(VoltageVal, TypeKind::Double);
}

// ============ Outputs Dictionary Codegen ============

TypedValue OutputsExprAST::codegen(CodegenContext& context)
{
    // Generate code to set output node voltage
    // First, evaluate the value expression
    TypedValue ValueTV = Value->codegen(context);
    if (!ValueTV.Val)
        return TypedValue();

    // Generate call to flux_set_output(name_ptr, value)
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
    llvm::Type* Int64Ty = llvm::Type::getInt64Ty(context.TheContext);

    llvm::Function* setOutputF = context.TheModule->getFunction("flux_set_output");
    if (!setOutputF) {
        auto funcType = llvm::FunctionType::get(llvm::Type::getVoidTy(context.TheContext), {DoubleTy, DoubleTy}, false);
        setOutputF =
            llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, "flux_set_output", context.TheModule);
    }

    // Convert node name string pointer to double (for JIT calling convention)
    uint64_t nameAddr = reinterpret_cast<uint64_t>(NodeName.c_str());
    llvm::Value* NameAsDouble = llvm::ConstantInt::get(context.TheContext, llvm::APInt(64, nameAddr));
    llvm::Value* NameLLVM = context.Builder.CreateBitCast(NameAsDouble, DoubleTy);

    // Convert value to double if needed
    llvm::Value* ValueLLVM = ValueTV.Val;
    if (ValueTV.Type.Kind == TypeKind::Int) {
        ValueLLVM = context.Builder.CreateSIToFP(ValueLLVM, DoubleTy, "int_to_double");
    }

    context.Builder.CreateCall(setOutputF, {NameLLVM, ValueLLVM});

    std::cout << "[CodeGen] Output set: " << NodeName << " = value" << std::endl;

    return TypedValue(ValueLLVM, TypeKind::Double);
}

// ============ Initial Condition Codegen ============

TypedValue InitialCondAST::codegen(CodegenContext& context)
{
    // Evaluate the value expression
    auto valTV = Value->codegen(context);
    if (!valTV.Val)
        return TypedValue();

    // Generate call to flux_set_initial_condition(node_ptr, voltage)
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);

    llvm::Function* setICF = context.TheModule->getFunction("flux_set_initial_condition");
    if (!setICF) {
        auto funcType = llvm::FunctionType::get(llvm::Type::getVoidTy(context.TheContext), {DoubleTy, DoubleTy}, false);
        setICF = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, "flux_set_initial_condition",
                                        context.TheModule);
    }

    // Convert node name string pointer to double (for JIT calling convention)
    uint64_t nameAddr = reinterpret_cast<uint64_t>(Node.c_str());
    llvm::Value* NameAsDouble = llvm::ConstantInt::get(context.TheContext, llvm::APInt(64, nameAddr));
    llvm::Value* NameLLVM = context.Builder.CreateBitCast(NameAsDouble, DoubleTy);

    // Convert value to double if needed
    llvm::Value* ValueLLVM = valTV.Val;
    if (valTV.Type.Kind == TypeKind::Int) {
        ValueLLVM = context.Builder.CreateSIToFP(ValueLLVM, DoubleTy, "int_to_double");
    }

    context.Builder.CreateCall(setICF, {NameLLVM, ValueLLVM});

    std::cout << "[CodeGen] Initial condition set: " << Node << " = value" << std::endl;

    return TypedValue(llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), 1.0), TypeKind::Double);
}

// ============ Transient Analysis Codegen ============

TypedValue TransientAnalysisAST::codegen(CodegenContext& context)
{
    std::cout << "[CodeGen] Transient analysis: timestep=" << Timestep << ", stop=" << StopTime << std::endl;

    // Generate call to set up transient simulation
    llvm::Function* setTimestepF = context.TheModule->getFunction("flux_set_timestep");
    if (!setTimestepF) {
        auto funcType = llvm::FunctionType::get(llvm::Type::getVoidTy(context.TheContext),
                                                {llvm::Type::getDoubleTy(context.TheContext)}, false);
        setTimestepF =
            llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, "flux_set_timestep", context.TheModule);
    }

    llvm::Value* timestepVal = llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), Timestep);
    context.Builder.CreateCall(setTimestepF, {timestepVal});

    std::cout << "[CodeGen] Timestep configured: " << Timestep << std::endl;

    return TypedValue(llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), 1.0), TypeKind::Double);
}

} // namespace Flux
