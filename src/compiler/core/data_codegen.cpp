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

#include "flux/compiler/ast.h"
#include "flux/compiler/codegen_helpers.h"
#include <iostream>

namespace Flux {

TypedValue VectorExprAST::codegen(CodegenContext& context)
{
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
    llvm::Type* Int32Ty = llvm::Type::getInt32Ty(context.TheContext);
    llvm::Type* Int64Ty = llvm::Type::getInt64Ty(context.TheContext);
    llvm::Type* VoidPtrTy = llvm::PointerType::get(context.TheContext, 0);
    llvm::Function* MallocF = context.TheModule->getFunction("malloc");
    if (!MallocF)
        MallocF = llvm::Function::Create(llvm::FunctionType::get(VoidPtrTy, {Int64Ty}, false),
                                         llvm::Function::ExternalLinkage, "malloc", context.TheModule);

    // Codegen all elements first to determine types
    std::vector<TypedValue> elemTVs;
    elemTVs.reserve(Elements.size());
    FluxType elementType(TypeKind::Double);
    bool hasVectorElements = false;
    for (auto& elem : Elements) {
        TypedValue etv = elem->codegen(context);
        elemTVs.push_back(etv);
        if (etv.Val && etv.Type.Kind == TypeKind::Vector) {
            hasVectorElements = true;
            elementType = etv.Type;
        }
    }

    if (hasVectorElements) {
        // Nested vector: store pointers to heap-allocated inner vectors
        llvm::Value* DataSize = llvm::ConstantInt::get(Int64Ty, Elements.size() * 8);
        llvm::Value* DataPtr = context.Builder.CreateCall(MallocF, {DataSize}, "vec_data");
        llvm::Type* VecStructTy = FluxType(TypeKind::Vector).getLLVMType(context.TheContext);
        for (size_t i = 0; i < Elements.size(); ++i) {
            TypedValue& ElemTV = elemTVs[i];
            if (!ElemTV.Val)
                continue;
            if (ElemTV.Type.Kind == TypeKind::Vector) {
                // Allocate heap space for inner vector struct, store it, store pointer
                llvm::Value* HeapVec =
                    context.Builder.CreateCall(MallocF, llvm::ConstantInt::get(Int64Ty, 16), "inner_vec_heap");
                llvm::Value* VecPtr = context.Builder.CreatePointerCast(
                    HeapVec, llvm::PointerType::get(context.TheContext, 0), "inner_vec_ptr");
                context.Builder.CreateStore(ElemTV.Val, VecPtr);
                // Bitcast pointer to double for storage in the double data array
                llvm::Value* PtrAsInt = context.Builder.CreatePtrToInt(HeapVec, Int64Ty, "heap_i64");
                llvm::Value* PtrAsDouble = context.Builder.CreateBitCast(PtrAsInt, DoubleTy, "ptr_as_dbl");
                llvm::Value* ElemPtr = context.Builder.CreateInBoundsGEP(
                    DoubleTy, DataPtr, {llvm::ConstantInt::get(Int64Ty, i)}, "elem_ptr");
                context.Builder.CreateStore(PtrAsDouble, ElemPtr);
            } else {
                // Scalar element in mixed vector — convert to double and store
                llvm::Value* Val = ElemTV.Val;
                if (Val->getType()->isIntegerTy())
                    Val = context.Builder.CreateSIToFP(Val, DoubleTy, "elem_dbl");
                else if (Val->getType()->isFloatTy())
                    Val = context.Builder.CreateFPExt(Val, DoubleTy, "elem_dbl");
                llvm::Value* ElemPtr = context.Builder.CreateInBoundsGEP(
                    DoubleTy, DataPtr, {llvm::ConstantInt::get(Int64Ty, i)}, "elem_ptr");
                context.Builder.CreateStore(Val, ElemPtr);
            }
        }
        FluxType vecType(TypeKind::Vector);
        vecType.VecElementType = new FluxType(elementType);
        llvm::StructType* VecSTy = llvm::cast<llvm::StructType>(vecType.getLLVMType(context.TheContext));
        llvm::Value* VecVal = llvm::PoisonValue::get(VecSTy);
        VecVal = context.Builder.CreateInsertValue(VecVal, DataPtr, 0);
        VecVal = context.Builder.CreateInsertValue(VecVal, llvm::ConstantInt::get(Int32Ty, Elements.size()), 1);
        return TypedValue(VecVal, vecType);
    }

    // Scalar elements: flat double array (original behavior)
    llvm::Value* DataSize = llvm::ConstantInt::get(Int64Ty, Elements.size() * 8);
    llvm::Value* DataPtr = context.Builder.CreateCall(MallocF, {DataSize}, "vec_data");
    for (size_t i = 0; i < Elements.size(); ++i) {
        TypedValue& ElemTV = elemTVs[i];
        if (ElemTV.Val) {
            llvm::Value* Val = ElemTV.Val;
            if (Val->getType()->isIntegerTy())
                Val = context.Builder.CreateSIToFP(Val, DoubleTy, "elem_dbl");
            else if (Val->getType()->isFloatTy())
                Val = context.Builder.CreateFPExt(Val, DoubleTy, "elem_dbl");
            llvm::Value* ElemPtr =
                context.Builder.CreateInBoundsGEP(DoubleTy, DataPtr, {llvm::ConstantInt::get(Int64Ty, i)}, "elem_ptr");
            context.Builder.CreateStore(Val, ElemPtr);
        }
    }
    FluxType vecType(TypeKind::Vector);
    // VecElementType left nullptr (nullptr = Double elements)
    llvm::StructType* VecSTy = llvm::cast<llvm::StructType>(vecType.getLLVMType(context.TheContext));
    llvm::Value* VecVal = llvm::PoisonValue::get(VecSTy);
    VecVal = context.Builder.CreateInsertValue(VecVal, DataPtr, 0);
    VecVal = context.Builder.CreateInsertValue(VecVal, llvm::ConstantInt::get(Int32Ty, Elements.size()), 1);
    return TypedValue(VecVal, vecType);
}

TypedValue MatrixExprAST::codegen(CodegenContext& context)
{
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
    llvm::Type* ComplexTy = llvm::VectorType::get(DoubleTy, 2, false);
    llvm::Type* Int32Ty = llvm::Type::getInt32Ty(context.TheContext);
    llvm::Type* VoidPtrTy = llvm::PointerType::get(context.TheContext, 0);

    bool isComplex = false;
    for (int r = 0; r < NumRows; ++r) {
        for (int c = 0; c < NumCols; ++c) {
            if (dynamic_cast<ComplexExprAST*>(Rows[r][c].get())) {
                isComplex = true;
                break;
            }
        }
        if (isComplex)
            break;
    }

    llvm::Type* ElemTy = isComplex ? ComplexTy : DoubleTy;

    // Allocate Eigen matrix directly — no malloc, no memcpy
    std::string newFnName = isComplex ? "flux_register_new_complex_matrix" : "flux_register_new_matrix";
    llvm::Function* NewMatF = context.TheModule->getFunction(newFnName);
    if (!NewMatF) {
        llvm::Type* Params[] = {Int32Ty, Int32Ty};
        NewMatF = llvm::Function::Create(llvm::FunctionType::get(VoidPtrTy, Params, false),
                                         llvm::Function::ExternalLinkage, newFnName, context.TheModule);
    }

    llvm::Value* RowsVal = llvm::ConstantInt::get(Int32Ty, NumRows);
    llvm::Value* ColsVal = llvm::ConstantInt::get(Int32Ty, NumCols);
    llvm::Value* DataPtr = context.Builder.CreateCall(NewMatF, {RowsVal, ColsVal}, "mat_data");
    DataPtr = context.Builder.CreateBitCast(DataPtr, llvm::PointerType::get(context.TheContext, 0));

    // Write elements directly into Eigen's internal data buffer
    UnitDimensions elemDims;
    for (int r = 0; r < NumRows; ++r) {
        for (int c = 0; c < NumCols; ++c) {
            TypedValue ElemTV = Rows[r][c]->codegen(context);
            if (ElemTV.Val) {
                if (r == 0 && c == 0)
                    elemDims = ElemTV.Type.Dimensions;
                if (isComplex && ElemTV.Type.Kind != TypeKind::Complex) {
                    ElemTV = promoteToComplex(ElemTV, context);
                }
                llvm::Value* ElemPtr = context.Builder.CreateInBoundsGEP(
                    ElemTy, DataPtr,
                    {llvm::ConstantInt::get(llvm::Type::getInt64Ty(context.TheContext), c * NumRows + r)}, "elem_ptr");
                context.Builder.CreateStore(ElemTV.Val, ElemPtr);
            }
        }
    }

    FluxType resType = isComplex ? FluxType(TypeKind::ComplexMatrix) : FluxType(TypeKind::Matrix);
    resType.Dimensions = elemDims;
    llvm::StructType* MatStructTy = llvm::cast<llvm::StructType>(resType.getLLVMType(context.TheContext));
    llvm::Value* MatVal = llvm::UndefValue::get(MatStructTy);
    MatVal = context.Builder.CreateInsertValue(MatVal, DataPtr, 0);
    MatVal = context.Builder.CreateInsertValue(MatVal, RowsVal, 1);
    MatVal = context.Builder.CreateInsertValue(MatVal, ColsVal, 2);

    return TypedValue(MatVal, resType);
}

TypedValue IndexExprAST::codegen(CodegenContext& context)
{
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
    llvm::Type* Int32Ty = llvm::Type::getInt32Ty(context.TheContext);
    llvm::Type* Int64Ty = llvm::Type::getInt64Ty(context.TheContext);
    llvm::Type* VoidPtrTy = llvm::PointerType::get(context.TheContext, 0);

    TypedValue ArrayTV = Array->codegen(context);
    if (!ArrayTV.Val) {
        std::cerr << "[FLUX ERROR] Index array expression failed to codegen" << std::endl;
        return TypedValue();
    }

    if (ArrayTV.Type.Kind == TypeKind::Vector) {
        llvm::Value* DataPtr = context.Builder.CreateExtractValue(ArrayTV.Val, 0, "vec_ptr");
        llvm::Value* LenVal = context.Builder.CreateExtractValue(ArrayTV.Val, 1, "vec_len");
        (void)LenVal;

        TypedValue IdxTV = RowIndex->codegen(context);
        if (!IdxTV.Val) {
            std::cerr << "[FLUX ERROR] Vector index expression failed to codegen" << std::endl;
            return TypedValue();
        }
        llvm::Value* IdxV = IdxTV.Val;
        if (IdxV->getType()->isFloatingPointTy())
            IdxV = context.Builder.CreateFPToSI(IdxV, Int32Ty, "idx_int");
        else if (IdxV->getType()->isIntegerTy(64))
            IdxV = context.Builder.CreateTrunc(IdxV, Int32Ty, "idx_int");

        // Check if vector contains nested vectors (VecElementType non-null and Kind == Vector)
        if (ArrayTV.Type.VecElementType && ArrayTV.Type.VecElementType->Kind == TypeKind::Vector) {
            // Load bit-cast pointer from data slot, inttoptr, then load inner vector struct
            llvm::Value* Idx64 = context.Builder.CreateSExt(IdxV, Int64Ty, "idx_64");
            llvm::Value* ElemSlot = context.Builder.CreateInBoundsGEP(DoubleTy, DataPtr, {Idx64}, "elem_slot");
            llvm::Value* DblBits = context.Builder.CreateLoad(DoubleTy, ElemSlot, "ptr_bits_as_dbl");
            llvm::Value* PtrInt = context.Builder.CreateBitCast(DblBits, Int64Ty, "ptr_bits_i64");
            llvm::Value* InnerVecVoidPtr = context.Builder.CreateIntToPtr(PtrInt, VoidPtrTy, "inner_vec_raw");
            llvm::Value* InnerVecVal = context.Builder.CreateLoad(
                FluxType(TypeKind::Vector).getLLVMType(context.TheContext), InnerVecVoidPtr, "inner_vec");
            return TypedValue(InnerVecVal, *ArrayTV.Type.VecElementType);
        }

        llvm::Value* Idx64 = context.Builder.CreateSExt(IdxV, Int64Ty, "idx_64");
        llvm::Value* ElemPtr = context.Builder.CreateInBoundsGEP(DoubleTy, DataPtr, {Idx64}, "elem_ptr");
        llvm::Value* ElemVal = context.Builder.CreateLoad(DoubleTy, ElemPtr, "elem_val");
        return TypedValue(ElemVal, TypeKind::Double);
    }

    if (ArrayTV.Type.Kind == TypeKind::Double && ArrayTV.Val->getType()->isPointerTy()) {
        TypedValue IdxTV = RowIndex->codegen(context);
        if (!IdxTV.Val)
            return TypedValue();
        llvm::Value* IdxV = IdxTV.Val;
        if (IdxV->getType()->isFloatingPointTy())
            IdxV = context.Builder.CreateFPToSI(IdxV, Int32Ty, "idx_int");
        else if (IdxV->getType()->isIntegerTy(64))
            IdxV = context.Builder.CreateTrunc(IdxV, Int32Ty, "idx_int");
        llvm::Value* Idx64 = context.Builder.CreateSExt(IdxV, Int64Ty, "idx_64");
        llvm::Value* ElemPtr = context.Builder.CreateGEP(DoubleTy, ArrayTV.Val, {Idx64}, "raw_ptr_idx");
        llvm::Value* ElemVal = context.Builder.CreateLoad(DoubleTy, ElemPtr, "raw_elem_val");
        return TypedValue(ElemVal, TypeKind::Double);
    }
    if (ArrayTV.Type.Kind != TypeKind::Matrix) {
        std::cerr << "Can only index into matrices or vectors." << std::endl;
        return TypedValue();
    }

    llvm::Value* MatPtr = context.Builder.CreateExtractValue(ArrayTV.Val, 0, "mat_ptr");

    // Extract matrix dimensions for bounds checking
    llvm::Value* RowsVal = context.Builder.CreateExtractValue(ArrayTV.Val, 1, "mat_rows");
    llvm::Value* ColsVal = context.Builder.CreateExtractValue(ArrayTV.Val, 2, "mat_cols");

    if (IsMatrixIndex) {
        // Check if either index is a RangeExprAST (for slicing)
        auto codegenRange = [&](std::unique_ptr<ExprAST>& idx, llvm::Value* dim, llvm::Value*& lo,
                                llvm::Value*& hi) -> bool {
            auto* r = dynamic_cast<RangeExprAST*>(idx.get());
            if (!r) {
                lo = llvm::ConstantInt::get(Int32Ty, 0);
                hi = dim;
                return true;
            }
            auto* sPtr = const_cast<ExprAST*>(r->getStart());
            auto* ePtr = const_cast<ExprAST*>(r->getEnd());
            if (!sPtr || !ePtr)
                return false;
            TypedValue s = sPtr->codegen(context);
            TypedValue e = ePtr->codegen(context);
            lo = s.Val;
            hi = e.Val;
            if (lo->getType()->isFloatingPointTy())
                lo = context.Builder.CreateFPToSI(lo, Int32Ty, "slice_lo");
            if (hi->getType()->isFloatingPointTy())
                hi = context.Builder.CreateFPToSI(hi, Int32Ty, "slice_hi");
            return true;
        };

        bool hasRowRange = dynamic_cast<RangeExprAST*>(RowIndex.get()) != nullptr;
        bool hasColRange = ColIndex ? dynamic_cast<RangeExprAST*>(ColIndex.get()) != nullptr : false;

        if (hasRowRange || hasColRange) {
            llvm::Value *r0, *r1, *c0, *c1;
            if (!codegenRange(RowIndex, RowsVal, r0, r1)) {
                std::cerr << "[FLUX ERROR] Matrix slice row range failed to codegen" << std::endl;
                return TypedValue();
            }
            if (!codegenRange(ColIndex ? ColIndex : RowIndex, ColsVal, c0, c1)) {
                std::cerr << "[FLUX ERROR] Matrix slice column range failed to codegen" << std::endl;
                return TypedValue();
            }

            llvm::Function* SliceF = context.TheModule->getFunction("flux_matrix_slice");
            if (!SliceF)
                SliceF = llvm::Function::Create(
                    llvm::FunctionType::get(VoidPtrTy, {VoidPtrTy, Int32Ty, Int32Ty, Int32Ty, Int32Ty}, false),
                    llvm::Function::ExternalLinkage, "flux_matrix_slice", context.TheModule);
            llvm::Value* SlicePtr = context.Builder.CreateCall(SliceF, {MatPtr, r0, r1, c0, c1}, "slice_ptr");

            // Get slice dimensions and wrap in matrix struct
            llvm::Function* RowsF = context.TheModule->getFunction("flux_matrix_rows");
            if (!RowsF)
                RowsF = llvm::Function::Create(llvm::FunctionType::get(Int32Ty, {VoidPtrTy}, false),
                                               llvm::Function::ExternalLinkage, "flux_matrix_rows", context.TheModule);
            llvm::Function* ColsF = context.TheModule->getFunction("flux_matrix_cols");
            if (!ColsF)
                ColsF = llvm::Function::Create(llvm::FunctionType::get(Int32Ty, {VoidPtrTy}, false),
                                               llvm::Function::ExternalLinkage, "flux_matrix_cols", context.TheModule);
            llvm::Value* SR = context.Builder.CreateCall(RowsF, {SlicePtr}, "sli_rows");
            llvm::Value* SC = context.Builder.CreateCall(ColsF, {SlicePtr}, "sli_cols");
            FluxType sliceMatType(TypeKind::Matrix);
            sliceMatType.Dimensions = ArrayTV.Type.Dimensions;
            llvm::StructType* MatSTy = llvm::cast<llvm::StructType>(sliceMatType.getLLVMType(context.TheContext));
            llvm::Value* MV = llvm::PoisonValue::get(MatSTy);
            MV = context.Builder.CreateInsertValue(MV, SlicePtr, 0);
            MV = context.Builder.CreateInsertValue(MV, SR, 1);
            MV = context.Builder.CreateInsertValue(MV, SC, 2);
            return TypedValue(MV, sliceMatType);
        }

        // Scalar element access: A[row, col]
        TypedValue RowTV = RowIndex->codegen(context);
        TypedValue ColTV = ColIndex ? ColIndex->codegen(context) : TypedValue();
        if (!RowTV.Val || !ColTV.Val) {
            std::cerr << "[FLUX ERROR] Matrix element index expression failed to codegen" << std::endl;
            return TypedValue();
        }

        llvm::Value* RowV = RowTV.Val;
        llvm::Value* ColV = ColTV.Val;

        if (RowV->getType()->isFloatingPointTy())
            RowV = context.Builder.CreateFPToSI(RowV, Int32Ty, "row_int");
        else if (RowV->getType()->isIntegerTy(64))
            RowV = context.Builder.CreateTrunc(RowV, Int32Ty, "row_int");

        if (ColV->getType()->isFloatingPointTy())
            ColV = context.Builder.CreateFPToSI(ColV, Int32Ty, "col_int");
        else if (ColV->getType()->isIntegerTy(64))
            ColV = context.Builder.CreateTrunc(ColV, Int32Ty, "col_int");

        llvm::Function* GetElemF = context.TheModule->getFunction("flux_matrix_get");
        if (!GetElemF)
            GetElemF = llvm::Function::Create(llvm::FunctionType::get(DoubleTy, {VoidPtrTy, Int32Ty, Int32Ty}, false),
                                              llvm::Function::ExternalLinkage, "flux_matrix_get", context.TheModule);

        FluxType idxRetType(TypeKind::Double);
        idxRetType.Dimensions = ArrayTV.Type.Dimensions;
        return TypedValue(context.Builder.CreateCall(GetElemF, {MatPtr, RowV, ColV}, "elem_val"), idxRetType);
    }

    return TypedValue(llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), 0.0), TypeKind::Double);
}

