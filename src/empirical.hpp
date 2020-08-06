/*
 * Empirical header -- everything needed in the compiler pipeline
 *
 * Copyright (C) 2019--2020 Empirical Software Solutions, LLC
 *
 * This program is distributed under the terms of the GNU Affero General
 * Public License with the Commons Clause.
 *
 */

#pragma once

#include <string>

#include <AST.h>
#include <HIR.h>

#include <VVM/vvm.hpp>

// internal compiler functions
enum class CompilerCodes: size_t {
  // TODO 'store' is a cheat here; it should use type parameters from VVM
  // (that, of course, doesn't exist yet)---eventually we'll have 'typeof', etc
  kStore
};

// functions to compile Empirical into VVM bytecode
AST::mod_t parse(const std::string& text, bool interactive, bool dump_ast);
HIR::mod_t sema(AST::mod_t ast, bool interactive, bool dump_hir);
VVM::Program codegen(HIR::mod_t hir, VVM::Mode vvm_mode, bool interactive,
                     bool dump_vvm);

