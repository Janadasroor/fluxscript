/* Copyright 2026 Janada Sroor
 SPDX-License-Identifier: Apache-2.0 */

// VioMATRIXC Integration: A-Device and WAVEFILE code generation

#include "flux/compiler/ast.h"
#include <llvm/IR/Function.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>

namespace Flux {

// ============================================================================
// ADeviceExprAST Static Helpers
// ============================================================================

std::string ADeviceExprAST::deviceTypeToString(ADeviceType type)
{
    switch (type) {
    case ADeviceType::BUF:
        return "BUF";
    case ADeviceType::NOT:
        return "NOT";
    case ADeviceType::AND:
        return "AND";
    case ADeviceType::OR:
        return "OR";
    case ADeviceType::NAND:
        return "NAND";
    case ADeviceType::NOR:
        return "NOR";
    case ADeviceType::XOR:
        return "XOR";
    case ADeviceType::XNOR:
        return "XNOR";
    case ADeviceType::SCHMITT:
        return "SCHMITT";
    case ADeviceType::DFF:
        return "DFF";
    case ADeviceType::JKFF:
        return "JKFF";
    case ADeviceType::SRFF:
        return "SRFF";
    case ADeviceType::DLATCH:
        return "DLATCH";
    case ADeviceType::COUNTER:
        return "COUNTER";
    default:
        return "BUF";
    }
}

std::string ADeviceExprAST::deviceTypeToModelName(ADeviceType type)
{
    switch (type) {
    case ADeviceType::BUF:
        return "d_buffer";
    case ADeviceType::NOT:
        return "d_inverter";
    case ADeviceType::AND:
        return "d_and";
    case ADeviceType::OR:
        return "d_or";
    case ADeviceType::NAND:
        return "d_nand";
    case ADeviceType::NOR:
        return "d_nor";
    case ADeviceType::XOR:
        return "d_xor";
    case ADeviceType::XNOR:
        return "d_xnor";
    case ADeviceType::SCHMITT:
        return "d_buffer";
    case ADeviceType::DFF:
        return "d_dff";
    case ADeviceType::JKFF:
        return "d_jkff";
    case ADeviceType::SRFF:
        return "d_srff";
    case ADeviceType::DLATCH:
        return "d_dlatch";
    case ADeviceType::COUNTER:
        return "d_fdiv";
    default:
        return "d_buffer";
    }
}

int ADeviceExprAST::deviceTypeToInputCount(ADeviceType type)
{
    switch (type) {
    case ADeviceType::BUF:
    case ADeviceType::NOT:
    case ADeviceType::SCHMITT:
        return 1;
    case ADeviceType::AND:
    case ADeviceType::OR:
    case ADeviceType::NAND:
    case ADeviceType::NOR:
    case ADeviceType::XOR:
    case ADeviceType::XNOR:
    case ADeviceType::DLATCH:
        return 2;
    case ADeviceType::DFF:
        return 2; // data, clk
    case ADeviceType::JKFF:
        return 3; // j, k, clk
    case ADeviceType::SRFF:
        return 2; // s, r
    case ADeviceType::COUNTER:
        return 1; // clock
    default:
        return 1;
    }
}

int ADeviceExprAST::deviceTypeToOutputCount(ADeviceType type)
{
    switch (type) {
    case ADeviceType::DFF:
    case ADeviceType::SRFF:
        return 2; // Q and nQ
    default:
        return 1; // Single output
    }
}

// ============================================================================
// ADeviceExprAST Codegen
// ============================================================================

TypedValue ADeviceExprAST::codegen(CodegenContext& context)
{
    // Register A-device with the netlist generator
    llvm::Function* RegisterADevice = context.TheModule->getFunction("flux_register_adevice");
    if (!RegisterADevice) {
        llvm::Type* VoidTy = llvm::Type::getVoidTy(context.TheContext);
        llvm::Type* CharPtrTy = llvm::PointerType::get(context.TheContext, 0);
        llvm::Type* Int32Ty = llvm::Type::getInt32Ty(context.TheContext);
        RegisterADevice =
            llvm::Function::Create(llvm::FunctionType::get(VoidTy, {CharPtrTy, Int32Ty, CharPtrTy, CharPtrTy}, true),
                                   llvm::Function::ExternalLinkage, "flux_register_adevice", context.TheModule);
    }

    // Build input nodes string (comma-separated)
    std::string inputStr;
    for (size_t i = 0; i < InputNodes.size(); i++) {
        if (i > 0)
            inputStr += ",";
        inputStr += InputNodes[i];
    }

    // Build output nodes string (comma-separated)
    std::string outputStr;
    for (size_t i = 0; i < OutputNodes.size(); i++) {
        if (i > 0)
            outputStr += ",";
        outputStr += OutputNodes[i];
    }

    return TypedValue(
        context.Builder.CreateCall(RegisterADevice, {context.Builder.CreateGlobalString(InstanceName),
                                                     context.Builder.getInt32(static_cast<int>(DeviceType)),
                                                     context.Builder.CreateGlobalString(inputStr),
                                                     context.Builder.CreateGlobalString(outputStr)}),
        TypeKind::Void);
}

// ============================================================================
// WaveFileExprAST Codegen
// ============================================================================

TypedValue WaveFileExprAST::codegen(CodegenContext& context)
{
    // Register WAVEFILE source with the netlist generator
    llvm::Function* RegisterWaveFile = context.TheModule->getFunction("flux_register_wavefile");
    if (!RegisterWaveFile) {
        llvm::Type* VoidTy = llvm::Type::getVoidTy(context.TheContext);
        llvm::Type* CharPtrTy = llvm::PointerType::get(context.TheContext, 0);
        llvm::Type* Int32Ty = llvm::Type::getInt32Ty(context.TheContext);
        RegisterWaveFile = llvm::Function::Create(
            llvm::FunctionType::get(VoidTy, {CharPtrTy, CharPtrTy, CharPtrTy, CharPtrTy, Int32Ty}, false),
            llvm::Function::ExternalLinkage, "flux_register_wavefile", context.TheModule);
    }

    return TypedValue(context.Builder.CreateCall(RegisterWaveFile, {context.Builder.CreateGlobalString(InstanceName),
                                                                    context.Builder.CreateGlobalString(PositiveNode),
                                                                    context.Builder.CreateGlobalString(NegativeNode),
                                                                    context.Builder.CreateGlobalString(FilePath),
                                                                    context.Builder.getInt32(Channel)}),
                      TypeKind::Void);
}

} // namespace Flux
