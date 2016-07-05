/*
 * Copyright © 2016 Bas Nieuwenhuizen
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "ac_nir_to_llvm.h"
#include "ac_binary.h"

#include "nir/nir.h"

enum radeon_llvm_calling_convention {
	RADEON_LLVM_AMDGPU_VS = 87,
	RADEON_LLVM_AMDGPU_GS = 88,
	RADEON_LLVM_AMDGPU_PS = 89,
	RADEON_LLVM_AMDGPU_CS = 90,
};

#define CONST_ADDR_SPACE 2
#define LOCAL_ADDR_SPACE 3

struct nir_to_llvm_context {
	LLVMContextRef context;
	LLVMModuleRef module;
	LLVMBuilderRef builder;
	LLVMValueRef main_function;

	struct hash_table *defs;

	LLVMValueRef descriptor_sets[4];
	LLVMValueRef workgroup_ids;
	LLVMValueRef local_invocation_ids;

	LLVMBasicBlockRef continue_block;
	LLVMBasicBlockRef break_block;

	LLVMTypeRef i1;
	LLVMTypeRef i32;
	LLVMTypeRef v4i32;
	LLVMTypeRef f32;

	unsigned uniform_md_kind;
	LLVMValueRef empty_md;
};

static void set_llvm_calling_convention(LLVMValueRef func,
                                        gl_shader_stage stage)
{
	enum radeon_llvm_calling_convention calling_conv;

	switch (stage) {
	case MESA_SHADER_VERTEX:
	case MESA_SHADER_TESS_CTRL:
	case MESA_SHADER_TESS_EVAL:
		calling_conv = RADEON_LLVM_AMDGPU_VS;
		break;
	case MESA_SHADER_GEOMETRY:
		calling_conv = RADEON_LLVM_AMDGPU_GS;
		break;
	case MESA_SHADER_FRAGMENT:
		calling_conv = RADEON_LLVM_AMDGPU_PS;
		break;
	case MESA_SHADER_COMPUTE:
		calling_conv = RADEON_LLVM_AMDGPU_CS;
		break;
	default:
		unreachable("Unhandle shader type");
	}

	LLVMSetFunctionCallConv(func, calling_conv);
}

static LLVMValueRef
create_llvm_function(LLVMContextRef ctx, LLVMModuleRef module,
                     LLVMBuilderRef builder, LLVMTypeRef *return_types,
                     unsigned num_return_elems, LLVMTypeRef *param_types,
                     unsigned param_count, unsigned array_params,
                     unsigned sgpr_params)
{
	LLVMTypeRef main_function_type, ret_type;
	LLVMBasicBlockRef main_function_body;

	if (num_return_elems)
		ret_type = LLVMStructTypeInContext(ctx, return_types,
		                                   num_return_elems, true);
	else
		ret_type = LLVMVoidTypeInContext(ctx);

	/* Setup the function */
	main_function_type =
	    LLVMFunctionType(ret_type, param_types, param_count, 0);
	LLVMValueRef main_function =
	    LLVMAddFunction(module, "main", main_function_type);
	main_function_body =
	    LLVMAppendBasicBlockInContext(ctx, main_function, "main_body");
	LLVMPositionBuilderAtEnd(builder, main_function_body);

	LLVMSetFunctionCallConv(main_function, RADEON_LLVM_AMDGPU_CS);
	for (unsigned i = 0; i < sgpr_params; ++i) {
		LLVMValueRef P = LLVMGetParam(main_function, i);

		if (i < array_params)
			LLVMAddAttribute(P, LLVMByValAttribute);
		else
			LLVMAddAttribute(P, LLVMInRegAttribute);
	}
	return main_function;
}

static LLVMTypeRef const_array(LLVMTypeRef elem_type, int num_elements)
{
	return LLVMPointerType(LLVMArrayType(elem_type, num_elements),
	                       CONST_ADDR_SPACE);
}

static void create_function(struct nir_to_llvm_context *ctx,
                            struct nir_shader *nir)
{
	LLVMTypeRef arg_types[20];
	unsigned arg_idx = 0;
	unsigned array_count = 0;
	unsigned sgpr_count = 0;

	for (unsigned i = 0; i < 4; ++i)
		arg_types[arg_idx++] = const_array(ctx->i32, 1024 * 1024);

