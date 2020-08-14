/*
 * Semantic Analysis -- type checking and identifier resolution
 *
 * Copyright (C) 2019--2020 Empirical Software Solutions, LLC
 *
 * This program is distributed under the terms of the GNU Affero General
 * Public License with the Commons Clause.
 *
 */

#include <vector>
#include <stack>
#include <sstream>
#include <iostream>
#include <unordered_map>

#include <empirical.hpp>
#include <traits.hpp>

#include <VVM/types.h>
#include <VVM/opcodes.h>
#include <VVM/utils/csv_infer.hpp>

// build high-level IR (HIR) from abstract syntax tree (AST)
class SemaVisitor : public AST::BaseVisitor {
  // store all prior IR
  std::vector<HIR::stmt_t> history_;

  /* function traits and compute modes */

  // whether a particular trait is present
  bool contains_trait(Traits traits, SingleTrait t) {
    return traits & size_t(t);
  }

  // return intersection of all expr traits
  Traits intersect_traits(const std::vector<HIR::expr_t>& exprs) {
    Traits t = -1;
    for (HIR::expr_t e: exprs) {
      if (e != nullptr) {
        t &= e->traits;
      }
    }
    return t;
  }

  // return compound mode from all expr modes
  HIR::compmode_t compound_mode(const std::vector<HIR::expr_t>& exprs) {
    HIR::compmode_t mode = HIR::compmode_t::kComptime;
    for (HIR::expr_t e: exprs) {
      if (e != nullptr) {
        // any stream
        if (e->mode == HIR::compmode_t::kStream) {
          return e->mode;
        }
        // all comptime
        if (e->mode != HIR::compmode_t::kComptime) {
          mode = HIR::compmode_t::kNormal;
        }
      }
    }
    return mode;
  }

  // determine a function call's traits and mode
  void determine_traits_and_mode(Traits func_traits,
                                 const std::vector<HIR::expr_t>& args,
                                 Traits& traits,
                                 HIR::compmode_t& mode) {
    Traits arg_traits = intersect_traits(args);
    traits = func_traits & arg_traits;
    HIR::compmode_t arg_mode = compound_mode(args);
    mode = HIR::compmode_t::kNormal;
    if (contains_trait(func_traits, SingleTrait::kAutostream) ||
        (contains_trait(func_traits, SingleTrait::kLinear) &&
         arg_mode == HIR::compmode_t::kStream)) {
      mode = HIR::compmode_t::kStream;
    } else if (contains_trait(func_traits, SingleTrait::kPure) &&
             arg_mode == HIR::compmode_t::kComptime) {
      mode = HIR::compmode_t::kComptime;
    }
  }

  // append vector of exprs converted from vector of aliases
  void append_exprs(const std::vector<HIR::alias_t>& xs,
                    std::vector<HIR::expr_t>& ys) {
    for (HIR::alias_t x: xs) {
      ys.push_back(x->value);
    }
  }

  // string-ify traits
  std::string to_string_traits(Traits traits) {
    std::string prepend, result;
    if (contains_trait(traits, SingleTrait::kPure)) {
      result += prepend + "pure";
      prepend = ", ";
    }
    if (contains_trait(traits, SingleTrait::kTransform)) {
      result += prepend + "transform";
      prepend = ", ";
    }
    if (contains_trait(traits, SingleTrait::kLinear)) {
      result += prepend + "linear";
      prepend = ", ";
    }
    if (contains_trait(traits, SingleTrait::kAutostream)) {
      result += prepend + "autostream";
      prepend = ", ";
    }
    if (result.empty()) {
      result = "none";
    }
    return result;
  }

  /* compile-time function evaluation */

  // return a literal expression if one can be obtained at compile time
  HIR::expr_t get_comptime_literal(HIR::expr_t node) {
    if (node == nullptr) {
      return nullptr;
    }
    switch (node->expr_kind) {
      // direct literals (but not floating point)
      case HIR::expr_::ExprKind::kIntegerLiteral:
      case HIR::expr_::ExprKind::kBoolLiteral:
      case HIR::expr_::ExprKind::kStr:
      case HIR::expr_::ExprKind::kChar:
        return node;
      // for IDs, just copy the literal if it exists
      case HIR::expr_::ExprKind::kId: {
        HIR::Id_t id = dynamic_cast<HIR::Id_t>(node);
        HIR::resolved_t ref = id->ref;
        if (ref == nullptr) {
          return nullptr;
        }
        switch (ref->resolved_kind) {
          case HIR::resolved_::ResolvedKind::kDeclRef: {
            HIR::DeclRef_t dr = dynamic_cast<HIR::DeclRef_t>(ref);
            HIR::declaration_t decl = dr->ref;
            if (decl->dt == HIR::decltype_t::kVar) {
              return nullptr;
            }
            return decl->comptime_literal;
          }
          default:
            return nullptr;
        }
      }
      // try to evaluate any other expression
      default:
        return eval_comptime_literal(node);
    }
  }

  // derive a literal expression if one can be obtained at compile time
  HIR::expr_t eval_comptime_literal(HIR::expr_t node) {
    if (node == nullptr) {
      return nullptr;
    }
    if (node->mode != HIR::compmode_t::kComptime) {
      return nullptr;
    }
    if (node->type != nullptr &&
        node->type->datatype_kind == HIR::datatype_::DatatypeKind::kVVMType) {
      HIR::VVMType_t vvm_type = dynamic_cast<HIR::VVMType_t>(node->type);
      switch (VVM::vvm_types(vvm_type->t)) {
        // must be representable as a literal (but not floating point)
        case VVM::vvm_types::i64s:
        case VVM::vvm_types::b8s:
        case VVM::vvm_types::Ss:
        case VVM::vvm_types::c8s: {
          // round-trip through VVM
          HIR::mod_t wrapper = HIR::Module({HIR::Expr(node)}, "");
          VVM:: Program program = codegen(wrapper, VVM::Mode::kComptime,
                                          true, false);
          std::string result = VVM::interpret(program, VVM::Mode::kComptime);
          AST::mod_t ast = parse(result, true, false);
          HIR::mod_t hir = sema(ast, true, false);
          HIR::Module_t mod = dynamic_cast<HIR::Module_t>(hir);
          HIR::Expr_t expr = dynamic_cast<HIR::Expr_t>(mod->body[0]);
          return expr->value;
        }
        default:
          return nullptr;
      }
    }
    return nullptr;
  }

  // "downgrade" a HIR literal to an AST literal; for templates
  AST::expr_t downgrade(HIR::expr_t node) {
    if (node == nullptr) {
      return nullptr;
    }
    switch (node->expr_kind) {
      case HIR::expr_::ExprKind::kIntegerLiteral: {
        HIR::IntegerLiteral_t n = dynamic_cast<HIR::IntegerLiteral_t>(node);
        return AST::IntegerLiteral(n->n);
      }
      case HIR::expr_::ExprKind::kBoolLiteral: {
        HIR::BoolLiteral_t b = dynamic_cast<HIR::BoolLiteral_t>(node);
        return AST::BoolLiteral(b->b);
      }
      case HIR::expr_::ExprKind::kStr: {
        HIR::Str_t s = dynamic_cast<HIR::Str_t>(node);
        return AST::Str(s->s);
      }
      case HIR::expr_::ExprKind::kChar: {
        HIR::Char_t c = dynamic_cast<HIR::Char_t>(node);
        return AST::Char(c->c);
      }
      default:
        return nullptr;
    }
  }

  /* get info from a node */

  // return resolved item's type, or nullptr if not available
  HIR::datatype_t get_type(HIR::resolved_t node) {
    if (node == nullptr) {
      return nullptr;
    }
    switch (node->resolved_kind) {
      case HIR::resolved_::ResolvedKind::kDeclRef: {
        HIR::DeclRef_t ptr = dynamic_cast<HIR::DeclRef_t>(node);
        return ptr->ref->type;
      }
      case HIR::resolved_::ResolvedKind::kFuncRef: {
        HIR::FuncRef_t ptr = dynamic_cast<HIR::FuncRef_t>(node);
        HIR::FunctionDef_t def = dynamic_cast<HIR::FunctionDef_t>(ptr->ref);
        return get_type(def);
      }
      case HIR::resolved_::ResolvedKind::kGenericRef: {
        HIR::GenericRef_t ptr = dynamic_cast<HIR::GenericRef_t>(node);
        HIR::GenericDef_t def = dynamic_cast<HIR::GenericDef_t>(ptr->ref);
        return get_type(def);
      }
      case HIR::resolved_::ResolvedKind::kTemplateRef: {
        HIR::TemplateRef_t ptr = dynamic_cast<HIR::TemplateRef_t>(node);
        HIR::TemplateDef_t def = dynamic_cast<HIR::TemplateDef_t>(ptr->ref);
        return get_type(def);
      }
      case HIR::resolved_::ResolvedKind::kDataRef: {
        HIR::DataRef_t dr = dynamic_cast<HIR::DataRef_t>(node);
        HIR::DataDef_t dd = dynamic_cast<HIR::DataDef_t>(dr->ref);
        return HIR::Kind(HIR::UDT(dd->name, node));
      }
      case HIR::resolved_::ResolvedKind::kModRef: {
        return nullptr;
      }
      case HIR::resolved_::ResolvedKind::kVVMOpRef: {
        HIR::VVMOpRef_t ptr = dynamic_cast<HIR::VVMOpRef_t>(node);
        return ptr->type;
      }
      case HIR::resolved_::ResolvedKind::kVVMTypeRef: {
        HIR::VVMTypeRef_t ptr = dynamic_cast<HIR::VVMTypeRef_t>(node);
        return HIR::Kind(HIR::VVMType(ptr->t));
      }
      case HIR::resolved_::ResolvedKind::kSemaFuncRef: {
        HIR::SemaFuncRef_t ptr = dynamic_cast<HIR::SemaFuncRef_t>(node);
        return ptr->type;
      }
      case HIR::resolved_::ResolvedKind::kSemaTypeRef: {
        HIR::SemaTypeRef_t ptr = dynamic_cast<HIR::SemaTypeRef_t>(node);
        return ptr->type;
      }
    }
  }

  // return resolved item's traits
  Traits get_traits(HIR::resolved_t node) {
    if (node == nullptr) {
      return empty_traits;
    }
    switch (node->resolved_kind) {
      case HIR::resolved_::ResolvedKind::kDeclRef: {
        HIR::DeclRef_t ptr = dynamic_cast<HIR::DeclRef_t>(node);
        return ptr->ref->traits;
      }
      case HIR::resolved_::ResolvedKind::kFuncRef: {
        HIR::FuncRef_t ptr = dynamic_cast<HIR::FuncRef_t>(node);
        HIR::FunctionDef_t def = dynamic_cast<HIR::FunctionDef_t>(ptr->ref);
        return def->traits;
      }
      case HIR::resolved_::ResolvedKind::kGenericRef: {
        HIR::GenericRef_t ptr = dynamic_cast<HIR::GenericRef_t>(node);
        HIR::GenericDef_t def = dynamic_cast<HIR::GenericDef_t>(ptr->ref);
        return def->traits;
      }
      case HIR::resolved_::ResolvedKind::kTemplateRef: {
        return all_traits;
      }
      case HIR::resolved_::ResolvedKind::kDataRef: {
        return all_traits;
      }
      case HIR::resolved_::ResolvedKind::kModRef: {
        return all_traits;
      }
      case HIR::resolved_::ResolvedKind::kVVMOpRef: {
        HIR::VVMOpRef_t ptr = dynamic_cast<HIR::VVMOpRef_t>(node);
        HIR::FuncType_t ft = dynamic_cast<HIR::FuncType_t>(ptr->type);
        return ft->traits;
      }
      case HIR::resolved_::ResolvedKind::kVVMTypeRef: {
        return all_traits;
      }
      case HIR::resolved_::ResolvedKind::kSemaFuncRef: {
        HIR::SemaFuncRef_t ptr = dynamic_cast<HIR::SemaFuncRef_t>(node);
        HIR::FuncType_t ft = dynamic_cast<HIR::FuncType_t>(ptr->type);
        return ft->traits;
      }
      case HIR::resolved_::ResolvedKind::kSemaTypeRef: {
        return all_traits;
      }
    }
  }

  // return resolved item's mode
  HIR::compmode_t get_mode(HIR::resolved_t node) {
    if (node == nullptr) {
      return HIR::compmode_t::kNormal;
    }
    switch (node->resolved_kind) {
      case HIR::resolved_::ResolvedKind::kDeclRef: {
        HIR::DeclRef_t ptr = dynamic_cast<HIR::DeclRef_t>(node);
        return ptr->ref->mode;
      }
      default:
        return HIR::compmode_t::kComptime;
    }
  }

