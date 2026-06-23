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

#include "flux/compiler/ast.h"
#include "flux/compiler/codegen_helpers.h"
#include "flux/compiler/lexer.h"
#include "flux/compiler/symbolic_ast.h"
#include "flux/runtime/units.h"
#include <iostream>
#include <llvm/IR/Verifier.h>
#include <string>

namespace Flux {

TypedValue VoltageExprAST::codegen(CodegenContext& context)
{
    llvm::Function* GetVF = context.TheModule->getFunction("flux_get_voltage");
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
    if (!GetVF) {
        GetVF = llvm::Function::Create(llvm::FunctionType::get(DoubleTy, {DoubleTy}, false),
                                       llvm::Function::ExternalLinkage, "flux_get_voltage", context.TheModule);
    }

    llvm::Value* StrPtr = context.Builder.CreateGlobalString(NodeName, "ptr_double");
    llvm::Value* IntPtr = context.Builder.CreatePtrToInt(StrPtr, llvm::Type::getInt64Ty(context.TheContext));
    llvm::Value* PtrDouble = context.Builder.CreateBitCast(IntPtr, DoubleTy, "ptr_double");

    return TypedValue(context.Builder.CreateCall(GetVF, {PtrDouble}, "v"), TypeKind::Double);
}

TypedValue CurrentExprAST::codegen(CodegenContext& context)
{
    llvm::Function* GetIF = context.TheModule->getFunction("flux_get_current");
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
    if (!GetIF) {
        GetIF = llvm::Function::Create(llvm::FunctionType::get(DoubleTy, {DoubleTy}, false),
                                       llvm::Function::ExternalLinkage, "flux_get_current", context.TheModule);
    }

    llvm::Value* StrPtr = context.Builder.CreateGlobalString(BranchName, "ptr_double");
    llvm::Value* IntPtr = context.Builder.CreatePtrToInt(StrPtr, llvm::Type::getInt64Ty(context.TheContext));
    llvm::Value* PtrDouble = context.Builder.CreateBitCast(IntPtr, DoubleTy, "ptr_double");

    return TypedValue(context.Builder.CreateCall(GetIF, {PtrDouble}, "i"), TypeKind::Double);
}

TypedValue ParameterExprAST::codegen(CodegenContext& context)
{
    llvm::Function* GetPF = context.TheModule->getFunction("flux_get_parameter");
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
    if (!GetPF) {
        GetPF = llvm::Function::Create(llvm::FunctionType::get(DoubleTy, {DoubleTy}, false),
                                       llvm::Function::ExternalLinkage, "flux_get_parameter", context.TheModule);
    }

    llvm::Value* StrPtr = context.Builder.CreateGlobalString(ParamName, "ptr_double");
    llvm::Value* IntPtr = context.Builder.CreatePtrToInt(StrPtr, llvm::Type::getInt64Ty(context.TheContext));
    llvm::Value* PtrDouble = context.Builder.CreateBitCast(IntPtr, DoubleTy, "ptr_double");

    return TypedValue(context.Builder.CreateCall(GetPF, {PtrDouble}, "p"), TypeKind::Double);
}

// ============================================================================
// SPICE Time-Domain Simulation Codegen
// ============================================================================

TypedValue BuiltinVarExprAST::codegen(CodegenContext& context)
{
    // Check local scope first — user-defined parameter/variable shadows the built-in
    auto localIt = context.NamedValues.find(Name);
    if (localIt != context.NamedValues.end()) {
        llvm::Value* V = localIt->second;
        if (auto* Alloca = llvm::dyn_cast<llvm::AllocaInst>(V)) {
            llvm::Type* Ty = Alloca->getAllocatedType();
            return TypedValue(context.Builder.CreateLoad(Ty, Alloca, Name.c_str()), TypeKind::Double);
        }
        FluxType FTy = typeFromLLVM(V->getType());
        return TypedValue(V, FTy);
    }

    // Generate calls for built-in variables: time, dt, temp
    std::string FuncName;
    if (Name == "time")
        FuncName = "flux_get_time";
    else if (Name == "dt")
        FuncName = "flux_get_dt";
    else if (Name == "temp")
        FuncName = "flux_get_temp";
    else {
        std::cerr << "Unknown built-in variable: " << Name << std::endl;
        return TypedValue();
    }

    llvm::Function* GetFunc = context.TheModule->getFunction(FuncName);
    if (!GetFunc) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        GetFunc = llvm::Function::Create(llvm::FunctionType::get(DoubleTy, {}, false), llvm::Function::ExternalLinkage,
                                         FuncName, context.TheModule);
    }
    return TypedValue(context.Builder.CreateCall(GetFunc, {}, Name.c_str()), TypeKind::Double);
}

TypedValue UpdateFuncAST::codegen(CodegenContext& context)
{
    // Generate update(t, inputs) or update(t, inputs, outputs, state) function
    std::vector<llvm::Type*> ArgTypes;
    std::vector<std::string> ArgNames;

    // Time argument (double)
    ArgTypes.push_back(llvm::Type::getDoubleTy(context.TheContext));
    ArgNames.push_back(TimeVar);

    // Inputs argument (pointer to inputs array)
    llvm::Type* DoublePtrTy = llvm::PointerType::get(context.TheContext, 0);
    ArgTypes.push_back(DoublePtrTy);
    ArgNames.push_back(InputsVar);

    // Optional outputs argument (pointer to outputs array)
    if (!OutputsVar.empty()) {
        ArgTypes.push_back(DoublePtrTy);
        ArgNames.push_back(OutputsVar);
    }

    // Optional state argument (i8* for opaque state pointer)
    if (!StateVar.empty()) {
        ArgTypes.push_back(llvm::PointerType::get(context.TheContext, 0));
        ArgNames.push_back(StateVar);
    }

    // Return type is double (output value)
    llvm::Type* RetTy = llvm::Type::getDoubleTy(context.TheContext);

    llvm::FunctionType* FT = llvm::FunctionType::get(RetTy, ArgTypes, false);
    llvm::Function* TheFunction =
        llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "update", context.TheModule);

    // Create entry block and return block
    llvm::BasicBlock* EntryBB = llvm::BasicBlock::Create(context.TheContext, "entry", TheFunction);
    llvm::BasicBlock* ReturnBB = llvm::BasicBlock::Create(context.TheContext, "return");
    context.Builder.SetInsertPoint(EntryBB);

    // Create return value alloca
    llvm::Value* RetValAlloca = context.Builder.CreateAlloca(RetTy, nullptr, "retval");
    context.Builder.CreateStore(llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), 0.0), RetValAlloca);

    // Update context for nested returns
    llvm::BasicBlock* SavedRetBB = context.CurrentReturnBB;
    llvm::Value* SavedRetAlloca = context.CurrentReturnValueAlloca;
    context.CurrentReturnBB = ReturnBB;
    context.CurrentReturnValueAlloca = RetValAlloca;

    // Set argument names and allocate on stack
    unsigned Idx = 0;
    for (auto& Arg : TheFunction->args()) {
        Arg.setName(ArgNames[Idx]);
        llvm::AllocaInst* Alloca = context.Builder.CreateAlloca(Arg.getType(), nullptr, ArgNames[Idx]);
        context.Builder.CreateStore(&Arg, Alloca);
        context.NamedValues[std::string(Arg.getName())] = Alloca;
        Idx++;
    }

    TypedValue RetTV = Body->codegen(context);

    // Jump to return block if not already terminated
    if (!context.Builder.GetInsertBlock()->getTerminator()) {
        llvm::Value* RetVal = RetTV.Val;
        if (RetVal && RetVal->getType() != RetTy) {
            if (RetTy->isFloatingPointTy() && RetVal->getType()->isIntegerTy()) {
                RetVal = context.Builder.CreateSIToFP(RetVal, RetTy, "cast_final");
            }
        }
        if (!RetVal)
            RetVal = llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), 0.0);
        context.Builder.CreateStore(RetVal, RetValAlloca);
        context.Builder.CreateBr(ReturnBB);
    }

    // Generate Return Block
    TheFunction->insert(TheFunction->end(), ReturnBB);
    context.Builder.SetInsertPoint(ReturnBB);
    llvm::Value* FinalRetVal = context.Builder.CreateLoad(RetTy, RetValAlloca, "ret_final");
    context.Builder.CreateRet(FinalRetVal);

    // Restore context
    context.CurrentReturnBB = SavedRetBB;
    context.CurrentReturnValueAlloca = SavedRetAlloca;

    if (llvm::verifyFunction(*TheFunction, &llvm::errs())) {
        std::cerr << "LLVM IR Verification FAILED for function 'update'. Dumping IR:" << std::endl;
        TheFunction->print(llvm::errs());
    }
    return TypedValue(TheFunction, TypeKind::Double);

    TheFunction->eraseFromParent();
    return TypedValue();
}

