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

// Codegen for Section 7.2: Mixed-Signal & Modeling Extensions
#include "flux/compiler/ast.h"
#include "flux/runtime/units.h"
#include <iostream>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>
#include <string>

namespace Flux {

// ============================================================================
// Event-driven constructs
// ============================================================================

TypedValue CrossExprAST::codegen(CodegenContext& context)
{
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);

    // Get or declare cross detection runtime function
    // double flux_cross_detect(double value, int rise_fall)
    llvm::Function* crossFunc = context.TheModule->getFunction("flux_cross_detect");
    if (!crossFunc) {
        llvm::FunctionType* FT = llvm::FunctionType::get(
            DoubleTy, llvm::ArrayRef<llvm::Type*>{DoubleTy, llvm::Type::getInt32Ty(context.TheContext)}, false);
        crossFunc = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "flux_cross_detect", context.TheModule);
    }

    // Codegen the expression
    auto ExprVal = Expression->codegen(context);
    llvm::Value* ExprLLVM = ExprVal.Val;
    if (ExprVal.Type.Kind == TypeKind::Int) {
        ExprLLVM = context.Builder.CreateSIToFP(ExprLLVM, DoubleTy, "inttodouble");
    }

    llvm::Value* RiseFallVal = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context.TheContext), RiseFall, true);
    return TypedValue(context.Builder.CreateCall(crossFunc, {ExprLLVM, RiseFallVal}, "cross_result"), TypeKind::Double);
}

TypedValue AboveExprAST::codegen(CodegenContext& context)
{
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);

    // Get or declare above detection runtime function
    // double flux_above_detect(double value, double threshold)
    llvm::Function* aboveFunc = context.TheModule->getFunction("flux_above_detect");
    if (!aboveFunc) {
        llvm::FunctionType* FT =
            llvm::FunctionType::get(DoubleTy, llvm::ArrayRef<llvm::Type*>{DoubleTy, DoubleTy}, false);
        aboveFunc = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "flux_above_detect", context.TheModule);
    }

    auto ExprVal = Expression->codegen(context);
    llvm::Value* ExprLLVM = ExprVal.Val;
    if (ExprVal.Type.Kind == TypeKind::Int) {
        ExprLLVM = context.Builder.CreateSIToFP(ExprLLVM, DoubleTy, "inttodouble");
    }

    auto ThreshVal = Threshold->codegen(context);
    llvm::Value* ThreshLLVM = ThreshVal.Val;
    if (ThreshVal.Type.Kind == TypeKind::Int) {
        ThreshLLVM = context.Builder.CreateSIToFP(ThreshLLVM, DoubleTy, "inttodouble");
    }

    return TypedValue(context.Builder.CreateCall(aboveFunc, {ExprLLVM, ThreshLLVM}, "above_result"), TypeKind::Double);
}

TypedValue TimerExprAST::codegen(CodegenContext& context)
{
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);

    // Get or declare timer runtime function
    // double flux_timer_get()
    llvm::Function* timerFunc = context.TheModule->getFunction("flux_timer_get");
    if (!timerFunc) {
        llvm::FunctionType* FT = llvm::FunctionType::get(DoubleTy, false);
        timerFunc = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "flux_timer_get", context.TheModule);
    }

    return TypedValue(context.Builder.CreateCall(timerFunc, {}, "timer_value"), TypeKind::Double);
}

// ============================================================================
// Real-valued digital modeling
// ============================================================================