  // return type from a function definition
  HIR::datatype_t get_type(HIR::FunctionDef_t node) {
    std::vector<HIR::datatype_t> argtypes;
    for (HIR::declaration_t arg: node->args) {
      argtypes.push_back(arg->type);
    }
    HIR::datatype_t rettype = node->rettype;
    Traits traits = node->traits;
    return FuncType(argtypes, rettype, traits);
  }

  // return type from a generic function definition
  HIR::datatype_t get_type(HIR::GenericDef_t node) {
    std::vector<HIR::datatype_t> argtypes;
    for (HIR::declaration_t arg: node->args) {
      argtypes.push_back(arg->type);
    }
    HIR::datatype_t rettype = node->rettype;
    Traits traits = node->traits;
    return FuncType(argtypes, rettype, traits);
  }

  // return type from a template function definition
  HIR::datatype_t get_type(HIR::TemplateDef_t node) {
    std::vector<HIR::datatype_t> types;
    for (HIR::declaration_t t: node->templates) {
      types.push_back(t->type);
    }
    return TemplateType(types);
  }

  // return resolved item's scope, or zero if not available
  size_t get_scope(HIR::resolved_t node) {
    if (node == nullptr) {
      return 0;
    }
    size_t scope = 0;
    switch (node->resolved_kind) {
      case HIR::resolved_::ResolvedKind::kDataRef: {
        HIR::DataRef_t dr = dynamic_cast<HIR::DataRef_t>(node);
        HIR::DataDef_t dd = dynamic_cast<HIR::DataDef_t>(dr->ref);
        scope = dd->scope;
        break;
      }
      default:
        break;
    }
    return scope;
  }

  // get type's scope, or zero if not available
  size_t get_scope(HIR::datatype_t node) {
    if (node == nullptr) {
      return 0;
    }
    size_t scope = 0;
    switch (node->datatype_kind) {
      case HIR::datatype_::DatatypeKind::kUDT: {
        HIR::UDT_t udt = dynamic_cast<HIR::UDT_t>(node);
        scope = get_scope(udt->ref);
        break;
      }
      default:
        break;
    }
    return scope;
  }

  // get underlying data definition from a user-defined type
  HIR::DataDef_t get_data_def(HIR::datatype_t node) {
    switch (node->datatype_kind) {
      case HIR::datatype_::DatatypeKind::kUDT: {
        HIR::UDT_t udt = dynamic_cast<HIR::UDT_t>(node);
        HIR::DataRef_t dr = dynamic_cast<HIR::DataRef_t>(udt->ref);
        HIR::DataDef_t dd = dynamic_cast<HIR::DataDef_t>(dr->ref);
        return dd;
      }
      default:
        return nullptr;
    }
  }

  /* symbol resolution */

  // symbol map for a single scope
  struct Scope {
    typedef std::vector<HIR::resolved_t> Resolveds;
    typedef std::unordered_map<std::string, Resolveds> Map;
    Map map;
    const size_t previous_scope;
    Scope(size_t prev): previous_scope(prev) {}
  };

  // symbol resolution table
  std::vector<Scope> symbol_table_;
  size_t current_scope_;
  HIR::expr_t preferred_scope_;

  // return array of pointers to HIR nodes where symbol was declared
  Scope::Resolveds find_symbol(const std::string& symbol,
                               bool* in_preferred = nullptr) {
    // check the preferred scope first (query/sort/join have implied members)
    if (preferred_scope_ != nullptr) {
      size_t idx = get_scope(preferred_scope_->type);
      Scope::Resolveds initial = find_symbol_in_scope(symbol, idx);
      if (!initial.empty()) {
        if (in_preferred != nullptr) {
          *in_preferred = true;
        }
        return initial;
      }
    }
    if (in_preferred != nullptr) {
      *in_preferred = false;
    }

    // iteratively check current and prior scopes
    size_t i = current_scope_;
    bool done = false;
    while (!done) {
      Scope& scope = symbol_table_[i];
      auto symbol_iter = scope.map.find(symbol);
      if (symbol_iter != scope.map.end()) {
        return symbol_iter->second;
      }
      if (i != 0) {
        i = scope.previous_scope;
      } else {
        done = true;
      }
    }
    return Scope::Resolveds();
  }

  // search only the given scope
  Scope::Resolveds find_symbol_in_scope(const std::string& symbol,
                                        size_t idx) {
    Scope& scope = symbol_table_[idx];
    auto symbol_iter = scope.map.find(symbol);
    if (symbol_iter != scope.map.end()) {
      return symbol_iter->second;
    }
    return Scope::Resolveds();
  }

  // save pointer to HIR node for symbol; return false if already there
  bool store_symbol(const std::string& symbol, HIR::resolved_t ptr) {
    Scope& scope = symbol_table_[current_scope_];
    auto iter = scope.map.find(symbol);
    if (iter != scope.map.end()) {
      // check that we can overload if the symbol already exists
      auto& resolveds = iter->second;
      for (size_t i = 0; i < resolveds.size(); i++) {
        if (!is_overloadable(resolveds[i], ptr)) {
          if (interactive_ && is_overridable(resolveds[i])) {
            resolveds[i] = ptr;
            return true;
          } else {
            return false;
          }
        }
      }
      resolveds.push_back(ptr);
    } else {
      scope.map.emplace(symbol, std::initializer_list<HIR::resolved_t>{ptr});
    }
    return true;
  }

  // remove symbol from current scope; return false if not found
  bool remove_symbol(const std::string& symbol) {
    Scope& scope = symbol_table_[current_scope_];
    return scope.map.erase(symbol) > 0;
  }

  // remove symbol reference---needed to unwind scope during errors
  void remove_symbol_ref(const std::string& symbol, HIR::resolved_t ptr) {
    Scope& scope = symbol_table_[current_scope_];
    auto iter = scope.map.find(symbol);
    if (iter != scope.map.end()) {
      auto& resolveds = iter->second;
      for (size_t i = 0; i < resolveds.size(); i++) {
        if (resolveds[i] == ptr) {
          resolveds.erase(resolveds.begin() + i);
          break;
        }
      }
    }
  }

  // activate a new scope
  void push_scope() {
    symbol_table_.emplace_back(current_scope_);
    current_scope_ = symbol_table_.size() - 1;
  }

  // deactivate current scope
  void pop_scope() {
    current_scope_ = symbol_table_[current_scope_].previous_scope;
  }

  /* type check */

  // list of return types for each function definition in stack
  struct RetInfo {
    HIR::datatype_t type;
    Traits traits;
    RetInfo(HIR::datatype_t dt, Traits t): type(dt), traits(t) {}
  };
  std::stack<std::vector<RetInfo>> retinfo_stack_;

  // string-ify a datatype
  std::string to_string(HIR::datatype_t node) {
    if (node == nullptr) {
      return "_";
    }
    switch (node->datatype_kind) {
      case HIR::datatype_::DatatypeKind::kVVMType: {
        HIR::VVMType_t b = dynamic_cast<HIR::VVMType_t>(node);
        return VVM::empirical_type_strings[b->t];
      }
      case HIR::datatype_::DatatypeKind::kUDT: {
        HIR::UDT_t udt = dynamic_cast<HIR::UDT_t>(node);
        return udt->s;
      }
      case HIR::datatype_::DatatypeKind::kArray: {
        HIR::Array_t df = dynamic_cast<HIR::Array_t>(node);
        return "[" + to_string(df->type) + "]";
      }
      case HIR::datatype_::DatatypeKind::kFuncType: {
        HIR::FuncType_t ft = dynamic_cast<HIR::FuncType_t>(node);
        std::string result = "(";
        if (ft->argtypes.size() >= 1) {
          result += to_string(ft->argtypes[0]);
          for (size_t i = 1; i < ft->argtypes.size(); i++) {
            result += ", " + to_string(ft->argtypes[i]);
          }
        }
        result += ") -> " + to_string(ft->rettype);
        return result;
      }
      case HIR::datatype_::DatatypeKind::kTemplateType: {
        HIR::TemplateType_t ft = dynamic_cast<HIR::TemplateType_t>(node);
        std::string result = "{";
        if (ft->types.size() >= 1) {
          result += to_string(ft->types[0]);
          for (size_t i = 1; i < ft->types.size(); i++) {
            result += ", " + to_string(ft->types[i]);
          }
        }
        result += "}";
        return result;
      }
      case HIR::datatype_::DatatypeKind::kKind: {
        HIR::Kind_t k = dynamic_cast<HIR::Kind_t>(node);
        return "Kind(" + to_string(k->type) + ")";
      }
      case HIR::datatype_::DatatypeKind::kVoid: {
        return "()";
      }
    }
  }

  // string-ify the underlying values of a UDT
  std::string to_string_udt(HIR::datatype_t node) {
    std::string result;
    if (node != nullptr &&
        node->datatype_kind == HIR::datatype_::DatatypeKind::kUDT) {
      HIR::DataDef_t dd = get_data_def(node);
      result = "(";
      for (size_t i = 0; i < dd->body.size(); i++) {
        if (i > 0) {
          result += ", ";
        }
        result += to_string(dd->body[i]->type);
      }
      result += ")";
    }
    return result;
  }

  // string-ify generic arguments; useful for name mangling
  std::string to_string_generics(const std::vector<HIR::expr_t>& args) {
    std::string result = "(";
    if (args.size() >= 1) {
      result += to_string(args[0]->type);
      for (size_t i = 1; i < args.size(); i++) {
        result += ", " + to_string(args[i]->type);
      }
    }
    result += ")";
    return result;
  }

  // string-ify template parameters; useful for name mangling
  std::string to_string_templates(const std::vector<HIR::expr_t>& templates) {
    std::string result = "{";
    if (templates.size() >= 1) {
      result += to_string_literal(templates[0]);
      for (size_t i = 1; i < templates.size(); i++) {
        result += ", " + to_string_literal(templates[i]);
      }
    }
    result += "}";
    return result;
  }

  // return string of a literal value
  std::string to_string_literal(HIR::expr_t node) {
    if (node == nullptr) {
      return "";
    }
    switch (node->expr_kind) {
      case HIR::expr_::ExprKind::kIntegerLiteral: {
        HIR::IntegerLiteral_t n = dynamic_cast<HIR::IntegerLiteral_t>(node);
        return std::to_string(n->n);
      }
      case HIR::expr_::ExprKind::kBoolLiteral: {
        HIR::BoolLiteral_t b = dynamic_cast<HIR::BoolLiteral_t>(node);
        return b->b ? "true" : "false";
      }
      case HIR::expr_::ExprKind::kStr: {
        HIR::Str_t s = dynamic_cast<HIR::Str_t>(node);
        return '"' + s->s + '"';
      }
      case HIR::expr_::ExprKind::kChar: {
        HIR::Char_t c = dynamic_cast<HIR::Char_t>(node);
        return "'" + std::string(1, c->c) + "'";
      }
      default: {
        if (is_kind_type(node->type)) {
          HIR::Kind_t k = dynamic_cast<HIR::Kind_t>(node->type);
          return to_string(k->type);
        }
      }
    }
    return "";
  }

