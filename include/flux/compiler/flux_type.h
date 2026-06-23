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

#ifndef FLUX_COMPILER_FLUX_TYPE_H
#define FLUX_COMPILER_FLUX_TYPE_H

#include "flux/compiler/lexer.h"
#include <cstdint>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Type.h>
#include <string>
#include <vector>

namespace Flux {

// Unit dimensions for dimensional analysis
struct UnitDimensions
{
    int8_t mass = 0;        // kg
    int8_t length = 0;      // m
    int8_t time = 0;        // s
    int8_t current = 0;     // A
    int8_t temperature = 0; // K
    int8_t amount = 0;      // mol
    int8_t luminous = 0;    // cd

    bool isDimensionless() const
    {
        return mass == 0 && length == 0 && time == 0 && current == 0 && temperature == 0 && amount == 0 &&
               luminous == 0;
    }

    bool operator==(const UnitDimensions& other) const
    {
        return mass == other.mass && length == other.length && time == other.time && current == other.current &&
               temperature == other.temperature && amount == other.amount && luminous == other.luminous;
    }

    bool operator!=(const UnitDimensions& other) const { return !(*this == other); }

    UnitDimensions operator*(const UnitDimensions& other) const
    {
        return {static_cast<int8_t>(mass + other.mass),
                static_cast<int8_t>(length + other.length),
                static_cast<int8_t>(time + other.time),
                static_cast<int8_t>(current + other.current),
                static_cast<int8_t>(temperature + other.temperature),
                static_cast<int8_t>(amount + other.amount),
                static_cast<int8_t>(luminous + other.luminous)};
    }

    UnitDimensions operator/(const UnitDimensions& other) const
    {
        return {static_cast<int8_t>(mass - other.mass),
                static_cast<int8_t>(length - other.length),
                static_cast<int8_t>(time - other.time),
                static_cast<int8_t>(current - other.current),
                static_cast<int8_t>(temperature - other.temperature),
                static_cast<int8_t>(amount - other.amount),
                static_cast<int8_t>(luminous - other.luminous)};
    }

    std::string toString() const;
};

// Type system for explicit typing
enum class TypeKind
{
    Auto,         // Inferred type
    Double,       // Default type (double precision float)
    Float,        // Single precision float
    Int,          // Integer
    Bool,         // Boolean
    Void,         // Void (for return types)
    Complex,      // Complex number (double complex)
    String,       // String (i8*)
    Matrix,       // Matrix { double*, i32, i32 }
    Vector,       // Vector { double*, i32 }
    Symbolic,     // Symbolic expression handle (double/uintptr_t)
    Fixed,        // Fixed-point integer (Q format)
    ComplexMatrix, // Complex Matrix { std::complex<double>*, i32, i32 }
    Generic,      // Generic type parameter placeholder (e.g., T in def foo[T](x: T))
    UserStruct,   // User-defined struct type (e.g., struct Vec2 { x: Double, y: Double })
    UserEnum,     // User-defined enum type (e.g., enum Color { Red, Green, Blue })
    Ref,          // Reference type: &T, &'a T, &mut T
    TraitObject   // Trait object type: dyn TraitName (fat pointer: data_ptr + vtable_ptr)
};

class FluxType
{
public:
    TypeKind Kind;
    UnitDimensions Dimensions;
    int Bits = 32;  // Total bits for Fixed
    int Fract = 16; // Fractional bits for Fixed
    std::string GenericName; // Type parameter name for TypeKind::Generic
    int StructTypeId = 0;    // Struct type ID for TypeKind::UserStruct (index into CodegenContext.StructTypes)
    llvm::Type* StructLLVMType = nullptr; // LLVM struct type pointer for UserStruct (cached)
    int EnumTypeId = 0;      // Enum type ID for TypeKind::UserEnum (index into CodegenContext.EnumTypes)
    llvm::Type* EnumLLVMType = nullptr; // LLVM tagged union type for UserEnum with payload
    int TraitObjectTypeId = 0;     // Trait ID for TypeKind::TraitObject (index into CodegenContext.Traits)
    llvm::Type* TraitObjectVTableTy = nullptr; // LLVM vtable struct type for this trait object
    // Generic type args for UserStruct/UserEnum (e.g., [Double, Double] for Result[Double, Double])
    std::vector<FluxType> GenericArgs;

