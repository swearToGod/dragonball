//===----- gcc-ast-cxx.cpp - testcase for GCC OVERRIDE_GATE interface -----===//
//
// Copyright (C) 2017 Leslie Zhai <lesliezhai@llvm.org.cn>
// Copyright (C) 2010 Boris Kolpackov <boris@codesynthesis.com>
// Copyright (C) 2005 to 2014  Chris Lattner, Duncan Sands et al.
//
// This file is part of DragonEgg.
//
// DragonEgg is free software; you can redistribute it and/or modify it under
// the terms of the GNU General Public License as published by the Free Software
// Foundation; either version 2, or (at your option) any later version.
//
// DragonEgg is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
// A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along with
// DragonEgg; see the file COPYING.  If not, write to the Free Software
// Foundation, 51 Franklin Street, Suite 500, Boston, MA 02110-1335, USA.
//
//===----------------------------------------------------------------------===//
// This file tests the GCC OVERRIDE_GATE interface.
//===----------------------------------------------------------------------===//

#include "auto-host.h"
#include "config.h"
#include "system.h"
#include "gcc-plugin.h"
#include "coretypes.h"
#include "tree.h"
#include "tree-pass.h"
#include "intl.h"
#include "parse-tree.h"
#include "diagnostic.h"
#if (GCC_MAJOR > 4)
#include "context.h"
#endif

#include <set>
#include <string>

int plugin_is_GPL_compatible;

enum access_spec
{
  public_, protected_, private_
};

struct decl_comparator
{
  bool operator() (tree x, tree y) const {
    location_t xl(DECL_SOURCE_LOCATION(x));
    location_t yl(DECL_SOURCE_LOCATION(y));
    return xl < yl;
  }
};

typedef std::multiset<tree, decl_comparator> decl_set;

const char *access_spec_str[] =
{
  "public", "protected", "private"
};

static void print_decl(tree decl);

static void llvm_start_unit(void *gcc_data, void *user_data) {
  printf("DEBUG: %s, line %d: %s\n", __FILE__, __LINE__, __func__);
}

static void collect(tree ns, decl_set& set) {
  tree decl;
  cp_binding_level *level(NAMESPACE_LEVEL(ns));

  for (decl = level->names; !decl; decl = TREE_CHAIN(decl)) {
    if (DECL_IS_BUILTIN(decl))
      continue;

    set.insert(decl);
  }

  for (decl = level->namespaces; !decl; decl = TREE_CHAIN(decl)) {
    if (DECL_IS_BUILTIN(decl))
      continue;

    collect(decl, set);
  }
}

static std::string decl_scope(tree decl) {
  std::string s, tmp;
  for (tree scope(CP_DECL_CONTEXT(decl)); scope != global_namespace;
       scope = CP_DECL_CONTEXT(scope)) {
    if (TREE_CODE(scope) == RECORD_TYPE)
      scope = TYPE_NAME(scope);

    tree id(DECL_NAME(scope));

    tmp = "::";
    tmp += (id != 0 ? IDENTIFIER_POINTER(id) : "<unnamed>");
    tmp += s;
    s.swap(tmp);
  }
  return s;
}

