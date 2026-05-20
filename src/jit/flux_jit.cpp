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

#include "flux/jit/flux_jit.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/CodeGen.h"
#include "llvm/ExecutionEngine/Orc/ThreadSafeModule.h"
#include "llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/ExecutionEngine/Orc/ExecutorProcessControl.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/DynamicLibrary.h"
#include "llvm/Support/Error.h"
#include "llvm/TargetParser/Host.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/ExecutionEngine/Orc/ExecutionUtils.h"
#include <iostream>
#include <cstdio>
#include <sstream>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif
#include <cstdint>

// Complex number helper functions (C linkage for easy JIT binding)
extern "C" {
    double complex_real(double c_real, double c_imag) {
        return c_real;
    }
    double complex_imag(double c_real, double c_imag) {
        return c_imag;
    }
    double complex_make_real(double r, double i) {
        return r;
    }
    double complex_make_imag(double r, double i) {
        return i;
    }
    void println_string(const char* str) {
        printf("%s\n", str);
    }
}

namespace Flux {

namespace {

void logError(llvm::Error Err, llvm::StringRef Prefix) {
    if (!Err)
        return;
    llvm::errs() << Prefix << "\n";
    llvm::logAllUnhandledErrors(std::move(Err), llvm::errs(), "  ");
}

llvm::ExitOnError exitOnErr;

} // namespace

FluxJIT::FluxJIT(OptimizationLevel optLevel)
    : m_dataLayout(""),
      m_optLevel(optLevel) {
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();

    llvm::sys::DynamicLibrary::LoadLibraryPermanently(nullptr);

    auto JTMB = llvm::orc::JITTargetMachineBuilder::detectHost();
    if (!JTMB) {
        logError(JTMB.takeError(), "Failed to detect host");
        return;
    }
    JTMB->setCodeModel(llvm::CodeModel::Large);
    JTMB->setRelocationModel(llvm::Reloc::PIC_);

    // Create a target machine for potential standalone compilation
    auto TM = JTMB->createTargetMachine();
    if (TM) m_targetMachine = TM->release();

    auto JIT = llvm::orc::LLJITBuilder()
        .setJITTargetMachineBuilder(std::move(*JTMB))
        .setObjectLinkingLayerCreator([&](llvm::orc::ExecutionSession &ES) {
            auto Layer = std::make_unique<llvm::orc::ObjectLinkingLayer>(ES, std::make_unique<llvm::jitlink::InProcessMemoryManager>(16384));
            return Layer;
        })
        .create();
    if (!JIT) {
        logError(JIT.takeError(), "Failed to create LLJIT");
        return;
    }
    m_lljit = std::move(*JIT);
    m_dataLayout = m_lljit->getDataLayout();
    m_targetTriple = llvm::sys::getProcessTriple();

    m_passBuilder = std::make_unique<llvm::PassBuilder>();

    llvm::Expected<llvm::orc::JITDylib&> runtimeJDE =
        m_lljit->createJITDylib("flux_runtime");
    if (!runtimeJDE) {
        logError(runtimeJDE.takeError(), "Failed to create runtime JIT dylib");
        return;
    }
    m_runtimeDylib = &runtimeJDE.get();

    auto ProcessSymbols =
        llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(
            m_lljit->getDataLayout().getGlobalPrefix());
    if (!ProcessSymbols) {
        logError(ProcessSymbols.takeError(),
                 "Failed to create current-process symbol generator");
        return;
    }
    m_runtimeDylib->addGenerator(std::move(*ProcessSymbols));

    auto& mainJD = m_lljit->getMainJITDylib();
    mainJD.addToLinkOrder(*m_runtimeDylib);

    registerComplexHelpers();
}

void FluxJIT::registerComplexHelpers() {
    if (!m_lljit || !m_runtimeDylib) return;

    auto registerSym = [&](const std::string& name, void* ptr) {
        llvm::orc::SymbolMap sym;
        sym[m_lljit->mangleAndIntern(name)] =
            { llvm::orc::ExecutorAddr::fromPtr(ptr), llvm::JITSymbolFlags::Exported };
        (void)m_runtimeDylib->define(llvm::orc::absoluteSymbols(std::move(sym)));
    };

    registerSym("complex_real", reinterpret_cast<void*>(&complex_real));
    registerSym("complex_imag", reinterpret_cast<void*>(&complex_imag));
    registerSym("println_string", reinterpret_cast<void*>(&println_string));
}

FluxJIT::~FluxJIT() = default;

void FluxJIT::setOptimizationLevel(OptimizationLevel level) {
    m_optLevel = level;
}

void FluxJIT::setSIMDOptions(const SIMDOptions& options) {
    m_simdOptions = options;
}

SIMDOptions FluxJIT::getSIMDOptions() const {
    return m_simdOptions;
}

void FluxJIT::setTCOOptions(const TCOOptions& options) {
    m_tcoOptions = options;
}

TCOOptions FluxJIT::getTCOOptions() const {
    return m_tcoOptions;
}

void FluxJIT::optimizeModule(llvm::Module* M, OptimizationLevel level) {
    if (!M || level == OptimizationLevel::O0) return;

    size_t totalInsts = 0;
    for (auto& F : *M) {
        for (auto& BB : F) totalInsts += BB.size();
    }
    if (totalInsts > 2000) {
        llvm::errs() << "Module too large for optimization (" << totalInsts
                     << " instrs, skipping).\n";
        return;
    }

    llvm::LoopAnalysisManager LAM;
    llvm::FunctionAnalysisManager FAM;
    llvm::CGSCCAnalysisManager CGAM;
    llvm::ModuleAnalysisManager MAM;

    m_passBuilder->registerModuleAnalyses(MAM);
    m_passBuilder->registerCGSCCAnalyses(CGAM);
    m_passBuilder->registerFunctionAnalyses(FAM);
    m_passBuilder->registerLoopAnalyses(LAM);
    m_passBuilder->crossRegisterProxies(LAM, FAM, CGAM, MAM);

    llvm::ModulePassManager MPM;
    switch (level) {
        case OptimizationLevel::O1:
            MPM = m_passBuilder->buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O1);
            break;
        case OptimizationLevel::O2:
            MPM = m_passBuilder->buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O2);
            break;
        case OptimizationLevel::O3:
            MPM = m_passBuilder->buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O3);
            break;
        case OptimizationLevel::O0:
        default:
            return;
    }

    MPM.run(*M, MAM);
}

