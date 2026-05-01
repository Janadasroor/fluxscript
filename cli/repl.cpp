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

#include <fstream>
#include <iostream>
#include <string>

#include "flux/jit_engine.h"

#ifdef _WIN32
#include <io.h>
#define isatty _isatty
#define fileno _fileno
#else
#include <unistd.h>
#endif

namespace {

void printBanner() {
    std::cout << "FluxScript REPL\n";
    std::cout << "Commands: :help  :quit  :load <file>  :cache on|off\n\n";
}

void printHelp() {
    std::cout << ":help         Show help\n";
    std::cout << ":quit         Exit\n";
    std::cout << ":load <file>  Load and compile a file\n";
    std::cout << ":cache on|off Toggle JIT cache\n";
}

void printResult(const Flux::FluxValue& value) {
    if (std::holds_alternative<double>(value)) {
        std::cout << std::get<double>(value) << "\n";
        return;
    }
    if (std::holds_alternative<int>(value)) {
        std::cout << std::get<int>(value) << "\n";
        return;
    }
    if (std::holds_alternative<std::complex<double>>(value)) {
        const auto complexValue = std::get<std::complex<double>>(value);
        std::cout << complexValue.real() << (complexValue.imag() >= 0 ? "+" : "")
                  << complexValue.imag() << "j\n";
        return;
    }

    const auto matrix = std::get<Flux::MatrixResult>(value);
    std::cout << "<matrix " << matrix.rows << "x" << matrix.cols << ">\n";
}

bool loadFile(const std::string& path, std::string& out) {
    std::ifstream in(path);
    if (!in)
        return false;

    out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return true;
}

} // namespace

int main() {
    auto& engine = Flux::JITEngine::instance();
    engine.initialize();

    const bool interactive = isatty(fileno(stdin));
    if (interactive)
        printBanner();

    std::string line;
    while (true) {
        if (interactive) {
            std::cout << "flux> ";
            std::cout.flush();
        }

        if (!std::getline(std::cin, line))
            break;

        if (line == ":quit" || line == ":exit")
            break;
        if (line == ":help") {
            printHelp();
            continue;
        }
        if (line == ":cache on") {
            engine.setJITCacheEnabled(true);
            std::cout << "cache enabled\n";
            continue;
        }
        if (line == ":cache off") {
            engine.setJITCacheEnabled(false);
            std::cout << "cache disabled\n";
            continue;
        }
        if (line.rfind(":load ", 0) == 0) {
            std::string code;
            const std::string filePath = line.substr(6);
            if (!loadFile(filePath, code)) {
                std::cerr << "could not open " << filePath << "\n";
                continue;
            }

            std::string error;
            if (!engine.compileScript(code, &error)) {
                std::cerr << error << "\n";
                continue;
            }
            std::cout << "loaded " << filePath
                      << " (" << (engine.lastCompileUsedCache() ? "cache hit" : "cache miss") << ")\n";
            continue;
        }

        if (line.empty())
            continue;

        std::string error;
        if (!engine.compileScript(line, &error)) {
            std::cerr << error << "\n";
            continue;
        }

        const auto value = engine.callFunction("__anon_expr", {}, &error);
        if (!error.empty()) {
            std::cerr << error << "\n";
            continue;
        }
        printResult(value);
    }

    engine.finalize();
    return 0;
}
