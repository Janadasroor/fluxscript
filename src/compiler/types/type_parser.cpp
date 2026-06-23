/* Copyright 2026 Janada Sroor
 SPDX-License-Identifier: Apache-2.0 */

#include "flux/compiler/ast.h"
#include "flux/compiler/parser.h"

namespace Flux {

std::unique_ptr<StructDeclAST> Parser::ParseStructDecl()
{
    getNextToken(); // eat struct

    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected struct name");
        return nullptr;
    }
    std::string Name = m_lexer.IdentifierStr;
    getNextToken();

    // Optional generic params
    std::vector<std::string> GenericParams;
    if (CurTok == '[') {
        GenericParams = ParseGenericParams();
        if (hasError())
            return nullptr;
    }

    // Optional lifetime params: struct Foo<'a, 'b: 'a>
    std::vector<LifetimeParam> LifetimeParams;
    if (CurTok == '<') {
        getNextToken();
        while (CurTok == static_cast<int>(TokenType::tok_lifetime)) {
            std::string lt = m_lexer.IdentifierStr;
            if (!lt.empty() && lt[0] == '\'')
                lt = lt.substr(1);
            std::vector<std::string> outlives;
            getNextToken();
            while (CurTok == static_cast<int>(TokenType::tok_colon)) {
                getNextToken(); // eat :
                if (CurTok == static_cast<int>(TokenType::tok_lifetime)) {
                    std::string bound = m_lexer.IdentifierStr;
                    if (!bound.empty() && bound[0] == '\'')
                        bound = bound.substr(1);
                    outlives.push_back(bound);
                    getNextToken();
                } else {
                    break;
                }
            }
            LifetimeParams.emplace_back(lt, outlives);
            if (CurTok == ',')
                getNextToken();
            else
                break;
        }
        if (CurTok != '>') {
            ReportError("expected '>' to close lifetime parameters in struct declaration");
            return nullptr;
        }
        getNextToken(); // eat >
    }

    // Optional ~Copy annotation (opts out of Copy semantics → move-only)
    bool isNoCopy = false;
    if (CurTok == static_cast<int>(TokenType::tok_bitwise_not)) {
        getNextToken(); // eat ~
        if (CurTok == static_cast<int>(TokenType::tok_identifier) && m_lexer.IdentifierStr == "Copy") {
            isNoCopy = true;
            getNextToken(); // eat Copy
        } else {
            ReportError("expected 'Copy' after '~' in struct declaration");
            return nullptr;
        }
    }

    // Support both brace-delimited { ... } and end-delimited block syntax
    bool useBraceBlock = (CurTok == static_cast<int>(TokenType::tok_lbrace));
    if (useBraceBlock)
        getNextToken(); // eat {

    // Parse fields
    std::vector<std::pair<std::string, FluxType>> Fields;
    while (true) {
        if (useBraceBlock) {
            if (CurTok == static_cast<int>(TokenType::tok_rbrace))
                break;
        } else {
            if (CurTok == static_cast<int>(TokenType::tok_eof))
                break;
            // Allow 'end' as a field name if followed by ':', otherwise it closes the struct
            if (CurTok == static_cast<int>(TokenType::tok_end)) {
                int peek = m_lexer.peekToken();
                if (peek != static_cast<int>(TokenType::tok_colon))
                    break;
            }
        }

        if (CurTok != static_cast<int>(TokenType::tok_identifier) && CurTok != static_cast<int>(TokenType::tok_end)) {
            ReportError("expected field name in struct");
            return nullptr;
        }
        std::string FieldName = m_lexer.IdentifierStr;
        getNextToken();

        if (CurTok != static_cast<int>(TokenType::tok_colon)) {
            ReportError("expected ':' after struct field name");
            return nullptr;
        }
        getNextToken();

        Fields.push_back({FieldName, parseTypeName(GenericParams, LifetimeParams)});

        if (CurTok == ',')
            getNextToken();
    }

    if (useBraceBlock) {
        if (CurTok != static_cast<int>(TokenType::tok_rbrace)) {
            ReportError("expected '}' to close struct");
            return nullptr;
        }
        getNextToken(); // eat }
    } else {
        if (CurTok != static_cast<int>(TokenType::tok_end)) {
            ReportError("expected 'end' to close struct");
            return nullptr;
        }
        getNextToken(); // eat end
    }

