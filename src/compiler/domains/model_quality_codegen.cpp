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

#include "flux/compiler/model_quality_ast.h"
#include "flux/runtime/model_quality_engine.h"
#include <cmath>
#include <iostream>
#include <sstream>

namespace Flux {

// ============================================================================
// Transient Assertion Codegen
// ============================================================================

TypedValue AssertDeclAST::codegen(CodegenContext& context)
{
    std::cout << "[CodeGen] Assert: V(" << Node << ") " << Operator << " " << Bound;
    if (WithinTime > 0)
        std::cout << " within " << WithinTime << "s";
    if (!Message.empty())
        std::cout << " \"" << Message << "\"";
    std::cout << std::endl;

    // Generate call to assertion runtime
    llvm::Value* NodePtr = context.Builder.CreateGlobalString(Node, "assert_node");
    llvm::Value* OpPtr = context.Builder.CreateGlobalString(Operator, "assert_op");
    llvm::Value* BoundVal = llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), Bound);
    llvm::Value* WithinVal = llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), WithinTime);
    llvm::Value* MsgPtr = context.Builder.CreateGlobalString(Message, "assert_msg");

    llvm::Function* AssertF = context.TheModule->getFunction("flux_assert_voltage");
    if (!AssertF) {
        llvm::Type* CharPtrTy = llvm::PointerType::get(context.TheContext, 0);
        AssertF = llvm::Function::Create(
            llvm::FunctionType::get(llvm::Type::getInt1Ty(context.TheContext),
                                    {CharPtrTy, CharPtrTy, llvm::Type::getDoubleTy(context.TheContext),
                                     llvm::Type::getDoubleTy(context.TheContext), CharPtrTy},
                                    false),
            llvm::Function::ExternalLinkage, "flux_assert_voltage", context.TheModule);
    }

    auto call = context.Builder.CreateCall(AssertF, {NodePtr, OpPtr, BoundVal, WithinVal, MsgPtr}, "assert_result");

    return TypedValue(call, TypeKind::Int);
}

TypedValue SettleDeclAST::codegen(CodegenContext& context)
{
    std::cout << "[CodeGen] Settle: V(" << Node << ") tol=" << TolerancePercent << "% after " << AfterTime << "s"
              << std::endl;

    llvm::Value* NodePtr = context.Builder.CreateGlobalString(Node, "settle_node");
    llvm::Value* TolVal = llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), TolerancePercent);
    llvm::Value* AfterVal = llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), AfterTime);

    llvm::Function* SettleF = context.TheModule->getFunction("flux_check_settling");
    if (!SettleF) {
        llvm::Type* CharPtrTy = llvm::PointerType::get(context.TheContext, 0);
        SettleF =
            llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getDoubleTy(context.TheContext),
                                                           {CharPtrTy, llvm::Type::getDoubleTy(context.TheContext),
                                                            llvm::Type::getDoubleTy(context.TheContext)},
                                                           false),
                                   llvm::Function::ExternalLinkage, "flux_check_settling", context.TheModule);
    }

    auto call = context.Builder.CreateCall(SettleF, {NodePtr, TolVal, AfterVal}, "settle_time");

    return TypedValue(call, TypeKind::Double);
}

// ============================================================================
// Golden Waveform Codegen
// ============================================================================

TypedValue GoldenDeclAST::codegen(CodegenContext& context)
{
    std::cout << "[CodeGen] Golden: " << Name;
    if (!Filename.empty())
        std::cout << " from " << Filename;
    std::cout << " (" << Values.size() << " points)" << std::endl;

    // Load waveform if filename provided
    if (!Filename.empty()) {
        // Would call flux_load_golden() at runtime
        std::cout << "  Will load from: " << Filename << std::endl;
    }

    llvm::Value* NamePtr = context.Builder.CreateGlobalString(Name, "golden_name");
    llvm::Function* RegisterF = context.TheModule->getFunction("flux_register_golden");
    if (!RegisterF) {
        llvm::Type* CharPtrTy = llvm::PointerType::get(context.TheContext, 0);
        RegisterF = llvm::Function::Create(
            llvm::FunctionType::get(llvm::Type::getVoidTy(context.TheContext), {CharPtrTy}, false),
            llvm::Function::ExternalLinkage, "flux_register_golden", context.TheModule);
    }

    context.Builder.CreateCall(RegisterF, {NamePtr});

    return TypedValue(llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), 1.0), TypeKind::Double);
}

