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

// FluxScript Language Server Protocol (LSP) Server
// Provides IDE features: completions, diagnostics, hover, go-to-definition, signature help
#include <iostream>
#include <string>
#include <cstdlib>

#include "flux/tooling/lsp_server.h"

static void printUsage() {
    std::cerr << "Usage: flux-lsp [--stdio]\n";
    std::cerr << "\nFluxScript Language Server\n";
    std::cerr << "\nOptions:\n";
    std::cerr << "  --stdio    Use stdin/stdout for communication (default)\n";
    std::cerr << "\nCapabilities:\n";
    std::cerr << "  - textDocument/completion      Autocomplete with snippets\n";
    std::cerr << "  - textDocument/hover            Hover documentation\n";
    std::cerr << "  - textDocument/definition       Go to definition\n";
    std::cerr << "  - textDocument/signatureHelp    Function signature help\n";
    std::cerr << "  - textDocument/diagnostic       Real-time diagnostics\n";
    std::cerr << "  - textDocument/documentSymbol   Document symbols\n";
    std::cerr << "  - workspace/symbol              Workspace-wide symbol search\n";
}

int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            printUsage();
            return 0;
        } else if (arg == "--version" || arg == "-v") {
            std::cout << "flux-lsp 1.0.0 (LSP 3.17)\n";
            return 0;
        }
    }

    // Create and run the LSP server
    Flux::Tooling::LspServer server;
    return server.run();
}
