/* Copyright 2026 Janada Sroor
   SPDX-License-Identifier: Apache-2.0
   Wrappers bridging FluxScript String ABI to LLVM C-API functions.
   All extern-callable functions follow the FluxScript convention:
   opaque pointers / strings are passed as `double`. */

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>

#include <llvm-c/Analysis.h>
#include <llvm-c/Core.h>
#include <llvm-c/BitWriter.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Types.h>

/* ------------------------------------------------------------------ */
/*  Helpers (outside extern "C" to allow templates)                  */
/* ------------------------------------------------------------------ */

static thread_local std::vector<std::string> g_llvm_pool;

static inline uint64_t dbl_as_u64(double d) noexcept
{
    uint64_t u;
    std::memcpy(&u, &d, sizeof(u));
    return u;
}

static inline double u64_as_dbl(uint64_t u) noexcept
{
    double d;
    std::memcpy(&d, &u, sizeof(d));
    return d;
}

static inline const char* dbl_to_cstr(double d)
{
    return reinterpret_cast<const char*>(static_cast<uintptr_t>(dbl_as_u64(d)));
}

static inline double cstr_to_dbl(const char* s)
{
    if (!s)
        return 0.0;
    g_llvm_pool.emplace_back(s);
    return u64_as_dbl(reinterpret_cast<uintptr_t>(g_llvm_pool.back().c_str()));
}

template <typename T> static inline double ptr_to_dbl(T* p)
{
    return u64_as_dbl(reinterpret_cast<uintptr_t>(p));
}

template <typename T> static inline T* dbl_to_ptr(double d)
{
    return reinterpret_cast<T*>(static_cast<uintptr_t>(dbl_as_u64(d)));
}

/* ------------------------------------------------------------------ */
/*  Collector (thread-local array builder for C-array params)         */
/* ------------------------------------------------------------------ */

static thread_local std::vector<LLVMValueRef> g_call_args;
static thread_local std::vector<LLVMTypeRef> g_type_args;
static thread_local std::vector<LLVMValueRef> g_index_args;
static thread_local std::vector<LLVMValueRef> g_const_args;

static thread_local std::unordered_map<uint64_t, std::unordered_map<std::string, double>> g_hmaps;
static thread_local uint64_t g_next_hm_id = 1000;

/* ------------------------------------------------------------------ */
/*  String utilities for FluxScript lexer/parser                      */
/* ------------------------------------------------------------------ */

extern "C" double flux_str_len(double str_ptr)
{
    const char* s = dbl_to_cstr(str_ptr);
    if (!s) return 0.0;
    return static_cast<double>(std::strlen(s));
}

extern "C" double flux_str_at(double str_ptr, double idx)
{
    const char* s = dbl_to_cstr(str_ptr);
    if (!s) return 0.0;
    int i = static_cast<int>(dbl_as_u64(idx));
    return static_cast<double>(static_cast<unsigned char>(s[i]));
}

extern "C" double flux_str_slice(double str_ptr, double start, double end)
{
    const char* s = dbl_to_cstr(str_ptr);
    if (!s) return 0.0;
    int st = static_cast<int>(dbl_as_u64(start));
    int en = static_cast<int>(dbl_as_u64(end));
    int len = en - st;
    if (len < 0) len = 0;
    std::string result(s + st, static_cast<size_t>(len));
    g_llvm_pool.push_back(result);
    return u64_as_dbl(reinterpret_cast<uintptr_t>(g_llvm_pool.back().c_str()));
}

extern "C" double flux_str_from_char(double ch)
{
    char c = static_cast<char>(static_cast<int>(dbl_as_u64(ch)));
    std::string s(1, c);
    g_llvm_pool.push_back(s);
    return u64_as_dbl(reinterpret_cast<uintptr_t>(g_llvm_pool.back().c_str()));
}

extern "C" double flux_str_concat(double a_ptr, double b_ptr)
{
    const char* a = dbl_to_cstr(a_ptr);
    const char* b = dbl_to_cstr(b_ptr);
    if (!a && !b) return 0.0;
    std::string result(a ? a : "");
    result += (b ? b : "");
    g_llvm_pool.push_back(result);
    return u64_as_dbl(reinterpret_cast<uintptr_t>(g_llvm_pool.back().c_str()));
}