	array_count = arg_idx;
	switch (nir->stage) {
	case MESA_SHADER_COMPUTE:
		arg_types[arg_idx++] = LLVMVectorType(ctx->i32, 3);
		sgpr_count = arg_idx;

		arg_types[arg_idx++] = LLVMVectorType(ctx->i32, 3);
		break;
	default:
		unreachable("Shader stage not implemented");
	}

	ctx->main_function = create_llvm_function(
	    ctx->context, ctx->module, ctx->builder, NULL, 0, arg_types,
	    arg_idx, array_count, sgpr_count);
	set_llvm_calling_convention(ctx->main_function, nir->stage);

	arg_idx = 0;

	for (unsigned i = 0; i < 4; ++i)
		ctx->descriptor_sets[i] =
		    LLVMGetParam(ctx->main_function, arg_idx++);

	switch (nir->stage) {
	case MESA_SHADER_COMPUTE:
		ctx->workgroup_ids =
		    LLVMGetParam(ctx->main_function, arg_idx++);
		ctx->local_invocation_ids =
		    LLVMGetParam(ctx->main_function, arg_idx++);
		break;
	default:
		unreachable("Shader stage not implemented");
	}
}

static void setup_types(struct nir_to_llvm_context *ctx)
{
	ctx->i1 = LLVMIntTypeInContext(ctx->context, 1);
	ctx->i32 = LLVMIntTypeInContext(ctx->context, 32);
	ctx->v4i32 = LLVMVectorType(ctx->i32, 4);
	ctx->f32 = LLVMFloatTypeInContext(ctx->context);

	ctx->uniform_md_kind =
	    LLVMGetMDKindIDInContext(ctx->context, "amdgpu.uniform", 14);
	ctx->empty_md = LLVMMDNodeInContext(ctx->context, NULL, 0);
}

static LLVMValueRef trim_vector(struct nir_to_llvm_context *ctx,
                                LLVMValueRef value, unsigned count)
{
	LLVMTypeRef type = LLVMTypeOf(value);
	unsigned num_components = LLVMGetTypeKind(type) == LLVMVectorTypeKind
	                              ? LLVMGetVectorSize(type)
	                              : 1;
	if (count == num_components)
		return value;

	LLVMValueRef masks[] = {
	    LLVMConstInt(ctx->i32, 0, false), LLVMConstInt(ctx->i32, 1, false),
	    LLVMConstInt(ctx->i32, 2, false), LLVMConstInt(ctx->i32, 3, false)};

	if (count == 1)
		return LLVMBuildExtractElement(ctx->builder, value, masks[0],
		                               "");

	LLVMValueRef swizzle = LLVMConstVector(masks, count);
	return LLVMBuildShuffleVector(ctx->builder, value, value, swizzle, "");
}

static LLVMTypeRef get_def_type(struct nir_to_llvm_context *ctx,
                                nir_ssa_def *def)
{
	LLVMTypeRef type = LLVMIntTypeInContext(ctx->context, def->bit_size);
	if (def->num_components > 1) {
		type = LLVMVectorType(type, def->num_components);
	}
	return type;
}

static LLVMValueRef get_src(struct nir_to_llvm_context *ctx, nir_src src)
{
	assert(src.is_ssa);
	struct hash_entry *entry = _mesa_hash_table_search(ctx->defs, src.ssa);
	return (LLVMValueRef)entry->data;
}

static LLVMValueRef get_alu_src(struct nir_to_llvm_context *ctx,
                                nir_alu_src src)
{
	LLVMValueRef value = get_src(ctx, src.src);
	bool need_swizzle = false;

	assert(value);
	LLVMTypeRef type = LLVMTypeOf(value);
	unsigned num_components = LLVMGetTypeKind(type) == LLVMVectorTypeKind
	                              ? LLVMGetVectorSize(type)
	                              : 0;
	for (unsigned i = 0; i < num_components; ++i)
		if (src.swizzle[i] != i)
			need_swizzle = true;
	if (need_swizzle) {
		LLVMValueRef masks[] = {
		    LLVMConstInt(ctx->i32, src.swizzle[0], false),
		    LLVMConstInt(ctx->i32, src.swizzle[1], false),
		    LLVMConstInt(ctx->i32, src.swizzle[2], false),
		    LLVMConstInt(ctx->i32, src.swizzle[3], false)};
		LLVMValueRef swizzle = LLVMConstVector(masks, num_components);
		value = LLVMBuildShuffleVector(ctx->builder, value, value,
		                               swizzle, "");
	}
	assert(!src.negate);
	assert(!src.abs);
	return value;
}