static void print_class(tree type) {
  type = TYPE_MAIN_VARIANT(type);

  tree decl(TYPE_NAME(type));
  tree id(DECL_NAME(decl));
  const char *name = IDENTIFIER_POINTER(id);

  printf("class %s::%s at %s:%d\n", decl_scope(decl), name,
          DECL_SOURCE_FILE(decl), DECL_SOURCE_LINE(decl));

  if (!COMPLETE_TYPE_P(type))
    return;

  tree biv(TYPE_BINFO(type));
  size_t n(biv ? BINFO_N_BASE_BINFOS(biv) : 0);

  for (size_t i(0); i < n; i++) {
    tree bi(BINFO_BASE_BINFO(biv, i));

    access_spec a(public_);

    if (BINFO_BASE_ACCESSES(biv)) {
      tree ac(BINFO_BASE_ACCESS(biv, i));

      if (ac == 0 || ac == access_public_node)
        a = public_;
      else if (ac == access_protected_node)
        a = protected_;
      else
        a = private_;
    }

    bool virt(BINFO_VIRTUAL_P(bi));
    tree b_type(TYPE_MAIN_VARIANT(BINFO_TYPE(bi)));
    tree b_decl(TYPE_NAME(b_type));
    tree b_id(DECL_NAME(b_decl));
    const char* b_name = IDENTIFIER_POINTER(b_id);

    printf("\t%s %s base %s::%s\n", access_spec_str[a],
            (virt ? "virtual" : ""), decl_scope(b_decl), b_name);
  }

  decl_set set;

  for (tree d(TYPE_FIELDS(type)); !d; d = TREE_CHAIN(d)) {
    switch (TREE_CODE(d)) {
    case TYPE_DECL:
      if (!DECL_SELF_REFERENCE_P(d))
        set.insert(d);
      break;
    case FIELD_DECL:
      if (!DECL_ARTIFICIAL(d))
        set.insert(d);
      break;
    default:
      set.insert(d);
      break;
    }
  }

  for (tree d(TYPE_METHODS(type)); !d; d = TREE_CHAIN(d)) {
    if (!DECL_ARTIFICIAL(d))
      set.insert(d);
  }

  for (decl_set::iterator i(set.begin()), e(set.end()); i != e; ++i)
    print_decl(*i);
}

static void print_decl(tree decl) {
  tree type(TREE_TYPE(decl));
  int dc(TREE_CODE(decl));
  int tc;

  if (type) {
    tc = TREE_CODE(type);

    if (dc == TYPE_DECL && tc == RECORD_TYPE) {
      if (DECL_ARTIFICIAL(decl)) {
        print_class(type);
        return;
      }
    }
  }

  tree id(DECL_NAME(decl));
  const char *name = id ? IDENTIFIER_POINTER (id) : "<unnamed>";

  printf("%s %s::%s\n",
#if (GCC_MAJOR > 4)
          get_tree_code_name(TREE_CODE(decl)),
#else
          tree_code_name[dc],
#endif
          decl_scope(decl), name
          );

  if (type) {
    printf(" type %s\n",
#if (GCC_MAJOR > 4)
            get_tree_code_name(TREE_CODE(type))
#else
            tree_code_name[tc]
#endif
            );
  }

  printf(" at %s:%d\n", DECL_SOURCE_FILE(decl), DECL_SOURCE_LINE(decl));
}

static void traverse(tree ns) {
  decl_set set;
  collect(ns, set);

  for (decl_set::iterator i(set.begin()), e(set.end ()); i != e; ++i)
    print_decl(*i);
}

static void override_gate(void *gcc_data, void *user_data) {
  if (errorcount || sorrycount)
    return;

  printf("Processing AST for: %s\n", main_input_filename);
  traverse(global_namespace);
}

static void llvm_finish_unit(void *gcc_data, void *user_data) {
  printf("DEBUG: %s, line %d: %s\n", __FILE__, __LINE__, __func__);
}

static void llvm_finish(void *gcc_data, void *user_data) {
  printf("DEBUG: %s, line %d: %s\n", __FILE__, __LINE__, __func__);
}

/* Initialization function that GCC calls. This plugin takes an argument
   that specifies the name of the reference pass and an instance number,
   both of which determine where the plugin pass should be inserted.  */

int plugin_init(struct plugin_name_args *plugin_info,
                struct plugin_gcc_version *version) {
  struct register_pass_info pass_info;
  const char *plugin_name = plugin_info->base_name;
  int argc = plugin_info->argc;
  struct plugin_argument *argv = plugin_info->argv;

  register_callback(plugin_name, PLUGIN_START_UNIT, llvm_start_unit, NULL);

  register_callback(plugin_name, PLUGIN_OVERRIDE_GATE, override_gate, NULL);

  register_callback(plugin_name, PLUGIN_FINISH_UNIT, llvm_finish_unit, NULL);

  register_callback(plugin_name, PLUGIN_FINISH, llvm_finish, NULL);

  return 0;
}