// ============================================================================
// SPICE Behavioral Sources Codegen
// ============================================================================

TypedValue BSourceExprAST::codegen(CodegenContext& context)
{
    // Generate B-source netlist entry
    // Bxxx n+ n- V=<expression> or I=<expression>
    std::string SourceType = IsCurrent ? "I" : "V";
    std::string NetlistEntry = "B" + Name + " " + PositiveNode + " " + NegativeNode + " " + SourceType + "=";

    // The expression will be evaluated and stored as a string
    // For now, we generate a placeholder and mark it for later netlist generation
    llvm::Function* RegisterBSource = context.TheModule->getFunction("flux_register_bsource");
    if (!RegisterBSource) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        llvm::Type* CharPtrTy = llvm::PointerType::get(context.TheContext, 0);
        RegisterBSource = llvm::Function::Create(
            llvm::FunctionType::get(DoubleTy, {CharPtrTy, CharPtrTy, CharPtrTy, CharPtrTy}, false),
            llvm::Function::ExternalLinkage, "flux_register_bsource", context.TheModule);
    }

    return TypedValue(context.Builder.CreateCall(RegisterBSource, {context.Builder.CreateGlobalString(Name),
                                                                   context.Builder.CreateGlobalString(PositiveNode),
                                                                   context.Builder.CreateGlobalString(NegativeNode),
                                                                   context.Builder.CreateGlobalString(SourceType)}),
                      TypeKind::Double);
}

TypedValue ESourceExprAST::codegen(CodegenContext& context)
{
    // Generate E-source (VCVS) netlist entry
    // Exxx n+ n- nc+ nc- <gain>
    llvm::Function* RegisterESource = context.TheModule->getFunction("flux_register_esource");
    if (!RegisterESource) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        llvm::Type* CharPtrTy = llvm::PointerType::get(context.TheContext, 0);
        RegisterESource = llvm::Function::Create(
            llvm::FunctionType::get(DoubleTy, {CharPtrTy, CharPtrTy, CharPtrTy, CharPtrTy, CharPtrTy, DoubleTy}, false),
            llvm::Function::ExternalLinkage, "flux_register_esource", context.TheModule);
    }

    // Evaluate gain expression
    double GainVal = 1.0;
    if (Gain) {
        if (auto* NumGain = dynamic_cast<NumberExprAST*>(Gain.get())) {
            GainVal = NumGain->getValue();
        }
    }

    return TypedValue(context.Builder.CreateCall(RegisterESource,
                                                 {context.Builder.CreateGlobalString(Name),
                                                  context.Builder.CreateGlobalString(PositiveNode),
                                                  context.Builder.CreateGlobalString(NegativeNode),
                                                  context.Builder.CreateGlobalString(ControlPosNode),
                                                  context.Builder.CreateGlobalString(ControlNegNode),
                                                  llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), GainVal)}),
                      TypeKind::Double);
}

TypedValue FSourceExprAST::codegen(CodegenContext& context)
{
    // Generate F-source (CCCS) netlist entry
    // Fxxx n+ n- <vsource_name> <gain>
    llvm::Function* RegisterFSource = context.TheModule->getFunction("flux_register_fsource");
    if (!RegisterFSource) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        llvm::Type* CharPtrTy = llvm::PointerType::get(context.TheContext, 0);
        RegisterFSource = llvm::Function::Create(
            llvm::FunctionType::get(DoubleTy, {CharPtrTy, CharPtrTy, CharPtrTy, CharPtrTy, DoubleTy}, false),
            llvm::Function::ExternalLinkage, "flux_register_fsource", context.TheModule);
    }

    double GainVal = 1.0;
    if (Gain) {
        if (auto* NumGain = dynamic_cast<NumberExprAST*>(Gain.get())) {
            GainVal = NumGain->getValue();
        }
    }

    return TypedValue(context.Builder.CreateCall(RegisterFSource,
                                                 {context.Builder.CreateGlobalString(Name),
                                                  context.Builder.CreateGlobalString(PositiveNode),
                                                  context.Builder.CreateGlobalString(NegativeNode),
                                                  context.Builder.CreateGlobalString(VoltageSourceName),
                                                  llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), GainVal)}),
                      TypeKind::Double);
}

TypedValue GSourceExprAST::codegen(CodegenContext& context)
{
    // Generate G-source (VCCS) netlist entry
    // Gxxx n+ n- nc+ nc- <transconductance>
    llvm::Function* RegisterGSource = context.TheModule->getFunction("flux_register_gsource");
    if (!RegisterGSource) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        llvm::Type* CharPtrTy = llvm::PointerType::get(context.TheContext, 0);
        RegisterGSource = llvm::Function::Create(
            llvm::FunctionType::get(DoubleTy, {CharPtrTy, CharPtrTy, CharPtrTy, CharPtrTy, CharPtrTy, DoubleTy}, false),
            llvm::Function::ExternalLinkage, "flux_register_gsource", context.TheModule);
    }

    double TranscondVal = 1.0;
    if (Transconductance) {
        if (auto* NumVal = dynamic_cast<NumberExprAST*>(Transconductance.get())) {
            TranscondVal = NumVal->getValue();
        }
    }

    return TypedValue(
        context.Builder.CreateCall(
            RegisterGSource,
            {context.Builder.CreateGlobalString(Name), context.Builder.CreateGlobalString(PositiveNode),
             context.Builder.CreateGlobalString(NegativeNode), context.Builder.CreateGlobalString(ControlPosNode),
             context.Builder.CreateGlobalString(ControlNegNode),
             llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), TranscondVal)}),
        TypeKind::Double);
}

TypedValue HSourceExprAST::codegen(CodegenContext& context)
{
    // Generate H-source (CCVS) netlist entry
    // Hxxx n+ n- <vsource_name> <transresistance>
    llvm::Function* RegisterHSource = context.TheModule->getFunction("flux_register_hsource");
    if (!RegisterHSource) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        llvm::Type* CharPtrTy = llvm::PointerType::get(context.TheContext, 0);
        RegisterHSource = llvm::Function::Create(
            llvm::FunctionType::get(DoubleTy, {CharPtrTy, CharPtrTy, CharPtrTy, CharPtrTy, DoubleTy}, false),
            llvm::Function::ExternalLinkage, "flux_register_hsource", context.TheModule);
    }

    double TransresVal = 1.0;
    if (Transresistance) {
        if (auto* NumVal = dynamic_cast<NumberExprAST*>(Transresistance.get())) {
            TransresVal = NumVal->getValue();
        }
    }

    return TypedValue(
        context.Builder.CreateCall(
            RegisterHSource,
            {context.Builder.CreateGlobalString(Name), context.Builder.CreateGlobalString(PositiveNode),
             context.Builder.CreateGlobalString(NegativeNode), context.Builder.CreateGlobalString(VoltageSourceName),
             llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), TransresVal)}),
        TypeKind::Double);
}

// ============================================================================
// SPICE Analysis Control Codegen
// ============================================================================

void AnalysisExprAST::addParameter(const std::string& Name, std::unique_ptr<ExprAST> Value)
{
    Parameters[Name] = std::move(Value);
}