TypedValue CompareDeclAST::codegen(CodegenContext& context)
{
    std::cout << "[CodeGen] Compare: V(" << Node << ") vs golden '" << GoldenName << "' tol=" << TolerancePercent << "%"
              << std::endl;

    llvm::Value* NodePtr = context.Builder.CreateGlobalString(Node, "compare_node");
    llvm::Value* GoldenPtr = context.Builder.CreateGlobalString(GoldenName, "golden_name");
    llvm::Value* TolVal = llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), TolerancePercent);

    llvm::Function* CompareF = context.TheModule->getFunction("flux_compare_waveform");
    if (!CompareF) {
        llvm::Type* CharPtrTy = llvm::PointerType::get(context.TheContext, 0);
        CompareF = llvm::Function::Create(
            llvm::FunctionType::get(llvm::Type::getInt1Ty(context.TheContext),
                                    {CharPtrTy, CharPtrTy, llvm::Type::getDoubleTy(context.TheContext)}, false),
            llvm::Function::ExternalLinkage, "flux_compare_waveform", context.TheModule);
    }

    auto call = context.Builder.CreateCall(CompareF, {NodePtr, GoldenPtr, TolVal}, "compare_result");

    return TypedValue(call, TypeKind::Int);
}

// ============================================================================
// Convergence Diagnostics Codegen
// ============================================================================

TypedValue ConvergeDeclAST::codegen(CodegenContext& context)
{
    std::cout << "[CodeGen] Converge: V(" << Node << ") max_iter=" << MaxIterations << " eps=" << Epsilon << std::endl;

    llvm::Value* NodePtr = context.Builder.CreateGlobalString(Node, "converge_node");
    llvm::Value* MaxIterVal = llvm::ConstantInt::get(context.TheContext, llvm::APInt(32, MaxIterations));
    llvm::Value* EpsVal = llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), Epsilon);

    llvm::Function* ConvergeF = context.TheModule->getFunction("flux_check_convergence");
    if (!ConvergeF) {
        llvm::Type* CharPtrTy = llvm::PointerType::get(context.TheContext, 0);
        ConvergeF =
            llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getInt1Ty(context.TheContext),
                                                           {CharPtrTy, llvm::Type::getInt32Ty(context.TheContext),
                                                            llvm::Type::getDoubleTy(context.TheContext)},
                                                           false),
                                   llvm::Function::ExternalLinkage, "flux_check_convergence", context.TheModule);
    }

    auto call = context.Builder.CreateCall(ConvergeF, {NodePtr, MaxIterVal, EpsVal}, "converged");

    return TypedValue(call, TypeKind::Int);
}

TypedValue DiscontinuityDeclAST::codegen(CodegenContext& context)
{
    std::cout << "[CodeGen] Discontinuity: V(" << Node << ") threshold=" << Threshold << std::endl;

    llvm::Value* NodePtr = context.Builder.CreateGlobalString(Node, "disc_node");
    llvm::Value* ThreshVal = llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), Threshold);

    llvm::Function* DiscF = context.TheModule->getFunction("flux_detect_discontinuity");
    if (!DiscF) {
        llvm::Type* CharPtrTy = llvm::PointerType::get(context.TheContext, 0);
        DiscF = llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getInt1Ty(context.TheContext),
                                                               {CharPtrTy, llvm::Type::getDoubleTy(context.TheContext)},
                                                               false),
                                       llvm::Function::ExternalLinkage, "flux_detect_discontinuity", context.TheModule);
    }

    auto call = context.Builder.CreateCall(DiscF, {NodePtr, ThreshVal}, "discontinuity_found");

    return TypedValue(call, TypeKind::Int);
}