  // validate that two types have the same underlying structure
  bool is_same_type(HIR::datatype_t left, HIR::datatype_t right) {
    if (left == nullptr || right == nullptr) {
      return true;
    }
    if (left->datatype_kind != right->datatype_kind) {
      return false;
    }
    switch (left->datatype_kind) {
      case HIR::datatype_::DatatypeKind::kVVMType: {
        HIR::VVMType_t left_b = dynamic_cast<HIR::VVMType_t>(left);
        HIR::VVMType_t right_b = dynamic_cast<HIR::VVMType_t>(right);
        return left_b->t == right_b->t;
      }
      case HIR::datatype_::DatatypeKind::kUDT: {
        HIR::DataDef_t left_dd = get_data_def(left);
        HIR::DataDef_t right_dd = get_data_def(right);
        if (left_dd->body.size() != right_dd->body.size()) {
          return false;
        }
        for (size_t i = 0; i < left_dd->body.size(); i++) {
          if (!is_same_type(left_dd->body[i]->type,
                            right_dd->body[i]->type) ||
              (left_dd->body[i]->name != right_dd->body[i]->name)) {
            return false;
          }
        }
        break;
      }
      case HIR::datatype_::DatatypeKind::kArray: {
        HIR::Array_t left_arr = dynamic_cast<HIR::Array_t>(left);
        HIR::Array_t right_arr = dynamic_cast<HIR::Array_t>(right);
        if (!is_same_type(left_arr->type, right_arr->type)) {
          return false;
        }
        break;
      }
      case HIR::datatype_::DatatypeKind::kFuncType: {
        HIR::FuncType_t left_ft = dynamic_cast<HIR::FuncType_t>(left);
        HIR::FuncType_t right_ft = dynamic_cast<HIR::FuncType_t>(right);
        if (left_ft->argtypes.size() != right_ft->argtypes.size()) {
          return false;
        }
        for (size_t i = 0; i < left_ft->argtypes.size(); i++) {
          if (!is_same_type(left_ft->argtypes[i], right_ft->argtypes[i])) {
            return false;
          }
        }
        if (!is_same_type(left_ft->rettype, right_ft->rettype)) {
          return false;
        }
        break;
      }
      case HIR::datatype_::DatatypeKind::kTemplateType: {
        HIR::TemplateType_t left_ft = dynamic_cast<HIR::TemplateType_t>(left);
        HIR::TemplateType_t right_ft =
          dynamic_cast<HIR::TemplateType_t>(right);
        if (left_ft->types.size() != right_ft->types.size()) {
          return false;
        }
        for (size_t i = 0; i < left_ft->types.size(); i++) {
          if (!is_same_type(left_ft->types[i], right_ft->types[i])) {
            return false;
          }
        }
        break;
      }
      case HIR::datatype_::DatatypeKind::kKind: {
        HIR::Kind_t leftk = dynamic_cast<HIR::Kind_t>(left);
        HIR::Kind_t rightk = dynamic_cast<HIR::Kind_t>(right);
        return is_same_type(leftk->type, rightk->type);
      }
      case HIR::datatype_::DatatypeKind::kVoid: {
        return true;
      }
    }
    return true;
  }

  // ensure instantiated structure reflects array-ized underlying structure
  bool is_dataframe_type_valid(HIR::DataDef_t left, HIR::resolved_t ref) {
    HIR::DataRef_t dr = dynamic_cast<HIR::DataRef_t>(ref);
    HIR::DataDef_t right = dynamic_cast<HIR::DataDef_t>(dr->ref);

    if (left->body.size() != right->body.size()) {
      return false;
    }
    for (size_t i = 0; i < left->body.size(); i++) {
      if (!is_same_type(HIR::Array(left->body[i]->type),
                        right->body[i]->type) ||
          (left->body[i]->name != right->body[i]->name)) {
        return false;
      }
    }
    return true;
  }

  // find scalar UDT for a Dataframe name (assumes leading '!')
  HIR::DataDef_t get_underlying_udt(const std::string& name) {
    // find name
    std::string underlying_name = name.substr(1);
    Scope::Resolveds underlying_resolveds = find_symbol(underlying_name);
    if (underlying_resolveds.empty()) {
      return nullptr;
    }
    HIR::resolved_t ref = underlying_resolveds[0];

    // extract UDT
    HIR::datatype_t type = get_type(ref);
    if (is_kind_type(type)) {
      return get_data_def(get_underlying_type(type));
    }
    return nullptr;
  }

  // attempt to make Dataframe with the given type name
  HIR::datatype_t make_dataframe(const std::string& name) {
    // find underlying data definition first
    HIR::DataDef_t node = get_underlying_udt(name);
    if (node == nullptr) {
      return nullptr;
    }

    // see if the Dataframe already exists
    HIR::resolved_t ref = nullptr;
    Scope::Resolveds resolveds = find_symbol(name);
    if (resolveds.size() != 0) {
      // ensure underlying hasn't changed
      ref = resolveds[0];
      if (!is_dataframe_type_valid(node, ref)) {
        ref = nullptr;
      }
    }
    if (ref == nullptr) {
      // make Dataframe definition
      std::vector<HIR::declaration_t> body;
      push_scope();
      size_t scope = current_scope_;
      for (HIR::declaration_t b: node->body) {
        auto d =
          HIR::declaration(b->name, nullptr, b->value, b->dt,
                           HIR::Array(b->type), empty_traits,
                           HIR::compmode_t::kNormal, nullptr, b->offset);
        store_symbol(b->name, HIR::DeclRef(d));
        body.push_back(d);
      }
      pop_scope();
      HIR::expr_t single = nullptr;
      HIR::stmt_t new_node = HIR::DataDef(name, node->templates, body, single,
                                          scope);
      ref = HIR::DataRef(new_node);
      store_symbol(name, ref);
    }
    return HIR::UDT(name, ref);
  }

  bool is_string_type(HIR::datatype_t node) {
    if (node != nullptr &&
        node->datatype_kind == HIR::datatype_::DatatypeKind::kVVMType) {
      HIR::VVMType_t b = dynamic_cast<HIR::VVMType_t>(node);
      return b->t == size_t(VVM::vvm_types::Ss);
    }
    return false;
  }

  bool is_indexable_type(HIR::datatype_t node) {
    if (node != nullptr &&
        node->datatype_kind == HIR::datatype_::DatatypeKind::kVVMType) {
      HIR::VVMType_t b = dynamic_cast<HIR::VVMType_t>(node);
      return b->t == size_t(VVM::vvm_types::i64s);
    }
    return false;
  }

  bool is_boolean_type(HIR::datatype_t node) {
    if (node != nullptr &&
        node->datatype_kind == HIR::datatype_::DatatypeKind::kVVMType) {
      HIR::VVMType_t b = dynamic_cast<HIR::VVMType_t>(node);
      return b->t == size_t(VVM::vvm_types::b8s);
    }
    return false;
  }

  bool is_dataframe_type(HIR::datatype_t node) {
    if (node != nullptr &&
        node->datatype_kind == HIR::datatype_::DatatypeKind::kUDT) {
      HIR::UDT_t udt = dynamic_cast<HIR::UDT_t>(node);
      return udt->s[0] == '!';
    }
    return false;
  }

  bool is_array_type(HIR::datatype_t node) {
    return (node != nullptr &&
            node->datatype_kind == HIR::datatype_::DatatypeKind::kArray);
  }

  // can overload types and functions with new functions (prohibit sema)
  bool is_overloadable(HIR::resolved_t first, HIR::resolved_t second) {
    switch (first->resolved_kind) {
      // overload types with functions
      case HIR::resolved_::ResolvedKind::kVVMTypeRef:
      case HIR::resolved_::ResolvedKind::kDataRef: {
        switch (second->resolved_kind) {
          case HIR::resolved_::ResolvedKind::kVVMOpRef:
          case HIR::resolved_::ResolvedKind::kFuncRef:
            return true;
          default:
            return false;
        }
      }
      // overload functions with unique signatures
      case HIR::resolved_::ResolvedKind::kVVMOpRef:
      case HIR::resolved_::ResolvedKind::kFuncRef:
      case HIR::resolved_::ResolvedKind::kGenericRef: {
        switch (second->resolved_kind) {
          case HIR::resolved_::ResolvedKind::kVVMOpRef:
          case HIR::resolved_::ResolvedKind::kFuncRef:
          case HIR::resolved_::ResolvedKind::kGenericRef:
            return !is_same_type(get_type(first), get_type(second));
          default:
            return false;
        }
      }
      default:
        return false;
    }
  }

  // can override anything that isn't builtin
  bool is_overridable(HIR::resolved_t ref) {
    if (ref == nullptr) {
      return true;
    }
    switch (ref->resolved_kind) {
      case HIR::resolved_::ResolvedKind::kVVMOpRef:
      case HIR::resolved_::ResolvedKind::kVVMTypeRef:
      case HIR::resolved_::ResolvedKind::kSemaFuncRef:
      case HIR::resolved_::ResolvedKind::kSemaTypeRef:
        return false;
      default:
        return true;
    }
  }

  // can call functions and types (casts)
  bool is_callable(HIR::datatype_t node) {
    if (node == nullptr) {
      return true;
    }
    switch (node->datatype_kind) {
      case HIR::datatype_::DatatypeKind::kFuncType:
      case HIR::datatype_::DatatypeKind::kKind:
        return true;
      default:
        return false;
    }
  }

  // return an Id from a TemplatedId
  HIR::Id_t construct_id(HIR::expr_t node) {
    if (node == nullptr) {
      return nullptr;
    }
    if (node->expr_kind == HIR::expr_::ExprKind::kId) {
      return dynamic_cast<HIR::Id_t>(node);
    }
    if (node->expr_kind == HIR::expr_::ExprKind::kTemplatedId) {
      HIR::TemplatedId_t temp = dynamic_cast<HIR::TemplatedId_t>(node);
      HIR::expr_t e = HIR::Id(temp->name, temp->ref, temp->type, temp->traits,
                              temp->mode, temp->name);
      return dynamic_cast<HIR::Id_t>(e);
    }
    return nullptr;
  }

  bool is_generic(HIR::resolved_t node) {
    return (node != nullptr &&
            node->resolved_kind == HIR::resolved_::ResolvedKind::kGenericRef);
  }

  bool is_template(HIR::resolved_t node) {
    return (node != nullptr &&
            node->resolved_kind == HIR::resolved_::ResolvedKind::kTemplateRef);
  }

  bool is_overloaded(HIR::expr_t node) {
    return (node != nullptr &&
            node->expr_kind == HIR::expr_::ExprKind::kOverloadedId);
  }

  bool is_id(HIR::expr_t node) {
    return (node != nullptr &&
            node->expr_kind == HIR::expr_::ExprKind::kId);
  }

  bool is_expr(HIR::stmt_t node) {
    return (node != nullptr &&
            node->stmt_kind == HIR::stmt_::StmtKind::kExpr);
  }

  bool is_slice(HIR::slice_t node) {
    return (node != nullptr &&
            node->slice_kind == HIR::slice_::SliceKind::kSlice);
  }

  bool is_kind_type(HIR::datatype_t node) {
    return (node != nullptr &&
            node->datatype_kind == HIR::datatype_::DatatypeKind::kKind);
  }

  bool is_void_type(HIR::datatype_t node) {
    return (node != nullptr &&
            node->datatype_kind == HIR::datatype_::DatatypeKind::kVoid);
  }

  // expressions are temporary if they do not outlive their immediate use
  bool is_temporary(HIR::expr_t node) {
    if (node != nullptr) {
      switch (node->expr_kind) {
        case HIR::expr_::ExprKind::kMember:
        case HIR::expr_::ExprKind::kSubscript:
        case HIR::expr_::ExprKind::kId:
        case HIR::expr_::ExprKind::kImpliedMember:
        case HIR::expr_::ExprKind::kOverloadedId:
          return false;
        default:
          break;
      }
    }
    return true;
  }

  // return whether an expression can be written to; check is_temporary() too
  bool is_writeable(HIR::expr_t node) {
    if (node != nullptr) {
      switch (node->expr_kind) {
        case HIR::expr_::ExprKind::kMember: {
          HIR::Member_t mem = dynamic_cast<HIR::Member_t>(node);
          return is_writeable(mem->value);
        }
        case HIR::expr_::ExprKind::kSubscript: {
          HIR::Subscript_t sub = dynamic_cast<HIR::Subscript_t>(node);
          return is_writeable(sub->value);
        }
        case HIR::expr_::ExprKind::kId: {
          HIR::Id_t id = dynamic_cast<HIR::Id_t>(node);
          HIR::resolved_t ref = id->ref;
          if (ref == nullptr) {
            return true;
          }
          if (ref->resolved_kind != HIR::resolved_::ResolvedKind::kDeclRef) {
            return false;
          }
          HIR::DeclRef_t dr = dynamic_cast<HIR::DeclRef_t>(ref);
          return dr->ref->dt == HIR::decltype_t::kVar;
        }
        default:
          return true;
      }
    }
    return true;
  }

  // return underlying type from higher kinds
  HIR::datatype_t get_underlying_type(HIR::datatype_t node) {
    if (node == nullptr) {
      return nullptr;
    }
    switch (node->datatype_kind) {
      case HIR::datatype_::DatatypeKind::kArray: {
        HIR::Array_t arr = dynamic_cast<HIR::Array_t>(node);
        return arr->type;
      }
      case HIR::datatype_::DatatypeKind::kKind: {
        HIR::Kind_t k = dynamic_cast<HIR::Kind_t>(node);
        if (k->type == nullptr) {
          // this was a generic type
          return k;
        }
        return k->type;
      }
      default:
        return nullptr;
    }
  }