TypedValue FSMExprAST::codegen(CodegenContext& context)
{
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
    llvm::Type* Int32Ty = llvm::Type::getInt32Ty(context.TheContext);
    llvm::Type* Int64Ty = llvm::Type::getInt64Ty(context.TheContext);
    llvm::Type* VoidPtrTy = llvm::PointerType::get(context.TheContext, 0);

    // Create FSM runtime object
    // void* flux_fsm_create(int initial_state, double output_fn)
    llvm::Function* createFunc = context.TheModule->getFunction("flux_fsm_create");
    if (!createFunc) {
        llvm::FunctionType* FT =
            llvm::FunctionType::get(VoidPtrTy, llvm::ArrayRef<llvm::Type*>{Int32Ty, DoubleTy}, false);
        createFunc = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "flux_fsm_create", context.TheModule);
    }

    llvm::Value* InitStateVal = llvm::ConstantInt::get(Int32Ty, InitialState, true);

    llvm::Value* OutputFnStrPtr = context.Builder.CreateGlobalString(OutputFn, "output_fn_str");
    llvm::Value* OutputFnAsInt = context.Builder.CreatePtrToInt(OutputFnStrPtr, Int64Ty);
    llvm::Value* OutputFnAsDouble = context.Builder.CreateBitCast(OutputFnAsInt, DoubleTy);

    llvm::Value* FsmObj = context.Builder.CreateCall(createFunc, {InitStateVal, OutputFnAsDouble});

    // Add transitions
    // void flux_fsm_add_transition(void* fsm, int cur, int next, double (*cond)(void*), double (*out)(void*))
    llvm::Function* addTransFunc = context.TheModule->getFunction("flux_fsm_add_transition");
    if (!addTransFunc) {
        llvm::FunctionType* CondFnTy = llvm::FunctionType::get(DoubleTy, llvm::ArrayRef<llvm::Type*>{VoidPtrTy}, false);
        llvm::Type* CondFnPtrTy = llvm::PointerType::get(CondFnTy->getContext(), 0);
        llvm::Type* OutFnPtrTy = llvm::PointerType::get(CondFnTy->getContext(), 0);
        llvm::FunctionType* FT = llvm::FunctionType::get(
            llvm::Type::getVoidTy(context.TheContext),
            llvm::ArrayRef<llvm::Type*>{VoidPtrTy, Int32Ty, Int32Ty, CondFnPtrTy, OutFnPtrTy}, false);
        addTransFunc =
            llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "flux_fsm_add_transition", context.TheModule);
    }

    for (size_t i = 0; i < Transitions.size(); ++i) {
        auto& trans = Transitions[i];

        // Create condition function
        std::string CondFnName = "fsm_cond_" + std::to_string(i);
        llvm::FunctionType* CondFnTy = llvm::FunctionType::get(DoubleTy, llvm::ArrayRef<llvm::Type*>{VoidPtrTy}, false);
        llvm::Function* CondFn =
            llvm::Function::Create(CondFnTy, llvm::Function::InternalLinkage, CondFnName, context.TheModule);
        llvm::BasicBlock* CondEntry = llvm::BasicBlock::Create(context.TheContext, "entry", CondFn);
        llvm::IRBuilder<> CondBuilder(context.TheContext);
        CondBuilder.SetInsertPoint(CondEntry);

        // Evaluate condition expression
        // We save and restore NamedValues scope for the condition
        auto SavedNames = context.NamedValues;
        // The condition would be evaluated in a runtime context
        // For now, emit a stub that returns 1.0 (true)
        CondBuilder.CreateRet(llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), 1.0));
        context.NamedValues = SavedNames;

        // Create output function
        std::string OutFnName = "fsm_out_" + std::to_string(i);
        llvm::Function* OutFn =
            llvm::Function::Create(CondFnTy, llvm::Function::InternalLinkage, OutFnName, context.TheModule);
        llvm::BasicBlock* OutEntry = llvm::BasicBlock::Create(context.TheContext, "entry", OutFn);
        llvm::IRBuilder<> OutBuilder(context.TheContext);
        OutBuilder.SetInsertPoint(OutEntry);

        // Evaluate output expression
        auto SavedNames2 = context.NamedValues;
        // For now, emit a stub that returns 0.0
        OutBuilder.CreateRet(llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), 0.0));
        context.NamedValues = SavedNames2;

        llvm::Value* CurStateVal = llvm::ConstantInt::get(Int32Ty, trans.CurrentState, true);
        llvm::Value* NextStateVal = llvm::ConstantInt::get(Int32Ty, trans.NextState, true);
        context.Builder.CreateCall(addTransFunc, {FsmObj, CurStateVal, NextStateVal, CondFn, OutFn});
    }

    // Return FSM pointer as double (opaque handle)
    llvm::Value* PtrAsInt = context.Builder.CreatePtrToInt(FsmObj, Int64Ty, "fsm_handle");
    return TypedValue(context.Builder.CreateBitCast(PtrAsInt, DoubleTy, "fsm_double"), TypeKind::Double);
}

