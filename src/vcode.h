//
//  Copyright (C) 2014  Nick Gasson
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#ifndef _VCODE_H
#define _VCODE_H

#include "util.h"
#include "ident.h"
#include "prim.h"

typedef int vcode_type_t;
typedef int vcode_block_t;
typedef int vcode_var_t;
typedef int vcode_reg_t;

typedef enum {
   VCODE_CMP_EQ
} vcode_cmp_t;

typedef enum {
   VCODE_OP_CMP,
   VCODE_OP_FCALL,
   VCODE_OP_WAIT,
   VCODE_OP_CONST,
   VCODE_OP_ASSERT,
   VCODE_OP_JUMP,
   VCODE_OP_LOAD,
   VCODE_OP_STORE,
   VCODE_OP_MUL,
   VCODE_OP_ADD,
   VCODE_OP_BOUNDS,
   VCODE_OP_COMMENT,
   VCODE_OP_CONST_ARRAY
} vcode_op_t;

typedef enum {
   VCODE_TYPE_INT,
   VCODE_TYPE_CARRAY
} vtype_kind_t;

#define VCODE_INVALID_REG   -1
#define VCODE_INVALID_BLOCK -1
#define VCODE_INVALID_VAR   -1

vcode_type_t vtype_int(int64_t low, int64_t high);
vcode_type_t vtype_dynamic(vcode_reg_t low, vcode_reg_t high);
vcode_type_t vtype_bool(void);
vcode_type_t vtype_carray(const vcode_type_t *dim, int ndim,
                          vcode_type_t elem, vcode_type_t bounds);
bool vtype_eq(vcode_type_t a, vcode_type_t b);
bool vtype_includes(vcode_type_t type, vcode_type_t bounds);
vtype_kind_t vtype_kind(vcode_type_t type);
int64_t vtype_low(vcode_type_t type);
int64_t vtype_high(vcode_type_t type);

void vcode_opt(void);
void vcode_close(void);
void vcode_dump(void);
void vcode_select_unit(vcode_unit_t vu);
void vcode_select_block(vcode_block_t block);
int vcode_count_blocks(void);
const char *vcode_op_string(vcode_op_t op);
bool vcode_block_finished(void);
vcode_type_t vcode_reg_type(vcode_reg_t reg);

int vcode_count_ops(void);
vcode_op_t vcode_get_op(int op);
ident_t vcode_get_func(int op);
int64_t vcode_get_value(int op);
vcode_cmp_t vcode_get_cmp(int op);
vcode_block_t vcode_get_target(int op);
vcode_var_t vcode_get_address(int op);
int vcode_count_args(int op);
vcode_reg_t vcode_get_arg(int op, int arg);
vcode_type_t vcode_get_type(int op);
vcode_reg_t vcode_get_result(int op);

int vcode_count_vars(void);
ident_t vcode_var_name(vcode_var_t var);

vcode_unit_t emit_func(ident_t name);
vcode_unit_t emit_process(ident_t name);
vcode_block_t emit_block(void);
vcode_var_t emit_var(vcode_type_t type, vcode_type_t bounds, ident_t name);
vcode_reg_t emit_const(vcode_type_t type, int64_t value);
vcode_reg_t emit_const_array(vcode_type_t type, vcode_reg_t *values, int num);
vcode_reg_t emit_add(vcode_reg_t lhs, vcode_reg_t rhs);
vcode_reg_t emit_mul(vcode_reg_t lhs, vcode_reg_t rhs);
void emit_assert(vcode_reg_t value);
vcode_reg_t emit_cmp(vcode_cmp_t cmp, vcode_reg_t lhs, vcode_reg_t rhs);
vcode_reg_t emit_fcall(ident_t func, vcode_type_t type,
                       const vcode_reg_t *args, int nargs);
void emit_wait(vcode_block_t target, vcode_reg_t time);
void emit_jump(vcode_block_t target);
vcode_reg_t emit_load(vcode_var_t var);
void emit_store(vcode_reg_t reg, vcode_var_t var);
void emit_bounds(vcode_reg_t reg, vcode_type_t bounds);

#endif  // _VCODE_H