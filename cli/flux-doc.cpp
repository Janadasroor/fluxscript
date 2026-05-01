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

#include <string>

#include <llvm/Support/CommandLine.h>
#include <llvm/Support/InitLLVM.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/raw_ostream.h>

#include "flux/tooling/tooling.h"

namespace {
llvm::cl::OptionCategory FluxDocCategory("flux-doc options");
llvm::cl::opt<std::string> InputFilename(
    llvm::cl::Positional, llvm::cl::desc("<script.flux>"), llvm::cl::Required,
    llvm::cl::cat(FluxDocCategory));
}

int main(int argc, char** argv) {
    llvm::InitLLVM initLLVM(argc, argv);
    llvm::cl::HideUnrelatedOptions(FluxDocCategory);
    llvm::cl::ParseCommandLineOptions(argc, argv, "FluxScript documentation generator\n");

    auto input = llvm::MemoryBuffer::getFile(InputFilename);
    if (!input) {
        llvm::errs() << "Could not read " << InputFilename << ": "
                     << input.getError().message() << "\n";
        return 1;
    }

    llvm::outs() << Flux::Tooling::generateDocsMarkdown(
        std::string(input.get()->getBuffer()), InputFilename);
    return 0;
}
