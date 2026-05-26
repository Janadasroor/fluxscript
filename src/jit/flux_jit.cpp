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
#include "llvm/ExecutionEngine/Orc/ExecutorProcessControl.h"
#include "llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h"
#include "llvm/ExecutionEngine/Orc/ThreadSafeModule.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/CodeGen.h"
#include "llvm/Support/DynamicLibrary.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/TargetParser/Host.h"

#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/ExecutionEngine/Orc/ExecutionUtils.h"
#include "llvm/IR/MDBuilder.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Transforms/Scalar/TailRecursionElimination.h"
#include "llvm/Transforms/Utils/Cloning.h"
// raw_fd_ostream is in llvm/Support/raw_ostream.h, included transitively
#include <cstdio>
#include <iostream>
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
double complex_real(double c_real, double c_imag)
{
    return c_real;
}
double complex_imag(double c_real, double c_imag)
{
    return c_imag;
}
double complex_make_real(double r, double i)
{
    return r;
}
double complex_make_imag(double r, double i)
{
    return i;
}
void println_string(const char* str)
{
    printf("%s\n", str);
}
}

namespace Flux {

namespace {

void logError(llvm::Error Err, llvm::StringRef Prefix)
{
    if (!Err)
        return;
    llvm::errs() << Prefix << "\n";
    llvm::logAllUnhandledErrors(std::move(Err), llvm::errs(), "  ");
}

llvm::ExitOnError exitOnErr;

void applyBranchWeights(llvm::Function* F, const FunctionProfile& profile)
{
    // Apply edge count metadata as branch weights on terminators
    for (auto& BB : *F) {
        auto* term = BB.getTerminator();
        if (!term || term->getNumSuccessors() < 2)
            continue;

        uint64_t bbHash = BB.isEntryBlock() ? 0 : reinterpret_cast<uint64_t>(&BB);
        std::vector<uint32_t> weights;
        bool hasWeights = false;

        for (unsigned i = 0; i < term->getNumSuccessors(); ++i) {
            auto* succ = term->getSuccessor(i);
            uint64_t succHash = reinterpret_cast<uint64_t>(succ);
            uint64_t count = 0;
            for (auto& [edge, cnt] : profile.edgeCounts) {
                if (edge.first == bbHash && edge.second == succHash) {
                    count = cnt;
                    break;
                }
            }
            if (count > 0)
                hasWeights = true;
            weights.push_back(static_cast<uint32_t>(std::min<uint64_t>(count, UINT32_MAX)));
        }

        if (hasWeights && !weights.empty()) {
            llvm::MDBuilder MDB(F->getContext());
            term->setMetadata(llvm::LLVMContext::MD_prof, MDB.createBranchWeights(weights));
        }
    }
}

} // namespace

// ============================================================================
// PGOProfiler Implementation
// ============================================================================

void PGOProfiler::recordCall(const std::string& name, double elapsedUs, OptimizationLevel optLevel)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto& profile = m_profiles[name];
    profile.name = name;
    profile.currentOptLevel = optLevel;
    profile.recordCall(elapsedUs);
}

FunctionProfile* PGOProfiler::getProfile(const std::string& name)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_profiles.find(name);
    return it != m_profiles.end() ? &it->second : nullptr;
}

const FunctionProfile* PGOProfiler::getProfile(const std::string& name) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_profiles.find(name);
    return it != m_profiles.end() ? &it->second : nullptr;
}

std::unordered_map<std::string, FunctionProfile> PGOProfiler::getAllProfiles() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_profiles;
}

PGOProfiler::Hotness PGOProfiler::classifyFunction(const std::string& name) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_profiles.find(name);
    if (it == m_profiles.end())
        return Hotness::Cold;

    const auto& p = it->second;
    if (p.totalCalls < 5)
        return Hotness::Cold;
    if (p.totalCalls < 50)
        return Hotness::Warm;
    if (p.totalCalls < 500)
        return Hotness::Hot;
    return Hotness::VeryHot;
}

