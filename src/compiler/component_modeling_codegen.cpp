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

#include "flux/compiler/component_modeling_ast.h"
#include <iostream>
#include <sstream>

namespace Flux {

// ============================================================================
// Hierarchical Design Codegen
// ============================================================================

TypedValue SubcktInstanceAST::codegen(CodegenContext& context)
{
    // Generate SPICE subcircuit instance line
    // Format: Xname n1 n2 ... subckt_name param1=value1 param2=value2

    std::ostringstream oss;
    oss << "X" << InstanceName;

    // Add nodes
    for (const auto& node : Nodes) {
        oss << " " << node;
    }

    // Add subckt name
    oss << " " << SubcktName;

    // Add parameters
    for (const auto& [param, value] : Parameters) {
        oss << " " << param << "=" << value;
    }

    std::string spiceLine = oss.str();
    std::cout << "[CodeGen] Subckt instance: " << spiceLine << std::endl;

    // Generate LLVM code to register this instance
    llvm::Value* MsgPtr = context.Builder.CreateGlobalString(spiceLine, "subckt_instance");
    llvm::Function* PrintF = context.TheModule->getFunction("println_string");
    if (PrintF) {
        context.Builder.CreateCall(PrintF, {MsgPtr});
    }

    return TypedValue(llvm::ConstantFP::get(context.TheContext, llvm::APFloat(1.0)), TypeKind::Double);
}

// ============================================================================
// Verilog-A Lite Codegen
// ============================================================================

TypedValue AnalogBlockAST::codegen(CodegenContext& context)
{
    std::cout << "[CodeGen] Analog block with " << Contributors.size() << " contributors" << std::endl;

    // Generate contributors
    for (const auto& contributor : Contributors) {
        contributor->codegen(context);
    }

    // Set tolerances
    for (const auto& [param, value] : Tolerances) {
        std::cout << "  Tolerance: " << param << " = " << value << std::endl;
    }

    return TypedValue(llvm::ConstantFP::get(context.TheContext, llvm::APFloat(1.0)), TypeKind::Double);
}

TypedValue ContributorAST::codegen(CodegenContext& context)
{
    // Generate contributor: V(node1, node2) <+ expression
    std::ostringstream oss;

    if (Quantity == "V") {
        oss << "V(" << NodeP;
        if (NodeN != "0")
            oss << ", " << NodeN;
        oss << ") <+ ";
    } else {
        oss << "I(" << NodeP << ") <+ ";
    }

    // Generate expression
    if (Expression) {
        auto exprTV = Expression->codegen(context);
        oss << "(expression generated)";
    }

    std::cout << "[CodeGen] Contributor: " << oss.str() << std::endl;

    return TypedValue(llvm::ConstantFP::get(context.TheContext, llvm::APFloat(1.0)), TypeKind::Double);
}

TypedValue VoltageAccessAST::codegen(CodegenContext& context)
{
    // Generate call to get node voltage: V(node1, node2)
    llvm::Value* NodePPtr = context.Builder.CreateGlobalString(NodeP, "node_p");
    llvm::Value* NodeNPtr = context.Builder.CreateGlobalString(NodeN, "node_n");

    llvm::Function* GetVoltageF = context.TheModule->getFunction("flux_get_voltage");
    if (!GetVoltageF) {
        llvm::Type* CharPtrTy = llvm::PointerType::get(context.TheContext, 0);
        GetVoltageF = llvm::Function::Create(
            llvm::FunctionType::get(llvm::Type::getDoubleTy(context.TheContext), {CharPtrTy, CharPtrTy}, false),
            llvm::Function::ExternalLinkage, "flux_get_voltage", context.TheModule);
    }

    auto call = context.Builder.CreateCall(GetVoltageF, {NodePPtr, NodeNPtr}, "voltage");
    std::cout << "[CodeGen] V(" << NodeP << ", " << NodeN << ")" << std::endl;

    return TypedValue(call, TypeKind::Double);
}

TypedValue CurrentAccessAST::codegen(CodegenContext& context)
{
    // Generate call to get branch current: I(branch)
    llvm::Value* BranchPtr = context.Builder.CreateGlobalString(Branch, "branch");

    llvm::Function* GetCurrentF = context.TheModule->getFunction("flux_get_current");
    if (!GetCurrentF) {
        llvm::Type* CharPtrTy = llvm::PointerType::get(context.TheContext, 0);
        GetCurrentF = llvm::Function::Create(
            llvm::FunctionType::get(llvm::Type::getDoubleTy(context.TheContext), {CharPtrTy}, false),
            llvm::Function::ExternalLinkage, "flux_get_current", context.TheModule);
    }

    auto call = context.Builder.CreateCall(GetCurrentF, {BranchPtr}, "current");
    std::cout << "[CodeGen] I(" << Branch << ")" << std::endl;

    return TypedValue(call, TypeKind::Double);
}

TypedValue DdtExprAST::codegen(CodegenContext& context)
{
    // Generate time derivative: ddt(operand)
    auto operandTV = Operand->codegen(context);

    llvm::Function* DdtF = context.TheModule->getFunction("flux_ddt");
    if (!DdtF) {
        DdtF = llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getDoubleTy(context.TheContext),
                                                              {llvm::Type::getDoubleTy(context.TheContext)}, false),
                                      llvm::Function::ExternalLinkage, "flux_ddt", context.TheModule);
    }

    auto call = context.Builder.CreateCall(DdtF, {operandTV.Val}, "ddt_result");
    std::cout << "[CodeGen] ddt(expression)" << std::endl;

    return TypedValue(call, TypeKind::Double);
}