void FluxJIT::prepareModule(llvm::Module& M) {
    if (!m_lljit)
        return;
    M.setDataLayout(m_dataLayout);
    M.setTargetTriple(llvm::Triple(m_targetTriple));
    M.setCodeModel(llvm::CodeModel::Large);
    M.setPICLevel(llvm::PICLevel::BigPIC);
}

void FluxJIT::addModule(std::unique_ptr<llvm::Module> M,
                        std::unique_ptr<llvm::LLVMContext> Ctx) {
    if (!m_lljit || !M || !Ctx)
        return;

    prepareModule(*M);

    // ---- Tiered JIT: Save per-function IR as bitcode for recompilation ----
    for (auto& F : *M) {
        if (F.isDeclaration() || !F.hasExternalLinkage() || F.getName().empty())
            continue;
        std::string name = F.getName().str();
        if (name.find("anon_expr") != std::string::npos)
            continue;

        auto PerFnMod = llvm::CloneModule(*M);
        if (!PerFnMod) continue;

        llvm::Function* Target = PerFnMod->getFunction(F.getName());
        if (!Target) continue;

        for (auto& CF : *PerFnMod) {
            if (&CF != Target && !CF.isDeclaration())
                CF.deleteBody();
        }

        std::string bc;
        llvm::raw_string_ostream OS(bc);
        llvm::WriteBitcodeToFile(*PerFnMod, OS);
        OS.flush();

        SavedModule saved;
        saved.functionNames.push_back(std::move(name));
        saved.bitcode = std::move(bc);
        m_savedModules.push_back(std::move(saved));
    }

    if (llvm::verifyModule(*M, &llvm::errs())) {
        llvm::errs() << "Refusing to add invalid LLVM module to JIT\n";
        return;
    }

    // Collect user function names before moving the module
    std::vector<std::string> userFns;
    for (auto& F : *M) {
        if (!F.isDeclaration() && F.hasExternalLinkage() && !F.getName().empty()) {
            std::string name = F.getName().str();
            if (name.find("anon_expr") == std::string::npos)
                userFns.push_back(std::move(name));
        }
    }

    // Tier 0: compile at O0 for fast startup
    optimizeModule(M.get(), OptimizationLevel::O0);

    auto TSM = llvm::orc::ThreadSafeModule(std::move(M), std::move(Ctx));
    auto Err = m_lljit->addIRModule(std::move(TSM));
    if (Err) {
        logError(std::move(Err), "Failed to add module to JIT");
        return;
    }

    // Initialize call counters for profiling. Actual compilation is lazy —
    // ORC compiles each function on first call via getPointerToFunction.
    {
        std::lock_guard<std::mutex> lock(m_fnMapMutex);
        for (const auto& name : userFns)
            m_callCounts[name] = 0;
    }
}