    // Register struct type name so parseTypeName can recognize it
    // in subsequent type annotations (e.g. function parameters, nested struct fields).
    m_knownStructTypeNames.insert(Name);

    auto result = std::make_unique<StructDeclAST>(Name, std::move(Fields));
    result->IsNoCopy = isNoCopy;
    if (!GenericParams.empty())
        result->setGenericParams(std::move(GenericParams));
    if (!LifetimeParams.empty())
        result->setLifetimeParams(std::move(LifetimeParams));
    return result;
}

/// Helper: parse a type name from the current token.
/// Handles built-in type keywords (double, int, bool, etc.),
/// capitalized type names (Double, Int, Bool, etc.),
/// unit type names (Voltage, Current, etc.),
/// generic type parameters, and lifetime parameters.
FluxType Parser::parseTypeName(const std::vector<std::string>& genericParams,
                               const std::vector<LifetimeParam>& lifetimeParams)
{
    // Reference type: &T or &mut T or &'a T or &'a mut T
    if (CurTok == static_cast<int>(TokenType::tok_bitwise_and)) {
        getNextToken();
        std::string lifetime;
        bool isMut = false;
        // Check for lifetime annotation: &'a T
        if (CurTok == static_cast<int>(TokenType::tok_lifetime)) {
            lifetime = m_lexer.IdentifierStr;
            // Remove the leading ' from the lifetime string
            if (!lifetime.empty() && lifetime[0] == '\'')
                lifetime = lifetime.substr(1);
            // Validate lifetime is declared (if we are in a context with lifetime params)
            if (!lifetimeParams.empty()) {
                bool found = false;
                for (const auto& lp : lifetimeParams) {
                    if (lp.Name == lifetime) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    ReportError("lifetime '" + lifetime + "' is not declared in this scope");
                    return FluxType(TypeKind::Double);
                }
            }
            getNextToken();
        }
        // Check for mut: &'a mut T or &mut T
        if (CurTok == static_cast<int>(TokenType::tok_identifier) && m_lexer.IdentifierStr == "mut") {
            isMut = true;
            getNextToken();
        }
        FluxType inner = parseTypeName(genericParams, lifetimeParams);
        return FluxType::reference(inner, isMut, lifetime);
    }

    // Trait object type: dyn TraitName
    if (CurTok == static_cast<int>(TokenType::tok_identifier) && m_lexer.IdentifierStr == "dyn") {
        getNextToken(); // eat dyn
        if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
            ReportError("expected trait name after 'dyn'");
            return FluxType(TypeKind::Double);
        }
        std::string traitName = m_lexer.IdentifierStr;
        getNextToken();
        // Look up trait index
        auto traitIdxIt = m_knownTraitNameToIndex.find(traitName);
        if (traitIdxIt != m_knownTraitNameToIndex.end()) {
            return FluxType::traitObject(traitIdxIt->second);
        }
        ReportError("unknown trait '" + traitName + "' in dyn type");
        return FluxType(TypeKind::Double);
    }

    // Keyword tokens — built-in type keywords (matrix, vector, etc.)
    if (CurTok == static_cast<int>(TokenType::tok_type_matrix)) {
        getNextToken();
        return FluxType(TypeKind::Matrix);
    }
    if (CurTok == static_cast<int>(TokenType::tok_type_vector)) {
        getNextToken();
        return FluxType(TypeKind::Vector);
    }
    if (CurTok == static_cast<int>(TokenType::tok_type_float)) {
        getNextToken();
        return FluxType(TypeKind::Float);
    }
    if (CurTok == static_cast<int>(TokenType::tok_type_int)) {
        getNextToken();
        return FluxType(TypeKind::Int);
    }
    if (CurTok == static_cast<int>(TokenType::tok_type_bool)) {
        getNextToken();
        return FluxType(TypeKind::Bool);
    }
    if (CurTok == static_cast<int>(TokenType::tok_type_void)) {
        getNextToken();
        return FluxType(TypeKind::Void);
    }
    if (CurTok == static_cast<int>(TokenType::tok_type_complex)) {
        getNextToken();
        return FluxType(TypeKind::Complex);
    }
    if (CurTok == static_cast<int>(TokenType::tok_type_string)) {
        getNextToken();
        return FluxType(TypeKind::String);
    }
    if (CurTok == static_cast<int>(TokenType::tok_type_double)) {
        getNextToken();
        return FluxType(TypeKind::Double);
    }

    // Identifier — could be built-in type name (capitalized), unit type, or generic param
    if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
        std::string typeName = m_lexer.IdentifierStr;
        getNextToken();

        // Check built-in capitalized names
        if (typeName == "Double" || typeName == "double")
            return FluxType(TypeKind::Double);
        if (typeName == "Float" || typeName == "float")
            return FluxType(TypeKind::Float);
        if (typeName == "Int" || typeName == "int")
            return FluxType(TypeKind::Int);
        if (typeName == "Bool" || typeName == "bool")
            return FluxType(TypeKind::Bool);
        if (typeName == "Void" || typeName == "void")
            return FluxType(TypeKind::Void);
        if (typeName == "Complex" || typeName == "complex")
            return FluxType(TypeKind::Complex);
        if (typeName == "String" || typeName == "string")
            return FluxType(TypeKind::String);
        if (typeName == "Matrix" || typeName == "matrix")
            return FluxType(TypeKind::Matrix);
        if (typeName == "Vector" || typeName == "vector")
            return FluxType(TypeKind::Vector);

        // Check generic type parameter
        for (auto& gp : genericParams) {
            if (gp == typeName)
                return FluxType::generic(typeName);
        }

        // Check unit type names (Voltage, Current, etc.)
        FluxType unitType = FluxType::fromUnitName(typeName);
        if (unitType.Dimensions.mass != 0 || unitType.Dimensions.length != 0 || unitType.Dimensions.time != 0 ||
            unitType.Dimensions.current != 0 || unitType.Dimensions.temperature != 0 ||
            unitType.Dimensions.amount != 0 || unitType.Dimensions.luminous != 0) {
            return unitType;
        }

        // Check user-defined struct types
        if (m_knownStructTypeNames.count(typeName)) {
            FluxType t(TypeKind::UserStruct);
            t.StructTypeId = -1; // resolve at codegen time
            t.StructLLVMType = nullptr;
            t.GenericName = typeName; // carry name for resolution
            if (CurTok == '[')
                t.GenericArgs = ParseGenericTypeArgs();
            return t;
        }

        // Check user-defined enum types
        if (m_knownEnumTypeNames.count(typeName)) {
            FluxType t(TypeKind::UserEnum);
            t.EnumTypeId = -1; // resolve at codegen time
            t.EnumLLVMType = nullptr;
            t.GenericName = typeName; // carry name for resolution
            if (CurTok == '[')
                t.GenericArgs = ParseGenericTypeArgs();
            return t;
        }

        ReportError("unknown type: " + typeName);
        return FluxType(TypeKind::Double);
    }

    ReportError("expected type");
    return FluxType(TypeKind::Double);
}