OptimizationLevel PGOProfiler::suggestOptLevel(const std::string& name) const
{
    Hotness h = classifyFunction(name);
    switch (h) {
    case Hotness::Cold:
        return OptimizationLevel::O0;
    case Hotness::Warm:
        return OptimizationLevel::O1;
    case Hotness::Hot:
        return OptimizationLevel::O2;
    case Hotness::VeryHot:
        return OptimizationLevel::O3;
    }
    return OptimizationLevel::O2;
}

void PGOProfiler::annotateFunction(llvm::Function* F, const FunctionProfile& profile)
{
    if (!F)
        return;
    applyBranchWeights(F, profile);
}

void PGOProfiler::reset()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_profiles.clear();
}

bool PGOProfiler::exportProfiles(const std::string& filepath) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::error_code EC;
    llvm::raw_fd_ostream OS(filepath, EC);
    if (EC)
        return false;

    for (const auto& [name, profile] : m_profiles) {
        OS << "FUNCTION: " << name << "\n";
        OS << "  calls: " << profile.totalCalls << "\n";
        OS << "  total_time_us: " << profile.totalCpuTimeUs << "\n";
        OS << "  avg_time_us: " << profile.averageTimeUs() << "\n";
        OS << "  best_opt: " << static_cast<int>(profile.bestOptLevel) << "\n";
    }
    return true;
}

bool PGOProfiler::importProfiles(const std::string& filepath)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> buf = llvm::MemoryBuffer::getFile(filepath);
    if (!buf)
        return false;

    llvm::StringRef content = buf->get()->getBuffer();
    // Simple line-based parser (extend as needed)
    return !content.empty();
}

// ============================================================================
// KernelAutoTuner Implementation
// ============================================================================

BenchmarkResult KernelAutoTuner::benchmarkFunction(const std::string& name, void* funcPtr, OptimizationLevel level,
                                                   int numSamples)
{
    BenchmarkResult result;
    result.functionName = name;
    result.optLevel = level;
    result.samples = numSamples;
    result.valid = false;

    if (!funcPtr)
        return result;

    using Func = double (*)(double);
    auto fn = reinterpret_cast<Func>(funcPtr);

    // Warmup
    volatile double sink = 0.0;
    for (int i = 0; i < 10; i++)
        sink += fn(static_cast<double>(i));

    double total = 0.0;
    double minVal = 1e18;
    double maxVal = 0.0;

    for (int i = 0; i < numSamples; i++) {
        auto start = std::chrono::steady_clock::now();
        sink += fn(static_cast<double>(i));
        auto end = std::chrono::steady_clock::now();
        double us = std::chrono::duration<double, std::micro>(end - start).count();
        total += us;
        if (us < minVal)
            minVal = us;
        if (us > maxVal)
            maxVal = us;
    }

    result.avgTimeUs = total / numSamples;
    result.minTimeUs = minVal;
    result.maxTimeUs = maxVal;
    result.valid = true;

    (void)sink;
    return result;
}

OptimizationLevel KernelAutoTuner::findBestOptLevel(const std::string& name,
                                                    const std::vector<BenchmarkResult>& results)
{
    OptimizationLevel best = OptimizationLevel::O0;
    double bestTime = 1e18;

    for (const auto& r : results) {
        if (r.valid && r.avgTimeUs < bestTime) {
            bestTime = r.avgTimeUs;
            best = r.optLevel;
        }
    }
    return best;
}

KernelAutoTuner::TuneResult KernelAutoTuner::autoTuneFunction(const std::string& name,
                                                              const std::vector<void*>& optFuncPtrs)
{
    TuneResult result;
    result.bestLevel = OptimizationLevel::O0;

    std::vector<BenchmarkResult> benchmarks;
    for (size_t i = 0; i < optFuncPtrs.size(); i++) {
        if (!optFuncPtrs[i])
            continue;
        auto level = static_cast<OptimizationLevel>(i);
        auto br = benchmarkFunction(name, optFuncPtrs[i], level, m_numSamples);
        benchmarks.push_back(br);
    }

    if (benchmarks.empty())
        return result;

    result.bestLevel = findBestOptLevel(name, benchmarks);
    result.benchmarks = benchmarks;
    if (static_cast<size_t>(result.bestLevel) < optFuncPtrs.size())
        result.funcPtr = optFuncPtrs[static_cast<size_t>(result.bestLevel)];

    return result;
}