static LLVMValueRef emit_int_cmp(struct nir_to_llvm_context *ctx,
                                 LLVMIntPredicate pred, LLVMValueRef src0,
                                 LLVMValueRef src1)
{
	LLVMValueRef result = LLVMBuildICmp(ctx->builder, pred, src0, src1, "");
	return LLVMBuildSelect(ctx->builder, result,
	                       LLVMConstInt(ctx->i32, 0xFFFFFFFF, false),
	                       LLVMConstInt(ctx->i32, 0, false), "");
}

static void visit_alu(struct nir_to_llvm_context *ctx, nir_alu_instr *instr)
{
	LLVMValueRef src[4], result = NULL;

	assert(nir_op_infos[instr->op].num_inputs <= ARRAY_SIZE(src));
	for (unsigned i = 0; i < nir_op_infos[instr->op].num_inputs; i++)
		src[i] = trim_vector(ctx, get_alu_src(ctx, instr->src[i]),
		                     instr->dest.dest.ssa.num_components);

	switch (instr->op) {
	case nir_op_fmov:
	case nir_op_imov:
		result = src[0];
		break;
	case nir_op_iadd:
		result = LLVMBuildAdd(ctx->builder, src[0], src[1], "");
		break;
	case nir_op_isub:
		result = LLVMBuildSub(ctx->builder, src[0], src[1], "");
		break;
	case nir_op_imul:
		result = LLVMBuildMul(ctx->builder, src[0], src[1], "");
		break;
	case nir_op_seq:
		result = emit_int_cmp(ctx, LLVMIntEQ, src[0], src[1]);
		break;
	case nir_op_sne:
		result = emit_int_cmp(ctx, LLVMIntNE, src[0], src[1]);
		break;
	case nir_op_slt:
		result = emit_int_cmp(ctx, LLVMIntSLT, src[0], src[1]);
		break;
	case nir_op_sge:
		result = emit_int_cmp(ctx, LLVMIntSGE, src[0], src[1]);
		break;
	case nir_op_ult:
		result = emit_int_cmp(ctx, LLVMIntULT, src[0], src[1]);
		break;
	case nir_op_uge:
		result = emit_int_cmp(ctx, LLVMIntUGE, src[0], src[1]);
		break;
	default:
		fprintf(stderr, "Unknown NIR alu instr: ");
		nir_print_instr(&instr->instr, stderr);
		fprintf(stderr, "\n");
		abort();
	}

	if (result) {
		assert(instr->dest.dest.is_ssa);
		_mesa_hash_table_insert(ctx->defs, &instr->dest.dest.ssa,
		                        result);
	}
}

static void visit_load_const(struct nir_to_llvm_context *ctx,
                             nir_load_const_instr *instr)
{
	LLVMValueRef values[4], value = NULL;
	LLVMTypeRef element_type =
	    LLVMIntTypeInContext(ctx->context, instr->def.bit_size);

	for (unsigned i = 0; i < instr->def.num_components; ++i) {
		switch (instr->def.bit_size) {
		case 32:
			values[i] = LLVMConstInt(element_type,
			                         instr->value.u32[i], false);
			break;
		case 64:
			values[i] = LLVMConstInt(element_type,
			                         instr->value.u64[i], false);
			break;
		default:
			fprintf(stderr,
			        "unsupported nir load_const bit_size: %d\n",
			        instr->def.bit_size);
			abort();
		}
	}
	if (instr->def.num_components > 1) {
		value = LLVMConstVector(values, instr->def.num_components);
	} else
		value = values[0];

	_mesa_hash_table_insert(ctx->defs, &instr->def, value);
}

static LLVMValueRef cast_ptr(struct nir_to_llvm_context *ctx, LLVMValueRef ptr,
                             LLVMTypeRef type)
{
	int addr_space = LLVMGetPointerAddressSpace(LLVMTypeOf(ptr));
	return LLVMBuildBitCast(ctx->builder, ptr,
	                        LLVMPointerType(type, addr_space), "");
}