TypedValue EdgeExprAST::codegen(CodegenContext& context)
{
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
    llvm::Type* Int32Ty = llvm::Type::getInt32Ty(context.TheContext);

    // Get or declare edge detection runtime function
    // double flux_edge_detect(double value, int edge_type)
    llvm::Function* edgeFunc = context.TheModule->getFunction("flux_edge_detect");
    if (!edgeFunc) {
        llvm::FunctionType* FT =
            llvm::FunctionType::get(DoubleTy, llvm::ArrayRef<llvm::Type*>{DoubleTy, Int32Ty}, false);
        edgeFunc = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "flux_edge_detect", context.TheModule);
    }

    auto ExprVal = Expression->codegen(context);
    llvm::Value* ExprLLVM = ExprVal.Val;
    if (ExprVal.Type.Kind == TypeKind::Int) {
        ExprLLVM = context.Builder.CreateSIToFP(ExprLLVM, DoubleTy, "inttodouble");
    }

    llvm::Value* EdgeTypeVal = llvm::ConstantInt::get(Int32Ty, EdgeType, true);
    return TypedValue(context.Builder.CreateCall(edgeFunc, {ExprLLVM, EdgeTypeVal}, "edge_result"), TypeKind::Double);
}

TypedValue TriggeredExprAST::codegen(CodegenContext& context)
{
    // Evaluate event expression
    auto EventVal = EventExpr->codegen(context);

    // Check if event is true (non-zero)
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
    llvm::Value* EventLLVM = EventVal.Val;
    if (EventVal.Type.Kind == TypeKind::Int) {
        EventLLVM = context.Builder.CreateSIToFP(EventLLVM, DoubleTy, "inttodouble");
    }

    llvm::Value* IsEvent = context.Builder.CreateFCmpONE(
        EventLLVM, llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), 0.0), "is_event");

    // Create conditional block for body
    llvm::Function* TheFunction = context.Builder.GetInsertBlock()->getParent();
    llvm::BasicBlock* ThenBB = llvm::BasicBlock::Create(context.TheContext, "triggered_then", TheFunction);
    llvm::BasicBlock* MergeBB = llvm::BasicBlock::Create(context.TheContext, "triggered_merge");

    context.Builder.CreateCondBr(IsEvent, ThenBB, MergeBB);

    // Then block: evaluate body
    context.Builder.SetInsertPoint(ThenBB);
    auto BodyVal = Body->codegen(context);
    llvm::Value* ThenResult = BodyVal.Val;
    if (!ThenResult)
        ThenResult = llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), 0.0);
    if (BodyVal.Type.Kind == TypeKind::Int) {
        ThenResult = context.Builder.CreateSIToFP(ThenResult, DoubleTy, "inttodouble");
    }
    llvm::BasicBlock* ThenEndBB = context.Builder.GetInsertBlock();
    context.Builder.CreateBr(MergeBB);

    // Merge block
    TheFunction->insert(TheFunction->end(), MergeBB);
    context.Builder.SetInsertPoint(MergeBB);

    llvm::PHINode* PHI = context.Builder.CreatePHI(DoubleTy, 2, "triggered_result");
    PHI->addIncoming(ThenResult, ThenEndBB);
    PHI->addIncoming(llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), 0.0), ThenBB->getNextNode());

    return TypedValue(PHI, TypeKind::Double);
}

// ============================================================================
// Noise modeling primitives
// ============================================================================