  // return function's argument types
  std::vector<HIR::datatype_t> get_argtypes(HIR::datatype_t node) {
    if (node == nullptr) {
      return std::vector<HIR::datatype_t>();
    }
    switch (node->datatype_kind) {
      case HIR::datatype_::DatatypeKind::kFuncType: {
        HIR::FuncType_t ft = dynamic_cast<HIR::FuncType_t>(node);
        return ft->argtypes;
      }
      case HIR::datatype_::DatatypeKind::kTemplateType: {
        HIR::TemplateType_t ft = dynamic_cast<HIR::TemplateType_t>(node);
        return ft->types;
      }
      case HIR::datatype_::DatatypeKind::kKind: {
        HIR::Kind_t k = dynamic_cast<HIR::Kind_t>(node);
        std::vector<HIR::datatype_t> argtypes;
        HIR::DataDef_t dd = get_data_def(k->type);
        if (dd != nullptr) {
          for (HIR::declaration_t d: dd->body) {
            argtypes.push_back(d->type);
          }
        }
        return argtypes;
      }
      default:
        return std::vector<HIR::datatype_t>();
    }
  }

  // return function's return type
  HIR::datatype_t get_rettype(HIR::datatype_t node) {
    if (node == nullptr) {
      return nullptr;
    }
    switch (node->datatype_kind) {
      case HIR::datatype_::DatatypeKind::kFuncType: {
        HIR::FuncType_t ft = dynamic_cast<HIR::FuncType_t>(node);
        return ft->rettype;
      }
      case HIR::datatype_::DatatypeKind::kKind: {
        HIR::Kind_t k = dynamic_cast<HIR::Kind_t>(node);
        return k->type;
      }
      default:
         return nullptr;
    }
  }

  // return explanation of why function arguments didn't match
  std::string match_args(const std::vector<HIR::expr_t>& args,
                         HIR::datatype_t func_type) {
    if (func_type == nullptr) {
      return std::string();
    }
    std::ostringstream oss;
    std::vector<HIR::datatype_t> argtypes = get_argtypes(func_type);
    if (args.size() != argtypes.size()) {
      oss << "wrong number of arguments; expected " << argtypes.size()
          << " but got " << args.size();
    } else {
      for (size_t i = 0; i < args.size(); i++) {
        if (!is_same_type(args[i]->type, argtypes[i])) {
          oss << "argument type at position " << i << " does not match: "
              << to_string(args[i]->type) << " vs " << to_string(argtypes[i]);
          break;
        }
      }
    }
    return oss.str();
  }

  // replace overloaded ID with specific ID that matches arguments
  std::string choose_overloaded(HIR::expr_t& func,
                                const std::vector<HIR::expr_t>& args) {
    HIR::OverloadedId_t id = dynamic_cast<HIR::OverloadedId_t>(func);
    std::string err_msg;
    size_t counted_mismatch = 0;
    const size_t max_counted = 3;
    for (HIR::resolved_t ref: id->refs) {
      HIR::datatype_t func_type = get_type(ref);
      std::string result = match_args(args, func_type);
      if (result.empty()) {
        // replace overload with specific
        func = HIR::Id(id->s, ref, func_type, get_traits(ref),
                       HIR::compmode_t::kNormal, id->s);
        err_msg.clear();
        break;
      } else {
        counted_mismatch++;
        if (counted_mismatch <= max_counted) {
          err_msg += "\n  candidate: " + to_string(func_type) + "\n    "
                  + result;
        }
      }
    }
    if (!err_msg.empty()) {
      if (counted_mismatch > max_counted) {
        err_msg += "\n  ...\n  <"
                + std::to_string(counted_mismatch - max_counted)
                + " others>";
      }
      err_msg = "unable to match overloaded function " + id->s + err_msg;
    }
    return err_msg;
  }

  // replace the OverloadedId with the first Id
  HIR::expr_t unoverload(HIR::expr_t node) {
    if (is_overloaded(node)) {
      HIR::OverloadedId_t id = dynamic_cast<HIR::OverloadedId_t>(node);
      HIR::resolved_t ref = id->refs[0];
      return HIR::Id(id->s, ref, id->type, id->traits, id->mode, id->s);
    }
    return node;
  }

  // create an anonymous func name
  std::string anon_func_name() {
    static size_t counter = 0;
    std::ostringstream oss;
    oss << "anon__" << counter++;
    return oss.str();
  }

  // create an anonymous data name
  std::string anon_data_name() {
    static size_t counter = 0;
    std::ostringstream oss;
    oss << "Anon__" << counter++;
    return oss.str();
  }

  // return HIR node for a type definition string
  HIR::stmt_t create_datatype(const std::string& type_name,
                              const std::string& type_def) {
    std::string data_str = "data Anon: " + type_def + " end";
    AST::mod_t ast = parse(data_str, false, false);
    AST::Module_t mod = dynamic_cast<AST::Module_t>(ast);
    AST::stmt_t parsed = mod->body[0];
    AST::DataDef_t dd = dynamic_cast<AST::DataDef_t>(parsed);
    dd->name = type_name;
    return visit(parsed);
  }

  // return a type definition string from aliases
  std::string get_type_string(const std::vector<HIR::alias_t>& aliases) {
    std::string result;
    for (HIR::alias_t a: aliases) {
      std::string name = a->name.empty() ? a->value->name : a->name;
      HIR::datatype_t dt = is_array_type(a->value->type) ?
                             get_underlying_type(a->value->type) :
                             a->value->type;
      std::string new_item = name + ": " + to_string(dt);
      if (result.empty()) {
        result = new_item;
      } else {
        result += ", " + new_item;
      }
    }
    return result;
  }

  // return a type definition string from datatype
  std::string get_type_string(HIR::datatype_t node,
                              const std::string& sep = ", ") {
    std::string result;
    HIR::DataDef_t dd = get_data_def(node);
    for (HIR::declaration_t d: dd->body) {
      HIR::datatype_t dt = is_array_type(d->type) ?
                             get_underlying_type(d->type) :
                             d->type;
      std::string new_item = d->name + ": " + to_string(dt);
      if (result.empty()) {
        result = new_item;
      } else {
        result += sep + new_item;
      }
    }
    return result;
  }

  // drop a set of columns from a Dataframe; return string for further work
  std::string drop_columns(HIR::datatype_t orig_type,
                           HIR::datatype_t drop_type,
                           const std::string& extra) {
    HIR::DataDef_t orig_dd = get_data_def(orig_type);

    // store dropped names for easy look-up
    std::unordered_set<std::string> dropped_names;
    if (drop_type != nullptr) {
      HIR::DataDef_t drop_dd = get_data_def(drop_type);
      for (HIR::declaration_t d: drop_dd->body) {
        dropped_names.insert(d->name);
      }
    }
    if (!extra.empty()) {
      dropped_names.insert(extra);
    }

    // build string from entries that aren't among the dropped names
    std::string result;
    for (HIR::declaration_t d: orig_dd->body) {
      if (dropped_names.find(d->name) == dropped_names.end()) {
        HIR::datatype_t dt = is_array_type(d->type) ?
                               get_underlying_type(d->type) :
                               d->type;
        std::string new_item = d->name + ": " + to_string(dt);
        if (result.empty()) {
          result = new_item;
        } else {
          result += ", " + new_item;
        }
      }
    }
    return result;
  }

  /* builtin items */

  enum class SemaCodes: size_t {
    kTypeOf,
    kTraitsOf,
    kModeOf,
    kColumns,
    kCompile
  };

  // try to internally invoke function; return nullptr if unable
  HIR::expr_t attempt_sema_function(HIR::expr_t func,
                                    const std::vector<HIR::expr_t>& args) {
    if (func->expr_kind == HIR::expr_::ExprKind::kId) {
      HIR::Id_t id = dynamic_cast<HIR::Id_t>(func);
      if (id->ref != nullptr &&
          id->ref->resolved_kind ==
          HIR::resolved_::ResolvedKind::kSemaFuncRef) {
        HIR::SemaFuncRef_t ptr = dynamic_cast<HIR::SemaFuncRef_t>(id->ref);
        SemaCodes code = SemaCodes(ptr->code);
        switch (code) {
          case SemaCodes::kTypeOf: {
            return sema_function_type_of(args);
          }
          case SemaCodes::kTraitsOf: {
            return sema_function_traits_of(args);
          }
          case SemaCodes::kModeOf: {
            return sema_function_mode_of(args);
          }
          case SemaCodes::kColumns: {
            return sema_function_columns(args);
          }
          case SemaCodes::kCompile: {
            return sema_function_compile(args);
          }
        }
      }
    }
    return nullptr;
  }

  // type_of() function
  HIR::expr_t sema_function_type_of(const std::vector<HIR::expr_t>& args) {
    HIR::expr_t a = args[0];
    HIR::datatype_t t = a->type;
    std::string s = to_string(t);
    // no type for overloaded functions
    if (is_overloaded(a)) {
      HIR::OverloadedId_t id = dynamic_cast<HIR::OverloadedId_t>(a);
      if (id->type->datatype_kind ==
          HIR::datatype_::DatatypeKind::kFuncType) {
        s = "overloaded";
      }
    }
    return HIR::TypeOf(a, HIR::Kind(t), all_traits,
                       HIR::compmode_t::kComptime, s);
  }

  // traits_of() function
  HIR::expr_t sema_function_traits_of(const std::vector<HIR::expr_t>& args) {
    HIR::expr_t a = args[0];
    std::string s = to_string_traits(a->traits);
    return HIR::TraitsOf(a, s, HIR::Void(), all_traits,
                         HIR::compmode_t::kComptime, a->name);
  }

  // mode_of() function
  HIR::expr_t sema_function_mode_of(const std::vector<HIR::expr_t>& args) {
    HIR::expr_t a = args[0];
    std::string s = HIR::to_string(a->mode);
    return HIR::ModeOf(a, s, HIR::Void(), all_traits,
                       HIR::compmode_t::kComptime, a->name);
  }

  // columns() function
  HIR::expr_t sema_function_columns(const std::vector<HIR::expr_t>& args) {
    HIR::expr_t a = args[0];
    HIR::datatype_t t = a->type;
    std::string s = "<none>";
    if (is_kind_type(t)) {
      t = get_underlying_type(t);
    }
    // only show members if this is a UDT
    if (get_scope(t)) {
      s = get_type_string(t, "\n");
    }
    return HIR::Columns(a, s, HIR::Void(), all_traits,
                        HIR::compmode_t::kComptime, a->name);
  }

  // compile() function
  HIR::expr_t sema_function_compile(const std::vector<HIR::expr_t>& args) {
    HIR::expr_t a = args[0];
    std::vector<HIR::stmt_t> body;
    HIR::datatype_t typee = HIR::Void();
    Traits traits = all_traits;
    HIR::compmode_t mode = HIR::compmode_t::kComptime;

    // must derive the string at compile time
    HIR::expr_t literal = get_comptime_literal(a);
    if (literal == nullptr || !is_string_type(literal->type)) {
      sema_err_ << "Error: compile() requires a comptime string" << std::endl;
    } else {
      HIR::Str_t str = dynamic_cast<HIR::Str_t>(literal);
      AST::mod_t ast = parse(str->s, true, false);
      HIR::mod_t hir = sema(ast, true, false);
      HIR::Module_t mod = dynamic_cast<HIR::Module_t>(hir);
      body = std::move(mod->body);

      // if last stmt is a non-void expr, then capture its type/traits/mode
      auto last_stmt = body.back();
      if (last_stmt->stmt_kind == HIR::stmt_::StmtKind::kExpr) {
        HIR::Expr_t e = dynamic_cast<HIR::Expr_t>(last_stmt);
        HIR::datatype_t dt = e->value->type;
        if (!is_void_type(dt)) {
          HIR::expr_t value = e->value;
          typee = value->type;
          traits = value->traits;
          mode = value->mode;
        }
      }
    }
    return HIR::Compile(a, body, typee, traits, mode, a->name);
  }

  // save all builtin items so that id resolution will find them
  void save_builtins() {
    store_symbol("type_of", HIR::SemaFuncRef(size_t(SemaCodes::kTypeOf),
      HIR::FuncType({nullptr}, HIR::Void(), all_traits)));

    store_symbol("traits_of", HIR::SemaFuncRef(size_t(SemaCodes::kTraitsOf),
      HIR::FuncType({nullptr}, HIR::Void(), all_traits)));

    store_symbol("mode_of", HIR::SemaFuncRef(size_t(SemaCodes::kModeOf),
      HIR::FuncType({nullptr}, HIR::Void(), all_traits)));

    store_symbol("columns", HIR::SemaFuncRef(size_t(SemaCodes::kColumns),
      HIR::FuncType({nullptr}, HIR::Void(), all_traits)));

    store_symbol("compile", HIR::SemaFuncRef(size_t(SemaCodes::kCompile),
      HIR::FuncType({nullptr}, HIR::Void(), all_traits)));

    store_symbol("Type", HIR::SemaTypeRef(HIR::Kind(nullptr)));

#include <VVM/builtins.h>
  }