TypedValue AnalysisExprAST::codegen(CodegenContext& context)
{
    std::string AnalysisName;
    switch (Type) {
    case AnalysisType::TRAN:
        AnalysisName = "tran";
        break;
    case AnalysisType::DC:
        AnalysisName = "dc";
        break;
    case AnalysisType::AC:
        AnalysisName = "ac";
        break;
    case AnalysisType::NOISE:
        AnalysisName = "noise";
        break;
    case AnalysisType::OP:
        AnalysisName = "op";
        break;
    case AnalysisType::TF:
        AnalysisName = "tf";
        break;
    case AnalysisType::SENS:
        AnalysisName = "sens";
        break;
    case AnalysisType::FOURIER:
        AnalysisName = "fourier";
        break;
    }

    llvm::Function* RegisterAnalysis = context.TheModule->getFunction("flux_register_analysis");
    if (!RegisterAnalysis) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        llvm::Type* CharPtrTy = llvm::PointerType::get(context.TheContext, 0);
        RegisterAnalysis =
            llvm::Function::Create(llvm::FunctionType::get(DoubleTy, {CharPtrTy}, false),
                                   llvm::Function::ExternalLinkage, "flux_register_analysis", context.TheModule);
    }

    return TypedValue(context.Builder.CreateCall(RegisterAnalysis, {context.Builder.CreateGlobalString(AnalysisName)}),
                      TypeKind::Double);
}

void MeasureExprAST::addParameter(const std::string& Name, std::unique_ptr<ExprAST> Value)
{
    Parameters[Name] = std::move(Value);
}

TypedValue MeasureExprAST::codegen(CodegenContext& context)
{
    std::string MeasureName;
    switch (Type) {
    case MeasureType::MAX:
        MeasureName = "MAX";
        break;
    case MeasureType::MIN:
        MeasureName = "MIN";
        break;
    case MeasureType::AVG:
        MeasureName = "AVG";
        break;
    case MeasureType::RMS:
        MeasureName = "RMS";
        break;
    case MeasureType::TRIG:
        MeasureName = "TRIG";
        break;
    case MeasureType::TARG:
        MeasureName = "TARG";
        break;
    case MeasureType::WHEN:
        MeasureName = "WHEN";
        break;
    case MeasureType::FIND:
        MeasureName = "FIND";
        break;
    case MeasureType::DERIV:
        MeasureName = "DERIV";
        break;
    case MeasureType::INTEG:
        MeasureName = "INTEG";
        break;
    }

    llvm::Function* RegisterMeasure = context.TheModule->getFunction("flux_register_measure");
    if (!RegisterMeasure) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        llvm::Type* CharPtrTy = llvm::PointerType::get(context.TheContext, 0);
        RegisterMeasure =
            llvm::Function::Create(llvm::FunctionType::get(DoubleTy, {CharPtrTy, CharPtrTy}, false),
                                   llvm::Function::ExternalLinkage, "flux_register_measure", context.TheModule);
    }

    return TypedValue(context.Builder.CreateCall(RegisterMeasure, {context.Builder.CreateGlobalString(Name),
                                                                   context.Builder.CreateGlobalString(MeasureName)}),
                      TypeKind::Double);
}

TypedValue ProbeExprAST::codegen(CodegenContext& context)
{
    llvm::Function* RegisterProbe = context.TheModule->getFunction("flux_register_probe");
    if (!RegisterProbe) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        llvm::Type* CharPtrTy = llvm::PointerType::get(context.TheContext, 0);
        RegisterProbe =
            llvm::Function::Create(llvm::FunctionType::get(DoubleTy, {CharPtrTy, CharPtrTy}, false),
                                   llvm::Function::ExternalLinkage, "flux_register_probe", context.TheModule);
    }

    return TypedValue(context.Builder.CreateCall(RegisterProbe, {context.Builder.CreateGlobalString(VariableName),
                                                                 context.Builder.CreateGlobalString(
                                                                     OutputName.empty() ? VariableName : OutputName)}),
                      TypeKind::Double);
}

TypedValue SaveExprAST::codegen(CodegenContext& context)
{
    llvm::Function* RegisterSave = context.TheModule->getFunction("flux_register_save");
    if (!RegisterSave) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        llvm::Type* CharPtrTy = llvm::PointerType::get(context.TheContext, 0);
        RegisterSave = llvm::Function::Create(llvm::FunctionType::get(DoubleTy, {CharPtrTy}, false),
                                              llvm::Function::ExternalLinkage, "flux_register_save", context.TheModule);
    }

    return TypedValue(context.Builder.CreateCall(RegisterSave, {context.Builder.CreateGlobalString(VariableName)}),
                      TypeKind::Double);
}

// ============================================================================
// SPICE Subcircuit and Model Codegen
// ============================================================================

void SubcktExprAST::addParameter(const std::string& Name, std::unique_ptr<ExprAST> Value)
{
    Parameters.push_back({Name, std::move(Value)});
}

void SubcktExprAST::addStatement(std::unique_ptr<ExprAST> Stmt)
{
    Body.push_back(std::move(Stmt));
}

void SubcktExprAST::addFunction(std::unique_ptr<FunctionAST> Func)
{
    Functions.push_back(std::move(Func));
}

TypedValue SubcktExprAST::codegen(CodegenContext& context)
{
    // Build pin list string
    std::string PinsStr;
    for (size_t i = 0; i < Pins.size(); ++i) {
        if (i > 0)
            PinsStr += " ";
        PinsStr += Pins[i];
    }

    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
    llvm::Type* Int64Ty = llvm::Type::getInt64Ty(context.TheContext);

    llvm::Function* RegisterSubckt = context.TheModule->getFunction("flux_register_subckt");
    if (!RegisterSubckt) {
        // Updated signature: double flux_register_subckt(double name_ptr_double, double pins_ptr_double)
        RegisterSubckt =
            llvm::Function::Create(llvm::FunctionType::get(DoubleTy, {DoubleTy, DoubleTy}, false),
                                   llvm::Function::ExternalLinkage, "flux_register_subckt", context.TheModule);
    }

    // Use bitcast pattern for Name
    llvm::Value* NameStrPtr = context.Builder.CreateGlobalString(Name, "name_str");
    llvm::Value* NameIntPtr = context.Builder.CreatePtrToInt(NameStrPtr, Int64Ty);
    llvm::Value* NameDouble = context.Builder.CreateBitCast(NameIntPtr, DoubleTy, "name_double");

    // Use bitcast pattern for PinsStr
    llvm::Value* PinsStrPtr = context.Builder.CreateGlobalString(PinsStr, "pins_str");
    llvm::Value* PinsIntPtr = context.Builder.CreatePtrToInt(PinsStrPtr, Int64Ty);
    llvm::Value* PinsDouble = context.Builder.CreateBitCast(PinsIntPtr, DoubleTy, "pins_double");

    // Register the subcircuit
    TypedValue Result =
        TypedValue(context.Builder.CreateCall(RegisterSubckt, {NameDouble, PinsDouble}), TypeKind::Double);

    // Codegen body statements (netlist elements, local parameters, etc.)
    for (auto& Stmt : Body) {
        Stmt->codegen(context);
    }

    // Codegen local functions defined inside the subcircuit
    for (auto& Func : Functions) {
        Func->codegen(context);
    }

    return Result;
}

void ModelExprAST::addParameter(const std::string& Name, std::unique_ptr<ExprAST> Value)
{
    Parameters[Name] = std::move(Value);
}

TypedValue ModelExprAST::codegen(CodegenContext& context)
{
    llvm::Function* RegisterModel = context.TheModule->getFunction("flux_register_model");
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
    llvm::Type* Int64Ty = llvm::Type::getInt64Ty(context.TheContext);

    if (!RegisterModel) {
        RegisterModel =
            llvm::Function::Create(llvm::FunctionType::get(DoubleTy, {DoubleTy, DoubleTy}, false),
                                   llvm::Function::ExternalLinkage, "flux_register_model", context.TheModule);
    }

    // Use bitcast pattern for Name
    llvm::Value* NameStrPtr = context.Builder.CreateGlobalString(Name, "name_str");
    llvm::Value* NameIntPtr = context.Builder.CreatePtrToInt(NameStrPtr, Int64Ty);
    llvm::Value* NameDouble = context.Builder.CreateBitCast(NameIntPtr, DoubleTy, "name_double");

    // Use bitcast pattern for ModelType
    llvm::Value* TypeStrPtr = context.Builder.CreateGlobalString(ModelType, "type_str");
    llvm::Value* TypeIntPtr = context.Builder.CreatePtrToInt(TypeStrPtr, Int64Ty);
    llvm::Value* TypeDouble = context.Builder.CreateBitCast(TypeIntPtr, DoubleTy, "type_double");

    return TypedValue(context.Builder.CreateCall(RegisterModel, {NameDouble, TypeDouble}), TypeKind::Double);
}