void FluxJIT::addObjectFile(std::unique_ptr<llvm::MemoryBuffer> ObjectBuffer) {
    if (!m_lljit || !ObjectBuffer)
        return;
    if (auto Err = m_lljit->addObjectFile(std::move(ObjectBuffer))) {
        logError(std::move(Err), "Failed to add object file to JIT");
    }
}

bool FluxJIT::redirectFunction(const std::string& Name, void* oldAddr, void* newAddr) {
    if (!oldAddr || !newAddr) return false;

    // Relative JMP rel32: E9 <4-byte-little-endian-offset>
    // Offset = destination - (source + instruction_length)
    // instruction_length = 5 (1 byte opcode + 4 bytes offset)
    intptr_t src = reinterpret_cast<intptr_t>(oldAddr);
    intptr_t dst = reinterpret_cast<intptr_t>(newAddr);
    int64_t delta = dst - (src + 5);

    // Check we can reach with a 32-bit signed offset
    if (delta < INT32_MIN || delta > INT32_MAX) {
        llvm::errs() << "[FluxJIT] " << Name << " too far (" << delta
                     << " bytes) for rel32 JMP\n";
        return false;
    }
    int32_t offset = static_cast<int32_t>(delta);

#ifdef _WIN32
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    long pageSize = static_cast<long>(sysInfo.dwPageSize);
    void* pageStart = reinterpret_cast<void*>(
        reinterpret_cast<intptr_t>(oldAddr) & ~(pageSize - 1));
    DWORD oldProtect;
    if (!VirtualProtect(pageStart, pageSize, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        llvm::errs() << "[FluxJIT] VirtualProtect failed for " << Name << "\n";
        return false;
    }
#else
    long pageSize = sysconf(_SC_PAGESIZE);
    if (pageSize <= 0) pageSize = 4096;
    void* pageStart = reinterpret_cast<void*>(
        reinterpret_cast<intptr_t>(oldAddr) & ~(pageSize - 1));
    if (mprotect(pageStart, pageSize, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
        llvm::errs() << "[FluxJIT] mprotect failed for " << Name << "\n";
        return false;
    }
#endif

    // Write JMP rel32 at oldAddr
    auto* code = reinterpret_cast<uint8_t*>(oldAddr);
    code[0] = 0xE9;
    code[1] = static_cast<uint8_t>(offset & 0xFF);
    code[2] = static_cast<uint8_t>((offset >> 8) & 0xFF);
    code[3] = static_cast<uint8_t>((offset >> 16) & 0xFF);
    code[4] = static_cast<uint8_t>((offset >> 24) & 0xFF);

#ifdef _WIN32
    // Restore original protection
    VirtualProtect(pageStart, pageSize, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), oldAddr, 5);
#else
    // Restore protection (the JMP redirect is now live)
    mprotect(pageStart, pageSize, PROT_READ | PROT_EXEC);
    // Flush instruction cache to ensure the JMP is visible
    __builtin___clear_cache(reinterpret_cast<char*>(oldAddr),
        reinterpret_cast<char*>(reinterpret_cast<intptr_t>(oldAddr) + 5));
#endif

    return true;
}

void* FluxJIT::promoteFunction(const std::string& Name, OptimizationLevel targetLevel) {
    {
        std::lock_guard<std::mutex> lock(m_fnMapMutex);
        auto it = m_promotedPtrs.find(Name);
        if (it != m_promotedPtrs.end())
            return it->second;
    }

    // Grab the original compiled address before we overwrite m_functionPtrs
    void* originalAddr = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_fnMapMutex);
        auto it = m_functionPtrs.find(Name);
        if (it != m_functionPtrs.end())
            originalAddr = it->second;
    }

    // Find the saved bitcode for this function
    std::string bitcode;
    {
        std::lock_guard<std::mutex> lock(m_fnMapMutex);
        for (auto& saved : m_savedModules) {
            for (auto& fn : saved.functionNames) {
                if (fn == Name) {
                    bitcode = saved.bitcode;
                    break;
                }
            }
            if (!bitcode.empty()) break;
        }
    }

    if (bitcode.empty()) {
        llvm::errs() << "[FluxJIT] No saved IR for function: " << Name << "\n";
        return nullptr;
    }

    // Parse saved bitcode and add it to the JIT at the target optimization level
    auto ctx = std::make_unique<llvm::LLVMContext>();
    auto buf = llvm::MemoryBuffer::getMemBuffer(bitcode, "saved", false);
    auto mod = llvm::parseBitcodeFile(buf->getMemBufferRef(), *ctx);
    if (!mod) {
        logError(mod.takeError(), "Failed to parse saved bitcode");
        return nullptr;
    }

    auto* fnModule = mod->get();
    auto* func = fnModule->getFunction(Name);
    if (!func) {
        llvm::errs() << "[FluxJIT] Function not found in saved IR: " << Name << "\n";
        return nullptr;
    }

    // Remove all other definitions from the module
    {
        std::vector<llvm::Function*> toRemove;
        for (auto& F : *fnModule) {
            if (&F != func && !F.isDeclaration())
                toRemove.push_back(&F);
        }
        for (auto* F : toRemove)
            F->eraseFromParent();
    }

    fnModule->setCodeModel(llvm::CodeModel::Large);
    fnModule->setPICLevel(llvm::PICLevel::BigPIC);

    optimizeModule(fnModule, targetLevel);

    // Rename to avoid ORC's first-definition-wins
    std::string promotedName = Name + ".__promoted";
    func->setName(promotedName);

    auto TSM = llvm::orc::ThreadSafeModule(std::move(*mod), std::move(ctx));

    if (auto Err = m_lljit->addIRModule(m_lljit->getMainJITDylib(), std::move(TSM))) {
        llvm::handleAllErrors(std::move(Err), [](const llvm::ErrorInfoBase& EI) {
            std::string msg = EI.message();
            if (msg.find("duplicate") == std::string::npos &&
                msg.find("already") == std::string::npos)
                llvm::errs() << "[FluxJIT] addIRModule error: " << msg << "\n";
        });
    }

    // Lookup by promoted name directly (bypasses first-definition-wins on the original)
    auto sym = m_lljit->lookup(promotedName);
    if (!sym) {
        logError(sym.takeError(), "Failed to lookup promoted function");
        return nullptr;
    }

    void* ptr = reinterpret_cast<void*>(sym->getValue());
    {
        std::lock_guard<std::mutex> lock(m_fnMapMutex);
        m_promotedPtrs[Name] = ptr;
        m_functionPtrs[Name] = ptr;
    }

    // Hot-patch original O0 code with a relative JMP to the promoted O3 code.
    // This redirects ALL existing callers (JIT-to-JIT calls that embedded the O0 address).
    if (originalAddr && originalAddr != ptr) {
        if (!redirectFunction(Name, originalAddr, ptr))
            llvm::errs() << "[FluxJIT] Warning: could not redirect existing callers of "
                         << Name << "\n";
    }

    return ptr;
}