  /* miscellaneous */

  std::ostringstream sema_err_;

  bool interactive_;

  void nyi(const std::string& rule) const {
    std::string msg = "Not yet implemented: " + rule + '\n';
    throw std::logic_error(msg);
  }

 protected:

  antlrcpp::Any visitModule(AST::Module_t node) override {
    sema_err_.str("");
    sema_err_.clear();
    std::vector<HIR::stmt_t> results;
    for (AST::stmt_t s: node->body) {
      results.push_back(visit(s));
    }
    history_.insert(history_.end(), results.begin(), results.end());
    return HIR::Module(results, node->docstring);
  }

  antlrcpp::Any visitFunctionDef(AST::FunctionDef_t node) override {
    size_t starting_err_length = sema_err_.str().size();
    // evaluate template parameters in a new scope
    size_t outer_scope = current_scope_;
    push_scope();
    size_t inner_scope = current_scope_;
    std::vector<HIR::declaration_t> templates;
    for (AST::declaration_t t: node->templates) {
      if (t->value != nullptr) {
        templates.push_back(visit(t));
      }
    }
    // evaluate arguments
    std::vector<HIR::declaration_t> args;
    for (AST::declaration_t a: node->args) {
      HIR::declaration_t d = visit(a);
      d->traits = all_traits;
      d->mode = HIR::compmode_t::kNormal;
      args.push_back(d);
    }
    // get explicit return type
    HIR::expr_t explicit_rettype = nullptr;
    if (node->explicit_rettype) {
      explicit_rettype = visit(node->explicit_rettype);
    }
    HIR::datatype_t rettype = nullptr;
    if (explicit_rettype != nullptr) {
      if (is_kind_type(explicit_rettype->type)) {
        rettype = get_underlying_type(explicit_rettype->type);
      } else {
        sema_err_ << "Error: return type for " << node->name
                  << " has invalid type" << std::endl;
      }
    }
    // create shell now so body can have recursion
    std::vector<HIR::stmt_t> body;
    HIR::expr_t single = nullptr;
    HIR::stmt_t new_node =
      HIR::FunctionDef(node->name, templates, args, body, single,
                       node->force_inline, explicit_rettype, node->docstring,
                       rettype, empty_traits, inner_scope, node);
    HIR::FunctionDef_t fd = dynamic_cast<HIR::FunctionDef_t>(new_node);
    HIR::resolved_t ref = HIR::FuncRef(new_node);
    // store name in outer scope
    current_scope_ = outer_scope;
    if (!store_symbol(node->name, ref)) {
      sema_err_ << "Error: symbol " << node->name
                << " was already defined" << std::endl;
    }
    // evaluate body in the inner scope
    current_scope_ = inner_scope;
    retinfo_stack_.push(std::vector<RetInfo>());
    for (AST::stmt_t b: node->body) {
      body.push_back(visit(b));
    }
    // evaluate single expression as if it were a return statement
    if (node->single) {
      single = visit(node->single);
      body.push_back(HIR::Return(single, get_comptime_literal(single)));
      HIR::datatype_t dt = single->type;
      Traits t = single->traits;
      retinfo_stack_.top().emplace_back(dt, t);
    }
    fd->body = body;
    fd->single = single;
    pop_scope();
    // get body's return type
    HIR::datatype_t body_rettype = nullptr;
    Traits traits = empty_traits;
    auto& retinfos = retinfo_stack_.top();
    if (retinfos.empty()) {
      body_rettype = HIR::Void();
    } else {
      body_rettype = retinfos[0].type;
      traits = retinfos[0].traits;
      for (size_t i = 1; i < retinfos.size(); i++) {
        if (!is_same_type(body_rettype, retinfos[i].type)) {
          sema_err_ << "Error: mismatched return types in function "
                    << node->name << ": " << to_string(body_rettype) << " vs "
                    << to_string(retinfos[i].type) << std::endl;
        }
        traits &= retinfos[i].traits;
      }
    }
    retinfo_stack_.pop();
    // infer return type if needed
    if (rettype == nullptr) {
      rettype = body_rettype;
    }
    if (rettype == nullptr) {
      sema_err_ << "Error: unable to determine return type for function "
                << node->name << std::endl;
    }
    if (!is_same_type(rettype, body_rettype)) {
      sema_err_ << "Error: mismatched return types: " << to_string(rettype)
                << " vs " << to_string(body_rettype) << std::endl;
    }
    // check if this had been a cast definition
    Scope::Resolveds resolveds = find_symbol(node->name);
    HIR::datatype_t cast_type = get_type(resolveds[0]);
    if (is_kind_type(cast_type)) {
      HIR::datatype_t expected = get_underlying_type(cast_type);
      if (!is_same_type(rettype, expected) &&
          !is_same_type(rettype, HIR::Array(expected))) {
        sema_err_ << "Error: cast definition for " << node->name
                  << " must return its own type" << std::endl;
      }
    }
    // put everything together
    fd->rettype = rettype;
    fd->traits = traits;
    if (sema_err_.str().size() != starting_err_length) {
      // remove from scope if an error had occurred
      remove_symbol_ref(node->name, ref);
    }
    return new_node;
  }

  antlrcpp::Any visitGenericDef(AST::GenericDef_t node) override {
    size_t starting_err_length = sema_err_.str().size();
    // get name
    AST::FunctionDef_t fd = dynamic_cast<AST::FunctionDef_t>(node->original);
    std::string name = fd->name;

    // get explicit return type
    HIR::expr_t explicit_rettype = nullptr;
    if (node->explicit_rettype) {
      explicit_rettype = visit(node->explicit_rettype);
    }
    HIR::datatype_t rettype = nullptr;
    if (explicit_rettype != nullptr) {
      if (is_kind_type(explicit_rettype->type)) {
        rettype = get_underlying_type(explicit_rettype->type);
      } else {
        sema_err_ << "Error: return type for " << name
                  << " has invalid type" << std::endl;
      }
    }

    // evaluate arguments in new scope
    push_scope();
    std::vector<HIR::declaration_t> args;
    for (AST::declaration_t a: node->args) {
      HIR::declaration_t d = visit(a);
      d->traits = all_traits;
      d->mode = HIR::compmode_t::kNormal;
      args.push_back(d);
    }
    pop_scope();

    // until explicit traits are a thing
    size_t traits = all_traits;

    // construct node
    std::vector<HIR::stmt_t> instantiated;
    HIR::stmt_t new_node =
      HIR::GenericDef(node->original, args, explicit_rettype, rettype, traits,
                      instantiated, current_scope_);
    HIR::resolved_t ref = HIR::GenericRef(new_node);

    // store node
    if (sema_err_.str().size() == starting_err_length) {
      if (!store_symbol(name, ref)) {
        sema_err_ << "Error: symbol " << name
                  << " was already defined" << std::endl;
      }
    }

    return new_node;
  }

  antlrcpp::Any visitTemplateDef(AST::TemplateDef_t node) override {
    size_t starting_err_length = sema_err_.str().size();
    // evaluate template parameters in a new scope
    push_scope();
    std::vector<HIR::declaration_t> templates;
    for (AST::declaration_t t: node->templates) {
      HIR::declaration_t d = visit(t);
      d->traits = all_traits;
      d->mode = HIR::compmode_t::kComptime;
      if (d->type == nullptr) {
        d->type = HIR::Kind(nullptr);
      }
      templates.push_back(d);
    }
    pop_scope();

    // construct node
    std::vector<HIR::stmt_t> instantiated;
    std::string name;
    AST::stmt_t original = reinterpret_cast<AST::stmt_t>(node->original);
    if (original->stmt_kind == AST::stmt_::StmtKind::kFunctionDef) {
      name = dynamic_cast<AST::FunctionDef_t>(original)->name;
    } else {
      name = dynamic_cast<AST::DataDef_t>(original)->name;
    }
    HIR::stmt_t new_node = HIR::TemplateDef(original, templates, instantiated,
                                            current_scope_);
    HIR::resolved_t ref = HIR::TemplateRef(new_node);

    // store node
    if (sema_err_.str().size() == starting_err_length) {
      if (!store_symbol(name, ref)) {
        sema_err_ << "Error: symbol " << name
                  << " was already defined" << std::endl;
      }
    }

    return new_node;
  }

  antlrcpp::Any visitDataDef(AST::DataDef_t node) override {
    size_t starting_err_length = sema_err_.str().size();
    // create shell now if we ever have recursive type definitions
    std::vector<HIR::declaration_t> templates;
    std::vector<HIR::declaration_t> body;
    HIR::expr_t single = nullptr;
    HIR::stmt_t new_node = HIR::DataDef(node->name, templates, body, single,
                                        0);
    HIR::DataDef_t dd = dynamic_cast<HIR::DataDef_t>(new_node);
    HIR::resolved_t ref = HIR::DataRef(new_node);
    // store name in current scope
    if (!store_symbol(node->name, ref)) {
      sema_err_ << "Error: symbol " << node->name
                << " was already defined" << std::endl;
    }
    // evaluate template parameters in new scope
    push_scope();
    size_t scope = current_scope_;
    for (AST::declaration_t t: node->templates) {
      if (t->value != nullptr) {
        templates.push_back(visit(t));
      }
    }
    // evaluate body
    size_t offset = 0;
    for (AST::declaration_t b: node->body) {
      HIR::declaration_t d = visit(b);
      d->offset = offset++;
      if (d->type == nullptr) {
        sema_err_ << "Error: unable to determine type for " << node->name
                  << "." << d->name << std::endl;
      }
      body.push_back(d);
    }
    // evaluate single expression
    if (node->single) {
      single = unoverload(visit(node->single));
      if (!is_kind_type(single->type)) {
        sema_err_ << "Error: cannot assign " << node->name << " to a value"
                  << std::endl;
      }
    }
    pop_scope();
    // put everything together
    dd->templates = templates;
    dd->body = body;
    dd->single = single;
    dd->scope = scope;
    if (sema_err_.str().size() != starting_err_length) {
      // remove from scope if an error had occurred
      remove_symbol_ref(node->name, ref);
    } else if (node->single) {
      // replace the reference if this was a rename
      remove_symbol_ref(node->name, ref);
      store_symbol(node->name, SemaTypeRef(single->type));
    }
    return new_node;
  }

  antlrcpp::Any visitReturn(AST::Return_t node) override {
    HIR::expr_t e = nullptr;
    if (node->value) {
      e = visit(node->value);
    }
    if (retinfo_stack_.empty()) {
      sema_err_ << "Error: return statement is not in function body"
                << std::endl;
    } else {
      HIR::datatype_t dt = e ? e->type : HIR::Void();
      Traits t = e ? e->traits : empty_traits;
      retinfo_stack_.top().emplace_back(dt, t);
    }
    return HIR::Return(e, get_comptime_literal(e));
  }

  antlrcpp::Any visitIf(AST::If_t node) override {
    HIR::expr_t test = visit(node->test);
    if (!is_boolean_type(test->type)) {
      sema_err_ << "Error: conditional must be a boolean, not "
                << to_string(test->type) << std::endl;
    }
    std::vector<HIR::stmt_t> body;
    push_scope();
    for (auto b: node->body) {
      body.push_back(visit(b));
    }
    pop_scope();
    std::vector<HIR::stmt_t> orelse;
    push_scope();
    for (auto o: node->orelse) {
      orelse.push_back(visit(o));
    }
    pop_scope();
    return HIR::If(test, body, orelse);
  }

  antlrcpp::Any visitWhile(AST::While_t node) override {
    HIR::expr_t test = visit(node->test);
    if (!is_boolean_type(test->type)) {
      sema_err_ << "Error: conditional must be a boolean, not "
                << to_string(test->type) << std::endl;
    }
    std::vector<HIR::stmt_t> body;
    push_scope();
    for (auto b: node->body) {
      body.push_back(visit(b));
    }
    pop_scope();
    return HIR::While(test, body);
  }

  antlrcpp::Any visitImport(AST::Import_t node) override {
    nyi("Import");
    return 0;
  }

  antlrcpp::Any visitImportFrom(AST::ImportFrom_t node) override {
    nyi("ImportFrom");
    return 0;
  }