TypedValue ParamExprAST::codegen(CodegenContext& context)
{
    // Parameters are handled at the simulation setup level
    // This just marks the parameter as declared
    llvm::Function* RegisterParam = context.TheModule->getFunction("flux_register_param");
    if (!RegisterParam) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        llvm::Type* CharPtrTy = llvm::PointerType::get(context.TheContext, 0);
        RegisterParam =
            llvm::Function::Create(llvm::FunctionType::get(DoubleTy, {CharPtrTy, DoubleTy}, false),
                                   llvm::Function::ExternalLinkage, "flux_register_param", context.TheModule);
    }

    double Val = 0.0;
    if (auto* NumVal = dynamic_cast<NumberExprAST*>(Value.get())) {
        Val = NumVal->getValue();
    }

    return TypedValue(
        context.Builder.CreateCall(RegisterParam, {context.Builder.CreateGlobalString(Name),
                                                   llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), Val)}),
        TypeKind::Double);
}

TypedValue ICExprAST::codegen(CodegenContext& context)
{
    llvm::Function* RegisterIC = context.TheModule->getFunction("flux_register_ic");
    if (!RegisterIC) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        llvm::Type* CharPtrTy = llvm::PointerType::get(context.TheContext, 0);
        RegisterIC = llvm::Function::Create(llvm::FunctionType::get(DoubleTy, {CharPtrTy, DoubleTy}, false),
                                            llvm::Function::ExternalLinkage, "flux_register_ic", context.TheModule);
    }

    double Val = 0.0;
    if (auto* NumVal = dynamic_cast<NumberExprAST*>(Value.get())) {
        Val = NumVal->getValue();
    }

    return TypedValue(
        context.Builder.CreateCall(RegisterIC, {context.Builder.CreateGlobalString(NodeName),
                                                llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), Val)}),
        TypeKind::Double);
}

TypedValue WorstCaseExprAST::codegen(CodegenContext& context)
{
    emitLocation(this, context);
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Module* TheModule = context.TheModule;

    // Build comma-separated strings for components
    std::string namesStr, nominalsStr, tolerancesStr;
    for (auto it = ComponentNominal.begin(); it != ComponentNominal.end(); ++it) {
        if (it != ComponentNominal.begin()) {
            namesStr += ",";
            nominalsStr += ",";
            tolerancesStr += ",";
        }
        namesStr += it->first;
        nominalsStr += std::to_string(it->second);
        auto tolIt = ComponentTolerance.find(it->first);
        tolerancesStr += (tolIt != ComponentTolerance.end()) ? std::to_string(tolIt->second) : "0.0";
    }

    llvm::Function* RegisterWC = TheModule->getFunction("flux_register_worst_case");
    if (!RegisterWC) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(Ctx);
        RegisterWC = llvm::Function::Create(
            llvm::FunctionType::get(DoubleTy, {DoubleTy, DoubleTy, DoubleTy, DoubleTy}, false),
            llvm::Function::ExternalLinkage, "flux_register_worst_case", TheModule);
    }

    // Convert string pointers to doubles (JIT ABI convention)
    auto stringToDouble = [&](const std::string& s) -> llvm::Value* {
        llvm::Value* Ptr = context.Builder.CreateGlobalString(s);
        llvm::Type* Int64Ty = llvm::Type::getInt64Ty(Ctx);
        llvm::Value* IntVal = context.Builder.CreatePtrToInt(Ptr, Int64Ty, "str_int");
        return context.Builder.CreateBitCast(IntVal, llvm::Type::getDoubleTy(Ctx), "str_dbl");
    };

    llvm::Value* Res = context.Builder.CreateCall(
        RegisterWC, {stringToDouble(OutputName), stringToDouble(namesStr), stringToDouble(nominalsStr),
                     stringToDouble(tolerancesStr)},
        "wc_res");
    return TypedValue(Res, TypeKind::Double);
}

TypedValue StabilityExprAST::codegen(CodegenContext& context)
{
    emitLocation(this, context);
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Module* TheModule = context.TheModule;

    llvm::Function* Fn = TheModule->getFunction("flux_stability_run");
    if (!Fn) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(Ctx);
        Fn = llvm::Function::Create(llvm::FunctionType::get(DoubleTy, {DoubleTy}, false),
                                    llvm::Function::ExternalLinkage,
                                    "flux_stability_run", TheModule);
    }

    // Convert string pointer to double for JIT ABI
    llvm::Value* Ptr = context.Builder.CreateGlobalString(OutputName, "stab_output");
    llvm::Value* IntVal = context.Builder.CreatePtrToInt(Ptr, llvm::Type::getInt64Ty(Ctx), "str_int");
    llvm::Value* StrDbl = context.Builder.CreateBitCast(IntVal, llvm::Type::getDoubleTy(Ctx), "str_dbl");

    llvm::Value* Res = context.Builder.CreateCall(Fn, {StrDbl}, "stab_res");
    return TypedValue(Res, TypeKind::Double);
}

TypedValue SensitivityExprAST::codegen(CodegenContext& context)
{
    emitLocation(this, context);
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Module* TheModule = context.TheModule;

    llvm::Function* Fn = TheModule->getFunction("flux_sensitivity_run");
    if (!Fn) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(Ctx);
        Fn = llvm::Function::Create(llvm::FunctionType::get(DoubleTy, {DoubleTy}, false),
                                    llvm::Function::ExternalLinkage,
                                    "flux_sensitivity_run", TheModule);
    }

    llvm::Value* Ptr = context.Builder.CreateGlobalString(OutputName, "sens_output");
    llvm::Value* IntVal = context.Builder.CreatePtrToInt(Ptr, llvm::Type::getInt64Ty(Ctx), "str_int");
    llvm::Value* StrDbl = context.Builder.CreateBitCast(IntVal, llvm::Type::getDoubleTy(Ctx), "str_dbl");

    llvm::Value* Res = context.Builder.CreateCall(Fn, {StrDbl}, "sens_res");
    return TypedValue(Res, TypeKind::Double);
}

TypedValue OptimizationExprAST::codegen(CodegenContext& context)
{
    emitLocation(this, context);
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Module* TheModule = context.TheModule;

    // Build comma-separated strings
    std::string namesStr, initsStr, minsStr, maxsStr;
    for (size_t i = 0; i < VarNames.size(); i++) {
        if (i > 0) {
            namesStr += ",";
            initsStr += ",";
            minsStr += ",";
            maxsStr += ",";
        }
        namesStr += VarNames[i];
        initsStr += (i < InitVals.size()) ? std::to_string(InitVals[i]) : "0.0";
        minsStr += (i < MinVals.size()) ? std::to_string(MinVals[i]) : "0.0";
        maxsStr += (i < MaxVals.size()) ? std::to_string(MaxVals[i]) : "0.0";
    }

    llvm::Function* Fn = TheModule->getFunction("flux_optimize_analyze");
    if (!Fn) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(Ctx);
        Fn = llvm::Function::Create(
            llvm::FunctionType::get(DoubleTy, {DoubleTy, DoubleTy, DoubleTy, DoubleTy, DoubleTy}, false),
            llvm::Function::ExternalLinkage, "flux_optimize_analyze", TheModule);
    }

    auto stringToDouble = [&](const std::string& s) -> llvm::Value* {
        llvm::Value* Ptr = context.Builder.CreateGlobalString(s);
        llvm::Type* Int64Ty = llvm::Type::getInt64Ty(Ctx);
        llvm::Value* IntVal = context.Builder.CreatePtrToInt(Ptr, Int64Ty, "str_int");
        return context.Builder.CreateBitCast(IntVal, llvm::Type::getDoubleTy(Ctx), "str_dbl");
    };

    llvm::Value* Res = context.Builder.CreateCall(
        Fn, {stringToDouble(OutputName), stringToDouble(namesStr), stringToDouble(initsStr),
             stringToDouble(minsStr), stringToDouble(maxsStr)},
        "opt_res");
    return TypedValue(Res, TypeKind::Double);
}