TypedValue RangeExprAST::codegen(CodegenContext& context)
{
    TypedValue StartTV = Start->codegen(context);
    TypedValue EndTV = End->codegen(context);
    TypedValue StepTV =
        Step ? Step->codegen(context)
             : TypedValue(llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), 1.0), TypeKind::Double);

    auto ensureFP = [&](TypedValue& tv) {
        if (!tv.Val) {
            tv.Val = llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), 0.0);
        } else if (tv.Val->getType()->isIntegerTy()) {
            tv.Val = context.Builder.CreateSIToFP(tv.Val, llvm::Type::getDoubleTy(context.TheContext), "range_cast");
        } else if (tv.Val->getType()->isIntegerTy(1)) {
            tv.Val = context.Builder.CreateUIToFP(tv.Val, llvm::Type::getDoubleTy(context.TheContext), "range_cast");
        }
    };
    ensureFP(StartTV);
    ensureFP(EndTV);
    ensureFP(StepTV);

    // Look up the extern function type entry for proper arg decomposition
    auto it = context.ExternFuncTypes.find("flux_create_range_sum");
    if (it == context.ExternFuncTypes.end()) {
        std::cerr << "RangeExprAST: flux_create_range_sum not registered with ExternFuncTypes\n";
        return TypedValue(nullptr, TypeKind::Void);
    }

    auto& retType = it->second.first;
    auto& argTypes = it->second.second;
    llvm::Type* RetLTy = (retType.Kind == TypeKind::Matrix || retType.Kind == TypeKind::ComplexMatrix)
                             ? llvm::PointerType::get(context.TheContext, 0)
                             : retType.getLLVMType(context.TheContext);

    std::vector<llvm::Type*> ArgLTys;
    for (auto& at : argTypes) {
        if (at.Kind == TypeKind::Matrix || at.Kind == TypeKind::ComplexMatrix)
            ArgLTys.push_back(llvm::PointerType::get(context.TheContext, 0));
        else if (at.Kind == TypeKind::Int)
            ArgLTys.push_back(llvm::Type::getInt32Ty(context.TheContext));
        else
            ArgLTys.push_back(at.getLLVMType(context.TheContext));
    }

    std::string fnName = "flux_create_range_sum";
    llvm::Function* F = context.TheModule->getFunction(fnName);
    if (!F) {
        llvm::FunctionType* FT = llvm::FunctionType::get(RetLTy, ArgLTys, false);
        F = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, fnName, context.TheModule);
    }

    std::vector<llvm::Value*> CallArgs;
    CallArgs.push_back(StartTV.Val);
    CallArgs.push_back(StepTV.Val);
    CallArgs.push_back(EndTV.Val);

    llvm::Value* ResPtr = context.Builder.CreateCall(F, CallArgs, "range_res");

    if (retType.Kind == TypeKind::Matrix || retType.Kind == TypeKind::ComplexMatrix) {
        // Wrap void* into {ptr, i32, i32}
        llvm::StructType* MatStructTy = llvm::cast<llvm::StructType>(retType.getLLVMType(context.TheContext));
        llvm::Function* RowsF = context.TheModule->getFunction("flux_matrix_rows");
        if (!RowsF)
            RowsF =
                llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getInt32Ty(context.TheContext),
                                                               {llvm::PointerType::get(context.TheContext, 0)}, false),
                                       llvm::Function::ExternalLinkage, "flux_matrix_rows", context.TheModule);
        llvm::Function* ColsF = context.TheModule->getFunction("flux_matrix_cols");
        if (!ColsF)
            ColsF =
                llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getInt32Ty(context.TheContext),
                                                               {llvm::PointerType::get(context.TheContext, 0)}, false),
                                       llvm::Function::ExternalLinkage, "flux_matrix_cols", context.TheModule);
        llvm::Value* ResRows = context.Builder.CreateCall(RowsF, {ResPtr}, "res_rows");
        llvm::Value* ResCols = context.Builder.CreateCall(ColsF, {ResPtr}, "res_cols");
        llvm::Value* MatVal = llvm::UndefValue::get(MatStructTy);
        MatVal = context.Builder.CreateInsertValue(MatVal, ResPtr, 0);
        MatVal = context.Builder.CreateInsertValue(MatVal, ResRows, 1);
        MatVal = context.Builder.CreateInsertValue(MatVal, ResCols, 2);
        return TypedValue(MatVal, retType);
    }

    return TypedValue(ResPtr, retType);
}

} // namespace Flux