TypedValue NoiseExprAST::codegen(CodegenContext& context)
{
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
    llvm::Type* Int64Ty = llvm::Type::getInt64Ty(context.TheContext);
    llvm::Type* VoidPtrTy = llvm::PointerType::get(context.TheContext, 0);

    // Get or declare noise runtime function
    // double flux_noise_generate(double type, double amplitude, double freq)
    llvm::Function* noiseFunc = context.TheModule->getFunction("flux_noise_generate");
    if (!noiseFunc) {
        llvm::FunctionType* FT =
            llvm::FunctionType::get(DoubleTy, llvm::ArrayRef<llvm::Type*>{DoubleTy, DoubleTy, DoubleTy}, false);
        noiseFunc =
            llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "flux_noise_generate", context.TheModule);
    }

    auto AmpVal = Amplitude->codegen(context);
    llvm::Value* AmpLLVM = AmpVal.Val;
    if (AmpVal.Type.Kind == TypeKind::Int) {
        AmpLLVM = context.Builder.CreateSIToFP(AmpLLVM, DoubleTy, "inttodouble");
    }

    llvm::Value* FreqLLVM = llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), 1.0); // Default 1 Hz
    if (Frequency) {
        auto FreqVal = Frequency->codegen(context);
        FreqLLVM = FreqVal.Val;
        if (FreqVal.Type.Kind == TypeKind::Int) {
            FreqLLVM = context.Builder.CreateSIToFP(FreqLLVM, DoubleTy, "inttodouble");
        }
    }

    llvm::Value* NoiseTypeStrPtr = context.Builder.CreateGlobalString(NoiseType, "noise_type_str");
    llvm::Value* TypeAsInt = context.Builder.CreatePtrToInt(NoiseTypeStrPtr, Int64Ty);
    llvm::Value* TypeAsDouble = context.Builder.CreateBitCast(TypeAsInt, DoubleTy, "type_double");

    return TypedValue(context.Builder.CreateCall(noiseFunc, {TypeAsDouble, AmpLLVM, FreqLLVM}, "noise_value"),
                      TypeKind::Double);
}

TypedValue WhiteNoiseExprAST::codegen(CodegenContext& context)
{
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);

    // double flux_white_noise(double amplitude)
    llvm::Function* func = context.TheModule->getFunction("flux_white_noise");
    if (!func) {
        llvm::FunctionType* FT = llvm::FunctionType::get(DoubleTy, llvm::ArrayRef<llvm::Type*>{DoubleTy}, false);
        func = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "flux_white_noise", context.TheModule);
    }

    auto AmpVal = Amplitude->codegen(context);
    llvm::Value* AmpLLVM = AmpVal.Val;
    if (AmpVal.Type.Kind == TypeKind::Int) {
        AmpLLVM = context.Builder.CreateSIToFP(AmpLLVM, DoubleTy, "inttodouble");
    }

    return TypedValue(context.Builder.CreateCall(func, {AmpLLVM}, "white_noise"), TypeKind::Double);
}

TypedValue FlickerNoiseExprAST::codegen(CodegenContext& context)
{
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);

    // double flux_flicker_noise(double amplitude, double corner_freq)
    llvm::Function* func = context.TheModule->getFunction("flux_flicker_noise");
    if (!func) {
        llvm::FunctionType* FT =
            llvm::FunctionType::get(DoubleTy, llvm::ArrayRef<llvm::Type*>{DoubleTy, DoubleTy}, false);
        func = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "flux_flicker_noise", context.TheModule);
    }

    auto AmpVal = Amplitude->codegen(context);
    llvm::Value* AmpLLVM = AmpVal.Val;
    if (AmpVal.Type.Kind == TypeKind::Int) {
        AmpLLVM = context.Builder.CreateSIToFP(AmpLLVM, DoubleTy, "inttodouble");
    }

    auto CfVal = CornerFreq->codegen(context);
    llvm::Value* CfLLVM = CfVal.Val;
    if (CfVal.Type.Kind == TypeKind::Int) {
        CfLLVM = context.Builder.CreateSIToFP(CfLLVM, DoubleTy, "inttodouble");
    }

    return TypedValue(context.Builder.CreateCall(func, {AmpLLVM, CfLLVM}, "flicker_noise"), TypeKind::Double);
}