TypedValue BodeExprAST::codegen(CodegenContext& context)
{
    emitLocation(this, context);
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Module* TheModule = context.TheModule;

    llvm::Function* Fn = TheModule->getFunction("flux_bode_analyze");
    if (!Fn) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(Ctx);
        llvm::Type* Int32Ty = llvm::Type::getInt32Ty(Ctx);
        Fn = llvm::Function::Create(
            llvm::FunctionType::get(DoubleTy, {DoubleTy, DoubleTy, DoubleTy, Int32Ty}, false),
            llvm::Function::ExternalLinkage, "flux_bode_analyze", TheModule);
    }

    auto stringToDouble = [&](const std::string& s) -> llvm::Value* {
        llvm::Value* Ptr = context.Builder.CreateGlobalString(s);
        llvm::Type* Int64Ty = llvm::Type::getInt64Ty(Ctx);
        llvm::Value* IntVal = context.Builder.CreatePtrToInt(Ptr, Int64Ty, "str_int");
        return context.Builder.CreateBitCast(IntVal, llvm::Type::getDoubleTy(Ctx), "str_dbl");
    };

    llvm::Value* Res = context.Builder.CreateCall(
        Fn, {stringToDouble(OutputName),
             llvm::ConstantFP::get(llvm::Type::getDoubleTy(Ctx), FreqStart),
             llvm::ConstantFP::get(llvm::Type::getDoubleTy(Ctx), FreqEnd),
             llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), PointsPerDecade)},
        "bode_res");
    return TypedValue(Res, TypeKind::Double);
}

TypedValue PlotExprAST::codegen(CodegenContext& context)
{
    emitLocation(this, context);
    llvm::LLVMContext& Ctx = context.TheContext;

    // plot(x, y) or plot(y) — first arg is x (optional), last is y
    auto xTV = Args[0]->codegen(context);
    if (!xTV.Val) {
        std::cerr << "[FLUX ERROR] Plot argument expression failed to codegen" << std::endl;
        return TypedValue();
    }
    auto yTV = Args.size() > 1 ? Args[1]->codegen(context) : TypedValue();

    // Verify types
    auto& primaryTV = Args.size() > 1 ? yTV : xTV;
    if (primaryTV.Type.Kind != TypeKind::Matrix) {
        std::cerr << "[FLUX ERROR] Plot requires matrix argument" << std::endl;
        context.TheModule = nullptr;
        return TypedValue();
    }

    // If only one arg, treat as y (x is implicit)
    // If two args, first is x, second is y
    llvm::Value* yPtr;
    llvm::Value* yRows;
    llvm::Value* yCols;
    llvm::Value* xPtr;

    if (Args.size() > 1) {
        // xTV = first arg (x), yTV = second arg (y)
        if (xTV.Type.Kind != TypeKind::Matrix || yTV.Type.Kind != TypeKind::Matrix) {
            std::cerr << "[FLUX ERROR] Plot requires matrix arguments for both x and y" << std::endl;
            context.TheModule = nullptr;
            return TypedValue();
        }
        yPtr = context.Builder.CreateExtractValue(yTV.Val, 0, "y_ptr");
        yRows = context.Builder.CreateExtractValue(yTV.Val, 1, "y_rows");
        yCols = context.Builder.CreateExtractValue(yTV.Val, 2, "y_cols");
        xPtr = context.Builder.CreateExtractValue(xTV.Val, 0, "x_ptr");
    } else {
        yPtr = context.Builder.CreateExtractValue(xTV.Val, 0, "y_ptr");
        yRows = context.Builder.CreateExtractValue(xTV.Val, 1, "y_rows");
        yCols = context.Builder.CreateExtractValue(xTV.Val, 2, "y_cols");
        xPtr = llvm::ConstantPointerNull::get(llvm::PointerType::get(Ctx, 0));
    }

    auto stringToDouble = [&](const std::string& s) -> llvm::Value* {
        llvm::Value* Ptr = context.Builder.CreateGlobalString(s);
        llvm::Type* Int64Ty = llvm::Type::getInt64Ty(Ctx);
        llvm::Value* IntVal = context.Builder.CreatePtrToInt(Ptr, Int64Ty, "str_int");
        return context.Builder.CreateBitCast(IntVal, llvm::Type::getDoubleTy(Ctx), "str_dbl");
    };

    llvm::Function* Fn = context.TheModule->getFunction("flux_plot_data");
    if (!Fn) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(Ctx);
        llvm::Type* Int32Ty = llvm::Type::getInt32Ty(Ctx);
        llvm::Type* PtrTy = llvm::PointerType::get(Ctx, 0);
        Fn = llvm::Function::Create(
            llvm::FunctionType::get(DoubleTy, {PtrTy, Int32Ty, Int32Ty, PtrTy, Int32Ty, DoubleTy}, false),
            llvm::Function::ExternalLinkage, "flux_plot_data", context.TheModule);
    }

    llvm::Value* yRowsI32 = context.Builder.CreateTrunc(yRows, llvm::Type::getInt32Ty(Ctx), "y_rows_i32");
    llvm::Value* yColsI32 = context.Builder.CreateTrunc(yCols, llvm::Type::getInt32Ty(Ctx), "y_cols_i32");

    (void)context.Builder.CreateCall(
        Fn, {yPtr, yRowsI32, yColsI32, xPtr,
             xTV.Val ? llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), 1)
                     : llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), 0),
             stringToDouble("")},
        "plot_res");
    return TypedValue(llvm::ConstantFP::get(llvm::Type::getDoubleTy(Ctx), 1.0), TypeKind::Double);
}

TypedValue FFTExprAST::codegen(CodegenContext& context)
{
    llvm::Function* Fn = context.TheModule->getFunction("flux_fft_analyze");
    if (!Fn) {
        llvm::Type* IntTy = llvm::Type::getInt32Ty(context.TheContext);
        llvm::Type* CharPtrTy = llvm::PointerType::get(context.TheContext, 0);
        Fn = llvm::Function::Create(llvm::FunctionType::get(IntTy, {CharPtrTy}, false), llvm::Function::ExternalLinkage,
                                    "flux_fft_analyze", context.TheModule);
    }
    return TypedValue(context.Builder.CreateCall(Fn, {context.Builder.CreateGlobalString(SignalName)}), TypeKind::Int);
}

// ============================================================================
// Statement-based Control Flow Codegen (void-returning, no PHI nodes)
// ============================================================================

