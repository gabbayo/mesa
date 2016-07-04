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
	LLVMTypeRef f32;
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
	ctx->f32 = LLVMFloatTypeInContext(ctx->context);
}

static LLVMValueRef get_src(struct nir_to_llvm_context *ctx, nir_src src)
{
	assert(src.is_ssa);
	struct hash_entry *entry = _mesa_hash_table_search(ctx->defs, src.ssa);
	return (LLVMValueRef)entry->data;
}

static void visit_cf_list(struct nir_to_llvm_context *ctx,
                          struct exec_list *list);

static void visit_block(struct nir_to_llvm_context *ctx, nir_block *block)
{

	nir_foreach_instr(instr, block)
	{
		switch (instr->type) {
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