  antlrcpp::Any visitDecl(AST::Decl_t node) override {
    HIR::decltype_t dt = visit(node->dt);
    std::vector<HIR::declaration_t> decls;
    for (AST::declaration_t p: node->decls) {
      HIR::declaration_t d = visit(p);
      d->dt = dt;
      // mutablility removes all traits
      if (dt == HIR::decltype_t::kVar) {
        d->traits = empty_traits;
        d->mode = HIR::compmode_t::kNormal;
      }
      if (d->type == nullptr) {
        sema_err_ << "Error: unable to determine type for " << d->name
                  << std::endl;
        remove_symbol(d->name);
      }
      decls.push_back(d);
    }
    return HIR::Decl(dt, decls);
  }

  antlrcpp::Any visitAssign(AST::Assign_t node) override {
    HIR::expr_t target = visit(node->target);
    HIR::expr_t value = visit(node->value);
    if (is_temporary(target)) {
      sema_err_ << "Error: target of assignment cannot be temporary"
                << std::endl;
    } else if (!is_writeable(target)) {
      sema_err_ << "Error: target of assignment is read only" << std::endl;
    } else if (is_void_type(value->type)) {
      sema_err_ << "Error: type 'void' is not assignable" << std::endl;
    } else if (!is_same_type(target->type, value->type)) {
      sema_err_ << "Error: mismatched types in assignment: "
                << to_string(target->type) << " vs " << to_string(value->type)
                << std::endl;
    }
    return HIR::Assign(target, value);
  }

  antlrcpp::Any visitDel(AST::Del_t node) override {
    std::vector<HIR::expr_t> target;
    for (AST::expr_t e: node->target) {
      target.push_back(visit(e));
    }
    // TODO remove target from scope
    return HIR::Del(target);
  }

  antlrcpp::Any visitExpr(AST::Expr_t node) override {
    return HIR::Expr(visit(node->value));
  }

  antlrcpp::Any visitQuery(AST::Query_t node) override {
    // determine table for query
    HIR::expr_t table = visit(node->table);
    if (!is_dataframe_type(table->type)) {
      sema_err_ << "Error: query must be on Dataframe, not "
                << to_string(table->type) << std::endl;
    }
    HIR::querytype_t qt = visit(node->qt);

    // table's scope is preferred
    preferred_scope_ = table;

    // 'by' gets its own Dataframe
    std::vector<HIR::alias_t> by;
    for (AST::alias_t b: node->by) {
      by.push_back(visit(b));
    }
    HIR::datatype_t by_type = nullptr;
    if (!by.empty()) {
      std::string ts = get_type_string(by);
      std::string by_name = anon_data_name();
      (void) create_datatype(by_name, ts);
      by_type = make_dataframe('!' + by_name);
    }

    // 'cols' change the resulting type
    std::vector<HIR::alias_t> cols;
    for (AST::alias_t c: node->cols) {
      HIR::alias_t col = visit(c);
      bool is_array = is_array_type(col->value->type);
      if (by.empty() && !is_array) {
        sema_err_ << "Error: resulting column must be an array" << std::endl;
      }
      if (!by.empty() && is_array) {
        sema_err_ << "Error: resulting column must be a scalar" << std::endl;
      }
      cols.push_back(col);
    }
    HIR::datatype_t type = table->type;
    if (!cols.empty()) {
      std::string byts = by.empty() ? "" : get_type_string(by) + ", ";
      std::string ts = byts + get_type_string(cols);
      std::string type_name = anon_data_name();
      (void) create_datatype(type_name, ts);
      type = make_dataframe('!' + type_name);
    } else {
      if (!by.empty()) {
        sema_err_ << "Error: must express aggregation if 'by' is listed"
                  << std::endl;
      }
    }

    // 'where' is just a boolean array
    HIR::expr_t where = nullptr;
    if (node->where) {
      where = visit(node->where);
      bool valid = false;
      if (is_array_type(where->type)) {
        HIR::Array_t arr = dynamic_cast<HIR::Array_t>(where->type);
        valid = is_boolean_type(arr->type);
      }
      if (!valid) {
        sema_err_ << "Error: 'where' must be a boolean array; got type "
                  << to_string(where->type) << std::endl;
      }
    }
    preferred_scope_ = nullptr;

    // traits and mode
    std::vector<HIR::expr_t> exprs = {table, where};
    append_exprs(cols, exprs);
    append_exprs(by, exprs);
    Traits traits;
    HIR::compmode_t mode;
    determine_traits_and_mode(all_traits, exprs, traits, mode);

    // put everything together
    return HIR::Query(table, qt, cols, by, where, by_type, type, traits, mode,
                      table->name);
  }

  antlrcpp::Any visitSort(AST::Sort_t node) override {
    // determine table for query
    HIR::expr_t table = visit(node->table);
    if (!is_dataframe_type(table->type)) {
      sema_err_ << "Error: sort must be on Dataframe, not "
                << to_string(table->type) << std::endl;
    }
    HIR::datatype_t type = table->type;

    // table's scope is preferred
    preferred_scope_ = table;
    std::vector<HIR::alias_t> by;
    for (AST::alias_t b: node->by) {
      by.push_back(visit(b));
    }
    preferred_scope_ = nullptr;

    // type of 'by' items is its own Dataframe
    std::string ts = get_type_string(by);
    std::string by_name = anon_data_name();
    (void) create_datatype(by_name, ts);
    HIR::datatype_t by_type = make_dataframe('!' + by_name);

    // traits and mode
    std::vector<HIR::expr_t> exprs = {table};
    append_exprs(by, exprs);
    Traits traits;
    HIR::compmode_t mode;
    determine_traits_and_mode(ra_traits, exprs, traits, mode);

    // put everything together
    return HIR::Sort(table, by, by_type, type, traits, mode, table->name);
  }

  antlrcpp::Any visitJoin(AST::Join_t node) override {
    // determine tables for query
    size_t starting_err_length = sema_err_.str().size();
    HIR::expr_t left = visit(node->left);
    if (left->type != nullptr && !is_dataframe_type(left->type)) {
      sema_err_ << "Error: join for left must be on Dataframe, not "
                << to_string(left->type) << std::endl;
    }
    HIR::expr_t right = visit(node->right);
    if (right->type != nullptr && !is_dataframe_type(right->type)) {
      sema_err_ << "Error: join for right must be on Dataframe, not "
                << to_string(right->type) << std::endl;
    }
    bool bad_dfs = sema_err_.str().size() != starting_err_length;

    // determine 'on' parameters
    std::vector<HIR::alias_t> left_on;
    std::vector<HIR::alias_t> right_on;
    HIR::datatype_t left_on_type = nullptr;
    HIR::datatype_t right_on_type = nullptr;
    if (!bad_dfs && !node->on.empty()) {
      // left's scope is preferred
      preferred_scope_ = left;
      for (AST::alias_t o: node->on) {
        left_on.push_back(visit(o));
      }
      preferred_scope_ = nullptr;

      // right's scope is preferred
      preferred_scope_ = right;
      for (AST::alias_t o: node->on) {
        right_on.push_back(visit(o));
      }
      preferred_scope_ = nullptr;

      // type of 'left_on' items is its own Dataframe
      std::string left_ts = get_type_string(left_on);
      std::string left_name = anon_data_name();
      (void) create_datatype(left_name, left_ts);
      left_on_type = make_dataframe('!' + left_name);

      // type of 'right_on' items is its own Dataframe
      std::string right_ts = get_type_string(right_on);
      std::string right_name = anon_data_name();
      (void) create_datatype(right_name, right_ts);
      right_on_type = make_dataframe('!' + right_name);

      // ensure that the 'on' types are the same
      if (!is_same_type(left_on_type, right_on_type)) {
        sema_err_ << "Error: join 'on' types are not compatible: "
                  << to_string_udt(left_on_type) << " vs "
                  << to_string_udt(right_on_type) << std::endl;
      }
    }

    // determine 'asof' parameters
    HIR::alias_t left_asof = nullptr;
    HIR::alias_t right_asof = nullptr;
    HIR::datatype_t left_asof_type = nullptr;
    HIR::datatype_t right_asof_type = nullptr;
    std::string right_asof_name;
    bool strict = node->strict;
    HIR::direction_t direction = visit(node->direction);
    HIR::expr_t within = nullptr;
    if (node->within != nullptr) {
      within = visit(node->within);
    }
    if (!bad_dfs && node->asof != nullptr) {
      // left's scope is preferred
      preferred_scope_ = left;
      left_asof = visit(node->asof);
      left_asof_type = left_asof->value->type;
      preferred_scope_ = nullptr;

      // right's scope is preferred
      preferred_scope_ = right;
      right_asof = visit(node->asof);
      right_asof_type = right_asof->value->type;
      right_asof_name = right_asof->name.empty() ? right_asof->value->name
                                                 : right_asof->name;
      preferred_scope_ = nullptr;

      // ensure that the 'asof' types are the same
      if (!is_same_type(left_asof_type, right_asof_type)) {
        sema_err_ << "Error: join 'asof' types are not compatible: "
                  << to_string(left_asof_type) << " vs "
                  << to_string(right_asof_type) << std::endl;
      }

      // ensure columns allow subtraction for nearest/within
      if (within != nullptr || direction == HIR::direction_t::kNearest) {
        // find resulting type from subtracting the two 'asof' columns
        // (this logic comes from the function call visitor)
        bool subtractable = false;
        std::vector<HIR::expr_t> args({left_asof->value, right_asof->value});
        HIR::expr_t func = visit(AST::Id(std::string("-")));
        HIR::OverloadedId_t id = dynamic_cast<HIR::OverloadedId_t>(func);
        for (HIR::resolved_t ref: id->refs) {
          HIR::datatype_t func_type = get_type(ref);
          std::string result = match_args(args, func_type);
          if (result.empty()) {
            // check that subtraction's type is the same as within's
            HIR::datatype_t rettype = get_rettype(func_type);
            if (is_array_type(rettype)) {
              subtractable = true;
              if (within != nullptr) {
                HIR::Array_t arr_type = dynamic_cast<HIR::Array_t>(rettype);
                if (!is_same_type(arr_type->type, within->type)) {
                  sema_err_ << "Error: join 'asof' types not compatible "
                            << "with 'within': expected "
                            << to_string(arr_type->type) << ", got "
                            << to_string(within->type) << std::endl;
                }
              }
            }
            break;
          }
        }
        if (!subtractable) {
          sema_err_ << "Error: join 'asof' types prohibit 'within' or "
                    <<"'nearest': " << to_string(left_asof_type) << std::endl;
        }
      }

      // nearest with strict makes no sense
      if (strict && direction == HIR::direction_t::kNearest) {
        sema_err_ << "Error: join 'asof' cannot be both 'nearest' and 'strict'"
                  << std::endl;
      }
    }

    // drop right_on/right_asof from right's table type
    HIR::datatype_t remaining_type = nullptr;
    std::string remaining_ts;
    if (!bad_dfs) {
      remaining_ts = drop_columns(right->type, right_on_type, right_asof_name);
      std::string remaining_name = anon_data_name();
      (void) create_datatype(remaining_name, remaining_ts);
      remaining_type = make_dataframe('!' + remaining_name);
    }

    // combine left's type and right's remaining type
    HIR::datatype_t full_type = nullptr;
    if (!bad_dfs) {
      std::string full_ts = get_type_string(left->type) + ", " + remaining_ts;
      std::string full_name = anon_data_name();
      (void) create_datatype(full_name, full_ts);
      full_type = make_dataframe('!' + full_name);
    }

    // traits and mode
    std::vector<HIR::expr_t> exprs = {left, right, within};
    append_exprs(left_on, exprs);
    append_exprs(right_on, exprs);
    if (left_asof != nullptr) {
      exprs.push_back(left_asof->value);
    }
    if (right_asof != nullptr) {
      exprs.push_back(right_asof->value);
    }
    Traits traits;
    HIR::compmode_t mode;
    determine_traits_and_mode(all_traits, exprs, traits, mode);

    // put everything together
    return HIR::Join(left, right, left_on, right_on, left_on_type,
                     right_on_type, left_asof, right_asof, strict, direction,
                     within, remaining_type, full_type, traits, mode,
                     left->name + right->name);
  }