TypedValue IfStmtAST::codegen(CodegenContext& context)
{
    llvm::Function* TheFunction = context.Builder.GetInsertBlock()->getParent();
    llvm::LLVMContext& Ctx = context.TheContext;

    // Generate condition
    TypedValue CondTV = Cond->codegen(context);
    if (!CondTV.Val) {
        std::cerr << "[FLUX ERROR] If-statement condition failed to codegen" << std::endl;
        return TypedValue();
    }

    // If the condition is already a boolean (i1), use it directly
    llvm::Value* IsTrue;
    if (CondTV.Val->getType()->isIntegerTy(1)) {
        IsTrue = CondTV.Val;
    } else {
        // Otherwise compare to non-zero
        IsTrue = context.Builder.CreateFCmpONE(CondTV.Val, llvm::ConstantFP::get(llvm::Type::getDoubleTy(Ctx), 0.0), "ifcond");
    }

    // Create basic blocks
    llvm::BasicBlock* ThenBB = llvm::BasicBlock::Create(Ctx, "then", TheFunction);
    llvm::BasicBlock* ElseBB = llvm::BasicBlock::Create(Ctx, "else", TheFunction);
    llvm::BasicBlock* MergeBB = llvm::BasicBlock::Create(Ctx, "ifcont");

    context.Builder.CreateCondBr(IsTrue, ThenBB, ElseBB);

    // Generate then block
    context.Builder.SetInsertPoint(ThenBB);
    TypedValue ThenTV(nullptr, TypeKind::Void);
    for (auto& Stmt : ThenBody) {
        if (context.Builder.GetInsertBlock()->getTerminator())
            break;
        ThenTV = Stmt->codegen(context);
    }
    bool thenTerminated = context.Builder.GetInsertBlock()->getTerminator() != nullptr;
    if (!thenTerminated) {
        context.Builder.CreateBr(MergeBB);
    }
    ThenBB = context.Builder.GetInsertBlock();

    // Generate else block
    context.Builder.SetInsertPoint(ElseBB);
    TypedValue ElseTV(nullptr, TypeKind::Void);
    for (auto& Stmt : ElseBody) {
        if (context.Builder.GetInsertBlock()->getTerminator())
            break;
        ElseTV = Stmt->codegen(context);
    }
    bool elseTerminated = context.Builder.GetInsertBlock()->getTerminator() != nullptr;
    if (!elseTerminated) {
        context.Builder.CreateBr(MergeBB);
    }
    ElseBB = context.Builder.GetInsertBlock();

    // Determine if we need to continue after the if
    if (thenTerminated && elseTerminated) {
        delete MergeBB;
        return ThenTV.Val ? ThenTV : TypedValue(llvm::ConstantFP::get(llvm::Type::getDoubleTy(Ctx), 0.0), TypeKind::Double);
    }

    // Continue at merge
    TheFunction->insert(TheFunction->end(), MergeBB);
    context.Builder.SetInsertPoint(MergeBB);

    // PHI node to merge values from reachable paths
    if (ThenTV.Type.Kind == TypeKind::Void && ElseTV.Type.Kind == TypeKind::Void) {
        return ThenTV;
    }

    TypedValue ResultTV = ThenTV.Val ? ThenTV : ElseTV;
    llvm::Type* PhiTy = ResultTV.Val ? ResultTV.Val->getType() : llvm::Type::getDoubleTy(Ctx);
    llvm::PHINode* PN = context.Builder.CreatePHI(PhiTy, 2, "ifphi");

    if (!thenTerminated) {
        llvm::Value* TV = ThenTV.Val;
        if (!TV)
            TV = llvm::Constant::getNullValue(PhiTy);
        else if (TV->getType() != PhiTy)
            TV = context.Builder.CreateSIToFP(TV, PhiTy, "cast_then");
        PN->addIncoming(TV, ThenBB);
    }
    if (!elseTerminated) {
        llvm::Value* EV = ElseTV.Val;
        if (!EV)
            EV = llvm::Constant::getNullValue(PhiTy);
        else if (EV->getType() != PhiTy)
            EV = context.Builder.CreateSIToFP(EV, PhiTy, "cast_else");
        PN->addIncoming(EV, ElseBB);
    }

    return TypedValue(PN, ResultTV.Type);
}

TypedValue ForStmtAST::codegen(CodegenContext& context)
{
    llvm::Function* TheFunction = context.Builder.GetInsertBlock()->getParent();
    llvm::LLVMContext& Ctx = context.TheContext;

    // Save loop context
    llvm::BasicBlock* SavedLoopEnd = context.CurrentLoopEnd;
    llvm::BasicBlock* SavedLoopCont = context.CurrentLoopCont;

    // Generate init  handle "var = expr" pattern for auto-declaring loop vars
    if (Init) {
        if (auto* AssignInit = dynamic_cast<AssignExprAST*>(Init.get())) {
            // Special case: for (i = 0.0; ...)  auto-declare the loop variable
            if (auto* VarRef = dynamic_cast<VariableExprAST*>(const_cast<ExprAST*>(AssignInit->getLHS()))) {
                std::string VarName = VarRef->getName();
                // Check if already declared
                if (context.NamedValues.find(VarName) == context.NamedValues.end()) {
                    llvm::AllocaInst* Alloca = context.Builder.CreateAlloca(llvm::Type::getDoubleTy(context.TheContext),
                                                                            nullptr, VarName.c_str());
                    context.NamedValues[VarName] = Alloca;
                }
                // Generate the assignment normally (will find the alloca we just created)
                Init->codegen(context);
            } else {
                Init->codegen(context);
            }
        } else if (auto* VarRef = dynamic_cast<VariableExprAST*>(Init.get())) {
            // Bare variable name: for (i; ...)  just ensure it exists
            std::string VarName = VarRef->getName();
            if (context.NamedValues.find(VarName) == context.NamedValues.end()) {
                llvm::AllocaInst* Alloca =
                    context.Builder.CreateAlloca(llvm::Type::getDoubleTy(context.TheContext), nullptr, VarName.c_str());
                context.NamedValues[VarName] = Alloca;
            }
        } else {
            Init->codegen(context);
        }
    }

    // Create blocks
    llvm::BasicBlock* CondBB = llvm::BasicBlock::Create(Ctx, "for.cond", TheFunction);
    llvm::BasicBlock* BodyBB = llvm::BasicBlock::Create(Ctx, "for.body", TheFunction);
    llvm::BasicBlock* StepBB = llvm::BasicBlock::Create(Ctx, "for.step", TheFunction);
    llvm::BasicBlock* AfterBB = llvm::BasicBlock::Create(Ctx, "for.end", TheFunction);

    context.Builder.CreateBr(CondBB);

    // Condition
    context.Builder.SetInsertPoint(CondBB);
    TypedValue CondTV = Cond->codegen(context);
    if (!CondTV.Val) {
        std::cerr << "[FLUX ERROR] For-statement condition failed to codegen" << std::endl;
        return TypedValue();
    }

    // If the condition is already a boolean (i1), use it directly
    llvm::Value* IsTrue;
    if (CondTV.Val->getType()->isIntegerTy(1)) {
        IsTrue = CondTV.Val;
    } else {
        // Otherwise compare to non-zero
        IsTrue = context.Builder.CreateFCmpONE(CondTV.Val, llvm::ConstantFP::get(llvm::Type::getDoubleTy(Ctx), 0.0), "forcond");
    }
    context.Builder.CreateCondBr(IsTrue, BodyBB, AfterBB);

    // Body
    context.CurrentLoopEnd = AfterBB;
    context.CurrentLoopCont = StepBB;
    context.Builder.SetInsertPoint(BodyBB);
    for (auto& Stmt : Body) {
        Stmt->codegen(context);
    }
    context.Builder.CreateBr(StepBB);

    // Step
    context.Builder.SetInsertPoint(StepBB);
    if (Step) {
        Step->codegen(context);
    }
    context.Builder.CreateBr(CondBB);

    // After
    context.Builder.SetInsertPoint(AfterBB);

    // Restore loop context
    context.CurrentLoopEnd = SavedLoopEnd;
    context.CurrentLoopCont = SavedLoopCont;

    return TypedValue(llvm::ConstantFP::get(llvm::Type::getDoubleTy(Ctx), 0.0), TypeKind::Double);
}

TypedValue WhileStmtAST::codegen(CodegenContext& context)
{
    llvm::Function* TheFunction = context.Builder.GetInsertBlock()->getParent();
    llvm::LLVMContext& Ctx = context.TheContext;

    // Save loop context
    llvm::BasicBlock* SavedLoopEnd = context.CurrentLoopEnd;
    llvm::BasicBlock* SavedLoopCont = context.CurrentLoopCont;

    // Create blocks
    llvm::BasicBlock* CondBB = llvm::BasicBlock::Create(Ctx, "while.cond", TheFunction);
    llvm::BasicBlock* BodyBB = llvm::BasicBlock::Create(Ctx, "while.body", TheFunction);
    llvm::BasicBlock* AfterBB = llvm::BasicBlock::Create(Ctx, "while.end", TheFunction);

    context.Builder.CreateBr(CondBB);

    // Condition
    context.Builder.SetInsertPoint(CondBB);
    TypedValue CondTV = Cond->codegen(context);
    if (!CondTV.Val) {
        std::cerr << "[FLUX ERROR] While-statement condition failed to codegen" << std::endl;
        return TypedValue();
    }

    // If the condition is already a boolean (i1), use it directly
    llvm::Value* IsTrue;
    if (CondTV.Val->getType()->isIntegerTy(1)) {
        IsTrue = CondTV.Val;
    } else {
        // Otherwise compare to non-zero
        IsTrue = context.Builder.CreateFCmpONE(CondTV.Val, llvm::ConstantFP::get(llvm::Type::getDoubleTy(Ctx), 0.0), "whilecond");
    }
    context.Builder.CreateCondBr(IsTrue, BodyBB, AfterBB);

    // Body
    context.CurrentLoopEnd = AfterBB;
    context.CurrentLoopCont = BodyBB; // continue jumps to condition check
    context.Builder.SetInsertPoint(BodyBB);
    for (auto& Stmt : Body) {
        Stmt->codegen(context);
    }
    context.Builder.CreateBr(CondBB);

    // After
    context.Builder.SetInsertPoint(AfterBB);

    // Restore loop context
    context.CurrentLoopEnd = SavedLoopEnd;
    context.CurrentLoopCont = SavedLoopCont;

    return TypedValue(llvm::ConstantFP::get(llvm::Type::getDoubleTy(Ctx), 0.0), TypeKind::Double);
}

