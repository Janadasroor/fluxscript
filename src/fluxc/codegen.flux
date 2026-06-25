// FluxScript bootstrapped compiler backend: LLVM C-API bindings and IR code generation.
// All LLVM opaque types (LLVMModuleRef, LLVMValueRef, etc.) are passed as Double.
// Strings are passed as Double (bitcast char* pointer).
// Unsigned/boolean values are passed as Double.

// --- Context ---
extern def flux_llvm_context_create() -> Double
extern def flux_llvm_context_dispose(ctx: Double) -> Void

// --- Module ---
extern def flux_llvm_module_create_with_name(name: String) -> Double
extern def flux_llvm_module_create_with_name_in_ctx(name: String, ctx: Double) -> Double
extern def flux_llvm_get_module_context(module: Double) -> Double
extern def flux_llvm_dispose_module(module: Double) -> Void
extern def flux_llvm_print_module_to_string(module: Double) -> String
extern def flux_llvm_dispose_message(msg: Double) -> Void

// --- Types ---
extern def flux_llvm_void_type_in_ctx(ctx: Double) -> Double
extern def flux_llvm_int1_type_in_ctx(ctx: Double) -> Double
extern def flux_llvm_int8_type_in_ctx(ctx: Double) -> Double
extern def flux_llvm_int32_type_in_ctx(ctx: Double) -> Double
extern def flux_llvm_int64_type_in_ctx(ctx: Double) -> Double
extern def flux_llvm_double_type_in_ctx(ctx: Double) -> Double
extern def flux_llvm_float_type_in_ctx(ctx: Double) -> Double
extern def flux_llvm_pointer_type_in_ctx(ctx: Double, addr_space: Double) -> Double
extern def flux_llvm_function_type(ret_ty: Double, param_count: Double, is_var_arg: Double) -> Double
extern def flux_llvm_struct_type_in_ctx(ctx: Double, elem_count: Double, packed: Double) -> Double
extern def flux_llvm_struct_create_named(ctx: Double, name: String) -> Double
extern def flux_llvm_struct_set_body(struct_ty: Double, elem_count: Double, packed: Double) -> Void
extern def flux_llvm_array_type(elem_ty: Double, elem_count: Double) -> Double
extern def flux_llvm_get_element_type(ty: Double) -> Double
extern def flux_llvm_get_type_kind(ty: Double) -> Double
extern def flux_llvm_struct_get_type_index(struct_ty: Double, i: Double) -> Double

// --- Builder ---
extern def flux_llvm_create_builder_in_ctx(ctx: Double) -> Double
extern def flux_llvm_create_builder() -> Double
extern def flux_llvm_position_builder_at_end(builder: Double, block: Double) -> Void
extern def flux_llvm_get_insert_block(builder: Double) -> Double
extern def flux_llvm_dispose_builder(builder: Double) -> Void
extern def flux_llvm_clear_insertion_position(builder: Double) -> Void

// --- Functions / Params / BasicBlocks ---
extern def flux_llvm_add_function(module: Double, name: String, fn_type: Double) -> Double
extern def flux_llvm_get_named_function(module: Double, name: String) -> Double
extern def flux_llvm_get_param(fn: Double, index: Double) -> Double
extern def flux_llvm_get_first_param(fn: Double) -> Double
extern def flux_llvm_count_params(fn: Double) -> Double
extern def flux_llvm_append_basic_block(fn: Double, name: String) -> Double
extern def flux_llvm_get_first_basic_block(fn: Double) -> Double
extern def flux_llvm_get_basic_block_terminator(block: Double) -> Double
extern def flux_llvm_set_value_name(val: Double, name: String) -> Void
extern def flux_llvm_get_value_name(val: Double) -> String
extern def flux_llvm_type_of(val: Double) -> Double
extern def flux_llvm_add_global(module: Double, ty: Double, name: String) -> Double
extern def flux_llvm_set_initializer(global: Double, const_val: Double) -> Void
extern def flux_llvm_set_linkage(global: Double, linkage: Double) -> Void
extern def flux_llvm_global_get_value_type(global_val: Double) -> Double

