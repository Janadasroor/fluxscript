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

// Module System (import) Tests
// Tests: basic import, alias import, selective import, multiple imports,
//        runtime function shadowing, namespaced variable lookup

#include "flux/compiler/compiler_instance.h"
#include "flux/compiler/lexer.h"
#include "flux/compiler/parser.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

static int g_passed = 0;
static int g_failed = 0;

#define TEST_CASE(name) std::cout << "  " << name << "... "
#define ASSERT_TRUE(cond) do { \
    if (!(cond)) { \
        std::cerr << "FAILED (assertion: " << #cond << ")\n"; \
        g_failed++; \
        return; \
    } \
} while(0)

#define PASS() do { std::cout << " PASSED\n"; g_passed++; } while(0)
#define FAIL(msg) do { std::cerr << "FAILED: " << msg << "\n"; g_failed++; } while(0)

static bool compileAndCheck(const std::string& mainCode, const std::string& inputName = "test.flux") {
    Flux::CompilerOptions opts;
    opts.inputName = inputName;
    opts.moduleName = "test_main";
    Flux::CompilerInstance compiler(opts);
    std::string error;
    auto artifacts = compiler.compileToIR(mainCode, &error);
    if (!artifacts) {
        std::cerr << "  Compile error: " << error << "\n";
        return false;
    }
    return true;
}

void test_basic_import() {
    TEST_CASE("Basic Import");
    std::string code = R"(
import math_utils
def test() math_utils::square(5.0)
)";
    if (compileAndCheck(code)) { PASS(); }
    else { FAIL("basic import compilation failed"); }
}

void test_alias_import() {
    TEST_CASE("Alias Import (as)");
    std::string code = R"(
import math_utils as m
def test() m::square(3.0) + m::cube(2.0)
)";
    if (compileAndCheck(code)) { PASS(); }
    else { FAIL("alias import compilation failed"); }
}

void test_selective_import() {
    TEST_CASE("Selective Import ({symbol})");
    std::string code = R"(
import math_utils {square}
def test() square(11.0)
)";
    if (compileAndCheck(code)) { PASS(); }
    else { FAIL("selective import compilation failed"); }
}

void test_multiple_imports() {
    TEST_CASE("Multiple Imports");
    std::string code = R"(
import math_utils as m
import strings as s
def test() m::square(3.0) + s::greet(0.0)
)";
    if (compileAndCheck(code)) { PASS(); }
    else { FAIL("multiple imports compilation failed"); }
}

void test_runtime_shadowing() {
    TEST_CASE("Runtime Symbol Shadowing");
    // User-defined 'square' should override runtime's 'square' wave function
    std::string code = R"(
def square(x) x * x
def test() square(6.0)
)";
    if (compileAndCheck(code)) { PASS(); }
    else { FAIL("runtime shadowing compilation failed"); }
}

void test_import_llvm_emit() {
    TEST_CASE("Import produces valid LLVM IR");
    std::string code = R"(
import math_utils
def test() math_utils::square(5.0)
)";
    Flux::CompilerOptions opts;
    opts.inputName = "test.flux";
    opts.moduleName = "test_main";
    Flux::CompilerInstance compiler(opts);
    std::string error;
    auto artifacts = compiler.compileToIR(code, &error);
    if (!artifacts) {
        FAIL("LLVM emit failed: " + error);
        return;
    }

    // Verify expected functions exist in module
    bool hasSquare = false, hasTest = false;
    for (auto& F : *artifacts->codegenContext->TheModule) {
        if (!F.isDeclaration()) {
            if (F.getName() == "square") hasSquare = true;
            if (F.getName() == "test") hasTest = true;
        }
    }
    ASSERT_TRUE(hasSquare);
    ASSERT_TRUE(hasTest);
    PASS();
}

int main() {
    std::cout << "\n========================================\n";
    std::cout << "  FluxScript Module System Tests\n";
    std::cout << "========================================\n\n";

    test_basic_import();
    test_alias_import();
    test_selective_import();
    test_multiple_imports();
    test_runtime_shadowing();
    test_import_llvm_emit();

    std::cout << "\n========================================\n";
    std::cout << "  Results: " << g_passed << "/" << (g_passed + g_failed) << " passed\n";
    std::cout << "========================================\n";

    if (g_failed == 0) {
        std::cout << "   ALL MODULE SYSTEM TESTS PASSED\n\n";
        return 0;
    } else {
        std::cout << "   " << g_failed << " TESTS FAILED\n\n";
        return 1;
    }
}