TypedValue TrainExprAST::codegen(CodegenContext& context)
{
    emitLocation(this, context);
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Module* TheModule = context.TheModule;

    TypedValue ModelTV = Model->codegen(context);
    TypedValue InTV = Inputs->codegen(context);
    TypedValue OutTV = Outputs->codegen(context);

    if (!ModelTV.Val || !InTV.Val || !OutTV.Val) {
        std::cerr << "[FLUX ERROR] Train expression failed to codegen model/inputs/outputs" << std::endl;
        return TypedValue();
    }

    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(Ctx);
    llvm::Type* Int32Ty = llvm::Type::getInt32Ty(Ctx);
    llvm::Type* VoidPtrTy = llvm::PointerType::get(Ctx, 0);

    // Call flux_nn_train(void* nn, const double* inputs, const double* outputs, int numSamples, int inDim, int outDim,
    // int epochs)
    llvm::Function* TrainF = TheModule->getFunction("flux_nn_train");
    if (!TrainF) {
        llvm::Type* Params[] = {VoidPtrTy, VoidPtrTy, VoidPtrTy, Int32Ty, Int32Ty, Int32Ty, Int32Ty};
        TrainF = llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getVoidTy(Ctx), Params, false),
                                        llvm::Function::ExternalLinkage, "flux_nn_train", TheModule);
    }

    // Extract info from Matrix/Vector inputs/outputs
    llvm::Value* InPtr = context.Builder.CreateExtractValue(InTV.Val, 0, "in_ptr");
    llvm::Value* InRows = context.Builder.CreateExtractValue(InTV.Val, 1, "in_rows");
    llvm::Value* InCols = context.Builder.CreateExtractValue(InTV.Val, 2, "in_cols");

    llvm::Value* OutPtr = context.Builder.CreateExtractValue(OutTV.Val, 0, "out_ptr");
    llvm::Value* OutCols = context.Builder.CreateExtractValue(OutTV.Val, 2, "out_cols");

    llvm::Type* Int64Ty = llvm::Type::getInt64Ty(Ctx);
    llvm::Value* ModelInt = context.Builder.CreateBitCast(ModelTV.Val, Int64Ty, "nn_int");
    llvm::Value* ModelPtr = context.Builder.CreateIntToPtr(ModelInt, VoidPtrTy, "nn_ptr");

    context.Builder.CreateCall(
        TrainF, {ModelPtr, InPtr, OutPtr, InRows, InCols, OutCols, llvm::ConstantInt::get(Int32Ty, Epochs)});

    return ModelTV;
}

TypedValue PredictExprAST::codegen(CodegenContext& context)
{
    emitLocation(this, context);
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Module* TheModule = context.TheModule;

    TypedValue ModelTV = Model->codegen(context);
    TypedValue InTV = Input->codegen(context);

    if (!ModelTV.Val || !InTV.Val) {
        std::cerr << "[FLUX ERROR] Predict expression failed to codegen model/input" << std::endl;
        return TypedValue();
    }

    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(Ctx);
    llvm::Type* Int32Ty = llvm::Type::getInt32Ty(Ctx);
    llvm::Type* VoidPtrTy = llvm::PointerType::get(Ctx, 0);

    // Call flux_nn_predict(void* nn, const double* input, double* output, int inDim, int outDim)
    // We'll return a new Vector

    // For now, let's assume output is a vector of size 1 for simplicity
    llvm::Function* PredictF = TheModule->getFunction("flux_nn_predict");
    if (!PredictF) {
        llvm::Type* Params[] = {VoidPtrTy, VoidPtrTy, VoidPtrTy, Int32Ty, Int32Ty};
        PredictF = llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getVoidTy(Ctx), Params, false),
                                          llvm::Function::ExternalLinkage, "flux_nn_predict", TheModule);
    }

    // Allocate output buffer on stack
    llvm::Value* OutBuf = context.Builder.CreateAlloca(DoubleTy, llvm::ConstantInt::get(Int32Ty, 1), "pred_out");

    llvm::Value* InPtr = context.Builder.CreateExtractValue(InTV.Val, 0, "in_ptr");
    llvm::Value* InSize = context.Builder.CreateExtractValue(InTV.Val, 1, "in_size");

    llvm::Type* Int64Ty = llvm::Type::getInt64Ty(Ctx);
    llvm::Value* ModelInt = context.Builder.CreateBitCast(ModelTV.Val, Int64Ty, "nn_int");
    llvm::Value* ModelPtr = context.Builder.CreateIntToPtr(ModelInt, VoidPtrTy, "nn_ptr");

    context.Builder.CreateCall(PredictF, {ModelPtr, InPtr, OutBuf, InSize, llvm::ConstantInt::get(Int32Ty, 1)});

    // Return the value from buffer
    return TypedValue(context.Builder.CreateLoad(DoubleTy, OutBuf), TypeKind::Double);
}

TypedValue GoalExprAST::codegen(CodegenContext& context)
{
    emitLocation(this, context);
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Module* TheModule = context.TheModule;

    TypedValue ExprTV = Expression->codegen(context);
    TypedValue TargetTV = Target->codegen(context);

    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(Ctx);

    llvm::Function* Fn = TheModule->getFunction("flux_register_goal");
    if (!Fn) {
        Fn = llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getVoidTy(Ctx), {DoubleTy, DoubleTy}, false),
                                    llvm::Function::ExternalLinkage, "flux_register_goal", TheModule);
    }

    context.Builder.CreateCall(Fn, {ExprTV.Val, TargetTV.Val});

    return TypedValue(llvm::ConstantFP::get(llvm::Type::getDoubleTy(Ctx), 0.0), TypeKind::Double);
}

TypedValue ThermalBlockAST::codegen(CodegenContext& context)
{
    emitLocation(this, context);
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Module* TheModule = context.TheModule;

    TypedValue PowerTV = Power->codegen(context);
    TypedValue ResTV = Resistance->codegen(context);
    TypedValue CapTV = Capacitance->codegen(context);

    if (!PowerTV.Val || !ResTV.Val || !CapTV.Val) {
        std::cerr << "[FLUX ERROR] Thermal block expression failed to codegen" << std::endl;
        return TypedValue();
    }

    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(Ctx);

    // Call flux_thermal_step(double power, double resistance, double capacitance)
    // Returns current temperature
    llvm::Function* Fn = TheModule->getFunction("flux_thermal_step");
    if (!Fn) {
        Fn = llvm::Function::Create(llvm::FunctionType::get(DoubleTy, {DoubleTy, DoubleTy, DoubleTy}, false),
                                    llvm::Function::ExternalLinkage, "flux_thermal_step", TheModule);
    }

    llvm::Value* Temp = context.Builder.CreateCall(Fn, {PowerTV.Val, ResTV.Val, CapTV.Val}, "temp");

    return TypedValue(Temp, TypeKind::Double);
}