static LLVMValueRef
emit_llvm_intrinsic(struct nir_to_llvm_context *ctx, const char *name,
                    LLVMTypeRef return_type, LLVMValueRef *params,
                    unsigned param_count, LLVMAttribute attribs)
{
	LLVMValueRef function;

	function = LLVMGetNamedFunction(ctx->module, name);
	if (!function) {
		LLVMTypeRef param_types[32], function_type;
		unsigned i;

		assert(param_count <= 32);

		for (i = 0; i < param_count; ++i) {
			assert(params[i]);
			param_types[i] = LLVMTypeOf(params[i]);
		}
		function_type =
		    LLVMFunctionType(return_type, param_types, param_count, 0);
		function = LLVMAddFunction(ctx->module, name, function_type);

		LLVMSetFunctionCallConv(function, LLVMCCallConv);
		LLVMSetLinkage(function, LLVMExternalLinkage);

		LLVMAddFunctionAttr(function, attribs | LLVMNoUnwindAttribute);
	}
	return LLVMBuildCall(ctx->builder, function, params, param_count, "");
}

static LLVMValueRef visit_vulkan_resource_index(struct nir_to_llvm_context *ctx,
                                                nir_intrinsic_instr *instr)
{
	LLVMValueRef index = get_src(ctx, instr->src[0]);
	LLVMValueRef desc_ptr =
	    ctx->descriptor_sets[nir_intrinsic_desc_set(instr)];
	unsigned base_offset = nir_intrinsic_binding(instr) * 16 / 4;
	LLVMValueRef offset = LLVMConstInt(ctx->i32, base_offset, false);
	LLVMValueRef stride = LLVMConstInt(ctx->i32, 4, false);
	index = LLVMBuildMul(ctx->builder, index, stride, "");
	offset = LLVMBuildAdd(ctx->builder, offset, index, "");

	LLVMValueRef indices[] = {LLVMConstInt(ctx->i32, 0, false), offset};
	desc_ptr = LLVMBuildGEP(ctx->builder, desc_ptr, indices, 2, "");
	desc_ptr = cast_ptr(ctx, desc_ptr, ctx->v4i32);
	LLVMSetMetadata(desc_ptr, ctx->uniform_md_kind, ctx->empty_md);

	return LLVMBuildLoad(ctx->builder, desc_ptr, "");
}

static void visit_store_ssbo(struct nir_to_llvm_context *ctx,
                             nir_intrinsic_instr *instr)
{
	assert(nir_intrinsic_write_mask(instr) ==
	       (1 << instr->num_components) - 1);
	const char *store_name;
	LLVMTypeRef data_type = ctx->f32;
	if (instr->num_components > 1)
		data_type = LLVMVectorType(ctx->f32, instr->num_components);

	if (instr->num_components == 4)
		store_name = "llvm.amdgcn.buffer.store.v4f32";
	else if (instr->num_components == 2)
		store_name = "llvm.amdgcn.buffer.store.v2f32";
	else if (instr->num_components == 1)
		store_name = "llvm.amdgcn.buffer.store.f32";
	else
		abort();

	LLVMValueRef params[] = {
	    LLVMBuildBitCast(ctx->builder, get_src(ctx, instr->src[0]),
	                     data_type, ""),
	    get_src(ctx, instr->src[1]), LLVMConstInt(ctx->i32, 0, false),
	    get_src(ctx, instr->src[2]), LLVMConstInt(ctx->i1, 0, false),
	    LLVMConstInt(ctx->i1, 0, false),
	};

	emit_llvm_intrinsic(ctx, store_name,
	                    LLVMVoidTypeInContext(ctx->context), params, 6, 0);
}

static LLVMValueRef visit_load_buffer(struct nir_to_llvm_context *ctx,
                                      nir_intrinsic_instr *instr)
{
	const char *load_name;
	LLVMTypeRef data_type = ctx->f32;
	if (instr->num_components > 1)
		data_type = LLVMVectorType(ctx->f32, instr->num_components);

	if (instr->num_components == 4)
		load_name = "llvm.amdgcn.buffer.load.v4f32";
	else if (instr->num_components == 2)
		load_name = "llvm.amdgcn.buffer.load.v2f32";
	else if (instr->num_components == 1)
		load_name = "llvm.amdgcn.buffer.load.f32";
	else
		abort();

	LLVMValueRef params[] = {
	    get_src(ctx, instr->src[1]),     LLVMConstInt(ctx->i32, 0, false),
	    get_src(ctx, instr->src[2]),     LLVMConstInt(ctx->i1, 0, false),
	    LLVMConstInt(ctx->i1, 0, false),
	};

	LLVMValueRef ret =
	    emit_llvm_intrinsic(ctx, load_name, data_type, params, 5, 0);

	return LLVMBuildBitCast(ctx->builder, ret,
	                        get_def_type(ctx, &instr->dest.ssa), "");
}