    // Element type for TypeKind::Vector (e.g., nested vectors: Vector<Vector<Double>>)
    // Nullptr means scalar (Double) elements.
    FluxType* VecElementType = nullptr;

    // Reference type fields (TypeKind::Ref)
    FluxType* RefInnerType = nullptr;
    bool RefIsMut = false;
    std::string Lifetime;   // e.g., "a" for &'a T (empty = elided)

    // Copy semantics flag: true = Copy by value (default), false = ~Copy (move-only)
    bool isCopy = true;

    FluxType(TypeKind K = TypeKind::Double, UnitDimensions D = {}) : Kind(K), Dimensions(D), VecElementType(nullptr), RefInnerType(nullptr) {}

    FluxType(TypeKind K, int bits, int fract) : Kind(K), Bits(bits), Fract(fract), VecElementType(nullptr), RefInnerType(nullptr) {}

    // Destructor, copy/move for RefInnerType and VecElementType management
    ~FluxType() { delete RefInnerType; delete VecElementType; }
    FluxType(const FluxType& other)
        : Kind(other.Kind), Dimensions(other.Dimensions), Bits(other.Bits), Fract(other.Fract),
          GenericName(other.GenericName), StructTypeId(other.StructTypeId), StructLLVMType(other.StructLLVMType),
          EnumTypeId(other.EnumTypeId), EnumLLVMType(other.EnumLLVMType),
          TraitObjectTypeId(other.TraitObjectTypeId), TraitObjectVTableTy(other.TraitObjectVTableTy),
          GenericArgs(other.GenericArgs),
          RefIsMut(other.RefIsMut), Lifetime(other.Lifetime), isCopy(other.isCopy)
    {
        if (other.RefInnerType)
            RefInnerType = new FluxType(*other.RefInnerType);
        if (other.VecElementType)
            VecElementType = new FluxType(*other.VecElementType);
    }
    FluxType(FluxType&& other) noexcept
        : Kind(other.Kind), Dimensions(other.Dimensions), Bits(other.Bits), Fract(other.Fract),
          GenericName(std::move(other.GenericName)), StructTypeId(other.StructTypeId),
          StructLLVMType(other.StructLLVMType),
          EnumTypeId(other.EnumTypeId), EnumLLVMType(other.EnumLLVMType),
          TraitObjectTypeId(other.TraitObjectTypeId), TraitObjectVTableTy(other.TraitObjectVTableTy),
          GenericArgs(std::move(other.GenericArgs)),
          VecElementType(other.VecElementType),
          RefInnerType(other.RefInnerType), RefIsMut(other.RefIsMut), Lifetime(std::move(other.Lifetime)),
          isCopy(other.isCopy)
    {
        other.RefInnerType = nullptr;
        other.VecElementType = nullptr;
    }
    FluxType& operator=(const FluxType& other)
    {
        if (this != &other) {
            Kind = other.Kind; Dimensions = other.Dimensions; Bits = other.Bits; Fract = other.Fract;
            GenericName = other.GenericName; StructTypeId = other.StructTypeId; StructLLVMType = other.StructLLVMType;
            EnumTypeId = other.EnumTypeId; EnumLLVMType = other.EnumLLVMType;
            TraitObjectTypeId = other.TraitObjectTypeId; TraitObjectVTableTy = other.TraitObjectVTableTy;
            GenericArgs = other.GenericArgs;
            RefIsMut = other.RefIsMut; Lifetime = other.Lifetime; isCopy = other.isCopy;
            delete RefInnerType;
            RefInnerType = other.RefInnerType ? new FluxType(*other.RefInnerType) : nullptr;
            delete VecElementType;
            VecElementType = other.VecElementType ? new FluxType(*other.VecElementType) : nullptr;
        }
        return *this;
    }
    FluxType& operator=(FluxType&& other) noexcept
    {
        if (this != &other) {
            delete RefInnerType; delete VecElementType;
            Kind = other.Kind; Dimensions = other.Dimensions; Bits = other.Bits; Fract = other.Fract;
            GenericName = std::move(other.GenericName); StructTypeId = other.StructTypeId;
            StructLLVMType = other.StructLLVMType;
            EnumTypeId = other.EnumTypeId; EnumLLVMType = other.EnumLLVMType;
            TraitObjectTypeId = other.TraitObjectTypeId; TraitObjectVTableTy = other.TraitObjectVTableTy;
            GenericArgs = std::move(other.GenericArgs);
            VecElementType = other.VecElementType;
            RefInnerType = other.RefInnerType;
            RefIsMut = other.RefIsMut; Lifetime = std::move(other.Lifetime); isCopy = other.isCopy;
            other.RefInnerType = nullptr;
            other.VecElementType = nullptr;
        }
        return *this;
    }