  antlrcpp::Any visitUnaryOp(AST::UnaryOp_t node) override {
    // operator expressions are just syntactic sugar for function calls
    AST::expr_t desugar = AST::FunctionCall(AST::Id(node->op),
                                            {node->operand});
    HIR::expr_t result = visit(desugar);
    HIR::FunctionCall_t func_call = dynamic_cast<HIR::FunctionCall_t>(result);

    // repack results into sugared form
    HIR::resolved_t ref = nullptr;
    if (func_call->func->expr_kind == HIR::expr_::ExprKind::kId) {
      HIR::Id_t id = dynamic_cast<HIR::Id_t>(func_call->func);
      ref = id->ref;
    }
    HIR::expr_t operand = func_call->args[0];
    return HIR::UnaryOp(node->op, operand, ref, func_call->type,
                        func_call->traits, func_call->mode, func_call->name);
  }

  antlrcpp::Any visitBinOp(AST::BinOp_t node) override {
    // operator expressions are just syntactic sugar for function calls
    AST::expr_t desugar = AST::FunctionCall(AST::Id(node->op),
                                            {node->left, node->right});
    HIR::expr_t result = visit(desugar);
    HIR::FunctionCall_t func_call = dynamic_cast<HIR::FunctionCall_t>(result);

    // repack results into sugared form
    HIR::resolved_t ref = nullptr;
    if (func_call->func->expr_kind == HIR::expr_::ExprKind::kId) {
      HIR::Id_t id = dynamic_cast<HIR::Id_t>(func_call->func);
      ref = id->ref;
    }
    HIR::expr_t left = func_call->args[0];
    HIR::expr_t right = func_call->args[1];
    return HIR::BinOp(left, node->op, right, ref, func_call->type,
                      func_call->traits, func_call->mode, func_call->name);
  }

  antlrcpp::Any visitFunctionCall(AST::FunctionCall_t node) override {
    size_t starting_err_length = sema_err_.str().size();
    // get function and arguments
    HIR::expr_t func = visit(node->func);
    if (!is_callable(func->type)) {
      sema_err_ << "Error: type " << to_string(func->type)
                << " is not callable" << std::endl;
    }
    std::vector<HIR::expr_t> args;
    for (AST::expr_t e: node->args) {
      args.push_back(visit(e));
    }
    // verify arguments
    if (is_overloaded(func)) {
      std::string err_msg = choose_overloaded(func, args);
      if (!err_msg.empty()) {
        sema_err_ << "Error: " << err_msg << std::endl;
      }
    } else {
      std::string err_msg = match_args(args, func->type);
      if (!err_msg.empty()) {
        sema_err_ << "Error: " << err_msg << std::endl;
      }
    }

    // instantiate generic function
    HIR::Id_t id = construct_id(func);
    if (sema_err_.str().size() == starting_err_length) {
      if (id != nullptr && is_generic(id->ref)) {
        // search to see if this already exists
        std::string instantiated_name = id->s + to_string_generics(args);
        Scope::Resolveds resolveds = find_symbol(instantiated_name);
        // build it
        if (resolveds.empty()) {
          // extract definition
          HIR::GenericRef_t ref = dynamic_cast<HIR::GenericRef_t>(id->ref);
          HIR::GenericDef_t def = dynamic_cast<HIR::GenericDef_t>(ref->ref);
          AST::FunctionDef_t original =
            reinterpret_cast<AST::FunctionDef_t>(def->original);
          // use the scope of where the generic was defined to handle globals
          size_t saved_scope = current_scope_;
          current_scope_ = def->scope;
          // create anonymous types for each function arg from caller's arg
          for (size_t i = 0; i < args.size(); i++) {
            std::string name = anon_data_name();
            store_symbol(name, HIR::SemaTypeRef(HIR::Kind(args[i]->type)));
            original->args[i]->explicit_type = AST::Id(name);
          }
          original->name = instantiated_name;
          HIR::stmt_t new_def = visit(original);
          // save the newly created function
          def->instantiated.push_back(new_def);
          resolveds = find_symbol(instantiated_name);
          current_scope_ = saved_scope;
        }
        //get info
        HIR::resolved_t ptr = resolveds.empty() ? nullptr : resolveds[0];
        HIR::datatype_t type = get_type(ptr);
        Traits traits = get_traits(ptr);
        HIR::compmode_t mode = get_mode(ptr);
        func = HIR::Id(instantiated_name, ptr, type, traits, mode, id->name);
        id = dynamic_cast<HIR::Id_t>(func);
      }
    }

    // analyze inline function
    std::vector<HIR::stmt_t> inline_stmts;
    if (sema_err_.str().size() == starting_err_length) {
      // check to see if this function is to be inlined
      if (id != nullptr &&
          id->ref->resolved_kind == HIR::resolved_::ResolvedKind::kFuncRef) {
        HIR::FuncRef_t ref = dynamic_cast<HIR::FuncRef_t>(id->ref);
        HIR::FunctionDef_t cur_def =
          dynamic_cast<HIR::FunctionDef_t>(ref->ref);
        if (cur_def->force_inline) {
          // recreate func def to duplicate arguments
          size_t saved_scope = current_scope_;
          current_scope_ = cur_def->scope;
          pop_scope();
          AST::FunctionDef_t original =
            reinterpret_cast<AST::FunctionDef_t>(cur_def->original);
          original->name = anon_func_name();
          HIR::stmt_t s = visit(original);
          HIR::FunctionDef_t new_def = dynamic_cast<HIR::FunctionDef_t>(s);
          current_scope_ = saved_scope;
          // fill each argument with caller's value
          for (size_t i = 0; i < args.size(); i++) {
            new_def->args[i]->value = args[i];
            new_def->args[i]->comptime_literal = get_comptime_literal(args[i]);
          }
          // put this together
          inline_stmts.push_back(HIR::Expr(new_def->single));
        }
      }
    }

    // try to invoke function now internally
    HIR::expr_t attempt = attempt_sema_function(func, args);
    if (attempt != nullptr) {
      return attempt;
    }
    // traits and mode
    Traits traits;
    HIR::compmode_t mode;
    determine_traits_and_mode(func->traits, args, traits, mode);
    // wrap everything together
    HIR::datatype_t rettype = get_rettype(func->type);
    std::string name = !args.empty() ? args[0]->name : func->name;
    return HIR::FunctionCall(func, args, inline_stmts, rettype, traits, mode,
                             name);
  }

  antlrcpp::Any visitTemplateInst(AST::TemplateInst_t node) override {
    // TODO for now value must be "load"; allow anything in future
    if (node->value->expr_kind != AST::expr_::ExprKind::kId) {
      nyi("TemplateInst on non-Id");
    }
    AST::Id_t ptr = dynamic_cast<AST::Id_t>(node->value);
    if (ptr->s != "load") {
      nyi("TemplateInst on non-load");
    }
    // "load" is always streaming IO
    Traits traits = io_traits | Traits(SingleTrait::kAutostream);
    HIR::compmode_t mode = HIR::compmode_t::kStream;
    HIR::expr_t value = HIR::Id("load", nullptr, nullptr, traits,
                                HIR::compmode_t::kNormal, "load");
    std::vector<HIR::expr_t> args;
    for (AST::expr_t e: node->args) {
      args.push_back(visit(e));
    }
    // statically evaluate arguments
    std::vector<HIR::stmt_t> resolutions;
    std::string type_name;
    for (HIR::expr_t e: args) {
      // ensure arg's type is String and then evaluate via VVM
      // TODO this needs proper CFTE because variables must be set already
      // (ie., need '$let' to force compiler to pre-set variables)
      if (is_string_type(e->type)) {
        HIR::mod_t mod = HIR::Module({HIR::Expr(e)}, "");
        VVM::Program program = codegen(mod, VVM::Mode::kComptime, true, false);
        std::string filename = VVM::interpret(program, VVM::Mode::kComptime);
        filename = filename.substr(1, filename.size() - 2);  // chop quotes
        std::string typestr = VVM::infer_table_from_file(filename);
        type_name = "Provider$" + filename;
        HIR::stmt_t datatype = create_datatype(type_name, typestr);
        resolutions.push_back(datatype);
      } else {
        sema_err_ << "Error: 'load' expects a String parameter" << std::endl;
      }
    }
    HIR::datatype_t rettype = make_dataframe('!' + type_name);
    return HIR::TemplateInst(value, args, resolutions, rettype, io_traits,
                             mode, value->name);
  }

  antlrcpp::Any visitMember(AST::Member_t node) override {
    HIR::expr_t value = visit(node->value);
    size_t scope = get_scope(value->type);
    if (scope == 0) {
      sema_err_ << "Error: value does not have members" << std::endl;
    }
    Scope::Resolveds resolveds = find_symbol_in_scope(node->member, scope);
    if (scope != 0 && resolveds.size() == 0) {
      sema_err_ << "Error: " << node->member
                << " is not a member" << std::endl;
    }
    HIR::resolved_t ref = (resolveds.size() == 1) ? resolveds[0] : nullptr;
    HIR::datatype_t type = get_type(ref);
    if (ref != nullptr && type == nullptr) {
      sema_err_ << "Error: unable to resolve type" << std::endl;
    }
    return HIR::Member(value, node->member, ref, type, value->traits,
                       value->mode, node->member);
  }

  antlrcpp::Any visitSubscript(AST::Subscript_t node) override {
    HIR::expr_t value = visit(node->value);
    if (!is_array_type(value->type)) {
      sema_err_ << "Error: value must be an array; got type "
                << to_string(value->type) << std::endl;
    }
    HIR::slice_t slice = visit(node->slice);
    // get type, traits, and mode for either full slice or standalone index
    HIR::datatype_t type;
    Traits traits;
    HIR::compmode_t mode;
    if (is_slice(slice)) {
      type = value->type;
      HIR::Slice_t s = dynamic_cast<HIR::Slice_t>(slice);
      determine_traits_and_mode(ra_traits,
                                {value, s->lower, s->upper, s->step},
                                traits, mode);
    } else {
      type = get_underlying_type(value->type);
      HIR::Index_t idx = dynamic_cast<HIR::Index_t>(slice);
      determine_traits_and_mode(ra_traits, {value, idx->value}, traits, mode);
    }
    return HIR::Subscript(value, slice, type, traits, mode, value->name);
  }

  antlrcpp::Any visitUserDefinedLiteral(AST::UserDefinedLiteral_t node)
    override {
    // user-defined literals are just syntactic sugar for funcion calls
    AST::expr_t desugar = AST::FunctionCall(AST::Id("suffix" + node->suffix),
                                            {node->literal});

    HIR::expr_t result = visit(desugar);
    HIR::FunctionCall_t func_call = dynamic_cast<HIR::FunctionCall_t>(result);

    // repack results into sugared form
    HIR::resolved_t ref = nullptr;
    if (func_call->func->expr_kind == HIR::expr_::ExprKind::kId) {
      HIR::Id_t id = dynamic_cast<HIR::Id_t>(func_call->func);
      ref = id->ref;
    }
    HIR::expr_t literal = func_call->args[0];
    return HIR::UserDefinedLiteral(literal, node->suffix, ref, func_call->type,
                                   func_call->traits, func_call->mode,
                                   func_call->name);
  }

  antlrcpp::Any visitIntegerLiteral(AST::IntegerLiteral_t node) override {
    return HIR::IntegerLiteral(node->n,
                               HIR::VVMType(size_t(VVM::vvm_types::i64s)),
                               all_traits, HIR::compmode_t::kComptime, "");
  }

  antlrcpp::Any visitFloatingLiteral(AST::FloatingLiteral_t node) override {
    return HIR::FloatingLiteral(node->n,
                                HIR::VVMType(size_t(VVM::vvm_types::f64s)),
                                all_traits, HIR::compmode_t::kComptime, "");
  }

  antlrcpp::Any visitBoolLiteral(AST::BoolLiteral_t node) override {
    return HIR::BoolLiteral(node->b,
                            HIR::VVMType(size_t(VVM::vvm_types::b8s)),
                            all_traits, HIR::compmode_t::kComptime, "");
  }

  antlrcpp::Any visitStr(AST::Str_t node) override {
    return HIR::Str(node->s, HIR::VVMType(size_t(VVM::vvm_types::Ss)),
                    all_traits, HIR::compmode_t::kComptime, "");
  }

  antlrcpp::Any visitChar(AST::Char_t node) override {
    return HIR::Char(node->c, HIR::VVMType(size_t(VVM::vvm_types::c8s)),
                     all_traits, HIR::compmode_t::kComptime, "");
  }