static void visit_intrinsic(struct nir_to_llvm_context *ctx,
                            nir_intrinsic_instr *instr)
{
	const nir_intrinsic_info *info = &nir_intrinsic_infos[instr->intrinsic];
	LLVMValueRef result = NULL;

	switch (instr->intrinsic) {
	case nir_intrinsic_load_work_group_id: {
		result = ctx->workgroup_ids;
		break;
	}
	case nir_intrinsic_load_local_invocation_id: {
		result = ctx->local_invocation_ids;
		break;
	}
	case nir_intrinsic_vulkan_resource_index:
		result = visit_vulkan_resource_index(ctx, instr);
		break;
	case nir_intrinsic_store_ssbo:
		visit_store_ssbo(ctx, instr);
		break;
	case nir_intrinsic_load_ssbo:
		result = visit_load_buffer(ctx, instr);
		break;
	case nir_intrinsic_load_ubo:
		result = visit_load_buffer(ctx, instr);
		break;
	default:
		fprintf(stderr, "Unknown intrinsic: ");
		nir_print_instr(&instr->instr, stderr);
		fprintf(stderr, "\n");
		break;
	}
	if (result) {
		assert(info->has_dest && instr->dest.is_ssa);
		_mesa_hash_table_insert(ctx->defs, &instr->dest.ssa, result);
	}
}

static void visit_cf_list(struct nir_to_llvm_context *ctx,
                          struct exec_list *list);

static void visit_block(struct nir_to_llvm_context *ctx, nir_block *block)
{

	nir_foreach_instr(instr, block)
	{
		switch (instr->type) {
		case nir_instr_type_alu:
			visit_alu(ctx, nir_instr_as_alu(instr));
			break;
		case nir_instr_type_load_const:
			visit_load_const(ctx, nir_instr_as_load_const(instr));
			break;
		case nir_instr_type_intrinsic:
			visit_intrinsic(ctx, nir_instr_as_intrinsic(instr));
			break;
		default:
			fprintf(stderr, "Unknown NIR instr type: ");
			nir_print_instr(instr, stderr);
			fprintf(stderr, "\n");
			abort();
		}
	}
}

static void visit_if(struct nir_to_llvm_context *ctx, nir_if *if_stmt)
{
	LLVMValueRef value = get_src(ctx, if_stmt->condition);

	LLVMBasicBlockRef merge_block =
	    LLVMAppendBasicBlockInContext(ctx->context, ctx->main_function, "");
	LLVMBasicBlockRef if_block =
	    LLVMAppendBasicBlockInContext(ctx->context, ctx->main_function, "");
	LLVMBasicBlockRef else_block = merge_block;
	if (!exec_list_is_empty(&if_stmt->else_list))
		else_block = LLVMAppendBasicBlockInContext(
		    ctx->context, ctx->main_function, "");

	LLVMValueRef cond = LLVMBuildICmp(ctx->builder, LLVMIntNE, value,
	                                  LLVMConstInt(ctx->i32, 0, false), "");
	LLVMBuildCondBr(ctx->builder, cond, if_block, else_block);

	LLVMPositionBuilderAtEnd(ctx->builder, if_block);
	visit_cf_list(ctx, &if_stmt->then_list);
	LLVMBuildBr(ctx->builder, merge_block);

	if (!exec_list_is_empty(&if_stmt->else_list)) {
		LLVMPositionBuilderAtEnd(ctx->builder, else_block);
		visit_cf_list(ctx, &if_stmt->else_list);
		LLVMBuildBr(ctx->builder, merge_block);
	}

	LLVMPositionBuilderAtEnd(ctx->builder, merge_block);
}

static void visit_loop(struct nir_to_llvm_context *ctx, nir_loop *loop)
{
	LLVMBasicBlockRef continue_parent = ctx->continue_block;
	LLVMBasicBlockRef break_parent = ctx->break_block;

	ctx->continue_block =
	    LLVMAppendBasicBlockInContext(ctx->context, ctx->main_function, "");
	ctx->break_block =
	    LLVMAppendBasicBlockInContext(ctx->context, ctx->main_function, "");

	LLVMBuildBr(ctx->builder, ctx->continue_block);
	LLVMPositionBuilderAtEnd(ctx->builder, ctx->continue_block);
	visit_cf_list(ctx, &loop->body);

	LLVMBuildBr(ctx->builder, ctx->continue_block);
	LLVMPositionBuilderAtEnd(ctx->builder, ctx->break_block);

	ctx->continue_block = continue_parent;
	ctx->break_block = break_parent;
}