    // Create a reference type: &T, &'a T, &mut T
    static FluxType reference(const FluxType& Inner, bool isMut = false, const std::string& Lifetime = "")
    {
        FluxType T(TypeKind::Ref);
        T.RefInnerType = new FluxType(Inner);
        T.RefIsMut = isMut;
        T.Lifetime = Lifetime;
        return T;
    }

    bool isRef() const { return Kind == TypeKind::Ref; }
    bool isMutRef() const { return Kind == TypeKind::Ref && RefIsMut; }
    const FluxType& getRefInnerType() const { return *RefInnerType; }
    const std::string& getRefLifetime() const { return Lifetime; }

    // Create a Generic type parameter placeholder
    static FluxType generic(const std::string& Name)
    {
        FluxType T(TypeKind::Generic);
        T.GenericName = Name;
        return T;
    }

    // Create a User-defined struct type
    static FluxType userStruct(int structId, llvm::Type* llvmType)
    {
        FluxType T(TypeKind::UserStruct);
        T.StructTypeId = structId;
        T.StructLLVMType = llvmType;
        return T;
    }

    bool isGeneric() const { return Kind == TypeKind::Generic; }
    bool isStruct() const { return Kind == TypeKind::UserStruct; }
    bool isEnum() const { return Kind == TypeKind::UserEnum; }
    bool isTraitObject() const { return Kind == TypeKind::TraitObject; }

    // Create a Trait Object type: dyn TraitName
    static FluxType traitObject(int traitId)
    {
        FluxType T(TypeKind::TraitObject);
        T.TraitObjectTypeId = traitId;
        return T;
    }

    // Create a User-defined enum type
    static FluxType userEnum(int enumTypeId, llvm::Type* llvmType = nullptr)
    {
        FluxType T(TypeKind::UserEnum);
        T.EnumTypeId = enumTypeId;
        T.EnumLLVMType = llvmType;
        return T;
    }