/// ParseStructConstructExpr — parse:
///   TypeName { name: expr, name: expr }
/// Called after the identifier and optional generic type args have been consumed.
std::unique_ptr<ExprAST> Parser::ParseStructConstructExpr(const std::string& TypeName,
                                                          const std::vector<FluxType>& GenericTypeArgs)
{
    // CurTok should be tok_lbrace already
    int line = m_lexer.getCurrentLine();
    int col = m_lexer.getCurrentColumn();
    getNextToken(); // eat {

    std::map<std::string, std::unique_ptr<ExprAST>> FieldValues;

    while (CurTok != static_cast<int>(TokenType::tok_rbrace) && CurTok != static_cast<int>(TokenType::tok_eof)) {
        if (CurTok != static_cast<int>(TokenType::tok_identifier) && CurTok != static_cast<int>(TokenType::tok_end)) {
            ReportError("expected field name in struct constructor");
            return nullptr;
        }
        std::string FieldName = m_lexer.IdentifierStr;
        getNextToken();

        if (CurTok != static_cast<int>(TokenType::tok_colon)) {
            ReportError("expected ':' after field name in struct constructor");
            return nullptr;
        }
        getNextToken();

        auto Value = ParseExpression();
        if (!Value)
            return nullptr;

        FieldValues[FieldName] = std::move(Value);

        if (CurTok == ',')
            getNextToken();
        else if (CurTok != static_cast<int>(TokenType::tok_rbrace)) {
            ReportError("expected ',' or '}' in struct constructor");
            return nullptr;
        }
    }

    if (CurTok != static_cast<int>(TokenType::tok_rbrace)) {
        ReportError("expected '}' to close struct constructor");
        return nullptr;
    }
    getNextToken(); // eat }

    auto result = std::make_unique<StructConstructExprAST>(TypeName, -1, std::move(FieldValues), GenericTypeArgs);
    result->setLocation(line, col);
    return result;
}