  antlrcpp::Any visitId(AST::Id_t node) override {
    // Dataframes need up-front attention
    if (node->s[0] == '!') {
      (void) make_dataframe(node->s);
    }
    // look for symbol
    bool in_preferred;
    Scope::Resolveds resolveds = find_symbol(node->s, &in_preferred);
    if (resolveds.empty()) {
      // check if this was a Dataframe over a template
      if (node->s[0] == '!') {
        std::string underlying_name = node->s.substr(1);
        Scope::Resolveds try_again = find_symbol(underlying_name);
        if (!try_again.empty() && is_template(try_again[0])) {
          resolveds = std::move(try_again);
        }
      }
      if (resolveds.empty()) {
        sema_err_ << "Error: symbol " << node->s
                  << " was not found" << std::endl;
      }
    }
    // get info, which will be a placeholder for overloaded IDs
    HIR::resolved_t ptr = resolveds.empty() ? nullptr : resolveds[0];
    HIR::datatype_t type = get_type(ptr);
    Traits traits = get_traits(ptr);
    HIR::compmode_t mode = get_mode(ptr);
    // determine how to present the ID
    if (resolveds.size() <= 1) {
      if (in_preferred) {
        return HIR::ImpliedMember(node->s, ptr, preferred_scope_, type,
                                  traits, mode, node->s);
      }
      return HIR::Id(node->s, ptr, type, traits, mode, node->s);
    }
    return HIR::OverloadedId(node->s, resolveds, type, traits, mode, node->s);
  }

  antlrcpp::Any visitTemplatedId(AST::TemplatedId_t node) override {
    size_t starting_err_length = sema_err_.str().size();
    // get ID
    HIR::expr_t id_expr = visit(node->id);
    if (id_expr->expr_kind != HIR::expr_::ExprKind::kId) {
      // templates are not overloadable, plus parser mandates an Id
      nyi("TemplatedId on non-Id");
    }
    HIR::Id_t id = dynamic_cast<HIR::Id_t>(id_expr);
    if (!is_template(id->ref)) {
      sema_err_ << "Error: type " << to_string(id->type)
                << " is not a template" << std::endl;
    }
    // get template parameters and extract non-types as literals
    std::vector<HIR::expr_t> templates;
    for (AST::expr_t t: node->templates) {
      templates.push_back(unoverload(visit(t)));
    }
    std::vector<HIR::expr_t> literals;
    for (size_t i = 0; i < templates.size(); i++) {
      HIR::expr_t lit = templates[i];
      if (!is_kind_type(lit->type)) {
        lit = get_comptime_literal(templates[i]);
        if (lit == nullptr) {
          sema_err_ << "Error: template parameter at position " << i
                    << " must be a comptime literal" << std::endl;
        }
      }
      literals.push_back(lit);
    }
    // verify parameters
    std::string err_msg = match_args(templates, id->type);
    if (!err_msg.empty()) {
      sema_err_ << "Error: " << err_msg << std::endl;
    }
    bool dataframe = false;
    std::string instantiated_name;
    Scope::Resolveds resolveds;
    // instantiate the template if no errors occurred so far
    if (sema_err_.str().size() == starting_err_length) {
      // search to see if this already exists
      instantiated_name = id->s + to_string_templates(literals);
      if (instantiated_name[0] == '!') {
        instantiated_name = instantiated_name.substr(1);
        dataframe = true;
      }
      resolveds = find_symbol(instantiated_name);
      // build it
      if (resolveds.empty()) {
        // extract definition
        HIR::TemplateRef_t ref = dynamic_cast<HIR::TemplateRef_t>(id->ref);
        HIR::TemplateDef_t template_def =
          dynamic_cast<HIR::TemplateDef_t>(ref->ref);
        AST::stmt_t original =
          reinterpret_cast<AST::stmt_t>(template_def->original);
        // use the scope of where the template was defined to handle globals
        size_t saved_scope = current_scope_;
        current_scope_ = template_def->scope;
        push_scope();
        // fill the AST templates; this will go through the visitor
        std::vector<AST::declaration_t> ast_templates(literals.size());
        for (size_t i = 0; i < literals.size(); i++) {
          std::string template_name = template_def->templates[i]->name;
          ast_templates[i] = AST::declaration(template_name, nullptr, nullptr);
          if (is_kind_type(literals[i]->type)) {
            // make the template name a type
            store_symbol(template_name, HIR::SemaTypeRef(literals[i]->type));
          } else {
            ast_templates[i]->value = downgrade(literals[i]);
          }
        }
        // run appropriate visitor
        if (original->stmt_kind == AST::stmt_::StmtKind::kFunctionDef) {
          AST::FunctionDef_t func_def =
            dynamic_cast<AST::FunctionDef_t>(original);
          func_def->name = instantiated_name;
          func_def->templates = ast_templates;
        } else {
          AST::DataDef_t data_def = dynamic_cast<AST::DataDef_t>(original);
          data_def->name = instantiated_name;
          data_def->templates = ast_templates;
        }
        HIR::stmt_t new_def = visit(original);
        // save the newly created item in the original scope
        template_def->instantiated.push_back(new_def);
        resolveds = find_symbol(instantiated_name);
        pop_scope();
        // re-store in template's scope
        store_symbol(instantiated_name, resolveds[0]);
        current_scope_ = saved_scope;
      }
    }
    // get info
    HIR::resolved_t ptr = resolveds.empty() ? nullptr : resolveds[0];
    if (dataframe) {
      instantiated_name = '!' + instantiated_name;
      HIR::datatype_t dt = make_dataframe(instantiated_name);
      ptr = dynamic_cast<HIR::UDT_t>(dt)->ref;
    }
    HIR::datatype_t type = get_type(ptr);
    Traits traits = get_traits(ptr);
    HIR::compmode_t mode = get_mode(ptr);
    // put it all together
    return HIR::TemplatedId(id, templates, ptr, type, traits, mode,
                            instantiated_name);
  }

  antlrcpp::Any visitList(AST::List_t node) override {
    std::vector<HIR::expr_t> values;
    for (AST::expr_t v: node->values) {
      HIR::expr_t e = visit(v);
      values.push_back(e);
    }
    // check that all types are the same
    HIR::datatype_t expected = !values.empty() ? values[0]->type : nullptr;
    for (HIR::expr_t e: values) {
      if (!is_same_type(e->type, expected)) {
        sema_err_ << "Error: mismtach in list: " << to_string(e->type)
                  << " vs " << to_string(expected) << std::endl;
      }
    }
    std::string name = !values.empty() ? values[0]->name : "";
    HIR::datatype_t type = nullptr;
    // a list of kinds means we have a kind of array
    if (is_kind_type(expected)) {
      type = HIR::Kind(HIR::Array(get_underlying_type(expected)));
      name = '[' + name + ']';
      if (values.size() >= 2) {
        sema_err_ << "Error: only one type allowed for lists" << std::endl;
      }
    } else {
      type = HIR::Array(expected);
    }
    Traits traits = intersect_traits(values);
    HIR::compmode_t mode = compound_mode(values);
    return HIR::List(values, type, traits, mode, name);
  }

  antlrcpp::Any visitParen(AST::Paren_t node) override {
    HIR::expr_t subexpr = visit(node->subexpr);
    return HIR::Paren(subexpr, subexpr->type, subexpr->traits, subexpr->mode,
                      subexpr->name);
  }

  antlrcpp::Any visitAnonData(AST::AnonData_t node) override {
    // create an anonymous data def
    std::string name = anon_data_name();
    std::vector<AST::declaration_t> templates;
    AST::expr_t single = nullptr;
    AST::stmt_t named_def = AST::DataDef(name, templates, node->body, single);
    HIR::stmt_t s = visit(named_def);
    HIR::DataDef_t new_def = dynamic_cast<HIR::DataDef_t>(s);

    // construct type of expr
    Scope::Resolveds resolveds = find_symbol(name);
    HIR::resolved_t ref = resolveds.empty() ? nullptr : resolveds[0];
    HIR::datatype_t type = HIR::Kind(HIR::UDT(name, ref));

    // put it all together
    return HIR::AnonData(new_def->body, new_def->scope, type, all_traits,
                         HIR::compmode_t::kComptime, name);
  }

  antlrcpp::Any visitSlice(AST::Slice_t node) override {
    HIR::expr_t lower = nullptr;
    if (node->lower) {
      lower = visit(node->lower);
    }
    if (lower != nullptr && !is_indexable_type(lower->type)) {
      sema_err_ << "Error: lower bound type " << to_string(lower->type)
                << " cannot be used as an index" << std::endl;
    }
    HIR::expr_t upper = nullptr;
    if (node->upper) {
      upper = visit(node->upper);
    }
    if (upper != nullptr && !is_indexable_type(upper->type)) {
      sema_err_ << "Error: upper bound type " << to_string(upper->type)
                << " cannot be used as an index" << std::endl;
    }
    HIR::expr_t step = nullptr;
    if (node->step) {
      step = visit(node->step);
    }
    if (step != nullptr && !is_indexable_type(step->type)) {
      sema_err_ << "Error: step type " << to_string(step->type)
                << " cannot be used as an index" << std::endl;
    }
    return HIR::Slice(lower, upper, step);
  }

  antlrcpp::Any visitIndex(AST::Index_t node) override {
    HIR::expr_t value = visit(node->value);
    if (!is_indexable_type(value->type)) {
      sema_err_ << "Error: type " << to_string(value->type)
                << " cannot be used as an index" << std::endl;
    }
    return HIR::Index(value);
  }

  antlrcpp::Any visitAlias(AST::alias_t node) override {
    HIR::expr_t value = visit(node->value);
    return HIR::alias(value, node->name);
  }

  antlrcpp::Any visitDeclaration(AST::declaration_t node) override {
    size_t starting_err_length = sema_err_.str().size();
    // get explcit type
    HIR::expr_t explicit_type = nullptr;
    if (node->explicit_type) {
      explicit_type = unoverload(visit(node->explicit_type));
    }
    HIR::datatype_t type = nullptr;
    if (explicit_type != nullptr) {
      if (is_kind_type(explicit_type->type)) {
        type = get_underlying_type(explicit_type->type);
      } else {
        sema_err_ << "Error: declaration for " << node->name
                  << " has invalid type" << std::endl;
      }
    }
    // get value
    HIR::expr_t value = nullptr;
    if (node->value) {
      value = visit(node->value);
    }
    if (type == nullptr && value != nullptr) {
      type = value->type;
    }
    if (value != nullptr && !is_same_type(type, value->type)) {
      sema_err_ << "Error: type of declaration does not match: "
                << to_string(type) << " vs " << to_string(value->type)
                << std::endl;
    }
    if (is_void_type(type)) {
      sema_err_ << "Error: symbol cannot have a 'void' type" << std::endl;
    }
    // traits and mode
    Traits traits = empty_traits;
    HIR::compmode_t mode = HIR::compmode_t::kNormal;
    if (value != nullptr) {
      traits = value->traits;
      mode = value->mode;
    }
    // construct reference if no errors occurred so far
    HIR::expr_t comptime_literal = get_comptime_literal(value);
    HIR::declaration_t new_node =
      HIR::declaration(node->name, explicit_type, value, HIR::decltype_t::kLet,
                       type, traits, mode, comptime_literal, 0);
    if (sema_err_.str().size() == starting_err_length) {
      if (!store_symbol(node->name, HIR::DeclRef(new_node))) {
        sema_err_ << "Error: symbol " << node->name
                  << " was already defined" << std::endl;
      }
    }
    return new_node;
  }

  antlrcpp::Any visitDecltype(AST::decltype_t value) override {
    return HIR::decltype_t(uint8_t(value));
  }

  antlrcpp::Any visitQuerytype(AST::querytype_t value) override {
    return HIR::querytype_t(uint8_t(value));
  }

  antlrcpp::Any visitDirection(AST::direction_t value) override {
    return HIR::direction_t(uint8_t(value));
  }

 public:

  SemaVisitor() {
    // start with a single global scope
    current_scope_ = 0;
    preferred_scope_ = nullptr;
    push_scope();

    // save all builtins to global scope
    save_builtins();
  }

  std::string get_errors() const {
    return sema_err_.str();
  }

  void set_interactive(bool b) {
    interactive_ = b;
  }
} sema_visitor;


// semantic analysis converts AST into HIR
HIR::mod_t sema(AST::mod_t ast, bool interactive, bool dump_hir) {
  // build high-level IR
  sema_visitor.set_interactive(interactive);
  HIR::mod_t hir = sema_visitor.visit(ast);
  std::string msg = sema_visitor.get_errors();
  if (!msg.empty()) {
    throw std::logic_error(msg);
  }

  // print high-level IR
  if (dump_hir) {
    std::cout << HIR::to_string(hir) << std::endl;
  }

  return hir;
}

