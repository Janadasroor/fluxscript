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

#ifndef FLUX_SCHEMATIC_AST_H
#define FLUX_SCHEMATIC_AST_H

#include "flux/compiler/ast.h"
#include <map>
#include <vector>
#include <string>

namespace Flux {

// Schematic AST nodes for code-to-hardware pipeline

// Component in schematic
class SchematicComponent {
public:
    std::string Name;        // R1, C1, U1, etc.
    std::string Type;        // R, C, L, D, Q, U, etc.
    std::string Value;       // 1k, 100n, etc.
    std::string Footprint;   // 0402, SOT-23, etc.
    double X, Y;            // Position on schematic
    std::map<std::string, std::string> Properties;

    SchematicComponent(const std::string& name, const std::string& type, const std::string& value)
        : Name(name), Type(type), Value(value), X(0), Y(0) {}
};

// Connection between pins
class SchematicConnection {
public:
    std::string FromComponent;
    std::string FromPin;
    std::string ToComponent;
    std::string ToPin;
    std::string NetName;

    SchematicConnection(const std::string& from_comp, const std::string& from_pin,
                       const std::string& to_comp, const std::string& to_pin)
        : FromComponent(from_comp), FromPin(from_pin),
          ToComponent(to_comp), ToPin(to_pin) {}
};

// Net (named connection)
class SchematicNet {
public:
    std::string Name;
    std::vector<std::pair<std::string, std::string>> Connections; // (component, pin)

    SchematicNet(const std::string& name) : Name(name) {}
};

// Port (I/O point)
class SchematicPort {
public:
    std::string Name;
    std::string Type;  // input, output, bidirectional, power
    std::string NetName;

    SchematicPort(const std::string& name, const std::string& type)
        : Name(name), Type(type) {}
};

// Schematic block
class SchematicExprAST : public ExprAST {
    std::string Name;
    std::vector<SchematicComponent> Components;
    std::vector<SchematicConnection> Connections;
    std::vector<SchematicNet> Nets;
    std::vector<SchematicPort> Ports;
    std::map<std::string, std::string> Properties;

public:
    SchematicExprAST(const std::string& name) : Name(name) {}
    TypedValue codegen(CodegenContext& context) override;

    void addComponent(const SchematicComponent& comp) { Components.push_back(comp); }
    void addConnection(const SchematicConnection& conn) { Connections.push_back(conn); }
    void addNet(const SchematicNet& net) { Nets.push_back(net); }
    void addPort(const SchematicPort& port) { Ports.push_back(port); }

    const std::string& getName() const { return Name; }
    const std::vector<SchematicComponent>& getComponents() const { return Components; }
    const std::vector<SchematicConnection>& getConnections() const { return Connections; }
    const std::vector<SchematicPort>& getPorts() const { return Ports; }
};

// Export schematic to KiCad
class ExportSchematicExprAST : public ExprAST {
    std::string SchematicName;
    std::string OutputFile;
    std::string Format;  // kicad, spice, pdf

public:
    ExportSchematicExprAST(const std::string& name, const std::string& output, const std::string& fmt)
        : SchematicName(name), OutputFile(output), Format(fmt) {}
    TypedValue codegen(CodegenContext& context) override;
};

} // namespace Flux

#endif // FLUX_SCHEMATIC_AST_H