// --- Constants ---
extern def flux_llvm_const_int(int_ty: Double, value: Double, sign_extend: Double) -> Double
extern def flux_llvm_const_real(real_ty: Double, value: Double) -> Double
extern def flux_llvm_const_null(ty: Double) -> Double
extern def flux_llvm_const_string_in_ctx(ctx: Double, str: String, length: Double, dont_null_terminate: Double) -> Double
extern def flux_llvm_const_struct_in_ctx(ctx: Double, elem_count: Double, packed: Double) -> Double
extern def flux_llvm_const_named_struct(struct_ty: Double, elem_count: Double) -> Double
extern def flux_llvm_const_bitcast(val: Double, dest_ty: Double) -> Double
extern def flux_llvm_get_undef(ty: Double) -> Double

// --- Terminator Instructions ---
extern def flux_llvm_build_ret_void(builder: Double) -> Double
extern def flux_llvm_build_ret(builder: Double, val: Double) -> Double
extern def flux_llvm_build_br(builder: Double, dest: Double) -> Double
extern def flux_llvm_build_cond_br(builder: Double, cond: Double, then_bb: Double, else_bb: Double) -> Double
extern def flux_llvm_build_unreachable(builder: Double) -> Double

// --- Arithmetic Instructions ---
extern def flux_llvm_build_add(builder: Double, lhs: Double, rhs: Double, name: String) -> Double
extern def flux_llvm_build_fadd(builder: Double, lhs: Double, rhs: Double, name: String) -> Double
extern def flux_llvm_build_sub(builder: Double, lhs: Double, rhs: Double, name: String) -> Double
extern def flux_llvm_build_fsub(builder: Double, lhs: Double, rhs: Double, name: String) -> Double
extern def flux_llvm_build_mul(builder: Double, lhs: Double, rhs: Double, name: String) -> Double
extern def flux_llvm_build_fmul(builder: Double, lhs: Double, rhs: Double, name: String) -> Double
extern def flux_llvm_build_sdiv(builder: Double, lhs: Double, rhs: Double, name: String) -> Double
extern def flux_llvm_build_fdiv(builder: Double, lhs: Double, rhs: Double, name: String) -> Double
extern def flux_llvm_build_srem(builder: Double, lhs: Double, rhs: Double, name: String) -> Double
extern def flux_llvm_build_frem(builder: Double, lhs: Double, rhs: Double, name: String) -> Double
extern def flux_llvm_build_neg(builder: Double, v: Double, name: String) -> Double
extern def flux_llvm_build_fneg(builder: Double, v: Double, name: String) -> Double

// --- Logical / Bitwise ---
extern def flux_llvm_build_and(builder: Double, lhs: Double, rhs: Double, name: String) -> Double
extern def flux_llvm_build_or(builder: Double, lhs: Double, rhs: Double, name: String) -> Double
extern def flux_llvm_build_xor(builder: Double, lhs: Double, rhs: Double, name: String) -> Double
extern def flux_llvm_build_shl(builder: Double, lhs: Double, rhs: Double, name: String) -> Double
extern def flux_llvm_build_ashr(builder: Double, lhs: Double, rhs: Double, name: String) -> Double

// --- Memory Instructions ---
extern def flux_llvm_build_alloca(builder: Double, ty: Double, name: String) -> Double
extern def flux_llvm_build_load2(builder: Double, ty: Double, ptr: Double, name: String) -> Double
extern def flux_llvm_build_store(builder: Double, val: Double, ptr: Double) -> Double
extern def flux_llvm_build_gep2(builder: Double, ty: Double, ptr: Double, num_indices: Double, name: String) -> Double
extern def flux_llvm_build_struct_gep2(builder: Double, ty: Double, ptr: Double, index: Double, name: String) -> Double
extern def flux_llvm_build_global_string_ptr(builder: Double, str: String, name: String) -> Double

// --- Call Instruction ---
extern def flux_llvm_build_call2(builder: Double, func_ty: Double, fn: Double, num_args: Double, name: String) -> Double

// --- Collectors (thread-local C-array builders) ---
extern def flux_llvm_call_arg_push(val: Double) -> Double
extern def flux_llvm_call_arg_reset() -> Double
extern def flux_llvm_type_arg_push(ty: Double) -> Double
extern def flux_llvm_type_arg_reset() -> Double
extern def flux_llvm_index_arg_push(idx: Double) -> Double
extern def flux_llvm_index_arg_reset() -> Double
extern def flux_llvm_const_arg_push(val: Double) -> Double
extern def flux_llvm_const_arg_reset() -> Double

