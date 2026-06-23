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

#ifndef FLUX_ADVANCED_FEATURES_H
#define FLUX_ADVANCED_FEATURES_H

#include "flux/compiler/ast.h"
#include <memory>

namespace Flux {

// Forward declarations for parser
class Parser;

// Parser method declarations
std::unique_ptr<ExprAST> ParsePlotDecl();
std::unique_ptr<ExprAST> ParseBenchmarkDecl();
std::unique_ptr<ExprAST> ParseOptimizeDecl();
std::unique_ptr<ExprAST> ParseSweepDecl();
std::unique_ptr<ExprAST> ParseReportDecl();

} // namespace Flux

#endif // FLUX_ADVANCED_FEATURES_H