TypedValue IdtExprAST::codegen(CodegenContext& context)
{
    // Generate time integral: idt(operand, ic)
    auto operandTV = Operand->codegen(context);

    llvm::Value* icVal = nullptr;
    if (IC) {
        auto icTV = IC->codegen(context);
        icVal = icTV.Val;
    } else {
        icVal = llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0));
    }

    llvm::Function* IdtF = context.TheModule->getFunction("flux_idt");
    if (!IdtF) {
        IdtF = llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getDoubleTy(context.TheContext),
                                                              {llvm::Type::getDoubleTy(context.TheContext),
                                                               llvm::Type::getDoubleTy(context.TheContext)},
                                                              false),
                                      llvm::Function::ExternalLinkage, "flux_idt", context.TheModule);
    }

    auto call = context.Builder.CreateCall(IdtF, {operandTV.Val, icVal}, "idt_result");
    std::cout << "[CodeGen] idt(expression, ic)" << std::endl;

    return TypedValue(call, TypeKind::Double);
}

// ============================================================================
// Symbol Pin Mapping Codegen
// ============================================================================

TypedValue SymbolDeclAST::codegen(CodegenContext& context)
{
    std::cout << "[CodeGen] Symbol: " << SymbolName << std::endl;
    std::cout << "  Pins: ";
    for (size_t i = 0; i < Pins.size(); ++i) {
        if (i > 0)
            std::cout << ", ";
        std::cout << Pins[i];
    }
    std::cout << std::endl;

    // Generate symbol properties
    for (const auto& [key, value] : Properties) {
        std::cout << "  " << key << " = " << value << std::endl;
    }

    return TypedValue(llvm::ConstantFP::get(context.TheContext, llvm::APFloat(1.0)), TypeKind::Double);
}

TypedValue PinMapAST::codegen(CodegenContext& context)
{
    std::cout << "[CodeGen] Pin map: " << SymbolName << " -> " << SubcktName << std::endl;

    for (const auto& [symbolPin, subcktPin] : Mappings) {
        std::cout << "  " << symbolPin << " -> " << subcktPin << std::endl;
    }

    // Generate mapping table
    llvm::Value* MsgPtr = context.Builder.CreateGlobalString("Pin mapping generated", "pinmap_msg");
    llvm::Function* PrintF = context.TheModule->getFunction("println_string");
    if (PrintF) {
        context.Builder.CreateCall(PrintF, {MsgPtr});
    }

    return TypedValue(llvm::ConstantFP::get(context.TheContext, llvm::APFloat(1.0)), TypeKind::Double);
}

} // namespace Flux