    llvm::Type* getLLVMType(llvm::LLVMContext& Context) const
    {
        switch (Kind) {
        case TypeKind::Bool:
            return llvm::Type::getInt1Ty(Context);
        case TypeKind::Float:
            return llvm::Type::getFloatTy(Context);
        case TypeKind::Int:
            return llvm::Type::getInt32Ty(Context);
        case TypeKind::Fixed:
            return llvm::Type::getIntNTy(Context, Bits);
        case TypeKind::Void:
            return llvm::Type::getVoidTy(Context);
        case TypeKind::Complex:
            // Complex numbers use native LLVM <2 x double> vector (SSE2 compatible)
            return llvm::VectorType::get(llvm::Type::getDoubleTy(Context), 2, false);
        case TypeKind::Matrix:
        case TypeKind::ComplexMatrix:
            // Matrix represented as { void*, i32, i32 }
            return llvm::StructType::get(Context, {llvm::PointerType::get(Context, 0), llvm::Type::getInt32Ty(Context),
                                                   llvm::Type::getInt32Ty(Context)});
        case TypeKind::Vector:
            // Vector represented as { double*, i32 }
            return llvm::StructType::get(Context,
                                         {llvm::PointerType::get(Context, 0), llvm::Type::getInt32Ty(Context)});
        case TypeKind::String:
            // Strings are represented as double (opaque handle)
            return llvm::Type::getDoubleTy(Context);
        case TypeKind::Generic:
            // Generic type parameters should be substituted before codegen.
            // Fall back to Double as a safe default.
            return llvm::Type::getDoubleTy(Context);
        case TypeKind::Ref:
            // &T is a pointer to the inner type
            return llvm::PointerType::get(Context, 0);
        case TypeKind::UserStruct:
            // Fall back to Double if the struct type hasn't been resolved yet.
            return StructLLVMType ? StructLLVMType : llvm::Type::getDoubleTy(Context);
        case TypeKind::UserEnum:
            return EnumLLVMType ? EnumLLVMType : llvm::Type::getInt32Ty(Context);
        case TypeKind::TraitObject:
            // Fat pointer: { i8* (data), i8* (vtable) } — two pointers
            return llvm::StructType::get(Context,
                {llvm::PointerType::get(Context, 0), llvm::PointerType::get(Context, 0)});
        case TypeKind::Double:
        default:
            return llvm::Type::getDoubleTy(Context);
        }
    }

    static FluxType fromLLVMType(llvm::Type* T)
    {
        if (T->isDoubleTy())
            return FluxType(TypeKind::Double);
        if (T->isFloatTy())
            return FluxType(TypeKind::Float);
        if (T->isIntegerTy(32))
            return FluxType(TypeKind::Int);
        if (T->isIntegerTy(1))
            return FluxType(TypeKind::Bool);
        if (T->isVoidTy())
            return FluxType(TypeKind::Void);
        // Complex is now <2 x double> (vector, not struct)
        if (T->isVectorTy()) {
            auto* VT = llvm::cast<llvm::VectorType>(T);
            if (!VT->getElementCount().isScalable() && VT->getElementCount().getKnownMinValue() == 2 &&
                VT->getElementType()->isDoubleTy())
                return FluxType(TypeKind::Complex);
        }
        if (T->isStructTy()) {
            if (T->getStructNumElements() == 3)
                return FluxType(TypeKind::Matrix);
            if (T->getStructNumElements() == 2) {
                // Vector is { double*, i32 }
                if (T->getStructElementType(0)->isPointerTy())
                    return FluxType(TypeKind::Vector);
            }
        }
        if (T->isPointerTy())
            return FluxType(TypeKind::String);
        return FluxType(TypeKind::Double);
    }

    static FluxType fromToken(int token)
    {
        switch (token) {
        case static_cast<int>(TokenType::tok_type_float):
            return FluxType(TypeKind::Float);
        case static_cast<int>(TokenType::tok_type_int):
            return FluxType(TypeKind::Int);
        case static_cast<int>(TokenType::tok_type_bool):
            return FluxType(TypeKind::Bool);
        case static_cast<int>(TokenType::tok_type_void):
            return FluxType(TypeKind::Void);
        case static_cast<int>(TokenType::tok_type_complex):
            return FluxType(TypeKind::Complex);
        case static_cast<int>(TokenType::tok_type_string):
            return FluxType(TypeKind::String);
        case static_cast<int>(TokenType::tok_type_matrix):
            return FluxType(TypeKind::Matrix);
        case static_cast<int>(TokenType::tok_type_vector):
            return FluxType(TypeKind::Vector);
        case static_cast<int>(TokenType::tok_type_double):
        default:
            return FluxType(TypeKind::Double);
        }
    }