/// ParseEnumDecl — parse:
///   enum Name
///       Variant
///       Variant(Type)
///       Variant { field1: Type1, field2: Type2 }
///   end
/// or:
///   enum Name { Variant, Variant(Type), Variant { field: Type } }
std::unique_ptr<EnumDeclAST> Parser::ParseEnumDecl(std::vector<std::unique_ptr<StructDeclAST>>* anonStructs)
{
    getNextToken(); // eat enum

    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected enum name");
        return nullptr;
    }
    std::string Name = m_lexer.IdentifierStr;
    getNextToken();

    // Optional generic params
    std::vector<std::string> GenericParams;
    if (CurTok == '[') {
        GenericParams = ParseGenericParams();
        if (hasError())
            return nullptr;
    }

    // Optional ~Copy annotation
    bool isNoCopy = false;
    if (CurTok == static_cast<int>(TokenType::tok_bitwise_not)) {
        getNextToken(); // eat ~
        if (CurTok == static_cast<int>(TokenType::tok_identifier) && m_lexer.IdentifierStr == "Copy") {
            isNoCopy = true;
            getNextToken(); // eat Copy
        } else {
            ReportError("expected 'Copy' after '~' in enum declaration");
            return nullptr;
        }
    }

    // Support both brace-delimited { ... } and end-delimited block syntax
    bool useBraceBlock = (CurTok == static_cast<int>(TokenType::tok_lbrace));
    if (useBraceBlock)
        getNextToken(); // eat {

    std::vector<std::string> Variants;
    std::vector<FluxType> Payloads;

    while (true) {
        // Check for block terminator
        if (useBraceBlock) {
            if (CurTok == static_cast<int>(TokenType::tok_rbrace))
                break;
        } else {
            if (CurTok == static_cast<int>(TokenType::tok_end) || CurTok == static_cast<int>(TokenType::tok_eof))
                break;
        }

        if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
            ReportError("expected variant name in enum");
            return nullptr;
        }
        std::string VariantName = m_lexer.IdentifierStr;
        getNextToken();

        FluxType Payload(TypeKind::Void);
        if (CurTok == '(') {
            getNextToken(); // eat (

            // Support named field syntax: Some(value: Double)
            if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
                int peek = m_lexer.peekToken();
                if (peek == static_cast<int>(TokenType::tok_colon)) {
                    // Named field: consume identifier and colon, then parse type
                    std::string fieldName = m_lexer.IdentifierStr;
                    getNextToken(); // eat field name
                    getNextToken(); // eat :

                    FluxType fieldType = parseTypeName(GenericParams);

                    if (CurTok != ')') {
                        ReportError("expected ')' after named field type");
                        return nullptr;
                    }
                    getNextToken(); // eat )

                    // Generate a synthetic struct type for this variant's payload
                    std::string anonName = "__enum_" + Name + "_" + VariantName + "_Fields";
                    m_knownStructTypeNames.insert(anonName);

                    std::vector<std::pair<std::string, FluxType>> fields;
                    fields.push_back({fieldName, fieldType});

                    Payload = FluxType(TypeKind::UserStruct);
                    Payload.StructTypeId = -1;
                    Payload.StructLLVMType = nullptr;
                    Payload.GenericName = anonName;

                    if (anonStructs) {
                        anonStructs->push_back(std::make_unique<StructDeclAST>(anonName, std::move(fields)));
                    }

                    Variants.push_back(VariantName);
                    Payloads.push_back(Payload);
                    if (useBraceBlock && CurTok == ',')
                        getNextToken();
                    continue;
                }
            }

            // Anonymous type: Some(Double)
            Payload = parseTypeName(GenericParams);

            if (CurTok != ')') {
                ReportError("expected ')' after variant type");
                return nullptr;
            }
            getNextToken(); // eat )
        } else if (CurTok == static_cast<int>(TokenType::tok_lbrace)) {
            getNextToken(); // eat {

            std::vector<std::pair<std::string, FluxType>> fields;
            while (CurTok != static_cast<int>(TokenType::tok_rbrace) &&
                   CurTok != static_cast<int>(TokenType::tok_eof)) {
                if (CurTok != static_cast<int>(TokenType::tok_identifier) &&
                    CurTok != static_cast<int>(TokenType::tok_end)) {
                    ReportError("expected field name in enum variant struct");
                    return nullptr;
                }
                std::string fieldName = m_lexer.IdentifierStr;
                getNextToken();

                if (CurTok != static_cast<int>(TokenType::tok_colon)) {
                    ReportError("expected ':' after struct field name");
                    return nullptr;
                }
                getNextToken();

                fields.push_back({fieldName, parseTypeName(GenericParams)});

                if (CurTok == ',')
                    getNextToken();
                else if (CurTok != static_cast<int>(TokenType::tok_rbrace)) {
                    ReportError("expected ',' or '}' in enum variant struct");
                    return nullptr;
                }
            }

            if (CurTok != static_cast<int>(TokenType::tok_rbrace)) {
                ReportError("expected '}' to close enum variant struct");
                return nullptr;
            }
            getNextToken(); // eat }

            // Generate a synthetic struct type for this variant's payload
            std::string anonName = "__enum_" + Name + "_" + VariantName + "_Fields";
            m_knownStructTypeNames.insert(anonName);

            Payload = FluxType(TypeKind::UserStruct);
            Payload.StructTypeId = -1;
            Payload.StructLLVMType = nullptr;
            Payload.GenericName = anonName;

            if (anonStructs) {
                anonStructs->push_back(std::make_unique<StructDeclAST>(anonName, std::move(fields)));
            }
        }

        Variants.push_back(VariantName);
        Payloads.push_back(Payload);

        // In brace block, consume comma separator
        if (useBraceBlock && CurTok == ',')
            getNextToken();
    }

    if (useBraceBlock) {
        if (CurTok != static_cast<int>(TokenType::tok_rbrace)) {
            ReportError("expected '}' to close enum");
            return nullptr;
        }
        getNextToken(); // eat }
    } else {
        if (CurTok != static_cast<int>(TokenType::tok_end)) {
            ReportError("expected 'end' to close enum");
            return nullptr;
        }
        getNextToken(); // eat end
    }

    // Register enum type name so parseTypeName can recognize it
    // in subsequent type annotations (e.g. function parameters, return types).
    m_knownEnumTypeNames.insert(Name);

    auto result = std::make_unique<EnumDeclAST>(Name, std::move(Variants), std::move(Payloads));
    result->IsNoCopy = isNoCopy;
    if (!GenericParams.empty())
        result->setGenericParams(std::move(GenericParams));
    return result;
}