extern "C" {

/* ------------------------------------------------------------------ */
/*  Context                                                           */
/* ------------------------------------------------------------------ */

double flux_llvm_context_create()
{
    return ptr_to_dbl(LLVMContextCreate());
}
double flux_llvm_context_dispose(double ctx)
{
    LLVMContextDispose(dbl_to_ptr<LLVMOpaqueContext>(ctx));
    return 0.0;
}

/* ------------------------------------------------------------------ */
/*  Module                                                            */
/* ------------------------------------------------------------------ */

double flux_llvm_module_create_with_name(double name)
{
    const char* n = dbl_to_cstr(name);
    return ptr_to_dbl(LLVMModuleCreateWithName(n));
}
double flux_llvm_module_create_with_name_in_ctx(double name, double ctx)
{
    const char* n = dbl_to_cstr(name);
    return ptr_to_dbl(LLVMModuleCreateWithNameInContext(n, dbl_to_ptr<LLVMOpaqueContext>(ctx)));
}
double flux_llvm_get_module_context(double module)
{
    return ptr_to_dbl(LLVMGetModuleContext(dbl_to_ptr<LLVMOpaqueModule>(module)));
}
double flux_llvm_dispose_module(double module)
{
    LLVMDisposeModule(dbl_to_ptr<LLVMOpaqueModule>(module));
    return 0.0;
}
double flux_llvm_print_module_to_string(double module)
{
    char* s = LLVMPrintModuleToString(dbl_to_ptr<LLVMOpaqueModule>(module));
    double result = cstr_to_dbl(s);
    LLVMDisposeMessage(s);
    return result;
}
double flux_llvm_dispose_message(double msg)
{
    LLVMDisposeMessage(dbl_to_cstr(msg));
    return 0.0;
}

/* ------------------------------------------------------------------ */
/*  Types                                                             */
/* ------------------------------------------------------------------ */

double flux_llvm_void_type_in_ctx(double ctx)
{
    return ptr_to_dbl(LLVMVoidTypeInContext(dbl_to_ptr<LLVMOpaqueContext>(ctx)));
}
double flux_llvm_int1_type_in_ctx(double ctx)
{
    return ptr_to_dbl(LLVMInt1TypeInContext(dbl_to_ptr<LLVMOpaqueContext>(ctx)));
}
double flux_llvm_int8_type_in_ctx(double ctx)
{
    return ptr_to_dbl(LLVMInt8TypeInContext(dbl_to_ptr<LLVMOpaqueContext>(ctx)));
}
double flux_llvm_int32_type_in_ctx(double ctx)
{
    return ptr_to_dbl(LLVMInt32TypeInContext(dbl_to_ptr<LLVMOpaqueContext>(ctx)));
}
double flux_llvm_int64_type_in_ctx(double ctx)
{
    return ptr_to_dbl(LLVMInt64TypeInContext(dbl_to_ptr<LLVMOpaqueContext>(ctx)));
}
double flux_llvm_double_type_in_ctx(double ctx)
{
    return ptr_to_dbl(LLVMDoubleTypeInContext(dbl_to_ptr<LLVMOpaqueContext>(ctx)));
}
double flux_llvm_float_type_in_ctx(double ctx)
{
    return ptr_to_dbl(LLVMFloatTypeInContext(dbl_to_ptr<LLVMOpaqueContext>(ctx)));
}
double flux_llvm_pointer_type_in_ctx(double ctx, double addr_space)
{
    return ptr_to_dbl(LLVMPointerTypeInContext(dbl_to_ptr<LLVMOpaqueContext>(ctx),
                                               static_cast<unsigned>(dbl_as_u64(addr_space))));
}
double flux_llvm_function_type(double ret_ty, double param_count, double is_var_arg)
{
    LLVMTypeRef* paramTypes = g_type_args.empty() ? nullptr : g_type_args.data();
    return ptr_to_dbl(LLVMFunctionType(dbl_to_ptr<LLVMOpaqueType>(ret_ty),
                                       paramTypes,
                                       static_cast<unsigned>(dbl_as_u64(param_count)),
                                       static_cast<int>(dbl_as_u64(is_var_arg))));
}
double flux_llvm_struct_type_in_ctx(double ctx, double elem_count, double packed)
{
    LLVMTypeRef* elemTypes = g_type_args.empty() ? nullptr : g_type_args.data();
    return ptr_to_dbl(LLVMStructTypeInContext(dbl_to_ptr<LLVMOpaqueContext>(ctx),
                                              elemTypes,
                                              static_cast<unsigned>(dbl_as_u64(elem_count)),
                                              static_cast<int>(dbl_as_u64(packed))));
}
double flux_llvm_struct_create_named(double ctx, double name)
{
    return ptr_to_dbl(LLVMStructCreateNamed(dbl_to_ptr<LLVMOpaqueContext>(ctx), dbl_to_cstr(name)));
}
double flux_llvm_struct_set_body(double struct_ty, double elem_count, double packed)
{
    LLVMTypeRef* elemTypes = g_type_args.empty() ? nullptr : g_type_args.data();
    LLVMStructSetBody(dbl_to_ptr<LLVMOpaqueType>(struct_ty), elemTypes,
                      static_cast<unsigned>(dbl_as_u64(elem_count)),
                      static_cast<int>(dbl_as_u64(packed)));
    return 0.0;
}
double flux_llvm_array_type(double elem_ty, double elem_count)
{
    return ptr_to_dbl(LLVMArrayType(dbl_to_ptr<LLVMOpaqueType>(elem_ty),
                                    static_cast<unsigned>(dbl_as_u64(elem_count))));
}
double flux_llvm_get_element_type(double ty)
{
    return ptr_to_dbl(LLVMGetElementType(dbl_to_ptr<LLVMOpaqueType>(ty)));
}
double flux_llvm_get_type_kind(double ty)
{
    return static_cast<double>(LLVMGetTypeKind(dbl_to_ptr<LLVMOpaqueType>(ty)));
}
double flux_llvm_struct_get_type_index(double struct_ty, double i)
{
    return static_cast<double>(LLVMStructGetTypeIndex(dbl_to_ptr<LLVMOpaqueType>(struct_ty),
                                                       static_cast<unsigned>(dbl_as_u64(i))));
}

/* ------------------------------------------------------------------ */
/*  Builder                                                           */
/* ------------------------------------------------------------------ */

double flux_llvm_create_builder_in_ctx(double ctx)
{
    return ptr_to_dbl(LLVMCreateBuilderInContext(dbl_to_ptr<LLVMOpaqueContext>(ctx)));
}
double flux_llvm_create_builder()
{
    return ptr_to_dbl(LLVMCreateBuilder());
}
double flux_llvm_position_builder_at_end(double builder, double block)
{
    LLVMPositionBuilderAtEnd(dbl_to_ptr<LLVMOpaqueBuilder>(builder),
                             dbl_to_ptr<LLVMOpaqueBasicBlock>(block));
    return 0.0;
}
double flux_llvm_get_insert_block(double builder)
{
    return ptr_to_dbl(LLVMGetInsertBlock(dbl_to_ptr<LLVMOpaqueBuilder>(builder)));
}
double flux_llvm_dispose_builder(double builder)
{
    LLVMDisposeBuilder(dbl_to_ptr<LLVMOpaqueBuilder>(builder));
    return 0.0;
}
double flux_llvm_clear_insertion_position(double builder)
{
    LLVMClearInsertionPosition(dbl_to_ptr<LLVMOpaqueBuilder>(builder));
    return 0.0;
}

/* ------------------------------------------------------------------ */
/*  Functions / Params / BasicBlocks                                  */
/* ------------------------------------------------------------------ */

double flux_llvm_add_function(double module, double name, double fn_type)
{
    return ptr_to_dbl(LLVMAddFunction(dbl_to_ptr<LLVMOpaqueModule>(module),
                                      dbl_to_cstr(name),
                                      dbl_to_ptr<LLVMOpaqueType>(fn_type)));
}
double flux_llvm_get_named_function(double module, double name)
{
    return ptr_to_dbl(LLVMGetNamedFunction(dbl_to_ptr<LLVMOpaqueModule>(module), dbl_to_cstr(name)));
}
double flux_llvm_get_param(double fn, double index)
{
    return ptr_to_dbl(LLVMGetParam(dbl_to_ptr<LLVMOpaqueValue>(fn), static_cast<unsigned>(dbl_as_u64(index))));
}
double flux_llvm_get_first_param(double fn)
{
    return ptr_to_dbl(LLVMGetFirstParam(dbl_to_ptr<LLVMOpaqueValue>(fn)));
}
double flux_llvm_count_params(double fn)
{
    return static_cast<double>(LLVMCountParams(dbl_to_ptr<LLVMOpaqueValue>(fn)));
}
double flux_llvm_append_basic_block(double fn, double name)
{
    return ptr_to_dbl(LLVMAppendBasicBlock(dbl_to_ptr<LLVMOpaqueValue>(fn), dbl_to_cstr(name)));
}
double flux_llvm_get_first_basic_block(double fn)
{
    return ptr_to_dbl(LLVMGetFirstBasicBlock(dbl_to_ptr<LLVMOpaqueValue>(fn)));
}
double flux_llvm_get_basic_block_terminator(double block)
{
    return ptr_to_dbl(LLVMGetBasicBlockTerminator(dbl_to_ptr<LLVMOpaqueBasicBlock>(block)));
}
double flux_llvm_set_value_name(double val, double name)
{
    LLVMSetValueName(dbl_to_ptr<LLVMOpaqueValue>(val), dbl_to_cstr(name));
    return 0.0;
}
double flux_llvm_get_value_name(double val)
{
    return cstr_to_dbl(LLVMGetValueName(dbl_to_ptr<LLVMOpaqueValue>(val)));
}
double flux_llvm_type_of(double val)
{
    return ptr_to_dbl(LLVMTypeOf(dbl_to_ptr<LLVMOpaqueValue>(val)));
}

/* ------------------------------------------------------------------ */
/*  Constants                                                         */
/* ------------------------------------------------------------------ */

double flux_llvm_const_int(double int_ty, double value, double sign_extend)
{
    return ptr_to_dbl(LLVMConstInt(dbl_to_ptr<LLVMOpaqueType>(int_ty),
                                   static_cast<unsigned long long>(dbl_as_u64(value)),
                                   static_cast<int>(dbl_as_u64(sign_extend))));
}
double flux_llvm_const_real(double real_ty, double value)
{
    return ptr_to_dbl(LLVMConstReal(dbl_to_ptr<LLVMOpaqueType>(real_ty), dbl_as_u64(value)));
}
double flux_llvm_const_null(double ty)
{
    return ptr_to_dbl(LLVMConstNull(dbl_to_ptr<LLVMOpaqueType>(ty)));
}
double flux_llvm_const_string_in_ctx(double ctx, double str, double length, double dont_null_terminate)
{
    const char* s = dbl_to_cstr(str);
    return ptr_to_dbl(LLVMConstStringInContext(dbl_to_ptr<LLVMOpaqueContext>(ctx), s,
                                               static_cast<unsigned>(dbl_as_u64(length)),
                                               static_cast<int>(dbl_as_u64(dont_null_terminate))));
}
double flux_llvm_const_struct_in_ctx(double ctx, double elem_count, double packed)
{
    LLVMValueRef* elems = g_const_args.empty() ? nullptr : g_const_args.data();
    return ptr_to_dbl(LLVMConstStructInContext(dbl_to_ptr<LLVMOpaqueContext>(ctx), elems,
                                               static_cast<unsigned>(dbl_as_u64(elem_count)),
                                               static_cast<int>(dbl_as_u64(packed))));
}
double flux_llvm_get_undef(double ty)
{
    return ptr_to_dbl(LLVMGetUndef(dbl_to_ptr<LLVMOpaqueType>(ty)));
}

/* ------------------------------------------------------------------ */
/*  Terminator Instructions                                           */
/* ------------------------------------------------------------------ */

double flux_llvm_build_ret_void(double builder)
{
    return ptr_to_dbl(LLVMBuildRetVoid(dbl_to_ptr<LLVMOpaqueBuilder>(builder)));
}
double flux_llvm_build_ret(double builder, double val)
{
    return ptr_to_dbl(LLVMBuildRet(dbl_to_ptr<LLVMOpaqueBuilder>(builder), dbl_to_ptr<LLVMOpaqueValue>(val)));
}
double flux_llvm_build_br(double builder, double dest)
{
    return ptr_to_dbl(LLVMBuildBr(dbl_to_ptr<LLVMOpaqueBuilder>(builder),
                                  dbl_to_ptr<LLVMOpaqueBasicBlock>(dest)));
}
double flux_llvm_build_cond_br(double builder, double cond, double then_bb, double else_bb)
{
    return ptr_to_dbl(LLVMBuildCondBr(dbl_to_ptr<LLVMOpaqueBuilder>(builder),
                                      dbl_to_ptr<LLVMOpaqueValue>(cond),
                                      dbl_to_ptr<LLVMOpaqueBasicBlock>(then_bb),
                                      dbl_to_ptr<LLVMOpaqueBasicBlock>(else_bb)));
}
double flux_llvm_build_unreachable(double builder)
{
    return ptr_to_dbl(LLVMBuildUnreachable(dbl_to_ptr<LLVMOpaqueBuilder>(builder)));
}

/* ------------------------------------------------------------------ */
/*  Arithmetic Instructions                                           */
/* ------------------------------------------------------------------ */

double flux_llvm_build_add(double builder, double lhs, double rhs, double name)
{
    return ptr_to_dbl(LLVMBuildAdd(dbl_to_ptr<LLVMOpaqueBuilder>(builder),
                                   dbl_to_ptr<LLVMOpaqueValue>(lhs), dbl_to_ptr<LLVMOpaqueValue>(rhs),
                                   dbl_to_cstr(name)));
}
double flux_llvm_build_fadd(double builder, double lhs, double rhs, double name)
{
    return ptr_to_dbl(LLVMBuildFAdd(dbl_to_ptr<LLVMOpaqueBuilder>(builder),
                                    dbl_to_ptr<LLVMOpaqueValue>(lhs), dbl_to_ptr<LLVMOpaqueValue>(rhs),
                                    dbl_to_cstr(name)));
}
double flux_llvm_build_sub(double builder, double lhs, double rhs, double name)
{
    return ptr_to_dbl(LLVMBuildSub(dbl_to_ptr<LLVMOpaqueBuilder>(builder),
                                   dbl_to_ptr<LLVMOpaqueValue>(lhs), dbl_to_ptr<LLVMOpaqueValue>(rhs),
                                   dbl_to_cstr(name)));
}
double flux_llvm_build_fsub(double builder, double lhs, double rhs, double name)
{
    return ptr_to_dbl(LLVMBuildFSub(dbl_to_ptr<LLVMOpaqueBuilder>(builder),
                                    dbl_to_ptr<LLVMOpaqueValue>(lhs), dbl_to_ptr<LLVMOpaqueValue>(rhs),
                                    dbl_to_cstr(name)));
}
double flux_llvm_build_mul(double builder, double lhs, double rhs, double name)
{
    return ptr_to_dbl(LLVMBuildMul(dbl_to_ptr<LLVMOpaqueBuilder>(builder),
                                   dbl_to_ptr<LLVMOpaqueValue>(lhs), dbl_to_ptr<LLVMOpaqueValue>(rhs),
                                   dbl_to_cstr(name)));
}
double flux_llvm_build_fmul(double builder, double lhs, double rhs, double name)
{
    return ptr_to_dbl(LLVMBuildFMul(dbl_to_ptr<LLVMOpaqueBuilder>(builder),
                                    dbl_to_ptr<LLVMOpaqueValue>(lhs), dbl_to_ptr<LLVMOpaqueValue>(rhs),
                                    dbl_to_cstr(name)));
}
double flux_llvm_build_sdiv(double builder, double lhs, double rhs, double name)
{
    return ptr_to_dbl(LLVMBuildSDiv(dbl_to_ptr<LLVMOpaqueBuilder>(builder),
                                    dbl_to_ptr<LLVMOpaqueValue>(lhs), dbl_to_ptr<LLVMOpaqueValue>(rhs),
                                    dbl_to_cstr(name)));
}
double flux_llvm_build_fdiv(double builder, double lhs, double rhs, double name)
{
    return ptr_to_dbl(LLVMBuildFDiv(dbl_to_ptr<LLVMOpaqueBuilder>(builder),
                                    dbl_to_ptr<LLVMOpaqueValue>(lhs), dbl_to_ptr<LLVMOpaqueValue>(rhs),
                                    dbl_to_cstr(name)));
}
double flux_llvm_build_srem(double builder, double lhs, double rhs, double name)
{
    return ptr_to_dbl(LLVMBuildSRem(dbl_to_ptr<LLVMOpaqueBuilder>(builder),
                                    dbl_to_ptr<LLVMOpaqueValue>(lhs), dbl_to_ptr<LLVMOpaqueValue>(rhs),
                                    dbl_to_cstr(name)));
}
double flux_llvm_build_frem(double builder, double lhs, double rhs, double name)
{
    return ptr_to_dbl(LLVMBuildFRem(dbl_to_ptr<LLVMOpaqueBuilder>(builder),
                                    dbl_to_ptr<LLVMOpaqueValue>(lhs), dbl_to_ptr<LLVMOpaqueValue>(rhs),
                                    dbl_to_cstr(name)));
}
double flux_llvm_build_neg(double builder, double v, double name)
{
    return ptr_to_dbl(LLVMBuildNeg(dbl_to_ptr<LLVMOpaqueBuilder>(builder),
                                   dbl_to_ptr<LLVMOpaqueValue>(v), dbl_to_cstr(name)));
}
double flux_llvm_build_fneg(double builder, double v, double name)
{
    return ptr_to_dbl(LLVMBuildFNeg(dbl_to_ptr<LLVMOpaqueBuilder>(builder),
                                    dbl_to_ptr<LLVMOpaqueValue>(v), dbl_to_cstr(name)));
}

/* ------------------------------------------------------------------ */
/*  Logical / Bitwise                                                 */
/* ------------------------------------------------------------------ */

double flux_llvm_build_and(double builder, double lhs, double rhs, double name)
{
    return ptr_to_dbl(LLVMBuildAnd(dbl_to_ptr<LLVMOpaqueBuilder>(builder),
                                   dbl_to_ptr<LLVMOpaqueValue>(lhs), dbl_to_ptr<LLVMOpaqueValue>(rhs),
                                   dbl_to_cstr(name)));
}
double flux_llvm_build_or(double builder, double lhs, double rhs, double name)
{
    return ptr_to_dbl(LLVMBuildOr(dbl_to_ptr<LLVMOpaqueBuilder>(builder),
                                  dbl_to_ptr<LLVMOpaqueValue>(lhs), dbl_to_ptr<LLVMOpaqueValue>(rhs),
                                  dbl_to_cstr(name)));
}
double flux_llvm_build_xor(double builder, double lhs, double rhs, double name)
{
    return ptr_to_dbl(LLVMBuildXor(dbl_to_ptr<LLVMOpaqueBuilder>(builder),
                                   dbl_to_ptr<LLVMOpaqueValue>(lhs), dbl_to_ptr<LLVMOpaqueValue>(rhs),
                                   dbl_to_cstr(name)));
}
double flux_llvm_build_shl(double builder, double lhs, double rhs, double name)
{
    return ptr_to_dbl(LLVMBuildShl(dbl_to_ptr<LLVMOpaqueBuilder>(builder),
                                   dbl_to_ptr<LLVMOpaqueValue>(lhs), dbl_to_ptr<LLVMOpaqueValue>(rhs),
                                   dbl_to_cstr(name)));
}
double flux_llvm_build_ashr(double builder, double lhs, double rhs, double name)
{
    return ptr_to_dbl(LLVMBuildAShr(dbl_to_ptr<LLVMOpaqueBuilder>(builder),
                                    dbl_to_ptr<LLVMOpaqueValue>(lhs), dbl_to_ptr<LLVMOpaqueValue>(rhs),
                                    dbl_to_cstr(name)));
}

/* ------------------------------------------------------------------ */
/*  Memory Instructions                                               */
/* ------------------------------------------------------------------ */

double flux_llvm_build_alloca(double builder, double ty, double name)
{
    return ptr_to_dbl(LLVMBuildAlloca(dbl_to_ptr<LLVMOpaqueBuilder>(builder),
                                      dbl_to_ptr<LLVMOpaqueType>(ty), dbl_to_cstr(name)));
}
double flux_llvm_build_load2(double builder, double ty, double ptr, double name)
{
    return ptr_to_dbl(LLVMBuildLoad2(dbl_to_ptr<LLVMOpaqueBuilder>(builder),
                                     dbl_to_ptr<LLVMOpaqueType>(ty), dbl_to_ptr<LLVMOpaqueValue>(ptr),
                                     dbl_to_cstr(name)));
}
double flux_llvm_build_store(double builder, double val, double ptr)
{
    return ptr_to_dbl(LLVMBuildStore(dbl_to_ptr<LLVMOpaqueBuilder>(builder),
                                     dbl_to_ptr<LLVMOpaqueValue>(val), dbl_to_ptr<LLVMOpaqueValue>(ptr)));
}
double flux_llvm_build_gep2(double builder, double ty, double ptr, double num_indices, double name)
{
    LLVMValueRef* indices = g_index_args.empty() ? nullptr : g_index_args.data();
    return ptr_to_dbl(LLVMBuildGEP2(dbl_to_ptr<LLVMOpaqueBuilder>(builder),
                                    dbl_to_ptr<LLVMOpaqueType>(ty), dbl_to_ptr<LLVMOpaqueValue>(ptr),
                                    indices, static_cast<unsigned>(dbl_as_u64(num_indices)),
                                    dbl_to_cstr(name)));
}
double flux_llvm_build_struct_gep2(double builder, double ty, double ptr, double index, double name)
{
    return ptr_to_dbl(LLVMBuildStructGEP2(dbl_to_ptr<LLVMOpaqueBuilder>(builder),
                                          dbl_to_ptr<LLVMOpaqueType>(ty), dbl_to_ptr<LLVMOpaqueValue>(ptr),
                                          static_cast<unsigned>(dbl_as_u64(index)), dbl_to_cstr(name)));
}
double flux_llvm_build_global_string_ptr(double builder, double str, double name)
{
    return ptr_to_dbl(LLVMBuildGlobalStringPtr(dbl_to_ptr<LLVMOpaqueBuilder>(builder),
                                               dbl_to_cstr(str), dbl_to_cstr(name)));
}

/* ------------------------------------------------------------------ */
/*  Call Instruction                                                  */
/* ------------------------------------------------------------------ */

double flux_llvm_build_call2(double builder, double func_ty, double fn, double num_args, double name)
{
    LLVMValueRef* args = g_call_args.empty() ? nullptr : g_call_args.data();
    return ptr_to_dbl(LLVMBuildCall2(dbl_to_ptr<LLVMOpaqueBuilder>(builder),
                                     dbl_to_ptr<LLVMOpaqueType>(func_ty), dbl_to_ptr<LLVMOpaqueValue>(fn),
                                     args, static_cast<unsigned>(dbl_as_u64(num_args)),
                                     dbl_to_cstr(name)));
}

/* ------------------------------------------------------------------ */
/*  Comparisons                                                       */
/* ------------------------------------------------------------------ */

double flux_llvm_build_icmp(double builder, double op, double lhs, double rhs, double name)
{
    return ptr_to_dbl(LLVMBuildICmp(dbl_to_ptr<LLVMOpaqueBuilder>(builder),
                                    static_cast<LLVMIntPredicate>(dbl_as_u64(op)),
                                    dbl_to_ptr<LLVMOpaqueValue>(lhs), dbl_to_ptr<LLVMOpaqueValue>(rhs),
                                    dbl_to_cstr(name)));
}
double flux_llvm_build_fcmp(double builder, double op, double lhs, double rhs, double name)
{
    return ptr_to_dbl(LLVMBuildFCmp(dbl_to_ptr<LLVMOpaqueBuilder>(builder),
                                    static_cast<LLVMRealPredicate>(dbl_as_u64(op)),
                                    dbl_to_ptr<LLVMOpaqueValue>(lhs), dbl_to_ptr<LLVMOpaqueValue>(rhs),
                                    dbl_to_cstr(name)));
}

/* ------------------------------------------------------------------ */
/*  Phi / Select                                                      */
/* ------------------------------------------------------------------ */

double flux_llvm_build_phi(double builder, double ty, double name)
{
    return ptr_to_dbl(LLVMBuildPhi(dbl_to_ptr<LLVMOpaqueBuilder>(builder),
                                   dbl_to_ptr<LLVMOpaqueType>(ty), dbl_to_cstr(name)));
}
double flux_llvm_build_select(double builder, double cond, double then_v, double else_v, double name)
{
    return ptr_to_dbl(LLVMBuildSelect(dbl_to_ptr<LLVMOpaqueBuilder>(builder),
                                      dbl_to_ptr<LLVMOpaqueValue>(cond),
                                      dbl_to_ptr<LLVMOpaqueValue>(then_v),
                                      dbl_to_ptr<LLVMOpaqueValue>(else_v), dbl_to_cstr(name)));
}
double flux_llvm_add_incoming(double phi, double val, double block)
{
    LLVMAddIncoming(dbl_to_ptr<LLVMOpaqueValue>(phi),
                    const_cast<LLVMValueRef*>(&dbl_to_ptr<LLVMOpaqueValue>(val)),
                    const_cast<LLVMBasicBlockRef*>(&dbl_to_ptr<LLVMOpaqueBasicBlock>(block)), 1);
    return 0.0;
}

/* ------------------------------------------------------------------ */
/*  Aggregate Instructions                                            */
/* ------------------------------------------------------------------ */

double flux_llvm_build_extract_value(double builder, double agg, double index, double name)
{
    return ptr_to_dbl(LLVMBuildExtractValue(dbl_to_ptr<LLVMOpaqueBuilder>(builder),
                                            dbl_to_ptr<LLVMOpaqueValue>(agg),
                                            static_cast<unsigned>(dbl_as_u64(index)), dbl_to_cstr(name)));
}
double flux_llvm_build_insert_value(double builder, double agg, double elem, double index, double name)
{
    return ptr_to_dbl(LLVMBuildInsertValue(dbl_to_ptr<LLVMOpaqueBuilder>(builder),
                                           dbl_to_ptr<LLVMOpaqueValue>(agg),
                                           dbl_to_ptr<LLVMOpaqueValue>(elem),
                                           static_cast<unsigned>(dbl_as_u64(index)), dbl_to_cstr(name)));
}

/* ------------------------------------------------------------------ */
/*  Cast Instructions                                                 */
/* ------------------------------------------------------------------ */

double flux_llvm_build_trunc(double builder, double val, double dest_ty, double name)
{
    return ptr_to_dbl(LLVMBuildTrunc(dbl_to_ptr<LLVMOpaqueBuilder>(builder),
                                     dbl_to_ptr<LLVMOpaqueValue>(val),
                                     dbl_to_ptr<LLVMOpaqueType>(dest_ty), dbl_to_cstr(name)));
}
double flux_llvm_build_zext(double builder, double val, double dest_ty, double name)
{
    return ptr_to_dbl(LLVMBuildZExt(dbl_to_ptr<LLVMOpaqueBuilder>(builder),
                                    dbl_to_ptr<LLVMOpaqueValue>(val),
                                    dbl_to_ptr<LLVMOpaqueType>(dest_ty), dbl_to_cstr(name)));
}
double flux_llvm_build_sext(double builder, double val, double dest_ty, double name)
{
    return ptr_to_dbl(LLVMBuildSExt(dbl_to_ptr<LLVMOpaqueBuilder>(builder),
                                    dbl_to_ptr<LLVMOpaqueValue>(val),
                                    dbl_to_ptr<LLVMOpaqueType>(dest_ty), dbl_to_cstr(name)));
}
double flux_llvm_build_fptosi(double builder, double val, double dest_ty, double name)
{
    return ptr_to_dbl(LLVMBuildFPToSI(dbl_to_ptr<LLVMOpaqueBuilder>(builder),
                                      dbl_to_ptr<LLVMOpaqueValue>(val),
                                      dbl_to_ptr<LLVMOpaqueType>(dest_ty), dbl_to_cstr(name)));
}
double flux_llvm_build_sitofp(double builder, double val, double dest_ty, double name)
{
    return ptr_to_dbl(LLVMBuildSIToFP(dbl_to_ptr<LLVMOpaqueBuilder>(builder),
                                      dbl_to_ptr<LLVMOpaqueValue>(val),
                                      dbl_to_ptr<LLVMOpaqueType>(dest_ty), dbl_to_cstr(name)));
}
double flux_llvm_build_ptrtoint(double builder, double val, double dest_ty, double name)
{
    return ptr_to_dbl(LLVMBuildPtrToInt(dbl_to_ptr<LLVMOpaqueBuilder>(builder),
                                        dbl_to_ptr<LLVMOpaqueValue>(val),
                                        dbl_to_ptr<LLVMOpaqueType>(dest_ty), dbl_to_cstr(name)));
}
double flux_llvm_build_inttoptr(double builder, double val, double dest_ty, double name)
{
    return ptr_to_dbl(LLVMBuildIntToPtr(dbl_to_ptr<LLVMOpaqueBuilder>(builder),
                                        dbl_to_ptr<LLVMOpaqueValue>(val),
                                        dbl_to_ptr<LLVMOpaqueType>(dest_ty), dbl_to_cstr(name)));
}
double flux_llvm_build_bitcast(double builder, double val, double dest_ty, double name)
{
    return ptr_to_dbl(LLVMBuildBitCast(dbl_to_ptr<LLVMOpaqueBuilder>(builder),
                                       dbl_to_ptr<LLVMOpaqueValue>(val),
                                       dbl_to_ptr<LLVMOpaqueType>(dest_ty), dbl_to_cstr(name)));
}

/* ------------------------------------------------------------------ */
/*  Verification                                                      */
/* ------------------------------------------------------------------ */

double flux_llvm_verify_module(double module, double action, double out_message)
{
    char* err = nullptr;
    LLVMBool result =
        LLVMVerifyModule(dbl_to_ptr<LLVMOpaqueModule>(module),
                         static_cast<LLVMVerifierFailureAction>(dbl_as_u64(action)), &err);
    if (err) {
        LLVMDisposeMessage(err);
    }
    return static_cast<double>(result);
}
double flux_llvm_verify_function(double fn, double action)
{
    return static_cast<double>(
        LLVMVerifyFunction(dbl_to_ptr<LLVMOpaqueValue>(fn),
                           static_cast<LLVMVerifierFailureAction>(dbl_as_u64(action))));
}

/* ------------------------------------------------------------------ */
/*  Target Machine                                                    */
/* ------------------------------------------------------------------ */

double flux_llvm_get_target_from_triple(double triple, double out_target)
{
    LLVMTargetRef target = nullptr;
    char* err = nullptr;
    if (LLVMGetTargetFromTriple(dbl_to_cstr(triple), &target, &err)) {
        if (err) LLVMDisposeMessage(err);
        return 0.0;
    }
    return ptr_to_dbl(target);
}
double flux_llvm_create_target_machine(double target, double triple, double cpu,
                                        double features, double opt_level,
                                        double reloc, double code_model)
{
    return ptr_to_dbl(LLVMCreateTargetMachine(
        reinterpret_cast<LLVMTargetRef>(reinterpret_cast<void*>(static_cast<uintptr_t>(dbl_as_u64(target)))),
        dbl_to_cstr(triple), dbl_to_cstr(cpu), dbl_to_cstr(features),
        static_cast<LLVMCodeGenOptLevel>(dbl_as_u64(opt_level)),
        static_cast<LLVMRelocMode>(dbl_as_u64(reloc)),
        static_cast<LLVMCodeModel>(dbl_as_u64(code_model))));
}
double flux_llvm_target_machine_emit_to_file(double tm, double module, double filename, double file_type)
{
    char* err = nullptr;
    if (LLVMTargetMachineEmitToFile(dbl_to_ptr<LLVMOpaqueTargetMachine>(tm),
                                     dbl_to_ptr<LLVMOpaqueModule>(module),
                                     const_cast<char*>(dbl_to_cstr(filename)),
                                     static_cast<LLVMCodeGenFileType>(dbl_as_u64(file_type)), &err)) {
        if (err) LLVMDisposeMessage(err);
        return 1.0;
    }
    return 0.0;
}
double flux_llvm_target_machine_emit_to_mem_buf(double tm, double module, double file_type)
{
    char* err = nullptr;
    LLVMMemoryBufferRef buf = nullptr;
    if (LLVMTargetMachineEmitToMemoryBuffer(dbl_to_ptr<LLVMOpaqueTargetMachine>(tm),
                                             dbl_to_ptr<LLVMOpaqueModule>(module),
                                             static_cast<LLVMCodeGenFileType>(dbl_as_u64(file_type)),
                                             &err, &buf)) {
        if (err) LLVMDisposeMessage(err);
        return 0.0;
    }
    return ptr_to_dbl(buf);
}
double flux_llvm_initialize_native_target()
{
    return static_cast<double>(LLVMInitializeNativeTarget());
}
double flux_llvm_initialize_native_asm_printer()
{
    return static_cast<double>(LLVMInitializeNativeAsmPrinter());
}

/* ------------------------------------------------------------------ */
/*  BitWriter                                                         */
/* ------------------------------------------------------------------ */

double flux_llvm_write_bitcode_to_file(double module, double path)
{
    if (LLVMWriteBitcodeToFile(dbl_to_ptr<LLVMOpaqueModule>(module), dbl_to_cstr(path)))
        return 1.0;
    return 0.0;
}

/* ------------------------------------------------------------------ */
/*  Debug Info                                                        */
/* ------------------------------------------------------------------ */

double flux_llvm_get_current_debug_location(double builder)
{
    return ptr_to_dbl(LLVMGetCurrentDebugLocation2(dbl_to_ptr<LLVMOpaqueBuilder>(builder)));
}
double flux_llvm_set_current_debug_location(double builder, double loc)
{
    LLVMSetCurrentDebugLocation2(dbl_to_ptr<LLVMOpaqueBuilder>(builder),
                                  dbl_to_ptr<LLVMOpaqueMetadata>(loc));
    return 0.0;
}

/* ------------------------------------------------------------------ */
/*  HashMap for symbol tables (String -> Double)                      */
/* ------------------------------------------------------------------ */

double flux_hm_create()
{
    uint64_t id = g_next_hm_id++;
    g_hmaps[id] = {};
    return u64_as_dbl(id);
}
double flux_hm_destroy(double hm_id)
{
    g_hmaps.erase(dbl_as_u64(hm_id));
    return 0.0;
}
double flux_hm_put(double hm_id, double key, double val)
{
    uint64_t id = dbl_as_u64(hm_id);
    const char* k = dbl_to_cstr(key);
    g_hmaps[id][std::string(k)] = val;
    return 0.0;
}
double flux_hm_get(double hm_id, double key)
{
    uint64_t id = dbl_as_u64(hm_id);
    const char* k = dbl_to_cstr(key);
    auto it = g_hmaps[id].find(std::string(k));
    if (it != g_hmaps[id].end())
        return it->second;
    return 0.0;
}
double flux_hm_has(double hm_id, double key)
{
    uint64_t id = dbl_as_u64(hm_id);
    const char* k = dbl_to_cstr(key);
    return g_hmaps[id].find(std::string(k)) != g_hmaps[id].end() ? 1.0 : 0.0;
}
double flux_hm_remove(double hm_id, double key)
{
    uint64_t id = dbl_as_u64(hm_id);
    const char* k = dbl_to_cstr(key);
    return static_cast<double>(g_hmaps[id].erase(std::string(k)));
}
double flux_hm_size(double hm_id)
{
    uint64_t id = dbl_as_u64(hm_id);
    return static_cast<double>(g_hmaps[id].size());
}
double flux_hm_keys(double hm_id)
{
    uint64_t id = dbl_as_u64(hm_id);
    auto& m = g_hmaps[id];
    std::string result;
    for (auto& [k, v] : m) {
        if (!result.empty()) result += "\n";
        result += k;
    }
    g_llvm_pool.push_back(result);
    return u64_as_dbl(reinterpret_cast<uintptr_t>(g_llvm_pool.back().c_str()));
}

} // extern "C"
