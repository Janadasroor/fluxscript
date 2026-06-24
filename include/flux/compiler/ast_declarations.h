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

#ifndef FLUX_COMPILER_AST_DECLARATIONS_H
#define FLUX_COMPILER_AST_DECLARATIONS_H

#include "flux/compiler/ast_core.h"

namespace Flux {

struct LifetimeParam
{
    std::string Name;
    std::vector<std::string> Outlives;

    LifetimeParam() = default;
    explicit LifetimeParam(std::string name) : Name(std::move(name)) {}
    LifetimeParam(std::string name, std::vector<std::string> outlives)
        : Name(std::move(name)), Outlives(std::move(outlives))
    {
    }
};

inline std::vector<std::string> getLifetimeNames(const std::vector<LifetimeParam>& params)
{
    std::vector<std::string> names;
    names.reserve(params.size());
    for (const auto& p : params)
        names.push_back(p.Name);
    return names;
}

class PrototypeAST
{
    std::string Name;
    std::vector<std::pair<std::string, FluxType>> Args; // Name-Type pairs
    FluxType ReturnType;
    int Line = 0;
    bool IsGenerator = false;
    bool IsAsync = false;
    std::vector<std::string> GenericParams;                             // Generic type parameter names (e.g., [T, U])
    std::map<std::string, std::vector<std::string>> GenericParamBounds; // param name → trait bounds
    std::vector<LifetimeParam> LifetimeParams;

public:
    PrototypeAST(const std::string& Name, std::vector<std::pair<std::string, FluxType>> Args,
                 FluxType RetType = FluxType(TypeKind::Double))
        : Name(Name), Args(std::move(Args)), ReturnType(RetType)
    {
    }
    llvm::Function* codegen(CodegenContext& context);
    const std::string& getName() const { return Name; }
    void setName(const std::string& N) { Name = N; }
    const std::vector<std::pair<std::string, FluxType>>& getArgs() const { return Args; }
    std::vector<std::pair<std::string, FluxType>>& getArgs() { return Args; }
    const FluxType& getReturnType() const { return ReturnType; }
    void setReturnType(const FluxType& RT) { ReturnType = RT; }
    void setArgType(size_t idx, const FluxType& T)
    {
        if (idx < Args.size())
            Args[idx].second = T;
    }

    void setLocation(int L) { Line = L; }
    int getLine() const { return Line; }

    void setGenerator(bool G) { IsGenerator = G; }
    bool isGenerator() const { return IsGenerator; }

    void setAsync(bool A) { IsAsync = A; }
    bool isAsync() const { return IsAsync; }

    // Generic type parameter support
    void setGenericParams(const std::vector<std::string>& Params) { GenericParams = Params; }
    const std::vector<std::string>& getGenericParams() const { return GenericParams; }
    bool isGeneric() const { return !GenericParams.empty(); }

    // Lifetime parameter support
    void setLifetimeParams(const std::vector<LifetimeParam>& Params) { LifetimeParams = Params; }
    const std::vector<LifetimeParam>& getLifetimeParams() const { return LifetimeParams; }
    bool hasLifetimeParams() const { return !LifetimeParams.empty(); }

    // Trait bounds on generic params
    void addGenericParamBound(const std::string& paramName, const std::string& traitName)
    {
        GenericParamBounds[paramName].push_back(traitName);
    }
    const std::map<std::string, std::vector<std::string>>& getGenericParamBounds() const { return GenericParamBounds; }

    // Create a concrete (specialized) prototype by substituting generic type params
    std::unique_ptr<PrototypeAST> specialize(const std::map<std::string, FluxType>& typeMap,
                                             const std::string& suffix) const
    {
        auto substitute = [&](const FluxType& T) -> FluxType {
            if (T.isGeneric()) {
                auto it = typeMap.find(T.GenericName);
                if (it != typeMap.end())
                    return it->second;
            }
            return T;
        };

        std::vector<std::pair<std::string, FluxType>> ConcreteArgs;
        ConcreteArgs.reserve(Args.size());
        for (const auto& Arg : Args) {
            ConcreteArgs.push_back({Arg.first, substitute(Arg.second)});
        }

        auto ConcreteProto =
            std::make_unique<PrototypeAST>(Name + suffix, std::move(ConcreteArgs), substitute(ReturnType));
        ConcreteProto->setLocation(Line);
        ConcreteProto->setLifetimeParams(LifetimeParams);
        if (IsGenerator)
            ConcreteProto->setGenerator(true);
        if (IsAsync)
            ConcreteProto->setAsync(true);
        return ConcreteProto;
    }
};

class FunctionAST
{
    std::unique_ptr<PrototypeAST> Proto;
    std::unique_ptr<ExprAST> Body;

public:
    std::vector<std::unique_ptr<EnumDeclAST>> LocalEnums;
    std::vector<std::unique_ptr<StructDeclAST>> LocalAnonStructs;