/// ParseTraitDecl — parse:
///   trait Name
///       def method1(self, arg: Type) -> RetType
///       def method2(self) -> RetType
///   end
std::unique_ptr<TraitDeclAST> Parser::ParseTraitDecl()
{
    getNextToken(); // eat trait

    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected trait name after 'trait'");
        return nullptr;
    }
    std::string Name = m_lexer.IdentifierStr;
    getNextToken();

    // Optional super-trait: trait Name: SuperTrait
    std::vector<std::string> SuperTraits;
    if (CurTok == static_cast<int>(TokenType::tok_colon)) {
        getNextToken(); // eat :
        if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
            ReportError("expected super-trait name");
            return nullptr;
        }
        SuperTraits.push_back(m_lexer.IdentifierStr);
        getNextToken();
        while (CurTok == '+') {
            getNextToken();
            if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
                ReportError("expected super-trait name after '+'");
                return nullptr;
            }
            SuperTraits.push_back(m_lexer.IdentifierStr);
            getNextToken();
        }
    }

    std::vector<TraitDeclAST::MethodProto> Methods;

    // Support both brace-delimited { ... } and end-delimited block syntax
    bool useBraceBlock = (CurTok == static_cast<int>(TokenType::tok_lbrace));
    if (useBraceBlock)
        getNextToken(); // eat {

    auto isBlockEnd = [&]() {
        if (useBraceBlock)
            return CurTok == static_cast<int>(TokenType::tok_rbrace);
        return CurTok == static_cast<int>(TokenType::tok_end) || CurTok == static_cast<int>(TokenType::tok_eof);
    };

    std::vector<std::string> AssociatedTypeNames;

    while (!isBlockEnd()) {
        if (CurTok == static_cast<int>(TokenType::tok_identifier) && m_lexer.IdentifierStr == "type") {
            getNextToken(); // eat type
            if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
                ReportError("expected associated type name after 'type' in trait");
                return nullptr;
            }
            AssociatedTypeNames.push_back(m_lexer.IdentifierStr);
            getNextToken();
        } else if (CurTok == static_cast<int>(TokenType::tok_def)) {
            getNextToken(); // eat def
            if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
                ReportError("expected method name in trait");
                return nullptr;
            }
            std::string MethodName = m_lexer.IdentifierStr;
            getNextToken();

            // Temporarily add trait name + associated types as valid generic params for method signatures
            std::vector<std::string> savedGenericParams = m_activeGenericParams;
            m_activeGenericParams.push_back(Name);
            m_activeGenericParams.insert(m_activeGenericParams.end(), AssociatedTypeNames.begin(),
                                         AssociatedTypeNames.end());

            // Parse parameter list (name: Type, ...)
            std::vector<std::pair<std::string, FluxType>> Args;
            if (CurTok == '(') {
                getNextToken(); // eat (
                while (CurTok != ')' && CurTok != static_cast<int>(TokenType::tok_eof)) {
                    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
                        ReportError("expected parameter name in trait method");
                        return nullptr;
                    }
                    std::string ArgName = m_lexer.IdentifierStr;
                    getNextToken();
                    FluxType ArgType(TypeKind::Double);
                    if (CurTok == static_cast<int>(TokenType::tok_colon)) {
                        getNextToken(); // eat :
                        ArgType = parseTypeName(m_activeGenericParams);
                    }
                    Args.push_back({ArgName, ArgType});
                    if (CurTok == ',')
                        getNextToken();
                    else if (CurTok != ')')
                        break;
                }
                if (CurTok != ')') {
                    ReportError("expected ')' in trait method parameter list");
                    return nullptr;
                }
                getNextToken(); // eat )
            }

            // Optional return type
            FluxType RetType(TypeKind::Void);
            if (CurTok == static_cast<int>(TokenType::tok_arrow)) {
                getNextToken();
                RetType = parseTypeName(m_activeGenericParams);
            }

            m_activeGenericParams = std::move(savedGenericParams);

            Methods.push_back({MethodName, std::move(Args), RetType});
        } else {
            getNextToken();
        }
    }

    if (useBraceBlock) {
        if (CurTok != static_cast<int>(TokenType::tok_rbrace)) {
            ReportError("expected '}' to close trait declaration");
            return nullptr;
        }
        getNextToken(); // eat }
    } else {
        if (CurTok != static_cast<int>(TokenType::tok_end)) {
            ReportError("expected 'end' to close trait declaration");
            return nullptr;
        }
        getNextToken(); // eat end
    }

    m_knownTraitNames.insert(Name);
    int traitIdx = static_cast<int>(m_knownTraitNameToIndex.size());
    m_knownTraitNameToIndex[Name] = traitIdx;

    auto trait = std::make_unique<TraitDeclAST>(Name, std::move(Methods), SuperTraits);
    if (!AssociatedTypeNames.empty())
        trait->setAssociatedTypes(std::move(AssociatedTypeNames));
    return trait;
}