// --- Comparisons ---
extern def flux_llvm_build_icmp(builder: Double, op: Double, lhs: Double, rhs: Double, name: String) -> Double
extern def flux_llvm_build_fcmp(builder: Double, op: Double, lhs: Double, rhs: Double, name: String) -> Double

// --- Phi / Select ---
extern def flux_llvm_build_phi(builder: Double, ty: Double, name: String) -> Double
extern def flux_llvm_build_select(builder: Double, cond: Double, then_v: Double, else_v: Double, name: String) -> Double
extern def flux_llvm_add_incoming(phi: Double, val: Double, block: Double) -> Double

// --- Aggregate Instructions ---
extern def flux_llvm_build_extract_value(builder: Double, agg: Double, index: Double, name: String) -> Double
extern def flux_llvm_build_insert_value(builder: Double, agg: Double, elem: Double, index: Double, name: String) -> Double

// --- Cast Instructions ---
extern def flux_llvm_build_trunc(builder: Double, val: Double, dest_ty: Double, name: String) -> Double
extern def flux_llvm_build_zext(builder: Double, val: Double, dest_ty: Double, name: String) -> Double
extern def flux_llvm_build_sext(builder: Double, val: Double, dest_ty: Double, name: String) -> Double
extern def flux_llvm_build_fptosi(builder: Double, val: Double, dest_ty: Double, name: String) -> Double
extern def flux_llvm_build_sitofp(builder: Double, val: Double, dest_ty: Double, name: String) -> Double
extern def flux_llvm_build_uitofp(builder: Double, val: Double, dest_ty: Double, name: String) -> Double
extern def flux_llvm_build_ptrtoint(builder: Double, val: Double, dest_ty: Double, name: String) -> Double
extern def flux_llvm_build_inttoptr(builder: Double, val: Double, dest_ty: Double, name: String) -> Double
extern def flux_llvm_build_bitcast(builder: Double, val: Double, dest_ty: Double, name: String) -> Double

// --- Verification ---
extern def flux_llvm_verify_module(module: Double, action: Double, out_message: Double) -> Double
extern def flux_llvm_verify_function(fn: Double, action: Double) -> Double

// --- Target Machine ---
extern def flux_llvm_get_target_from_triple(triple: String) -> Double
extern def flux_llvm_create_target_machine(triple: String, cpu: String, features: String, opt_level: Double, reloc: Double, code_model: Double) -> Double
extern def flux_llvm_target_machine_emit_to_file(tm: Double, module: Double, filename: String, file_type: Double) -> Double
extern def flux_llvm_target_machine_emit_to_mem_buf(tm: Double, module: Double, file_type: Double) -> Double
extern def flux_llvm_initialize_native_target() -> Double
extern def flux_llvm_initialize_native_asm_printer() -> Double

// --- BitWriter ---
extern def flux_llvm_write_bitcode_to_file(module: Double, path: String) -> Double

// --- Debug Info ---
extern def flux_llvm_get_current_debug_location(builder: Double) -> Double
extern def flux_llvm_set_current_debug_location(builder: Double, loc: Double) -> Void

// --- HashMap (symbol table) ---
extern def flux_hm_create() -> Double
extern def flux_hm_destroy(hm: Double) -> Double
extern def flux_hm_put(hm: Double, key: String, val: Double) -> Double
extern def flux_hm_get(hm: Double, key: String) -> Double
extern def flux_hm_has(hm: Double, key: String) -> Double
extern def flux_hm_remove(hm: Double, key: String) -> Double
extern def flux_hm_size(hm: Double) -> Double
extern def flux_hm_keys(hm: Double) -> String

// --- Dedicated type map ---
extern def flux_type_store(name: String, type_val: Double) -> Double
extern def flux_type_load(name: String) -> Double
extern def flux_type_has(name: String) -> Double

class Codegen {
    context: Double
    module: Double
    builder: Double

    def init(self) {
        self.context = flux_llvm_context_create();
        self.module = flux_llvm_module_create_with_name("fluxc_module");
        self.builder = flux_llvm_create_builder_in_ctx(self.context);
        flux_llvm_initialize_native_target();
        flux_llvm_initialize_native_asm_printer();
    }

    def destroy(self) {
        flux_llvm_dispose_builder(self.builder);
        flux_llvm_dispose_module(self.module);
        flux_llvm_context_dispose(self.context);
    }

    def emit_module_ir(self) -> String {
        flux_llvm_print_module_to_string(self.module)
    }
}