TypedValue ThermalNoiseExprAST::codegen(CodegenContext& context)
{
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);

    // double flux_thermal_noise(double resistance, double temperature)
    llvm::Function* func = context.TheModule->getFunction("flux_thermal_noise");
    if (!func) {
        llvm::FunctionType* FT =
            llvm::FunctionType::get(DoubleTy, llvm::ArrayRef<llvm::Type*>{DoubleTy, DoubleTy}, false);
        func = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "flux_thermal_noise", context.TheModule);
    }

    auto ResVal = Resistance->codegen(context);
    llvm::Value* ResLLVM = ResVal.Val;
    if (ResVal.Type.Kind == TypeKind::Int) {
        ResLLVM = context.Builder.CreateSIToFP(ResLLVM, DoubleTy, "inttodouble");
    }

    llvm::Value* TempLLVM =
        llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), 300.15); // Default 300.15K
    if (Temperature) {
        auto TVal = Temperature->codegen(context);
        TempLLVM = TVal.Val;
        if (TVal.Type.Kind == TypeKind::Int) {
            TempLLVM = context.Builder.CreateSIToFP(TempLLVM, DoubleTy, "inttodouble");
        }
    }

    return TypedValue(context.Builder.CreateCall(func, {ResLLVM, TempLLVM}, "thermal_noise"), TypeKind::Double);
}

// ============================================================================
// Piecewise and table-based models
// ============================================================================

TypedValue PiecewiseExprAST::codegen(CodegenContext& context)
{
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
    llvm::Type* Int64Ty = llvm::Type::getInt64Ty(context.TheContext);
    llvm::Type* VoidPtrTy = llvm::PointerType::get(context.TheContext, 0);

    // Create piecewise interpolation context
    // void* flux_piecewise_create(double interpolation)
    llvm::Function* createFunc = context.TheModule->getFunction("flux_piecewise_create");
    if (!createFunc) {
        llvm::FunctionType* FT = llvm::FunctionType::get(VoidPtrTy, llvm::ArrayRef<llvm::Type*>{DoubleTy}, false);
        createFunc =
            llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "flux_piecewise_create", context.TheModule);
    }

    llvm::Value* InterpStrPtr = context.Builder.CreateGlobalString(Interpolation, "interp_str");
    llvm::Value* InterpAsInt = context.Builder.CreatePtrToInt(InterpStrPtr, Int64Ty);
    llvm::Value* InterpAsDouble = context.Builder.CreateBitCast(InterpAsInt, DoubleTy, "interp_double");

    llvm::Value* PwObj = context.Builder.CreateCall(createFunc, {InterpAsDouble});

    // Add data points
    // void flux_piecewise_add_point(void* pw, double x, double y)
    llvm::Function* addPointFunc = context.TheModule->getFunction("flux_piecewise_add_point");
    if (!addPointFunc) {
        llvm::FunctionType* FT =
            llvm::FunctionType::get(llvm::Type::getVoidTy(context.TheContext),
                                    llvm::ArrayRef<llvm::Type*>{VoidPtrTy, DoubleTy, DoubleTy}, false);
        addPointFunc =
            llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "flux_piecewise_add_point", context.TheModule);
    }

    for (auto& pt : Points) {
        auto XVal = pt.X->codegen(context);
        llvm::Value* XLLVM = XVal.Val;
        if (XVal.Type.Kind == TypeKind::Int) {
            XLLVM = context.Builder.CreateSIToFP(XLLVM, DoubleTy, "inttodouble");
        }

        auto YVal = pt.Y->codegen(context);
        llvm::Value* YLLVM = YVal.Val;
        if (YVal.Type.Kind == TypeKind::Int) {
            YLLVM = context.Builder.CreateSIToFP(YLLVM, DoubleTy, "inttodouble");
        }

        context.Builder.CreateCall(addPointFunc, {PwObj, XLLVM, YLLVM});
    }

    // Evaluate at query x
    // double flux_piecewise_eval(void* pw, double x)
    llvm::Function* evalFunc = context.TheModule->getFunction("flux_piecewise_eval");
    if (!evalFunc) {
        llvm::FunctionType* FT =
            llvm::FunctionType::get(DoubleTy, llvm::ArrayRef<llvm::Type*>{VoidPtrTy, DoubleTy}, false);
        evalFunc =
            llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "flux_piecewise_eval", context.TheModule);
    }

    llvm::Value* QueryLLVM = llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), 0.0);
    if (QueryX) {
        auto QVal = QueryX->codegen(context);
        QueryLLVM = QVal.Val;
        if (QVal.Type.Kind == TypeKind::Int) {
            QueryLLVM = context.Builder.CreateSIToFP(QueryLLVM, DoubleTy, "inttodouble");
        }
    }

    return TypedValue(context.Builder.CreateCall(evalFunc, {PwObj, QueryLLVM}, "piecewise_result"), TypeKind::Double);
}

