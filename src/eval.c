//
//  Copyright (C) 2013-2015  Nick Gasson
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

#include "phase.h"
#include "util.h"
#include "common.h"
#include "vcode.h"

#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <limits.h>

#include <llvm-c/Core.h>
#include <llvm-c/BitReader.h>
#include <llvm-c/ExecutionEngine.h>

#undef NDEBUG
#include <assert.h>

static LLVMExecutionEngineRef eval_exec_engine_for(tree_t decl)
{
   static LLVMExecutionEngineRef exec_engine = NULL;

   if (exec_engine == NULL) {
      LLVMLinkInInterpreter();

      LLVMModuleRef dummy = LLVMModuleCreateWithName("dummy");

      char *error;
      if (LLVMCreateInterpreterForModule(&exec_engine, dummy, &error))
         fatal("error creating LLVM interpreter: %s", error);
   }

   if (tree_attr_str(decl, builtin_i) != NULL)
      return exec_engine;

   ident_t name = tree_ident(decl);
   printf("eval_llvm_func_for %s\n", istr(name));

   ident_t mangled = tree_attr_str(decl, mangled_i);
   if (mangled == NULL)
      return NULL;

   LLVMValueRef fn;
   if (!LLVMFindFunction(exec_engine, istr(mangled), &fn))
      return exec_engine;

   ident_t lname = ident_until(name, '.');
   ident_t uname = ident_runtil(name, '.');
   printf("lname=%s uname=%s\n", istr(lname), istr(uname));

   lib_t lib = lib_find(istr(lname), false, true);
   if (lib == NULL)
      return NULL;

   tree_t unit = lib_get(lib, uname);
   if (unit == NULL)
      return NULL;

   if (tree_kind(unit) == T_PACKAGE) {
      unit = lib_get(lib, ident_prefix(uname, ident_new("body"), '-'));
      if (unit == NULL)
         return NULL;
   }

   printf(" --> unit is %s\n", istr(tree_ident(unit)));
   printf(" --> LLVM module is %p\n", tree_attr_ptr(unit, llvm_i));

   LLVMModuleRef module = tree_attr_ptr(unit, llvm_i);
   assert(module);
   LLVMAddModule(exec_engine, module);

   if (LLVMFindFunction(exec_engine, istr(mangled), &fn))
      fatal("cannot find LLVM bitcode for %s\n", istr(mangled));

   printf(" --> fn %s, %p\n", istr(mangled), fn);

   return exec_engine;
}

tree_t eval(tree_t fcall)
{
   assert(tree_kind(fcall) == T_FCALL);

   type_t result = tree_type(fcall);
   if (!type_is_scalar(result))
      return fcall;

   const int nparams = tree_params(fcall);
   for (int i = 0; i < nparams; i++) {
      const tree_kind_t kind = tree_kind(tree_value(tree_param(fcall, i)));
      if (kind != T_LITERAL)
         return fcall;
   }

   LLVMExecutionEngineRef exec_engine = eval_exec_engine_for(tree_ref(fcall));
   if (exec_engine == NULL)
      return fcall;

   vcode_unit_t thunk = lower_thunk(fcall);
   if (thunk == NULL)
      return fcall;

   vcode_select_unit(thunk);
   vcode_dump();

   LLVMModuleRef module = cgen_thunk(thunk);
   printf("module=%p\n", module);

   LLVMAddModule(exec_engine, module);

   LLVMValueRef fn;
   if (LLVMFindFunction(exec_engine, istr(vcode_unit_name()), &fn))
      fatal("cannot find LLVM bitcode for thunk");

   LLVMGenericValueRef value = LLVMRunFunction(exec_engine, fn, 0, NULL);
   const int64_t ival = LLVMGenericValueToInt(value, true);
   printf("ival=%d\n", (int)ival);

   if (type_is_enum(result))
      return get_enum_lit(fcall, LLVMGenericValueToInt(value, true));
   else if (type_is_integer(result))
      return get_int_lit(fcall, LLVMGenericValueToInt(value, true));

   return fcall;
}