/// ParseImplDecl — parse:
///   impl TypeName
///       def method(...) ...
///       def method(...) ...
///   end
/// Or:
///   impl TraitName for TypeName
///       def method(...) ...
///   end
std::unique_ptr<ImplDeclAST> Parser::ParseImplDecl()
{
    getNextToken(); // eat impl

    // Optional lifetime params: impl<'a, 'b: 'a>
    std::vector<LifetimeParam> LifetimeParams;
    if (CurTok == '<') {
        getNextToken();
        while (CurTok == static_cast<int>(TokenType::tok_lifetime)) {
            std::string lt = m_lexer.IdentifierStr;
            if (!lt.empty() && lt[0] == '\'')
                lt = lt.substr(1);
            std::vector<std::string> outlives;
            getNextToken();
            while (CurTok == static_cast<int>(TokenType::tok_colon)) {
                getNextToken(); // eat :
                if (CurTok == static_cast<int>(TokenType::tok_lifetime)) {
                    std::string bound = m_lexer.IdentifierStr;
                    if (!bound.empty() && bound[0] == '\'')
                        bound = bound.substr(1);
                    outlives.push_back(bound);
                    getNextToken();
                } else {
                    break;
                }
            }
            LifetimeParams.emplace_back(lt, outlives);
            if (CurTok == ',')
                getNextToken();
            else
                break;
        }
        if (CurTok != '>') {
            ReportError("expected '>' to close lifetime parameters in impl declaration");
            return nullptr;
        }
        getNextToken(); // eat >
    }
    // Save and restore previous active lifetime params (for nested parsing)
    std::vector<LifetimeParam> savedLifetimeParams = std::move(m_activeLifetimeParams);
    m_activeLifetimeParams = LifetimeParams;

    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected type name after 'impl'");
        m_activeLifetimeParams = std::move(savedLifetimeParams);
        return nullptr;
    }
    std::string FirstName = m_lexer.IdentifierStr;
    getNextToken();

    // Will be set when we detect brace vs end-delimited block
    bool useBraceBlock = false;

    // Helper lambda: check if at block terminator
    auto isImplEnd = [&]() {
        if (useBraceBlock)
            return CurTok == static_cast<int>(TokenType::tok_rbrace);
        return CurTok == static_cast<int>(TokenType::tok_end) || CurTok == static_cast<int>(TokenType::tok_eof);
    };

    auto expectImplEnd = [&]() -> bool {
        if (useBraceBlock) {
            if (CurTok == static_cast<int>(TokenType::tok_rbrace)) {
                getNextToken(); // eat }
                return true;
            }
            ReportError("expected '}' to close impl block");
        } else {
            if (CurTok == static_cast<int>(TokenType::tok_end)) {
                getNextToken(); // eat end
                return true;
            }
            ReportError("expected 'end' to close impl block");
        }
        return false;
    };

    // Check for "impl TraitName for TypeName" syntax
    if (CurTok == static_cast<int>(TokenType::tok_for)) {
        // This is a trait impl: impl TraitName for TypeName
        std::string TraitName = FirstName;
        getNextToken(); // eat "for"
        if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
            ReportError("expected type name after 'for' in trait impl");
            m_activeLifetimeParams = std::move(savedLifetimeParams);
            return nullptr;
        }
        std::string TypeName = m_lexer.IdentifierStr;
        getNextToken();

        // Support both { } and end-delimited blocks
        useBraceBlock = (CurTok == static_cast<int>(TokenType::tok_lbrace));
        if (useBraceBlock)
            getNextToken(); // eat {

        std::vector<std::unique_ptr<FunctionAST>> Methods;
        std::map<std::string, FluxType> assocTypeMappings;
        while (!isImplEnd()) {
            if (CurTok == static_cast<int>(TokenType::tok_def)) {
                auto Method = ParseDefinition();
                if (!Method) {
                    m_activeLifetimeParams = std::move(savedLifetimeParams);
                    return nullptr;
                }
                Methods.push_back(std::move(Method));
            } else if (CurTok == static_cast<int>(TokenType::tok_identifier) && m_lexer.IdentifierStr == "type") {
                getNextToken(); // eat type
                if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
                    ReportError("expected associated type name after 'type' in impl");
                    m_activeLifetimeParams = std::move(savedLifetimeParams);
                    return nullptr;
                }
                std::string assocName = m_lexer.IdentifierStr;
                getNextToken();
                if (CurTok == '=') {
                    getNextToken(); // eat =
                    FluxType concreteType = parseTypeName(std::vector<std::string>{});
                    assocTypeMappings[assocName] = concreteType;
                }
                if (CurTok == static_cast<int>(TokenType::tok_semicolon))
                    getNextToken();
            } else {
                ReportError("expected 'def', 'type', or 'end' in impl block");
                m_activeLifetimeParams = std::move(savedLifetimeParams);
                return nullptr;
            }
        }

        if (!expectImplEnd()) {
            m_activeLifetimeParams = std::move(savedLifetimeParams);
            return nullptr;
        }

        auto impl = std::make_unique<ImplDeclAST>(TypeName, std::move(Methods));
        impl->setTraitName(TraitName);
        for (auto& [name, type] : assocTypeMappings)
            impl->setAssociatedTypeMapping(name, type);
        if (!LifetimeParams.empty())
            impl->setLifetimeParams(std::move(LifetimeParams));
        m_activeLifetimeParams = std::move(savedLifetimeParams);
        return impl;
    }

    // Inherent impl: impl TypeName ... end or impl TypeName { ... }
    std::string TypeName = FirstName;

    // Support both { } and end-delimited blocks
    useBraceBlock = (CurTok == static_cast<int>(TokenType::tok_lbrace));
    if (useBraceBlock)
        getNextToken(); // eat {

    std::vector<std::unique_ptr<FunctionAST>> Methods;

    while (!isImplEnd()) {
        if (CurTok == static_cast<int>(TokenType::tok_def)) {
            auto Method = ParseDefinition();
            if (!Method) {
                m_activeLifetimeParams = std::move(savedLifetimeParams);
                return nullptr;
            }
            Methods.push_back(std::move(Method));
        } else {
            ReportError("expected 'def' or 'end' in impl block");
            m_activeLifetimeParams = std::move(savedLifetimeParams);
            return nullptr;
        }
    }

    if (!expectImplEnd()) {
        m_activeLifetimeParams = std::move(savedLifetimeParams);
        return nullptr;
    }

    auto impl = std::make_unique<ImplDeclAST>(TypeName, std::move(Methods));
    if (!LifetimeParams.empty())
        impl->setLifetimeParams(std::move(LifetimeParams));
    m_activeLifetimeParams = std::move(savedLifetimeParams);
    return impl;
}