TypedValue TableExprAST::codegen(CodegenContext& context)
{
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
    llvm::Type* VoidPtrTy = llvm::PointerType::get(context.TheContext, 0);

    // Create table
    // void* flux_table_create()
    llvm::Function* createFunc = context.TheModule->getFunction("flux_table_create");
    if (!createFunc) {
        llvm::FunctionType* FT = llvm::FunctionType::get(VoidPtrTy, false);
        createFunc =
            llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "flux_table_create", context.TheModule);
    }

    llvm::Value* TableObj = context.Builder.CreateCall(createFunc);

    // Add entries
    // void flux_table_add_entry(void* table, double key, double value)
    llvm::Function* addEntryFunc = context.TheModule->getFunction("flux_table_add_entry");
    if (!addEntryFunc) {
        llvm::FunctionType* FT =
            llvm::FunctionType::get(llvm::Type::getVoidTy(context.TheContext),
                                    llvm::ArrayRef<llvm::Type*>{VoidPtrTy, DoubleTy, DoubleTy}, false);
        addEntryFunc =
            llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "flux_table_add_entry", context.TheModule);
    }

    for (auto& entry : Entries) {
        auto KVal = entry.Key->codegen(context);
        llvm::Value* KLLVM = KVal.Val;
        if (KVal.Type.Kind == TypeKind::Int) {
            KLLVM = context.Builder.CreateSIToFP(KLLVM, DoubleTy, "inttodouble");
        }

        auto VVal = entry.Value->codegen(context);
        llvm::Value* VLLVM = VVal.Val;
        if (VVal.Type.Kind == TypeKind::Int) {
            VLLVM = context.Builder.CreateSIToFP(VLLVM, DoubleTy, "inttodouble");
        }

        context.Builder.CreateCall(addEntryFunc, {TableObj, KLLVM, VLLVM});
    }

    // Set default if provided
    if (DefaultValue) {
        // void flux_table_set_default(void* table, double value)
        llvm::Function* setDefFunc = context.TheModule->getFunction("flux_table_set_default");
        if (!setDefFunc) {
            llvm::FunctionType* FT = llvm::FunctionType::get(llvm::Type::getVoidTy(context.TheContext),
                                                             llvm::ArrayRef<llvm::Type*>{VoidPtrTy, DoubleTy}, false);
            setDefFunc = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "flux_table_set_default",
                                                context.TheModule);
        }

        auto DVal = DefaultValue->codegen(context);
        llvm::Value* DLLVM = DVal.Val;
        if (DVal.Type.Kind == TypeKind::Int) {
            DLLVM = context.Builder.CreateSIToFP(DLLVM, DoubleTy, "inttodouble");
        }
        context.Builder.CreateCall(setDefFunc, {TableObj, DLLVM});
    }

    // Lookup query key
    // double flux_table_lookup(void* table, double key)
    llvm::Function* lookupFunc = context.TheModule->getFunction("flux_table_lookup");
    if (!lookupFunc) {
        llvm::FunctionType* FT =
            llvm::FunctionType::get(DoubleTy, llvm::ArrayRef<llvm::Type*>{VoidPtrTy, DoubleTy}, false);
        lookupFunc =
            llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "flux_table_lookup", context.TheModule);
    }

    llvm::Value* QueryLLVM = llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), 0.0);
    if (QueryKey) {
        auto QVal = QueryKey->codegen(context);
        QueryLLVM = QVal.Val;
        if (QVal.Type.Kind == TypeKind::Int) {
            QueryLLVM = context.Builder.CreateSIToFP(QueryLLVM, DoubleTy, "inttodouble");
        }
    }

    return TypedValue(context.Builder.CreateCall(lookupFunc, {TableObj, QueryLLVM}, "table_result"), TypeKind::Double);
}

