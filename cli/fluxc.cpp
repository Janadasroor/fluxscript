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

#include <algorithm>
#include <string>

#include <llvm/Support/CommandLine.h>
#include <llvm/Support/InitLLVM.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/raw_ostream.h>

#include "flux/tooling/tooling.h"

namespace {

/// Simple ASCII progress bar that redraws on the same line.
void printProgressBar(const std::string& stage, int current, int total)
{
    if (total <= 0) {
        llvm::outs() << "\r[" << stage << "] ...   ";
        llvm::outs().flush();
        return;
    }
    const int barWidth = 40;
    int done = current * barWidth / std::max(total, 1);
    int remain = barWidth - done;
    int pct = current * 100 / std::max(total, 1);
    llvm::outs() << "\r[" << stage << "] [";
    for (int i = 0; i < done; ++i) llvm::outs() << '=';
    if (done < barWidth) llvm::outs() << '>';
    for (int i = 0; i < remain - (done < barWidth ? 1 : 0); ++i) llvm::outs() << ' ';
    llvm::outs() << "] " << pct << "% (" << current << "/" << total << ")";
    llvm::outs().flush();
    if (current >= total)
        llvm::outs() << "\n";
}

} // namespace

namespace {

llvm::cl::OptionCategory FluxcCategory("fluxc options");

llvm::cl::opt<std::string> InputFilename(llvm::cl::Positional, llvm::cl::desc("<script.flux>"), llvm::cl::Required,
                                         llvm::cl::cat(FluxcCategory));

llvm::cl::opt<std::string> OutputFilename("o", llvm::cl::desc("Output file"), llvm::cl::Required,
                                          llvm::cl::cat(FluxcCategory));

llvm::cl::opt<unsigned> OptLevel(llvm::cl::Prefix, llvm::cl::desc("Optimization level"), llvm::cl::init(2),
                                 llvm::cl::cat(FluxcCategory));

llvm::cl::opt<bool> EmitShared("shared", llvm::cl::desc("Emit a shared library instead of an object file"),
                               llvm::cl::init(false), llvm::cl::cat(FluxcCategory));

llvm::cl::opt<bool> EmitDebug("debug", llvm::cl::desc("Emit DWARF debug information"), llvm::cl::init(false),
                              llvm::cl::cat(FluxcCategory));

llvm::cl::opt<int> Jobs("j", llvm::cl::desc("Number of parallel jobs for codegen (default 1)"),
                         llvm::cl::init(1), llvm::cl::cat(FluxcCategory));

Flux::OptimizationLevel toOptimizationLevel(unsigned level)
{
    switch (level) {
    case 0:
        return Flux::OptimizationLevel::O0;
    case 1:
        return Flux::OptimizationLevel::O1;
    case 2:
        return Flux::OptimizationLevel::O2;
    case 3:
        return Flux::OptimizationLevel::O3;
    default:
        return Flux::OptimizationLevel::O2;
    }
}

} // namespace

int main(int argc, char** argv)
{
    llvm::InitLLVM initLLVM(argc, argv);
    llvm::cl::HideUnrelatedOptions(FluxcCategory);
    llvm::cl::ParseCommandLineOptions(argc, argv, "FluxScript AOT compiler\n");

    auto input = llvm::MemoryBuffer::getFile(InputFilename);
    if (!input) {
        llvm::errs() << "Could not read " << InputFilename << ": " << input.getError().message() << "\n";
        return 1;
    }

    Flux::Tooling::AOTOptions options;
    options.inputName = InputFilename;
    options.moduleName = InputFilename;
    options.outputPath = OutputFilename;
    options.optimizationLevel = toOptimizationLevel(OptLevel);
    options.sharedLibrary = EmitShared;
    options.debugInfo = EmitDebug;
    options.numJobs = (Jobs > 0) ? Jobs : 1;
    options.progressCallback = [](const std::string& stage, int current, int total) -> bool {
        printProgressBar(stage, current, total);
        return true;
    };

    std::string error;
    if (!Flux::Tooling::emitArtifact(std::string(input.get()->getBuffer()), options, &error)) {
        llvm::outs() << "\n";
        llvm::errs() << "fluxc: " << error << "\n";
        return 1;
    }

    llvm::outs() << "Wrote " << OutputFilename << "\n";
    return 0;
}