TypedValue StateDeclAST::codegen(CodegenContext& context)
{
    std::cout << "[CodeGen] State: V(" << Node << ") depth=" << HistoryDepth << std::endl;

    llvm::Value* NodePtr = context.Builder.CreateGlobalString(Node, "state_node");
    llvm::Value* DepthVal = llvm::ConstantInt::get(context.TheContext, llvm::APInt(32, HistoryDepth));

    llvm::Function* StateF = context.TheModule->getFunction("flux_detect_hidden_state");
    if (!StateF) {
        llvm::Type* CharPtrTy = llvm::PointerType::get(context.TheContext, 0);
        StateF = llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getInt1Ty(context.TheContext),
                                                                {CharPtrTy, llvm::Type::getInt32Ty(context.TheContext)},
                                                                false),
                                        llvm::Function::ExternalLinkage, "flux_detect_hidden_state", context.TheModule);
    }

    auto call = context.Builder.CreateCall(StateF, {NodePtr, DepthVal}, "hidden_state_found");

    return TypedValue(call, TypeKind::Int);
}

TypedValue VerifyBlockAST::codegen(CodegenContext& context)
{
    std::cout << "[CodeGen] Verify block: " << Checks.size() << " checks, " << Comparisons.size() << " comparisons, "
              << Diagnostics.size() << " diagnostics" << std::endl;

    // Codegen all checks
    for (const auto& check : Checks) {
        check->codegen(context);
    }

    // Codegen all comparisons
    for (const auto& comp : Comparisons) {
        comp->codegen(context);
    }

    // Codegen all diagnostics
    for (const auto& diag : Diagnostics) {
        diag->codegen(context);
    }

    return TypedValue(llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), 1.0), TypeKind::Double);
}

TypedValue ToleranceDeclAST::codegen(CodegenContext& context)
{
    std::cout << "[CodeGen] Tolerance: abs=" << AbsoluteTolerance << " rel=" << RelativeTolerancePercent << "%"
              << std::endl;

    llvm::Value* AbsVal = llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), AbsoluteTolerance);
    llvm::Value* RelVal = llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), RelativeTolerancePercent);

    llvm::Function* TolF = context.TheModule->getFunction("flux_set_tolerance");
    if (!TolF) {
        TolF = llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getVoidTy(context.TheContext),
                                                              {llvm::Type::getDoubleTy(context.TheContext),
                                                               llvm::Type::getDoubleTy(context.TheContext)},
                                                              false),
                                      llvm::Function::ExternalLinkage, "flux_set_tolerance", context.TheModule);
    }

    context.Builder.CreateCall(TolF, {AbsVal, RelVal});

    return TypedValue(llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), 1.0), TypeKind::Double);
}

TypedValue DiagnosticDeclAST::codegen(CodegenContext& context)
{
    std::cout << "[CodeGen] Diagnostic: node=" << Node << " type=" << DiagnosticType
              << " threshold=" << Threshold << std::endl;

    // Set diagnostic parameters in runtime
    llvm::Function* DiagF = context.TheModule->getFunction("flux_set_diagnostic");
    if (!DiagF) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        DiagF = llvm::Function::Create(
            llvm::FunctionType::get(DoubleTy, {DoubleTy, DoubleTy, DoubleTy}, false),
            llvm::Function::ExternalLinkage, "flux_set_diagnostic", context.TheModule);
    }

    auto stringToDouble = [&](const std::string& s) -> llvm::Value* {
        llvm::Value* Ptr = context.Builder.CreateGlobalString(s);
        llvm::Type* Int64Ty = llvm::Type::getInt64Ty(context.TheContext);
        llvm::Value* IntVal = context.Builder.CreatePtrToInt(Ptr, Int64Ty, "str_int");
        return context.Builder.CreateBitCast(IntVal, llvm::Type::getDoubleTy(context.TheContext), "str_dbl");
    };

    context.Builder.CreateCall(DiagF, {stringToDouble(Node), stringToDouble(DiagnosticType),
                                       llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), Threshold)},
                               "diag_res");

    return TypedValue(llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), 1.0), TypeKind::Double);
}

} // namespace Flux