void* FluxJIT::getPointerToFunction(const std::string& Name) {
    // Check caches and count lookups
    bool needsLookup = false;
    void* ptr = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_fnMapMutex);
        auto it = m_functionPtrs.find(Name);
        if (it != m_functionPtrs.end()) {
            ptr = it->second;
        } else {
            needsLookup = true;
        }
    }

    if (needsLookup) {
        if (!m_lljit) return nullptr;
        auto ExprSymbol = m_lljit->lookup(Name);
        if (!ExprSymbol) {
            auto Err = ExprSymbol.takeError();
            llvm::errs() << "[FluxJIT] lookup failed: " << Name << "\n";
            llvm::handleAllErrors(std::move(Err), [](const llvm::ErrorInfoBase& EI) {
                llvm::errs() << "  " << EI.message() << "\n";
            });
            return nullptr;
        }
        ptr = reinterpret_cast<void*>(ExprSymbol->getValue());
        std::lock_guard<std::mutex> lock(m_fnMapMutex);
        m_functionPtrs[Name] = ptr;
    }

    // Count this lookup for profiling and auto-promote if threshold exceeded
    auto it = m_callCounts.find(Name);
    if (it != m_callCounts.end()) {
        int count = ++it->second;
        if (m_autoPromoteThreshold > 0 && count >= m_autoPromoteThreshold) {
            bool alreadyPromoted = false;
            {
                std::lock_guard<std::mutex> lock(m_fnMapMutex);
                alreadyPromoted = m_promotedPtrs.find(Name) != m_promotedPtrs.end();
            }
            if (!alreadyPromoted) {
                auto* promoted = promoteFunction(Name, OptimizationLevel::O3);
                if (promoted) {
                    ptr = promoted;
                }
            }
        }
    }

    return ptr;
}