// ============================================================================
// FluxJIT Implementation
// ============================================================================

FluxJIT::FluxJIT(OptimizationLevel optLevel) : m_dataLayout(""), m_optLevel(optLevel)
{
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
    if (TM)
        m_targetMachine = std::move(*TM);

    auto JIT = llvm::orc::LLJITBuilder()
                   .setJITTargetMachineBuilder(std::move(*JTMB))
                   .setObjectLinkingLayerCreator([&](llvm::orc::ExecutionSession& ES) {
                       auto Layer = std::make_unique<llvm::orc::ObjectLinkingLayer>(
                           ES, std::make_unique<llvm::jitlink::InProcessMemoryManager>(16384));
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

    llvm::Expected<llvm::orc::JITDylib&> runtimeJDE = m_lljit->createJITDylib("flux_runtime");
    if (!runtimeJDE) {
        logError(runtimeJDE.takeError(), "Failed to create runtime JIT dylib");
        return;
    }
    m_runtimeDylib = &runtimeJDE.get();

    auto ProcessSymbols =
        llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(m_lljit->getDataLayout().getGlobalPrefix());
    if (!ProcessSymbols) {
        logError(ProcessSymbols.takeError(), "Failed to create current-process symbol generator");
        return;
    }
    m_runtimeDylib->addGenerator(std::move(*ProcessSymbols));

    auto& mainJD = m_lljit->getMainJITDylib();
    mainJD.addToLinkOrder(*m_runtimeDylib);

    registerComplexHelpers();
}

void FluxJIT::registerComplexHelpers()
{
    if (!m_lljit || !m_runtimeDylib)
        return;

    auto registerSym = [&](const std::string& name, void* ptr) {
        llvm::orc::SymbolMap sym;
        sym[m_lljit->mangleAndIntern(name)] = {llvm::orc::ExecutorAddr::fromPtr(ptr), llvm::JITSymbolFlags::Exported};
        (void)m_runtimeDylib->define(llvm::orc::absoluteSymbols(std::move(sym)));
    };

    registerSym("complex_real", reinterpret_cast<void*>(&complex_real));
    registerSym("complex_imag", reinterpret_cast<void*>(&complex_imag));
    registerSym("println_string", reinterpret_cast<void*>(&println_string));
}

FluxJIT::~FluxJIT() = default;

void FluxJIT::setOptimizationLevel(OptimizationLevel level)
{
    m_optLevel = level;
}

void FluxJIT::setSIMDOptions(const SIMDOptions& options)
{
    m_simdOptions = options;
}

SIMDOptions FluxJIT::getSIMDOptions() const
{
    return m_simdOptions;
}

void FluxJIT::setTCOOptions(const TCOOptions& options)
{
    m_tcoOptions = options;
}

TCOOptions FluxJIT::getTCOOptions() const
{
    return m_tcoOptions;
}

void FluxJIT::applySIMDToPassBuilder(llvm::ModulePassManager& MPM, llvm::FunctionPassManager& FPM)
{
    // No-op in the module/function pass managers — SIMD flags are set globally
    // on the PassBuilder itself via pipeline tuning.
    (void)MPM;
    (void)FPM;
}

void FluxJIT::optimizeModule(llvm::Module* M, OptimizationLevel level)
{
    if (!M || level == OptimizationLevel::O0)
        return;

    // Bump the instruction limit from 2000 to 50000 — small modules don't need
    // skipping, and real-world engineering functions can easily exceed 2000.
    size_t totalInsts = 0;
    for (auto& F : *M) {
        for (auto& BB : F)
            totalInsts += BB.size();
    }
    if (totalInsts > 50000) {
        llvm::errs() << "[FluxJIT] Large module (" << totalInsts
                     << " instrs) — skipping full optimization, using O1 instead.\n";
        level = OptimizationLevel::O1;
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

    // Map our enum to LLVM's optimization level
    llvm::OptimizationLevel LLVMLevel = llvm::OptimizationLevel::O2;
    switch (level) {
    case OptimizationLevel::O1:
        LLVMLevel = llvm::OptimizationLevel::O1;
        break;
    case OptimizationLevel::O2:
        LLVMLevel = llvm::OptimizationLevel::O2;
        break;
    case OptimizationLevel::O3:
        LLVMLevel = llvm::OptimizationLevel::O3;
        break;
    default:
        break;
    }

    // In LLVM 21+, SIMD options are handled via target machine features
    // rather than PipelineTuningOptions. We set target-features attributes
    // on the module to influence the vectorizer.
    if (!m_simdOptions.enableLoopVectorization || !m_simdOptions.enableSLPVectorization) {
        llvm::AttrBuilder AB(M->getContext());
        std::string features;
        if (!m_simdOptions.enableLoopVectorization)
            features += ",-loop-vectorize";
        if (!m_simdOptions.enableSLPVectorization)
            features += ",-slp-vectorize";
        AB.addAttribute("target-features", features);
        for (auto& F : *M)
            F.addFnAttrs(AB);
    }

    // Build the LLVM optimization pipeline
    llvm::ModulePassManager MPM = m_passBuilder->buildPerModuleDefaultPipeline(LLVMLevel);

    // If TCO is enabled, add a tail-call-elimination pass explicitly
    if (m_tcoOptions.enableTailCallElimination) {
#if LLVM_VERSION_MAJOR >= 17
        MPM.addPass(llvm::createModuleToFunctionPassAdaptor(llvm::TailCallElimPass()));
#else
        MPM.addPass(llvm::createModuleToFunctionPassAdaptor(llvm::createTailCallEliminationPass()));
#endif
    }

    MPM.run(*M, MAM);
}

void FluxJIT::prepareModule(llvm::Module& M)
{
    if (!m_lljit)
        return;
    M.setDataLayout(m_dataLayout);
    M.setTargetTriple(llvm::Triple(m_targetTriple));
    M.setCodeModel(llvm::CodeModel::Large);
    M.setPICLevel(llvm::PICLevel::BigPIC);
}

void FluxJIT::addModule(std::unique_ptr<llvm::Module> M, std::unique_ptr<llvm::LLVMContext> Ctx)
{
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
        if (!PerFnMod)
            continue;

        llvm::Function* Target = PerFnMod->getFunction(F.getName());
        if (!Target)
            continue;

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

    // Initial compilation at O0 for fast startup and safe symbol resolution.
    // The tiered-JIT system will promote hot functions to higher levels
    // (O2/O3) via promoteFunction() when call-count thresholds are exceeded.
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

void FluxJIT::addObjectFile(std::unique_ptr<llvm::MemoryBuffer> ObjectBuffer)
{
    if (!m_lljit || !ObjectBuffer)
        return;
    if (auto Err = m_lljit->addObjectFile(std::move(ObjectBuffer))) {
        logError(std::move(Err), "Failed to add object file to JIT");
    }
}

bool FluxJIT::redirectFunction(const std::string& Name, void* oldAddr, void* newAddr)
{
    if (!oldAddr || !newAddr)
        return false;

    // Relative JMP rel32: E9 <4-byte-little-endian-offset>
    // Offset = destination - (source + instruction_length)
    // instruction_length = 5 (1 byte opcode + 4 bytes offset)
    intptr_t src = reinterpret_cast<intptr_t>(oldAddr);
    intptr_t dst = reinterpret_cast<intptr_t>(newAddr);
    int64_t delta = dst - (src + 5);

    // Check we can reach with a 32-bit signed offset
    if (delta < INT32_MIN || delta > INT32_MAX) {
        llvm::errs() << "[FluxJIT] " << Name << " too far (" << delta << " bytes) for rel32 JMP\n";
        return false;
    }
    int32_t offset = static_cast<int32_t>(delta);

#ifdef _WIN32
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    long pageSize = static_cast<long>(sysInfo.dwPageSize);
    void* pageStart = reinterpret_cast<void*>(reinterpret_cast<intptr_t>(oldAddr) & ~(pageSize - 1));
    DWORD oldProtect;
    if (!VirtualProtect(pageStart, pageSize, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        llvm::errs() << "[FluxJIT] VirtualProtect failed for " << Name << "\n";
        return false;
    }
#else
    long pageSize = sysconf(_SC_PAGESIZE);
    if (pageSize <= 0)
        pageSize = 4096;
    void* pageStart = reinterpret_cast<void*>(reinterpret_cast<intptr_t>(oldAddr) & ~(pageSize - 1));
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

void* FluxJIT::promoteFunction(const std::string& Name, OptimizationLevel targetLevel)
{
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
            if (!bitcode.empty())
                break;
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
            if (msg.find("duplicate") == std::string::npos && msg.find("already") == std::string::npos)
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
            llvm::errs() << "[FluxJIT] Warning: could not redirect existing callers of " << Name << "\n";
    }

    return ptr;
}

void* FluxJIT::getPointerToFunction(const std::string& Name)
{
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
        if (!m_lljit)
            return nullptr;
        auto ExprSymbol = m_lljit->lookup(Name);
        if (!ExprSymbol) {
            llvm::consumeError(ExprSymbol.takeError());
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

        // PGO: record cold start for profiling
        if (m_profilingEnabled) {
            m_pgoProfiler.recordCall(Name, 0.0, m_optLevel);
        }

        if (m_autoPromoteThreshold > 0 && count >= m_autoPromoteThreshold) {
            bool alreadyPromoted = false;
            {
                std::lock_guard<std::mutex> lock(m_fnMapMutex);
                alreadyPromoted = m_promotedPtrs.find(Name) != m_promotedPtrs.end();
            }
            if (!alreadyPromoted) {
                // Use PGO to suggest the target optimization level
                OptimizationLevel targetLevel = OptimizationLevel::O3;
                if (m_profilingEnabled) {
                    auto hotness = m_pgoProfiler.classifyFunction(Name);
                    switch (hotness) {
                    case PGOProfiler::Hotness::Cold:
                        targetLevel = OptimizationLevel::O1;
                        break;
                    case PGOProfiler::Hotness::Warm:
                        targetLevel = OptimizationLevel::O2;
                        break;
                    case PGOProfiler::Hotness::Hot:
                        targetLevel = OptimizationLevel::O3;
                        break;
                    case PGOProfiler::Hotness::VeryHot:
                        targetLevel = OptimizationLevel::O3;
                        break;
                    }
                }
                auto* promoted = promoteFunction(Name, targetLevel);
                if (promoted) {
                    ptr = promoted;
                    // Benchmark and auto-tune if profiling is enabled
                    if (m_profilingEnabled) {
                        auto* profile = m_pgoProfiler.getProfile(Name);
                        if (profile) {
                            profile->bestOptLevel = targetLevel;
                        }
                    }
                }
            }
        }
    }

    return ptr;
}

int FluxJIT::getCallCount(const std::string& Name) const
{
    auto it = m_callCounts.find(Name);
    return (it != m_callCounts.end()) ? it->second.load() : 0;
}

void FluxJIT::resetCallCounts()
{
    for (auto& [name, count] : m_callCounts)
        count = 0;
}

bool FluxJIT::isPromoted(const std::string& Name) const
{
    std::lock_guard<std::mutex> lock(m_fnMapMutex);
    return m_promotedPtrs.find(Name) != m_promotedPtrs.end();
}

void FluxJIT::registerFunction(const std::string& Name, void* FuncPtr)
{
    if (!m_lljit || !m_runtimeDylib)
        return;

    // Register in the JITDylib for ORC internal symbol resolution.
    auto& ES = m_lljit->getExecutionSession();
    auto internedName = m_lljit->mangleAndIntern(Name);
    llvm::orc::SymbolMap symMap;
    symMap[internedName] = {llvm::orc::ExecutorAddr::fromPtr(FuncPtr), llvm::JITSymbolFlags::Exported};

    (void)m_runtimeDylib->define(
        llvm::orc::absoluteSymbols(llvm::orc::SymbolMap({{internedName, symMap[internedName]}})));

    auto& mainJD = m_lljit->getMainJITDylib();
    if (auto Err = mainJD.define(llvm::orc::absoluteSymbols(std::move(symMap)))) {
        llvm::handleAllErrors(std::move(Err), [](const llvm::ErrorInfoBase& EI) {
            std::string msg = EI.message();
            if (msg.find("duplicate definition") == std::string::npos)
                llvm::errs() << "JIT symbol error: " << msg << "\n";
        });
    }

    // Also add to the global process symbol table so the fallback
    // DynamicLibrarySearchGenerator can find it via dlsym.
    // This is essential on macOS where two-level namespace linking
    // can hide symbols from libFluxScript.dylib from dlsym(nullptr,...).
    llvm::sys::DynamicLibrary::AddSymbol(Name, FuncPtr);
}

int FluxJIT::autoTuneHotFunctions()
{
    if (!m_profilingEnabled) {
        llvm::errs() << "[FluxJIT] Cannot auto-tune: profiling is disabled. "
                     << "Call setProfilingEnabled(true) first.\n";
        return 0;
    }

    auto profiles = m_pgoProfiler.getAllProfiles();
    int tuned = 0;

    for (auto& [name, profile] : profiles) {
        // Only tune functions that have been promoted (have a O3 version)
        bool isPromoted = false;
        {
            std::lock_guard<std::mutex> lock(m_fnMapMutex);
            isPromoted = m_promotedPtrs.find(name) != m_promotedPtrs.end();
        }
        if (!isPromoted)
            continue;

        void* originalPtr = nullptr;
        {
            std::lock_guard<std::mutex> lock(m_fnMapMutex);
            auto it = m_functionPtrs.find(name);
            if (it != m_functionPtrs.end())
                originalPtr = it->second;
        }
        if (!originalPtr)
            continue;

        // Benchmark the current (promoted) version
        auto benchResult = m_autoTuner.benchmarkFunction(name, originalPtr, profile.currentOptLevel, 50);

        // Gather available opt-level pointers
        std::vector<void*> optPtrs(4, nullptr);
        optPtrs[static_cast<size_t>(OptimizationLevel::O0)] = originalPtr;
        // Try to get O2 and O3 versions
        if (profile.currentOptLevel != OptimizationLevel::O2) {
            auto* p = promoteFunction(name, OptimizationLevel::O2);
            optPtrs[static_cast<size_t>(OptimizationLevel::O2)] = p;
        }
        if (profile.currentOptLevel != OptimizationLevel::O3) {
            auto* p = promoteFunction(name, OptimizationLevel::O3);
            optPtrs[static_cast<size_t>(OptimizationLevel::O3)] = p;
        }

        // Run auto-tuner
        auto tuneResult = m_autoTuner.autoTuneFunction(name, optPtrs);

        if (tuneResult.bestLevel != profile.currentOptLevel) {
            std::lock_guard<std::mutex> lock(m_fnMapMutex);
            profile.bestOptLevel = tuneResult.bestLevel;
            if (tuneResult.funcPtr) {
                m_functionPtrs[name] = tuneResult.funcPtr;
                m_promotedPtrs[name] = tuneResult.funcPtr;
            }
            tuned++;

            llvm::outs() << "[FluxJIT] Auto-tuned '" << name << "' from " << static_cast<int>(profile.currentOptLevel)
                         << " -> " << static_cast<int>(tuneResult.bestLevel) << "\n";
        }
    }

    return tuned;
}

} // namespace Flux