TypedValue CsvImportExprAST::codegen(CodegenContext& context)
{
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
    llvm::Type* Int64Ty = llvm::Type::getInt64Ty(context.TheContext);
    llvm::Type* VoidPtrTy = llvm::PointerType::get(context.TheContext, 0);

    // Import CSV data
    // void* flux_csv_import(double filename, double options_json)
    llvm::Function* importFunc = context.TheModule->getFunction("flux_csv_import");
    if (!importFunc) {
        llvm::FunctionType* FT =
            llvm::FunctionType::get(VoidPtrTy, llvm::ArrayRef<llvm::Type*>{DoubleTy, DoubleTy}, false);
        importFunc = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "flux_csv_import", context.TheModule);
    }

    llvm::Value* FilenameStrPtr = context.Builder.CreateGlobalString(Filename, "filename_str");
    llvm::Value* FilenameAsInt = context.Builder.CreatePtrToInt(FilenameStrPtr, Int64Ty);
    llvm::Value* FilenameAsDouble = context.Builder.CreateBitCast(FilenameAsInt, DoubleTy, "filename_double");

    // Serialize options to JSON string
    std::string optsJson = "{}";
    if (!Options.empty()) {
        optsJson = "{";
        bool first = true;
        for (auto& [k, v] : Options) {
            if (!first)
                optsJson += ",";
            optsJson += "\"" + k + "\":\"" + v + "\"";
            first = false;
        }
        optsJson += "}";
    }

    llvm::Value* OptsStrPtr = context.Builder.CreateGlobalString(optsJson, "opts_str");
    llvm::Value* OptsAsInt = context.Builder.CreatePtrToInt(OptsStrPtr, Int64Ty);
    llvm::Value* OptsAsDouble = context.Builder.CreateBitCast(OptsAsInt, DoubleTy, "opts_double");

    llvm::Value* CsvObj = context.Builder.CreateCall(importFunc, {FilenameAsDouble, OptsAsDouble});

    // Return as double handle
    llvm::Value* PtrAsInt = context.Builder.CreatePtrToInt(CsvObj, Int64Ty, "csv_handle");
    return TypedValue(context.Builder.CreateBitCast(PtrAsInt, DoubleTy, "csv_double"), TypeKind::Double);
}

// ============================================================================
// Units and dimensional analysis
// ============================================================================

TypedValue UnitExprAST::codegen(CodegenContext& context)
{
    auto ValResult = Value->codegen(context);
    if (!ValResult.Val)
        return TypedValue();

    UnitDimensions dims;
    llvm::Value* V = ValResult.Val;
    if (ValResult.Type.Kind == TypeKind::Int) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        V = context.Builder.CreateSIToFP(V, DoubleTy, "inttodouble");
    }

    if (!this->UnitStr.empty()) {
        try {
            auto scaledUnit = Units::UnitRegistry::instance().parseUnitString(this->UnitStr);
            dims.mass = scaledUnit.dimensions.mass;
            dims.length = scaledUnit.dimensions.length;
            dims.time = scaledUnit.dimensions.time;
            dims.current = scaledUnit.dimensions.current;
            dims.temperature = scaledUnit.dimensions.temperature;
            dims.amount = scaledUnit.dimensions.amount;
            dims.luminous = scaledUnit.dimensions.luminous;

            if (scaledUnit.scale != 1.0) {
                llvm::Value* Scale =
                    llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), scaledUnit.scale);
                V = context.Builder.CreateFMul(V, Scale, "unit_scaled");
            }
        } catch (const std::exception& e) {
            llvm::errs() << "[Flux] unit(): unknown unit \"" << this->UnitStr << "\" (" << e.what() << ")\n";
        }
    }

    return TypedValue(V, FluxType(TypeKind::Double, dims));
}