int FluxJIT::getCallCount(const std::string& Name) const {
    auto it = m_callCounts.find(Name);
    return (it != m_callCounts.end()) ? it->second.load() : 0;
}

void FluxJIT::resetCallCounts() {
    for (auto& [name, count] : m_callCounts)
        count = 0;
}

bool FluxJIT::isPromoted(const std::string& Name) const {
    std::lock_guard<std::mutex> lock(m_fnMapMutex);
    return m_promotedPtrs.find(Name) != m_promotedPtrs.end();
}

void FluxJIT::registerFunction(const std::string& Name, void* FuncPtr) {
    if (!m_lljit || !m_runtimeDylib) return;

    auto internedName = m_lljit->getExecutionSession().intern(Name);
    llvm::orc::SymbolMap symMap;
    symMap[internedName] =
        { llvm::orc::ExecutorAddr::fromPtr(FuncPtr), llvm::JITSymbolFlags::Exported };
    if (auto Err = m_runtimeDylib->define(llvm::orc::absoluteSymbols(std::move(symMap)))) {
        llvm::handleAllErrors(std::move(Err), [](const llvm::ErrorInfoBase &EI) {
            std::string msg = EI.message();
            if (msg.find("duplicate definition") == std::string::npos)
                llvm::errs() << "JIT symbol error: " << msg << "\n";
        });
    }
}

} // namespace Flux
