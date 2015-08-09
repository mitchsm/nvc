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

   if (tree_attr_int(decl, impure_i, 0))
      return NULL;

   if (exec_engine == NULL) {
      LLVMModuleRef dummy = LLVMModuleCreateWithName("dummy");

      LLVMInitializeNativeTarget();
#ifdef LLVM_HAS_MCJIT
      LLVMInitializeNativeAsmPrinter();
      LLVMLinkInMCJIT();

      struct LLVMMCJITCompilerOptions options;
      LLVMInitializeMCJITCompilerOptions(&options, sizeof(options));

      char *error;
      if (LLVMCreateMCJITCompilerForModule(&exec_engine, dummy, &options,
                                           sizeof(options), &error))
         fatal("error creating MCJIT compiler: %s", error);
#else
      LLVMInitializeNativeTarget();
      LLVMLinkInJIT();

      char *error;
      if (LLVMCreateExecutionEngineForModule(&exec_engine, dummy, &error))
         fatal("error creating execution engine: %s", error);
#endif
   }

   if (tree_attr_str(decl, builtin_i) != NULL)
      return exec_engine;

   ident_t name = tree_ident(decl);
   printf("eval_llvm_func_for %s\n", istr(name));

   ident_t mangled = tree_attr_str(decl, mangled_i);
   printf(" --> %s mangled=%p\n", tree_kind_str(tree_kind(decl)), mangled);
   if (mangled == NULL)
      return NULL;

   LLVMValueRef fn;
   if (!LLVMFindFunction(exec_engine, istr(mangled), &fn))
      return exec_engine;

   ident_t lname = ident_until(name, '.');
   ident_t uname = ident_runtil(name, '.');

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
   if (module == NULL) {
      char *bcname LOCAL = xasprintf("_%s.bc", istr(tree_ident(unit)));
      char path[PATH_MAX];
      lib_realpath(lib, bcname, path, sizeof(path));

      notef("loading LLVM bitcode for %s", istr(tree_ident(unit)));

      char *error;
      LLVMMemoryBufferRef buf;
      if (LLVMCreateMemoryBufferWithContentsOfFile(path, &buf, &error))
         fatal("error reading bitcode from %s: %s", path, error);

      if (LLVMParseBitcode(buf, &module, &error))
         fatal("error parsing bitcode: %s", error);

      LLVMDisposeMemoryBuffer(buf);
   }

   LLVMAddModule(exec_engine, module);

   if (LLVMFindFunction(exec_engine, istr(mangled), &fn))
      fatal("cannot find LLVM bitcode for %s\n", istr(mangled));

   printf(" --> fn %s, %p\n", istr(mangled), fn);

   return exec_engine;
}

static bool eval_possible(tree_t fcall)
{
   type_t result = tree_type(fcall);
   if (!type_is_scalar(result))
      return false;

   if (tree_attr_int(tree_ref(fcall), impure_i, 0))
      return NULL;

   const int nparams = tree_params(fcall);
   for (int i = 0; i < nparams; i++) {
      tree_t p = tree_value(tree_param(fcall, i));
      const tree_kind_t kind = tree_kind(p);
      switch (kind) {
      case T_LITERAL:
         break;

      case T_FCALL:
         if (!eval_possible(p))
            return false;
         break;

      case T_REF:
         {
            const tree_kind_t dkind = tree_kind(tree_ref(p));
            if (dkind == T_UNIT_DECL || dkind == T_ENUM_LIT)
               break;
         }
         // Fall-through

      default:
         return false;
      }
   }

   return true;
}

tree_t eval(tree_t fcall)
{
   assert(tree_kind(fcall) == T_FCALL);

   if (!eval_possible(fcall))
      return fcall;

   LLVMExecutionEngineRef exec_engine = eval_exec_engine_for(tree_ref(fcall));
   if (exec_engine == NULL)
      return fcall;

   vcode_unit_t thunk = lower_thunk(fcall);
   if (thunk == NULL)
      return fcall;

   vcode_select_unit(thunk);

   LLVMModuleRef module = cgen_thunk(thunk);
   LLVMAddModule(exec_engine, module);

   LLVMValueRef fn;
   if (LLVMFindFunction(exec_engine, istr(vcode_unit_name()), &fn))
      fatal("cannot find LLVM bitcode for thunk");

   LLVMGenericValueRef value = LLVMRunFunction(exec_engine, fn, 0, NULL);

   type_t result = tree_type(fcall);
   if (type_is_enum(result))
      return get_enum_lit(fcall, LLVMGenericValueToInt(value, false));
   else if (type_is_integer(result) || type_is_physical(result))
      return get_int_lit(fcall, LLVMGenericValueToInt(value, true));
   else if (type_is_real(result)) {
      const double dval = LLVMGenericValueToFloat(LLVMDoubleType(), value);
      return get_real_lit(fcall, dval);
   }

   return fcall;
}