TypedValue DimensionExprAST::codegen(CodegenContext& context)
{
    auto ExprResult = Expression->codegen(context);
    if (!ExprResult.Val)
        return TypedValue();

    const auto& dims = ExprResult.Type.Dimensions;
    std::string dimStr = dims.toString();
    if (dimStr.empty()) {
        dimStr = "dimensionless";
    }

    llvm::errs() << "[Flux] dimension: " << dimStr << "\n";

    auto DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
    auto Zero = llvm::ConstantFP::get(DoubleTy, 0.0);
    return TypedValue(Zero, TypeKind::Double);
}

TypedValue ConvertExprAST::codegen(CodegenContext& context)
{
    auto ValResult = Value->codegen(context);
    if (!ValResult.Val)
        return TypedValue();

    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
    llvm::Value* V = ValResult.Val;
    if (ValResult.Type.Kind == TypeKind::Int) {
        V = context.Builder.CreateSIToFP(V, DoubleTy, "inttodouble");
    }

    UnitDimensions dims;
    if (!FromUnit.empty() && !ToUnit.empty()) {
        try {
            auto fromUnit = Units::UnitRegistry::instance().parseUnitString(FromUnit);
            auto toUnit = Units::UnitRegistry::instance().parseUnitString(ToUnit);

            if (fromUnit.dimensions != toUnit.dimensions) {
                llvm::errs() << "[Flux] convert(): incompatible units \"" << FromUnit << "\" and \"" << ToUnit
                             << "\"\n";
                return TypedValue();
            }

            double convFactor = fromUnit.scale / toUnit.scale;
            dims.mass = toUnit.dimensions.mass;
            dims.length = toUnit.dimensions.length;
            dims.time = toUnit.dimensions.time;
            dims.current = toUnit.dimensions.current;
            dims.temperature = toUnit.dimensions.temperature;
            dims.amount = toUnit.dimensions.amount;
            dims.luminous = toUnit.dimensions.luminous;

            if (convFactor != 1.0) {
                llvm::Value* CF = llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), convFactor);
                V = context.Builder.CreateFMul(V, CF, "converted");
            }
        } catch (const std::exception& e) {
            llvm::errs() << "[Flux] convert(): error (" << e.what() << ")\n";
            return TypedValue();
        }
    }

    return TypedValue(V, FluxType(TypeKind::Double, dims));
}

TypedValue HasUnitExprAST::codegen(CodegenContext& context)
{
    auto ValResult = Value->codegen(context);
    if (!ValResult.Val)
        return TypedValue();

    const auto& valDims = ValResult.Type.Dimensions;
    bool match = false;

    if (!this->UnitStr.empty()) {
        try {
            auto scaledUnit = Units::UnitRegistry::instance().parseUnitString(this->UnitStr);
            match = valDims.mass == scaledUnit.dimensions.mass && valDims.length == scaledUnit.dimensions.length &&
                    valDims.time == scaledUnit.dimensions.time && valDims.current == scaledUnit.dimensions.current &&
                    valDims.temperature == scaledUnit.dimensions.temperature &&
                    valDims.amount == scaledUnit.dimensions.amount &&
                    valDims.luminous == scaledUnit.dimensions.luminous;
        } catch (const std::exception& e) {
            llvm::errs() << "[Flux] has_unit(): unknown unit \"" << this->UnitStr << "\" (" << e.what() << ")\n";
        }
    }

    return TypedValue(llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), match ? 1.0 : 0.0),
                      TypeKind::Double);
}

} // namespace Flux