    FunctionAST(std::unique_ptr<PrototypeAST> Proto, std::unique_ptr<ExprAST> Body)
        : Proto(std::move(Proto)), Body(std::move(Body))
    {
    }
    llvm::Function* codegen(CodegenContext& context);
    llvm::Function* codegenWithProto(CodegenContext& context, PrototypeAST* customProto);
    PrototypeAST* getProto() const { return Proto.get(); }
    const ExprAST* getBody() const { return Body.get(); }
    ExprAST* getBody() { return Body.get(); }
};

// ============================================================================
// Struct Definition AST Node
// ============================================================================

/// StructDeclAST - Represents `struct Name` with zero or more typed fields.
/// Each field is a name/type pair.  The struct gets a unique StructTypeId at
/// parse time (index into CodegenContext::StructTypes).
class StructDeclAST
{
    std::string Name;
    std::string ParentName;
    std::vector<std::pair<std::string, FluxType>> Fields;
    int StructTypeId;
    std::vector<std::string> GenericParams;
    std::vector<LifetimeParam> LifetimeParams;

public:
    bool IsNoCopy = false;

    StructDeclAST(const std::string& Name, std::vector<std::pair<std::string, FluxType>> Fields,
                  const std::string& ParentName = "")
        : Name(Name), ParentName(ParentName), Fields(std::move(Fields)), StructTypeId(-1)
    {
    }

    const std::string& getName() const { return Name; }
    const std::string& getParentName() const { return ParentName; }
    void setParentName(const std::string& parent) { ParentName = parent; }
    const std::vector<std::pair<std::string, FluxType>>& getFields() const { return Fields; }
    std::vector<std::pair<std::string, FluxType>>& getFields() { return Fields; }
    int getStructTypeId() const { return StructTypeId; }
    void setStructTypeId(int id) { StructTypeId = id; }
    void setGenericParams(const std::vector<std::string>& Params) { GenericParams = Params; }
    const std::vector<std::string>& getGenericParams() const { return GenericParams; }
    bool isGeneric() const { return !GenericParams.empty(); }
    void setLifetimeParams(const std::vector<LifetimeParam>& Params) { LifetimeParams = Params; }
    const std::vector<LifetimeParam>& getLifetimeParams() const { return LifetimeParams; }
    bool hasLifetimeParams() const { return !LifetimeParams.empty(); }
    void codegen(CodegenContext& context);
};

/// StructConstructExprAST - Represents `TypeName { name: expr, ... }`.
/// Fields can be in any order; missing fields get a default (zero-initialized) value.
class StructConstructExprAST : public ExprAST
{
    std::string StructName;
    int StructTypeId;
    std::map<std::string, std::unique_ptr<ExprAST>> FieldValues;
    std::vector<FluxType> GenericTypeArgs;

public:
    StructConstructExprAST(const std::string& Name, int TypeId, std::map<std::string, std::unique_ptr<ExprAST>> Fields)
        : StructName(Name), StructTypeId(TypeId), FieldValues(std::move(Fields))
    {
    }

    StructConstructExprAST(const std::string& Name, int TypeId, std::map<std::string, std::unique_ptr<ExprAST>> Fields,
                           std::vector<FluxType> GenericTypeArgs)
        : StructName(Name), StructTypeId(TypeId), FieldValues(std::move(Fields)),
          GenericTypeArgs(std::move(GenericTypeArgs))
    {
    }

    TypedValue codegen(CodegenContext& context) override;
    const std::string& getStructName() const { return StructName; }
    int getStructTypeId() const { return StructTypeId; }
    const std::vector<FluxType>& getGenericTypeArgs() const { return GenericTypeArgs; }
    bool hasGenericTypeArgs() const { return !GenericTypeArgs.empty(); }
    bool containsYield() const override
    {
        for (auto& [_, val] : FieldValues)
            if (val->containsYield())
                return true;
        return false;
    }
};

// ============================================================================
// Enum Definition AST Node
// ============================================================================

/// EnumDeclAST - Represents `enum Name` with zero or more variants.
/// Each variant is a simple name (discriminant = index).
/// Variants may carry a payload type: VariantName(Type)
class EnumDeclAST
{
    std::string Name;
    std::vector<std::string> Variants;
    std::vector<FluxType> VariantPayloads;
    int EnumTypeId;
    std::vector<std::string> GenericParams;

public:
    bool IsNoCopy = false;