TypedValue MonteCarloExprAST::codegen(CodegenContext& context)
{
    emitLocation(this, context);
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Module* TheModule = context.TheModule;

    // Build comma-separated strings for components
    std::string namesStr, nominalsStr, tolerancesStr;
    for (size_t i = 0; i < ComponentNames.size(); i++) {
        if (i > 0) {
            namesStr += ",";
            nominalsStr += ",";
            tolerancesStr += ",";
        }
        namesStr += ComponentNames[i];
        nominalsStr += (i < Nominals.size()) ? std::to_string(Nominals[i]) : "0.0";
        tolerancesStr += (i < Tolerances.size()) ? std::to_string(Tolerances[i]) : "0.01";
    }

    llvm::Function* Fn = TheModule->getFunction("flux_monte_carlo_analyze");
    if (!Fn) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(Ctx);
        llvm::Type* Int32Ty = llvm::Type::getInt32Ty(Ctx);
        Fn = llvm::Function::Create(
            llvm::FunctionType::get(DoubleTy, {DoubleTy, DoubleTy, DoubleTy, DoubleTy, Int32Ty}, false),
            llvm::Function::ExternalLinkage, "flux_monte_carlo_analyze", TheModule);
    }

    // Convert string pointers to doubles (JIT ABI convention)
    auto stringToDouble = [&](const std::string& s) -> llvm::Value* {
        llvm::Value* Ptr = context.Builder.CreateGlobalString(s);
        llvm::Type* Int64Ty = llvm::Type::getInt64Ty(Ctx);
        llvm::Value* IntVal = context.Builder.CreatePtrToInt(Ptr, Int64Ty, "str_int");
        return context.Builder.CreateBitCast(IntVal, llvm::Type::getDoubleTy(Ctx), "str_dbl");
    };

    llvm::Value* Res = context.Builder.CreateCall(
        Fn, {stringToDouble(OutputName), stringToDouble(namesStr), stringToDouble(nominalsStr),
             stringToDouble(tolerancesStr), llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), Iterations)},
        "mc_res");
    return TypedValue(Res, TypeKind::Double);
}

TypedValue VirtualProbeExprAST::codegen(CodegenContext& context)
{
    emitLocation(this, context);
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Module* TheModule = context.TheModule;

    TypedValue SigTV = Signal->codegen(context);
    if (!SigTV.Val) {
        std::cerr << "[FLUX ERROR] Virtual probe signal expression failed to codegen" << std::endl;
        return TypedValue();
    }

    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(Ctx);
    llvm::Type* VoidPtrTy = llvm::PointerType::get(Ctx, 0);

    // Call flux_virtual_probe(double value, const char* name)
    llvm::Function* Fn = TheModule->getFunction("flux_virtual_probe");
    if (!Fn) {
        Fn = llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getVoidTy(Ctx), {DoubleTy, VoidPtrTy}, false),
                                    llvm::Function::ExternalLinkage, "flux_virtual_probe", TheModule);
    }

    context.Builder.CreateCall(Fn, {SigTV.Val, context.Builder.CreateGlobalString(ProbeName)});

    return TypedValue(SigTV.Val, TypeKind::Double);
}

TypedValue HotSwapExprAST::codegen(CodegenContext& context)
{
    emitLocation(this, context);
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Module* TheModule = context.TheModule;

    TypedValue NameTV = SubcktName->codegen(context);
    TypedValue ModelTV = Model->codegen(context);

    if (!NameTV.Val || !ModelTV.Val) {
        std::cerr << "[FLUX ERROR] Hot-swap subcircuit or model expression failed to codegen" << std::endl;
        return TypedValue();
    }

    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(Ctx);
    llvm::Type* VoidPtrTy = llvm::PointerType::get(Ctx, 0);

    llvm::Value* NamePtr = NameTV.Val;
    if (NameTV.Type.Kind == TypeKind::String) {
        // Already handled in CallExprAST::codegen? No, need to bitcast here.
        if (NamePtr->getType()->isFloatingPointTy()) {
            llvm::Type* Int64Ty = llvm::Type::getInt64Ty(Ctx);
            NamePtr = context.Builder.CreateBitCast(NamePtr, Int64Ty, "name_int");
            NamePtr = context.Builder.CreateIntToPtr(NamePtr, VoidPtrTy, "name_ptr");
        }
    } else if (NameTV.Type.Kind == TypeKind::Double) {
        // Assume it's a null pointer (0.0) or an address
        llvm::Type* Int64Ty = llvm::Type::getInt64Ty(Ctx);
        NamePtr = context.Builder.CreateFPToSI(NamePtr, Int64Ty, "name_int");
        NamePtr = context.Builder.CreateIntToPtr(NamePtr, VoidPtrTy, "name_ptr");
    }

    // Call flux_hot_swap(const char* subckt_name, double model_ptr)
    llvm::Function* Fn = TheModule->getFunction("flux_hot_swap");
    if (!Fn) {
        Fn = llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getVoidTy(Ctx), {VoidPtrTy, DoubleTy}, false),
                                    llvm::Function::ExternalLinkage, "flux_hot_swap", TheModule);
    }

    context.Builder.CreateCall(Fn, {NamePtr, ModelTV.Val});

    return TypedValue(llvm::ConstantFP::get(llvm::Type::getDoubleTy(Ctx), 1.0), TypeKind::Double);
}

TypedValue SpawnExprAST::codegen(CodegenContext& context)
{
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(Ctx);
    llvm::Type* Int64Ty = llvm::Type::getInt64Ty(Ctx);
    llvm::Type* VoidPtrTy = llvm::PointerType::get(Ctx, 0);

    // 1. Evaluate all arguments
    size_t nargs = Args.size();
    std::vector<llvm::Value*> argVals;
    argVals.reserve(nargs);
    for (auto& arg : Args) {
        TypedValue tv = arg->codegen(context);
        if (!tv.Val) return TypedValue();
        argVals.push_back(tv.Val);
    }

    // 2. Get the LLVM function pointer for the callee
    llvm::Function* calleeFn = context.TheModule->getFunction(Callee);
    if (!calleeFn) {
        std::vector<llvm::Type*> paramTys(nargs, DoubleTy);
        auto* ft = llvm::FunctionType::get(DoubleTy, paramTys, false);
        calleeFn = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, Callee, context.TheModule);
    }

    // 3. Create a double[] array on the stack to hold argument values
    llvm::AllocaInst* argsArray = context.Builder.CreateAlloca(
        llvm::ArrayType::get(DoubleTy, nargs), nullptr, "spawn_args");
    for (size_t i = 0; i < nargs; i++) {
        llvm::Value* idx0 = llvm::ConstantInt::get(Int64Ty, 0);
        llvm::Value* idx1 = llvm::ConstantInt::get(Int64Ty, static_cast<uint64_t>(i));
        llvm::Value* gep = context.Builder.CreateGEP(
            argsArray->getAllocatedType(), argsArray, {idx0, idx1}, "spawn_arg");
        context.Builder.CreateStore(argVals[i], gep);
    }

    // 4. Cast function pointer to void*
    llvm::Value* fnPtr = context.Builder.CreateBitCast(calleeFn, VoidPtrTy, "spawn_fn_ptr");

    // 5. Cast args array to void*
    llvm::Value* argsPtr = context.Builder.CreateBitCast(argsArray, VoidPtrTy, "spawn_args_ptr");

    // 6. Declare / get flux_spawn(void*, void*, i64) -> double
    llvm::Function* spawnFn = context.TheModule->getFunction("flux_spawn");
    if (!spawnFn) {
        auto* spawnFT = llvm::FunctionType::get(DoubleTy, {VoidPtrTy, VoidPtrTy, Int64Ty}, false);
        spawnFn = llvm::Function::Create(spawnFT, llvm::Function::ExternalLinkage, "flux_spawn", context.TheModule);
    }

    llvm::Value* nargsVal = llvm::ConstantInt::get(Int64Ty, static_cast<uint64_t>(nargs));
    llvm::Value* handle = context.Builder.CreateCall(spawnFn, {fnPtr, argsPtr, nargsVal}, "spawn_handle");

    return TypedValue(handle, FluxType(TypeKind::Double));
}

TypedValue JoinExprAST::codegen(CodegenContext& context)
{
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(Ctx);

    // 1. Evaluate the handle expression
    TypedValue handleTV = Handle->codegen(context);
    if (!handleTV.Val) return TypedValue();

    // 2. Declare / get flux_join(double) -> double
    llvm::Function* joinFn = context.TheModule->getFunction("flux_join");
    if (!joinFn) {
        auto* joinFT = llvm::FunctionType::get(DoubleTy, {DoubleTy}, false);
        joinFn = llvm::Function::Create(joinFT, llvm::Function::ExternalLinkage, "flux_join", context.TheModule);
    }

    llvm::Value* result = context.Builder.CreateCall(joinFn, {handleTV.Val}, "join_result");
    return TypedValue(result, FluxType(TypeKind::Double));
}


} // namespace Flux