bool Parser::ParseClassDecl(std::unique_ptr<StructDeclAST>* classStruct, std::unique_ptr<ImplDeclAST>* classImpl)
{
    getNextToken(); // eat class

    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected class name");
        return false;
    }
    std::string Name = m_lexer.IdentifierStr;
    getNextToken();

    // Optional generic params
    std::vector<std::string> GenericParams;
    if (CurTok == '[') {
        GenericParams = ParseGenericParams();
        if (hasError())
            return false;
    }

    // Optional inheritance : Parent
    std::string ParentName = "";
    if (CurTok == static_cast<int>(TokenType::tok_colon)) {
        getNextToken(); // eat :
        if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
            ReportError("expected parent class name after ':'");
            return false;
        }
        ParentName = m_lexer.IdentifierStr;
        getNextToken();
    }

    if (CurTok != static_cast<int>(TokenType::tok_lbrace)) {
        ReportError("expected '{' to open class body");
        return false;
    }
    getNextToken(); // eat {

    std::vector<std::pair<std::string, FluxType>> Fields;
    std::vector<std::unique_ptr<FunctionAST>> Methods;

    while (CurTok != static_cast<int>(TokenType::tok_rbrace) && CurTok != static_cast<int>(TokenType::tok_eof)) {
        if (CurTok == static_cast<int>(TokenType::tok_def)) {
            auto Method = ParseDefinition();
            if (!Method)
                return false;
            Methods.push_back(std::move(Method));
        } else if (CurTok == static_cast<int>(TokenType::tok_identifier) ||
                   CurTok == static_cast<int>(TokenType::tok_end)) {
            std::string FieldName = m_lexer.IdentifierStr;
            getNextToken();

            if (CurTok != static_cast<int>(TokenType::tok_colon)) {
                ReportError("expected ':' after class field name");
                return false;
            }
            getNextToken();

            Fields.emplace_back(FieldName, parseTypeName(GenericParams));

            if (CurTok == ';') {
                getNextToken(); // optional semicolon
            } else if (CurTok == ',') {
                getNextToken(); // optional comma
            }
        } else if (CurTok == ';') {
            getNextToken(); // stray semicolons
        } else {
            ReportError("expected field declaration or method definition in class");
            return false;
        }
    }

    if (CurTok != static_cast<int>(TokenType::tok_rbrace)) {
        ReportError("expected '}' to close class body");
        return false;
    }
    getNextToken(); // eat }

    // Register class struct name so parseTypeName can recognize it
    m_knownStructTypeNames.insert(Name);

    // Create the StructDeclAST (fields)
    auto structDecl = std::make_unique<StructDeclAST>(Name, std::move(Fields), ParentName);
    if (!GenericParams.empty())
        structDecl->setGenericParams(GenericParams);

    // Create the ImplDeclAST (methods)
    auto implDecl = std::make_unique<ImplDeclAST>(Name, std::move(Methods), ParentName);

    *classStruct = std::move(structDecl);
    *classImpl = std::move(implDecl);

    return true;
}
} // namespace Flux