    // Create a FluxType from a unit type name (e.g., "Voltage", "Current").
    // Returns Double with the appropriate UnitDimensions if the name is
    // recognised, or an empty type (Double, dimensionless) otherwise.
    static FluxType fromUnitName(const std::string& name)
    {
        // Map common engineering unit type names to dimensions.
        // The numeric type is always Double; only the dimensions differ.
        // SI base dimensions order: {mass, length, time, current, temperature, amount, luminous}
        if (name == "Voltage" || name == "Volt" || name == "V")
            return FluxType(TypeKind::Double, UnitDimensions{1, 2, -3, -1, 0, 0, 0}); // V = kg*m^2*s^-3*A^-1
        if (name == "Current" || name == "Ampere" || name == "A")
            return FluxType(TypeKind::Double, UnitDimensions{0, 0, 0, 1, 0, 0, 0}); // A
        if (name == "Resistance" || name == "Ohm")
            return FluxType(TypeKind::Double, UnitDimensions{1, 2, -3, -2, 0, 0, 0}); // Ohm = kg*m^2*s^-3*A^-2
        if (name == "Capacitance" || name == "Farad" || name == "F")
            return FluxType(TypeKind::Double, UnitDimensions{-1, -2, 4, 2, 0, 0, 0}); // F = kg^-1*m^-2*s^4*A^2
        if (name == "Inductance" || name == "Henry" || name == "H")
            return FluxType(TypeKind::Double, UnitDimensions{1, 2, -2, -2, 0, 0, 0}); // H = kg*m^2*s^-2*A^-2
        if (name == "Power" || name == "Watt" || name == "W")
            return FluxType(TypeKind::Double, UnitDimensions{1, 2, -3, 0, 0, 0, 0}); // W = kg*m^2*s^-3
        if (name == "Energy" || name == "Joule" || name == "J")
            return FluxType(TypeKind::Double, UnitDimensions{1, 2, -2, 0, 0, 0, 0}); // J = kg*m^2*s^-2
        if (name == "Frequency" || name == "Hertz" || name == "Hz")
            return FluxType(TypeKind::Double, UnitDimensions{0, 0, -1, 0, 0, 0, 0}); // Hz = s^-1
        if (name == "Force" || name == "Newton" || name == "N")
            return FluxType(TypeKind::Double, UnitDimensions{1, 1, -2, 0, 0, 0, 0}); // N = kg*m*s^-2
        if (name == "Pressure" || name == "Pascal" || name == "Pa")
            return FluxType(TypeKind::Double, UnitDimensions{1, -1, -2, 0, 0, 0, 0}); // Pa = kg*m^-1*s^-2
        if (name == "Charge" || name == "Coulomb" || name == "C")
            return FluxType(TypeKind::Double, UnitDimensions{0, 0, 1, 1, 0, 0, 0}); // C = s*A
        if (name == "Conductance" || name == "Siemens" || name == "S")
            return FluxType(TypeKind::Double, UnitDimensions{-1, -2, 3, 2, 0, 0, 0}); // S = kg^-1*m^-2*s^3*A^2
        if (name == "Time" || name == "Second" || name == "s")
            return FluxType(TypeKind::Double, UnitDimensions{0, 0, 1, 0, 0, 0, 0}); // s
        if (name == "Length" || name == "Meter" || name == "m")
            return FluxType(TypeKind::Double, UnitDimensions{0, 1, 0, 0, 0, 0, 0}); // m
        if (name == "Mass" || name == "Kilogram" || name == "kg")
            return FluxType(TypeKind::Double, UnitDimensions{1, 0, 0, 0, 0, 0, 0}); // kg
        if (name == "Temperature" || name == "Kelvin" || name == "K")
            return FluxType(TypeKind::Double, UnitDimensions{0, 0, 0, 0, 1, 0, 0}); // K
        if (name == "Admittance")
            return FluxType(TypeKind::Double, UnitDimensions{-1, -2, 3, 2, 0, 0, 0}); // S

        return FluxType(TypeKind::Double); // dimensionless fallback
    }
};

} // namespace Flux

#endif // FLUX_COMPILER_FLUX_TYPE_H