static void visit_cf_list(struct nir_to_llvm_context *ctx,
                          struct exec_list *list)
{
	foreach_list_typed(nir_cf_node, node, node, list)
	{
		switch (node->type) {
		case nir_cf_node_block:
			visit_block(ctx, nir_cf_node_as_block(node));
			break;

		case nir_cf_node_if:
			visit_if(ctx, nir_cf_node_as_if(node));
			break;

		case nir_cf_node_loop:
			visit_loop(ctx, nir_cf_node_as_loop(node));
			break;

		default:
			assert(0);
		}
	}
}

LLVMModuleRef ac_translate_nir_to_llvm(LLVMTargetMachineRef tm,
                                       struct nir_shader *nir)
{
	struct nir_to_llvm_context ctx = {};
	struct nir_function *func;
	ctx.context = LLVMContextCreate();
	ctx.module = LLVMModuleCreateWithNameInContext("shader", ctx.context);
	setup_types(&ctx);

	ctx.builder = LLVMCreateBuilderInContext(ctx.context);
	create_function(&ctx, nir);

	ctx.defs = _mesa_hash_table_create(NULL, _mesa_hash_pointer,
	                                   _mesa_key_pointer_equal);

	func = (struct nir_function *)exec_list_get_head(&nir->functions);
	visit_cf_list(&ctx, &func->impl->body);

	LLVMBuildRetVoid(ctx.builder);
	LLVMDisposeBuilder(ctx.builder);
	ralloc_free(ctx.defs);

	return ctx.module;
}

static void ac_diagnostic_handler(LLVMDiagnosticInfoRef di, void *context)
{
	unsigned *retval = (unsigned *)context;
	LLVMDiagnosticSeverity severity = LLVMGetDiagInfoSeverity(di);
	char *description = LLVMGetDiagInfoDescription(di);

	if (severity == LLVMDSError) {
		*retval = 1;
		fprintf(stderr, "LLVM triggered Diagnostic Handler: %s\n",
		        description);
	}

	LLVMDisposeMessage(description);
}

static unsigned ac_llvm_compile(LLVMModuleRef M,
                                struct ac_shader_binary *binary,
                                LLVMTargetMachineRef tm)
{
	unsigned retval = 0;
	char *err;
	LLVMContextRef llvm_ctx;
	LLVMMemoryBufferRef out_buffer;
	unsigned buffer_size;
	const char *buffer_data;
	LLVMBool mem_err;

	/* Setup Diagnostic Handler*/
	llvm_ctx = LLVMGetModuleContext(M);

	LLVMContextSetDiagnosticHandler(llvm_ctx, ac_diagnostic_handler,
	                                &retval);

	/* Compile IR*/
	mem_err = LLVMTargetMachineEmitToMemoryBuffer(tm, M, LLVMObjectFile,
	                                              &err, &out_buffer);

	/* Process Errors/Warnings */
	if (mem_err) {
		fprintf(stderr, "%s: %s", __FUNCTION__, err);
		free(err);
		retval = 1;
		goto out;
	}

	/* Extract Shader Code*/
	buffer_size = LLVMGetBufferSize(out_buffer);
	buffer_data = LLVMGetBufferStart(out_buffer);

	ac_elf_read(buffer_data, buffer_size, binary);

	/* Clean up */
	LLVMDisposeMemoryBuffer(out_buffer);

out:
	return retval;
}

void ac_compile_nir_shader(LLVMTargetMachineRef tm,
                           struct ac_shader_binary *binary,
                           struct ac_shader_config *config,
                           struct nir_shader *nir)
{
	LLVMModuleRef llvm_module = ac_translate_nir_to_llvm(tm, nir);
	LLVMDumpModule(llvm_module);

	memset(binary, 0, sizeof(*binary));
	int v = ac_llvm_compile(llvm_module, binary, tm);
	if (v) {
		fprintf(stderr, "compile failed\n");
	}

	printf("disasm:\n%s\n", binary->disasm_string);

	ac_shader_binary_read_config(binary, config, 0);

	LLVMContextRef ctx = LLVMGetModuleContext(llvm_module);
	LLVMDisposeModule(llvm_module);
	LLVMContextDispose(ctx);
}
