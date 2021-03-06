/*
 * Copyright 2010 Jerome Glisse <glisse@freedesktop.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * on the rights to use, copy, modify, merge, publish, distribute, sub
 * license, and/or sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHOR(S) AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *      Jerome Glisse
 */
#ifndef R600_H
#define R600_H

#include "../../winsys/radeon/drm/radeon_winsys.h"
#include "util/u_double_list.h"
#include "util/u_transfer.h"

#define R600_ERR(fmt, args...) \
	fprintf(stderr, "EE %s:%d %s - "fmt, __FILE__, __LINE__, __func__, ##args)

struct winsys_handle;

enum radeon_family {
	CHIP_UNKNOWN,
	CHIP_CAYMAN,
	CHIP_TAHITI,
	CHIP_PITCAIRN,
	CHIP_VERDE,
	CHIP_LAST,
};

enum chip_class {
	CAYMAN,
	TAHITI,
};

struct r600_tiling_info {
	unsigned num_channels;
	unsigned num_banks;
	unsigned group_bytes;
};

struct r600_resource {
	struct u_resource		b;

	/* Winsys objects. */
	struct pb_buffer		*buf;
	struct radeon_winsys_cs_handle	*cs_buf;

	/* Resource state. */
	unsigned			domains;
};

/* R600/R700 STATES */
#define R600_GROUP_MAX			16
#define R600_BLOCK_MAX_BO		32
#define R600_BLOCK_MAX_REG		128

/* each range covers 9 bits of dword space = 512 dwords = 2k bytes */
/* there is a block entry for each register so 512 blocks */
/* we have no registers to read/write below 0x8000 (0x2000 in dw space) */
/* we use some fake offsets at 0x40000 to do evergreen sampler borders so take 0x42000 as a max bound*/
#define RANGE_OFFSET_START 0x8000
#define HASH_SHIFT 9
#define NUM_RANGES (0x42000 - RANGE_OFFSET_START) / (4 << HASH_SHIFT) /* 128 << 9 = 64k */

#define CTX_RANGE_ID(offset) ((((offset - RANGE_OFFSET_START) >> 2) >> HASH_SHIFT) & 255)
#define CTX_BLOCK_ID(offset) (((offset - RANGE_OFFSET_START) >> 2) & ((1 << HASH_SHIFT) - 1))

struct r600_pipe_reg {
	uint32_t			value;
	struct r600_block 		*block;
	struct r600_resource		*bo;
	enum radeon_bo_usage		bo_usage;
	uint32_t			id;
};

struct r600_pipe_state {
	unsigned			id;
	unsigned			nregs;
	struct r600_pipe_reg		regs[R600_BLOCK_MAX_REG];
};

#define R600_BLOCK_STATUS_ENABLED	(1 << 0)
#define R600_BLOCK_STATUS_DIRTY		(1 << 1)

struct r600_block_reloc {
	struct r600_resource	*bo;
	enum radeon_bo_usage	bo_usage;
	unsigned		bo_pm4_index;
};

struct r600_block {
	struct list_head	list;
	struct list_head	enable_list;
	unsigned		status;
	unsigned                flags;
	unsigned		start_offset;
	unsigned		pm4_ndwords;
	unsigned		nbo;
	uint16_t 		nreg;
	uint16_t                nreg_dirty;
	uint32_t		*reg;
	uint32_t		pm4[R600_BLOCK_MAX_REG];
	unsigned		pm4_bo_index[R600_BLOCK_MAX_REG];
	struct r600_block_reloc	reloc[R600_BLOCK_MAX_BO];
};

struct r600_range {
	struct r600_block	**blocks;
};

struct r600_query {
	union {
		uint64_t			u64;
		boolean				b;
		struct pipe_query_data_so_statistics so;
	} result;
	/* The kind of query */
	unsigned				type;
	/* Offset of the first result for current query */
	unsigned				results_start;
	/* Offset of the next free result after current query data */
	unsigned				results_end;
	/* Size of the result in memory for both begin_query and end_query,
	 * this can be one or two numbers, or it could even be a size of a structure. */
	unsigned				result_size;
	/* The buffer where query results are stored. It's used as a ring,
	 * data blocks for current query are stored sequentially from
	 * results_start to results_end, with wrapping on the buffer end */
	struct r600_resource			*buffer;
	/* The number of dwords for begin_query or end_query. */
	unsigned				num_cs_dw;
	/* linked list of queries */
	struct list_head			list;
};

struct r600_so_target {
	struct pipe_stream_output_target b;

	/* The buffer where BUFFER_FILLED_SIZE is stored. */
	struct r600_resource	*filled_size;
	unsigned		stride;
	unsigned		so_index;
};

#define R600_CONTEXT_DRAW_PENDING	(1 << 0)
#define R600_CONTEXT_DST_CACHES_DIRTY	(1 << 1)
#define R600_CONTEXT_CHECK_EVENT_FLUSH	(1 << 2)

struct r600_draw {
	uint32_t		vgt_num_indices;
	uint32_t		vgt_num_instances;
	uint32_t		vgt_index_type;
	uint32_t		vgt_draw_initiator;
	uint32_t		indices_bo_offset;
	unsigned		db_render_override;
	unsigned		db_render_control;
	struct r600_resource	*indices;
};

struct r600_context;
struct r600_screen;

void r600_get_backend_mask(struct r600_context *ctx);
void r600_context_fini(struct r600_context *ctx);
void r600_context_pipe_state_set(struct r600_context *ctx, struct r600_pipe_state *state);
void r600_context_flush(struct r600_context *ctx, unsigned flags);

struct r600_query *r600_context_query_create(struct r600_context *ctx, unsigned query_type);
void r600_context_query_destroy(struct r600_context *ctx, struct r600_query *query);
boolean r600_context_query_result(struct r600_context *ctx,
				struct r600_query *query,
				boolean wait, void *vresult);
void r600_query_begin(struct r600_context *ctx, struct r600_query *query);
void r600_query_end(struct r600_context *ctx, struct r600_query *query);
void r600_context_queries_suspend(struct r600_context *ctx);
void r600_context_queries_resume(struct r600_context *ctx);
void r600_query_predication(struct r600_context *ctx, struct r600_query *query, int operation,
			    int flag_wait);
void r600_context_emit_fence(struct r600_context *ctx, struct r600_resource *fence,
                             unsigned offset, unsigned value);
void r600_inval_shader_cache(struct r600_context *ctx);
void r600_inval_texture_cache(struct r600_context *ctx);
void r600_inval_vertex_cache(struct r600_context *ctx);
void r600_flush_framebuffer(struct r600_context *ctx, bool flush_now);

void r600_context_streamout_begin(struct r600_context *ctx);
void r600_context_streamout_end(struct r600_context *ctx);
void r600_context_draw_opaque_count(struct r600_context *ctx, struct r600_so_target *t);
void r600_need_cs_space(struct r600_context *ctx, unsigned num_dw, boolean count_draw_in);
void r600_context_block_emit_dirty(struct r600_context *ctx, struct r600_block *block);
void r600_context_block_resource_emit_dirty(struct r600_context *ctx, struct r600_block *block);

int si_context_init(struct r600_context *ctx);
void si_context_draw(struct r600_context *ctx, const struct r600_draw *draw);

void _r600_pipe_state_add_reg(struct r600_context *ctx,
			      struct r600_pipe_state *state,
			      uint32_t offset, uint32_t value,
			      uint32_t range_id, uint32_t block_id,
			      struct r600_resource *bo,
			      enum radeon_bo_usage usage);

void r600_pipe_state_add_reg_noblock(struct r600_pipe_state *state,
				     uint32_t offset, uint32_t value,
				     struct r600_resource *bo,
				     enum radeon_bo_usage usage);

#define r600_pipe_state_add_reg(state, offset, value, bo, usage) _r600_pipe_state_add_reg(rctx, state, offset, value, CTX_RANGE_ID(offset), CTX_BLOCK_ID(offset), bo, usage)

static inline void r600_pipe_state_mod_reg(struct r600_pipe_state *state,
					   uint32_t value)
{
	state->regs[state->nregs].value = value;
	state->nregs++;
}

static inline void r600_pipe_state_mod_reg_bo(struct r600_pipe_state *state,
					      uint32_t value, struct r600_resource *bo,
					      enum radeon_bo_usage usage)
{
	state->regs[state->nregs].value = value;
	state->regs[state->nregs].bo = bo;
	state->regs[state->nregs].bo_usage = usage;
	state->nregs++;
}

#endif