    EnumDeclAST(const std::string& Name, std::vector<std::string> Variants, std::vector<FluxType> VariantPayloads = {})
        : Name(Name), Variants(std::move(Variants)), VariantPayloads(std::move(VariantPayloads)), EnumTypeId(-1)
    {
    }

    const std::string& getName() const { return Name; }
    const std::vector<std::string>& getVariants() const { return Variants; }
    const std::vector<FluxType>& getVariantPayloads() const { return VariantPayloads; }
    int getEnumTypeId() const { return EnumTypeId; }
    void setEnumTypeId(int id) { EnumTypeId = id; }
    void setGenericParams(const std::vector<std::string>& Params) { GenericParams = Params; }
    const std::vector<std::string>& getGenericParams() const { return GenericParams; }
    bool isGeneric() const { return !GenericParams.empty(); }
    void codegen(CodegenContext& context);
};

/// ImplDeclAST - Represents an impl block:
///   impl TypeName
///       def method1(self, args) -> ReturnType ... end
///       def method2(self, args) -> ReturnType ... end
///   end
/// Or a trait impl:
///   impl TraitName for TypeName
///       ...
///   end
class ImplDeclAST
{
    std::string TypeName;
    std::string TraitName; // Empty for inherent impl, set for trait impl
    std::string ParentName;
    std::vector<std::unique_ptr<FunctionAST>> Methods;
    std::vector<LifetimeParam> LifetimeParams;
    std::map<std::string, FluxType> AssociatedTypeMappings;

public:
    ImplDeclAST(const std::string& TypeName, std::vector<std::unique_ptr<FunctionAST>> Methods,
                const std::string& ParentName = "")
        : TypeName(TypeName), TraitName(), ParentName(ParentName), Methods(std::move(Methods))
    {
    }

    const std::string& getTypeName() const { return TypeName; }
    const std::string& getTraitName() const { return TraitName; }
    void setTraitName(const std::string& T) { TraitName = T; }
    bool isTraitImpl() const { return !TraitName.empty(); }
    const std::string& getParentName() const { return ParentName; }
    void setParentName(const std::string& parent) { ParentName = parent; }
    void setLifetimeParams(const std::vector<LifetimeParam>& Params) { LifetimeParams = Params; }
    const std::vector<LifetimeParam>& getLifetimeParams() const { return LifetimeParams; }
    bool hasLifetimeParams() const { return !LifetimeParams.empty(); }
    const std::map<std::string, FluxType>& getAssociatedTypeMappings() const { return AssociatedTypeMappings; }
    void setAssociatedTypeMapping(const std::string& name, const FluxType& type)
    {
        AssociatedTypeMappings[name] = type;
    }
    const std::vector<std::unique_ptr<FunctionAST>>& getMethods() const { return Methods; }
    std::vector<std::unique_ptr<FunctionAST>>& getMethods() { return Methods; }
    void codegen(CodegenContext& context);
};

/// TraitDeclAST - Represents a trait declaration:
///   trait Display
///       def to_string(self) -> String
///   end
class TraitDeclAST
{
public:
    // Method prototypes stored as (name, args, returnType)
    struct MethodProto
    {
        std::string Name;
        std::vector<std::pair<std::string, FluxType>> Args;
        FluxType ReturnType;
    };

private:
    std::string Name;
    std::vector<std::string> SuperTraits;
    std::vector<MethodProto> Methods;
    std::vector<std::string> AssociatedTypes;

public:
    TraitDeclAST(const std::string& Name, std::vector<MethodProto> Methods,
                 const std::vector<std::string>& SuperTraits = {})
        : Name(Name), SuperTraits(SuperTraits), Methods(std::move(Methods))
    {
    }

    const std::string& getName() const { return Name; }
    const std::vector<std::string>& getSuperTraits() const { return SuperTraits; }
    const std::vector<MethodProto>& getMethods() const { return Methods; }
    const std::vector<std::string>& getAssociatedTypes() const { return AssociatedTypes; }
    void setAssociatedTypes(const std::vector<std::string>& types) { AssociatedTypes = types; }
    void codegen(CodegenContext& context);
};

// Debug info helper
void emitLocation(ExprAST* ast, CodegenContext& context);

// ============================================================================

} // namespace Flux

#endif // FLUX_COMPILER_AST_DECLARATIONS_H
