/*
 * Code Generation -- produce VVM bytecode
 *
 * Copyright (C) 2019--2020 Empirical Software Solutions, LLC
 *
 * This program is distributed under the terms of the GNU Affero General
 * Public License with the Commons Clause.
 *
 */

#include <cctype>
#include <vector>
#include <unordered_map>

#include <empirical.hpp>
#include <traits.hpp>

// build VVM bytecode from high-level IR (HIR)
class CodegenVisitor : public HIR::BaseVisitor {
  /* types */

  // convert Empirical's type to VVM's type string
  std::string get_vvm_type(HIR::datatype_t node, char append = 's') {
    switch (node->datatype_kind) {
      case HIR::datatype_::DatatypeKind::kVVMType: {
        HIR::VVMType_t b = dynamic_cast<HIR::VVMType_t>(node);
        std::string type = VVM::type_strings[b->t];
        type.pop_back();
        return type + append;
      }
      case HIR::datatype_::DatatypeKind::kUDT: {
        HIR::UDT_t udt = dynamic_cast<HIR::UDT_t>(node);
        HIR::DataDef_t dd = dynamic_cast<HIR::DataDef_t>(udt->def);
        VVM::type_t typee = type_map_get(dd);
        return VVM::decode_type(typee);
      }
      case HIR::datatype_::DatatypeKind::kArray: {
        HIR::Array_t arr = dynamic_cast<HIR::Array_t>(node);
        return get_vvm_type(arr->type, 'v');
      }
      case HIR::datatype_::DatatypeKind::kFuncType: {
        return "<func>";
      }
      case HIR::datatype_::DatatypeKind::kTemplateType: {
        return "<template>";
      }
      case HIR::datatype_::DatatypeKind::kPlaceholder: {
        return "<placeholder>";
      }
      case HIR::datatype_::DatatypeKind::kKind: {
        return "<type>";
      }
      case HIR::datatype_::DatatypeKind::kVoid: {
        return "<void>";
      }
    }
  }

  // can this type be shown to VVM
  bool is_type_vvm_capable(HIR::datatype_t node) {
    if (node == nullptr) {
      return false;
    }

    switch (node->datatype_kind) {
      case HIR::datatype_::DatatypeKind::kFuncType:
      case HIR::datatype_::DatatypeKind::kTemplateType:
      case HIR::datatype_::DatatypeKind::kPlaceholder:
      case HIR::datatype_::DatatypeKind::kKind:
      case HIR::datatype_::DatatypeKind::kVoid:
        return false;
      default:
        return true;
    }
  }

  // convert Empirical's type to VVM's type code
  VVM::type_t get_type_code(HIR::datatype_t node) {
    if (is_type_vvm_capable(node)) {
      return VVM::encode_type(get_vvm_type(node));
    }
    return VVM::type_t(0);
  }

  // convert Empirical's type to a VVM operand via type code
  VVM::operand_t get_type_operand(HIR::datatype_t node) {
    if (is_type_vvm_capable(node)) {
      return VVM::encode_operand(get_vvm_type(node));
    }
    return VVM::operand_t(0);
  }

  // map a UDT's scope to a VVM type code; cache previously seen types
  std::unordered_map<size_t, VVM::type_t> type_map_;
  std::unordered_map<std::string, VVM::type_t> seen_types_;
  VVM::type_t last_type_ = 0;

  // return type or make it on demand
  VVM::type_t type_map_get(HIR::DataDef_t dd) {
    auto iter = type_map_.find(dd->scope);
    return (iter != type_map_.end()) ? iter->second : VVM::type_t(visit(dd));
  }

  // claim type code; user must map HIR scope if necessary
  VVM::type_t reserve_type(VVM::TypeMask mask = VVM::TypeMask::kUserDefined) {
    VVM::type_t last = last_type_++;
    auto typee = VVM::encode_type(last, mask);
    return typee;
  }

  // number of fields in a user-defined type
  size_t number_of_fields(HIR::datatype_t node) {
    if (node->datatype_kind != HIR::datatype_::DatatypeKind::kUDT) {
      return 0;
    }
    HIR::UDT_t udt = dynamic_cast<HIR::UDT_t>(node);
    HIR::DataDef_t dd = dynamic_cast<HIR::DataDef_t>(udt->def);
    return dd->body.size();
  }

  // return whether the type represents a func
  bool is_func_type(HIR::datatype_t node) {
    return (node != nullptr &&
            node->datatype_kind == HIR::datatype_::DatatypeKind::kFuncType);
  }

  // return whether the type represents a template
  bool is_template_type(HIR::datatype_t node) {
    return (node != nullptr &&
      node->datatype_kind == HIR::datatype_::DatatypeKind::kTemplateType);
  }

  // return whether the type represents a kind
  bool is_kind_type(HIR::datatype_t node) {
    return (node != nullptr &&
            node->datatype_kind == HIR::datatype_::DatatypeKind::kKind);
  }

  // return whether the type represents a void
  bool is_void_type(HIR::datatype_t node) {
    return (node != nullptr &&
            node->datatype_kind == HIR::datatype_::DatatypeKind::kVoid);
  }

  // type definitions
  VVM::defined_types_t types_;

  /* registers */

  // map a HIR node's address to VVM's register bank
  std::unordered_map<HIR::declaration_t, VVM::operand_t> reg_map_;
  std::unordered_map<HIR::FunctionDef_t, VVM::operand_t> func_map_;
  std::unordered_map<HIR::expr_t, VVM::operand_t> implied_reg_map_;
  std::vector<size_t> last_operands_ = {0, 0, 0};

  // return function or make it on demand
  VVM::operand_t func_map_get(HIR::FunctionDef_t fd) {
    auto iter = func_map_.find(fd);
    return (iter != func_map_.end()) ? iter->second
                                     : VVM::operand_t(visit(fd));
  }

  // claim register space; map HIR node if necessary
  VVM::operand_t reserve_space(VVM::OpMask mask = VVM::OpMask::kLocal) {
    size_t idx = last_operands_[size_t(mask)]++;
    auto op = VVM::encode_operand(idx, mask);
    return op;
  }

  // non-immediate constant values are stored in a pool
  VVM::const_pool_t constants_;

  // store a constant in the pool; save unique literals in a cache
  template<class T>
  VVM::operand_t store_constant(T value,
                                std::unordered_map<T, VVM::operand_t>& cache) {
    auto iter = cache.find(value);
    if (iter != cache.end()) {
      return iter->second;
    }

    VVM::operand_t key = reserve_space(VVM::OpMask::kGlobal);
    constants_[key] = VVM::encode_ptr(new T(value));
    cache[value] = key;
    return key;
  }

  // these can't be static to the class because we have multiple VM objects
  std::unordered_map<int64_t, VVM::operand_t> integer_register_cache;
  std::unordered_map<double, VVM::operand_t> floating_register_cache;
  std::unordered_map<std::string, VVM::operand_t> string_register_cache;

  /* basic blocks */

  // instead of using a control-flow graph, define blocks as an integer
  typedef size_t block_t;
  block_t latest_block_ = 0;

  // by using integers, we can treat blocks as simply labels
  VVM::Labeler<> labeler_;

  // return a new block
  block_t new_block() {
    return latest_block_++;
  }

  // indicate that a block is the current location in the bytecode
  void use_block(block_t b) {
    size_t loc = instructions_.size();
    labeler_.set_location(b, loc);
  }

  /* bytecode */

  VVM::instructions_t instructions_;

  template<class T>
  void emit(T opcode, const std::vector<size_t>& ops) {
    instructions_.push_back(size_t(opcode));
    for (auto o: ops) {
      instructions_.push_back(o);
    }
  }

  template<class T>
  void emit_label(T opcode, block_t b) {
    instructions_.push_back(size_t(opcode));
    instructions_.push_back(0);
    size_t loc = instructions_.size() - 1;
    labeler_.append_dep(b, loc);
  }

  template<class T>
  void emit_label(T opcode, size_t op, block_t b) {
    instructions_.push_back(size_t(opcode));
    instructions_.push_back(op);
    instructions_.push_back(0);
    size_t loc = instructions_.size() - 1;
    labeler_.append_dep(b, loc);
  }

  size_t specialize_opcode(const std::string& opcode, HIR::datatype_t node) {
    std::string opstr = opcode + '_' + get_vvm_type(node);
    return VVM::encode_opcode(opstr);
  }

  size_t specialize_opcode(const std::string& opcode, HIR::datatype_t node1,
                           HIR::datatype_t node2) {
    std::string opstr = opcode + '_' + get_vvm_type(node1) + '_' +
                        get_vvm_type(node2);
    return VVM::encode_opcode(opstr);
  }

  /* miscellaneous */

  bool interactive_;
  bool directed_repr_;

  // save string directly; no need to escape or infer an expr's type
  VVM::operand_t direct_repr(const std::string& s) {
    directed_repr_ = true;
    return visit(HIR::Str(s, nullptr, all_traits, HIR::compmode_t::kComptime,
                          ""));
  }

  void nyi(const std::string& rule) const {
    std::string msg = "Not yet implemented: " + rule + '\n';
    throw std::logic_error(msg);
  }

  void invalid(const std::string& rule) const {
    std::string msg = "Should not have reached: " + rule + '\n';
    throw std::logic_error(msg);
  }

 protected:

  /* all visitors return the target operand */

  antlrcpp::Any visitModule(HIR::Module_t node) override {
    // reset our state
    directed_repr_ = false;
    types_.clear();
    constants_.clear();
    instructions_.clear();
    labeler_.clear();
    // iteratively scan statements
    VVM::operand_t last_stmt_value;
    for (HIR::stmt_t s: node->body) {
      VVM::operand_t op = visit(s);
      last_stmt_value = op;
    }
    // if last stmt is a non-void expr, then display its value
    if (interactive_ && !node->body.empty()) {
      if (!directed_repr_) {
        auto last_stmt = node->body.back();
        if (last_stmt->stmt_kind == HIR::stmt_::StmtKind::kExpr) {
          HIR::Expr_t e = dynamic_cast<HIR::Expr_t>(last_stmt);
          std::string name = e->value->name;
          HIR::datatype_t dt = e->value->type;
          VVM::operand_t repr_value;
          if (!is_void_type(dt)) {
            if (is_func_type(dt)) {
              name = std::isalpha(name[0]) ? name : ("(" + name + ")");
              repr_value = direct_repr("<func: " + name + ">");
            } else if (is_template_type(dt)) {
              repr_value = direct_repr("<template: " + name + ">");
            } else if (is_kind_type(dt)) {
              repr_value = direct_repr("<type: " + name + ">");
            } else {
              // invoke repr
              repr_value = reserve_space();
              VVM::operand_t typee = get_type_operand(dt);
              emit(VVM::opcodes::repr, {last_stmt_value, typee, repr_value});
            }
            emit(VVM::opcodes::save, {repr_value});
            last_stmt_value = repr_value;
          }
        }
      } else {
        // the string to save was already generated
        emit(VVM::opcodes::save, {last_stmt_value});
      }
    }
    // finish instructions
    emit(VVM::opcodes::halt, {});
    labeler_.resolve(instructions_);
    return last_stmt_value;
  }

  antlrcpp::Any visitFunctionDef(HIR::FunctionDef_t node) override {
    // inline functions are not generated
    if (node->force_inline) {
      return VVM::operand_t(0);
    }

    // specialized generics are seen twice, so return first instance
    auto iter = func_map_.find(node);
    if (iter != func_map_.end()) {
      return iter->second;
    }

    VVM::FunctionDef* fd = new VVM::FunctionDef;
    fd->name = node->name;
    // attach everything to a global now so body can have recursion
    VVM::operand_t result = reserve_space(VVM::OpMask::kGlobal);
    constants_[result] = VVM::encode_ptr(fd);
    func_map_[node] = result;
    // save frame information
    const size_t local_mask = size_t(VVM::OpMask::kLocal);
    size_t saved_last_operand_local = last_operands_[local_mask];
    last_operands_[local_mask] = 0;
    VVM::instructions_t saved_bytecode = std::move(instructions_);
    VVM::Labeler<> saved_labeler = std::move(labeler_);
    // function arguments get the first set of registers
    for (auto decl: node->args) {
      VVM::operand_t value = reserve_space();
      reg_map_[decl] = value;
      VVM::named_type_t nt;
      nt.typee = get_type_code(decl->type);
      nt.name = decl->name;
      fd->args.push_back(nt);
    }
    fd->rettype = !is_void_type(node->rettype) ? get_type_code(node->rettype)
                                               : VVM::encode_type("i64s");
    // template parameters are next
    for (auto decl: node->templates) {
      if (decl->comptime_literal != nullptr) {
        // template was a value, as opposed to a type
        visit(decl);
      }
    }
    // recursively visit body
    for (auto b: node->body) {
      visit(b);
    }
    emit(VVM::opcodes::halt, {});
    labeler_.resolve(instructions_);
    fd->body = std::move(instructions_);
    // restore frame information
    last_operands_[local_mask] = saved_last_operand_local;
    instructions_ = std::move(saved_bytecode);
    labeler_ = std::move(saved_labeler);
    return result;
  }

  antlrcpp::Any visitGenericDef(HIR::GenericDef_t node) override {
    for (HIR::stmt_t i: node->instantiated) {
      visit(i);
    }
    return VVM::operand_t(0);
  }

  antlrcpp::Any visitMacroDef(HIR::MacroDef_t node) override {
    return visit(node->implied_template);
  }

  antlrcpp::Any visitTemplateDef(HIR::TemplateDef_t node) override {
    for (HIR::stmt_t i: node->instantiated) {
      visit(i);
    }
    return VVM::operand_t(0);
  }

  antlrcpp::Any visitDataDef(HIR::DataDef_t node) override {
    // a rename does not trigger a build
    if (node->body.empty()) {
      return VVM::operand_t(0);
    }
    // build the type
    VVM::type_definition_t type_def;
    for (HIR::declaration_t b: node->body) {
      VVM::named_type_t nt;
      nt.typee = get_type_code(b->type);
      nt.name = b->name;
      type_def.push_back(nt);
    }
    // check if this has already been made
    VVM::type_t typee;
    std::string disassem = VVM::decode_types(type_def);
    auto iter = seen_types_.find(disassem);
    if (iter != seen_types_.end()){
      typee = iter->second;
    } else {
      typee = reserve_type();
      seen_types_[disassem] = typee;
      types_[typee] = type_def;
    }
    // save the type
    type_map_[node->scope] = typee;
    return typee;
  }

  antlrcpp::Any visitReturn(HIR::Return_t node) override {
    VVM::operand_t e = VVM::encode_operand(0, VVM::OpMask::kImmediate);
    if (node->comptime_literal != nullptr) {
      e = visit(node->comptime_literal);
    } else if (node->value != nullptr) {
      e = visit(node->value);
    }
    emit(VVM::opcodes::ret, {e});
    return VVM::operand_t(0);
  }

  antlrcpp::Any visitIf(HIR::If_t node) override {
    if (node->orelse.empty()) {
      auto end = new_block();
      VVM::operand_t cond = visit(node->test);
      emit_label(VVM::opcodes::bfalse, cond, end);
      for (auto b: node->body) {
        visit(b);
      }
      use_block(end);
    } else {
      auto next = new_block();
      auto end = new_block();
      VVM::operand_t cond = visit(node->test);
      emit_label(VVM::opcodes::bfalse, cond, next);
      for (auto b: node->body) {
        visit(b);
      }
      emit_label(VVM::opcodes::br, end);
      use_block(next);
      for (auto o: node->orelse) {
        visit(o);
      }
      use_block(end);
    }
    return VVM::operand_t(0);
  }

  antlrcpp::Any visitWhile(HIR::While_t node) override {
    auto loop = new_block();
    auto end = new_block();
    use_block(loop);
    VVM::operand_t cond = visit(node->test);
    emit_label(VVM::opcodes::bfalse, cond, end);
    for (auto b: node->body) {
      visit(b);
    }
    emit_label(VVM::opcodes::br, loop);
    use_block(end);
    return VVM::operand_t(0);
  }

  antlrcpp::Any visitImport(HIR::Import_t node) override {
    nyi("Import");
    return 0;
  }

  antlrcpp::Any visitImportFrom(HIR::ImportFrom_t node) override {
    nyi("ImportFrom");
    return 0;
  }

  antlrcpp::Any visitDecl(HIR::Decl_t node) override {
    for (auto d: node->decls) {
      visit(d);
    }
    return VVM::operand_t(0);
  }

  antlrcpp::Any visitAssign(HIR::Assign_t node) override {
    VVM::operand_t target = visit(node->target);
    VVM::operand_t value = visit(node->value);
    VVM::operand_t typee = get_type_operand(node->value->type);
    emit(VVM::opcodes::assign, {value, typee, target});
    return target;
  }

  antlrcpp::Any visitDel(HIR::Del_t node) override {
    for (auto t: node->target) {
      VVM::operand_t target = visit(t);
      size_t opcode = specialize_opcode("del", t->type);
      emit(opcode, {target});
    }
    return VVM::operand_t(0);
  }

  antlrcpp::Any visitExpr(HIR::Expr_t node) override {
    return visit(node->value);
  }

  antlrcpp::Any visitQuery(HIR::Query_t node) override {
    VVM::operand_t table = visit(node->table);
    implied_reg_map_[node->table] = table;

    // we might need to shadow the register temporarily
    VVM::operand_t orig_table = table;

    if (node->where) {
      VVM::operand_t where = visit(node->where);
      VVM::operand_t typee = get_type_operand(node->table->type);
      VVM::operand_t result = reserve_space();
      emit(VVM::opcodes::where, {table, where, typee, result});
      table = result;
      implied_reg_map_[node->table] = table;
    }

    VVM::operand_t by_table;
    VVM::operand_t by_typee;
    if (!node->by.empty()) {
      by_table = reserve_space();
      by_typee = get_type_operand(node->by_type);
      emit(VVM::opcodes::alloc, {by_typee, by_table});
      for (size_t i = 0; i < node->by.size(); i++) {
        HIR::expr_t b = node->by[i]->value;
        VVM::operand_t by = visit(b);
        VVM::operand_t offset =
          VVM::encode_operand(i, VVM::OpMask::kImmediate);
        VVM::operand_t dst = reserve_space();
        emit(VVM::opcodes::member, {by_table, offset, dst});
        VVM::operand_t typee = get_type_operand(b->type);
        emit(VVM::opcodes::assign, {by, typee, dst});
      }
    }

    if (!node->cols.empty()) {
      VVM::operand_t result = reserve_space();
      VVM::operand_t typee = get_type_operand(node->type);
      VVM::operand_t counter;
      block_t loop;
      block_t end;
      VVM::opcodes assign_opcode = VVM::opcodes::assign;
      size_t num_leading_cols = 0;
      if (!node->by.empty()) {
        // group the table and begin a loop over the sub tables
        VVM::operand_t orig_type = get_type_operand(node->table->type);
        VVM::operand_t groups = reserve_space();
        VVM::operand_t length = reserve_space();
        emit(VVM::opcodes::group, {orig_type, table, by_typee, by_table,
                                   typee, result, groups, length});
        counter = reserve_space();
        VVM::operand_t i64s = VVM::encode_operand("i64s");
        emit(VVM::opcodes::assign, {0, i64s, counter});
        loop = new_block();
        end = new_block();
        use_block(loop);
        VVM::operand_t cmp_result = reserve_space();
        emit(VVM::opcodes::lt_i64s_i64s, {counter, length, cmp_result});
        emit_label(VVM::opcodes::bfalse, cmp_result, end);
        VVM::operand_t sub_table = reserve_space();
        implied_reg_map_[node->table] = sub_table;
        emit(VVM::opcodes::member, {groups, counter, sub_table});
        assign_opcode = VVM::opcodes::append;
        num_leading_cols = number_of_fields(node->by_type);
      } else {
        emit(VVM::opcodes::alloc, {typee, result});
      }
      for (size_t i = 0; i < node->cols.size(); i++) {
        HIR::expr_t c = node->cols[i]->value;
        VVM::operand_t col = visit(c);
        VVM::operand_t offset = VVM::encode_operand(i + num_leading_cols,
            VVM::OpMask::kImmediate);
        VVM::operand_t dst = reserve_space();
        emit(VVM::opcodes::member, {result, offset, dst});
        VVM::operand_t typee = get_type_operand(c->type);
        emit(assign_opcode, {col, typee, dst});
      }
      if (!node->by.empty()) {
        VVM::operand_t one = VVM::encode_operand(1, VVM::OpMask::kImmediate);
        emit(VVM::opcodes::add_i64s_i64s, {counter, one, counter});
        emit_label(VVM::opcodes::br, loop);
        use_block(end);
      }
      table = result;
    }

    implied_reg_map_[node->table] = orig_table;
    return table;
  }

  antlrcpp::Any visitSort(HIR::Sort_t node) override {
    VVM::operand_t table = visit(node->table);
    implied_reg_map_[node->table] = table;
    VVM::operand_t typee = get_type_operand(node->type);

    // put 'by' in its own table
    VVM::operand_t by_table = reserve_space();
    VVM::operand_t by_typee = get_type_operand(node->by_type);
    emit(VVM::opcodes::alloc, {by_typee, by_table});
    for (size_t i = 0; i < node->by.size(); i++) {
      HIR::expr_t b = node->by[i]->value;
      VVM::operand_t by = visit(b);
      VVM::operand_t offset = VVM::encode_operand(i,
          VVM::OpMask::kImmediate);
      VVM::operand_t dst = reserve_space();
      emit(VVM::opcodes::member, {by_table, offset, dst});
      VVM::operand_t typee = get_type_operand(b->type);
      emit(VVM::opcodes::assign, {by, typee, dst});
    }

    // sort table according to indices from 'by'
    VVM::operand_t indices = reserve_space();
    VVM::operand_t result = reserve_space();
    emit(VVM::opcodes::isort, {by_table, by_typee, indices});
    emit(VVM::opcodes::multidx, {table, indices, typee, result});
    return result;
  }

  antlrcpp::Any visitJoin(HIR::Join_t node) override {
    VVM::operand_t left = visit(node->left);
    implied_reg_map_[node->left] = left;
    VVM::operand_t right = visit(node->right);
    implied_reg_map_[node->right] = right;
    VVM::operand_t left_typee = get_type_operand(node->left->type);
    VVM::operand_t right_typee = get_type_operand(node->right->type);
    VVM::operand_t right_remaining_typee =
      get_type_operand(node->right_remaining_type);
    VVM::operand_t typee = get_type_operand(node->type);

    // get all parameters for matches
    VVM::operand_t left_indices;
    VVM::operand_t right_indices;
    VVM::operand_t left_on_table;
    VVM::operand_t left_on_typee;
    VVM::operand_t right_on_table;
    VVM::operand_t right_on_typee;
    VVM::operand_t left_asof_value;
    VVM::operand_t left_asof_typee;
    VVM::operand_t right_asof_value;
    VVM::operand_t strict;
    VVM::operand_t direction;
    VVM::operand_t within;
    if (!node->left_on.empty()) {
      // put 'left_on' in its own table
      left_on_table = reserve_space();
      left_on_typee = get_type_operand(node->left_on_type);
      emit(VVM::opcodes::alloc, {left_on_typee, left_on_table});
      for (size_t i = 0; i < node->left_on.size(); i++) {
        HIR::expr_t b = node->left_on[i]->value;
        VVM::operand_t left_on = visit(b);
        VVM::operand_t offset = VVM::encode_operand(i,
            VVM::OpMask::kImmediate);
        VVM::operand_t dst = reserve_space();
        emit(VVM::opcodes::member, {left_on_table, offset, dst});
        VVM::operand_t typee = get_type_operand(b->type);
        emit(VVM::opcodes::assign, {left_on, typee, dst});
      }

      // put 'right_on' in its own table
      right_on_table = reserve_space();
      right_on_typee = get_type_operand(node->right_on_type);
      emit(VVM::opcodes::alloc, {right_on_typee, right_on_table});
      for (size_t i = 0; i < node->right_on.size(); i++) {
        HIR::expr_t b = node->right_on[i]->value;
        VVM::operand_t right_on = visit(b);
        VVM::operand_t offset = VVM::encode_operand(i,
            VVM::OpMask::kImmediate);
        VVM::operand_t dst = reserve_space();
        emit(VVM::opcodes::member, {right_on_table, offset, dst});
        VVM::operand_t typee = get_type_operand(b->type);
        emit(VVM::opcodes::assign, {right_on, typee, dst});
      }
    }
    if (node->left_asof != nullptr) {
      // get 'left_asof' as array
      HIR::expr_t lv = node->left_asof->value;
      left_asof_value = visit(lv);
      left_asof_typee = get_type_operand(lv->type);

      // get 'right_asof' as array
      HIR::expr_t rv= node->right_asof->value;
      right_asof_value = visit(rv);

      // encode strictness
      strict = VVM::encode_operand(size_t(node->strict),
                                   VVM::OpMask::kImmediate);

      // encode direction
      VVM::AsofDirection direct = VVM::AsofDirection::kBackward;
      if (node->direction == HIR::direction_t::kForward) {
        direct = VVM::AsofDirection::kForward;
      }
      if (node->direction == HIR::direction_t::kNearest) {
        direct = VVM::AsofDirection::kNearest;
      }
      direction = VVM::encode_operand(size_t(direct),
                                      VVM::OpMask::kImmediate);
    }

    // match based on parameters
    if (!node->left_on.empty() && node->left_asof == nullptr) {
      left_indices = reserve_space();
      right_indices = reserve_space();
      emit(VVM::opcodes::eqmatch, {left_on_typee, left_on_table,
                                   right_on_table, left_indices,
                                   right_indices});
    }
    if (node->left_on.empty() && node->left_asof != nullptr) {
      if (node->within == nullptr &&
          node->direction != HIR::direction_t::kNearest) {
        left_indices = reserve_space();
        right_indices = reserve_space();
        emit(VVM::opcodes::asofmatch, {left_asof_typee, left_asof_value,
                                       right_asof_value, strict, direction,
                                       left_indices, right_indices});
      } else if (node->within == nullptr) {
        left_indices = reserve_space();
        right_indices = reserve_space();
        emit(VVM::opcodes::asofnear, {left_asof_typee, left_asof_value,
                                      right_asof_value, strict, direction,
                                      left_indices, right_indices});
      } else {
        within = visit(node->within);
        left_indices = reserve_space();
        right_indices = reserve_space();
        emit(VVM::opcodes::asofwithin, {left_asof_typee, left_asof_value,
                                        right_asof_value, strict, direction,
                                        within, left_indices, right_indices});
      }
    }
    if (!node->left_on.empty() && node->left_asof != nullptr) {
      if (node->within == nullptr &&
          node->direction != HIR::direction_t::kNearest) {
        left_indices = reserve_space();
        right_indices = reserve_space();
        emit(VVM::opcodes::eqasofmatch, {left_on_typee, left_on_table,
                                         right_on_table, left_asof_typee,
                                         left_asof_value, right_asof_value,
                                         strict, direction,
                                         left_indices, right_indices});
      } else if (node->within == nullptr) {
        left_indices = reserve_space();
        right_indices = reserve_space();
        emit(VVM::opcodes::eqasofnear, {left_on_typee, left_on_table,
                                        right_on_table, left_asof_typee,
                                        left_asof_value, right_asof_value,
                                        strict, direction,
                                        left_indices, right_indices});
      } else {
        within = visit(node->within);
        left_indices = reserve_space();
        right_indices = reserve_space();
        emit(VVM::opcodes::eqasofwithin, {left_on_typee, left_on_table,
                                          right_on_table, left_asof_typee,
                                          left_asof_value, right_asof_value,
                                          strict, direction, within,
                                          left_indices, right_indices});
      }
    }

    // multidx 'left' and 'right' from matched indices
    VVM::operand_t left_part = reserve_space();
    VVM::operand_t right_part = reserve_space();
    emit(VVM::opcodes::multidx, {left, left_indices, left_typee, left_part});
    emit(VVM::opcodes::multidx,
         {right, right_indices, right_typee, right_part});

    // take 'right_remaining_type' from right
    VVM::operand_t right_remaining_part = reserve_space();
    emit(VVM::opcodes::take, {right_typee, right_remaining_typee, right_part,
                              right_remaining_part});

    // concat left and remaining tables
    VVM::operand_t result = reserve_space();
    emit(VVM::opcodes::concat,
         {typee, left_part, right_remaining_part, result});
    return result;
  }

  antlrcpp::Any visitUnaryOp(HIR::UnaryOp_t node) override {
    // operator expressions are just syntactic sugar for function calls
    HIR::expr_t id = HIR::Id(node->op, node->ref, nullptr, empty_traits,
                             HIR::compmode_t::kNormal, node->op);
    HIR::expr_t desugar =
      HIR::FunctionCall(id, {node->operand}, node->inline_expr, node->type,
                        node->traits, node->mode, id->name);
    return visit(desugar);
  }

  antlrcpp::Any visitBinOp(HIR::BinOp_t node) override {
    // operator expressions are just syntactic sugar for function calls
    HIR::expr_t id = HIR::Id(node->op, node->ref, nullptr, empty_traits,
                             HIR::compmode_t::kNormal, node->op);
    HIR::expr_t desugar =
      HIR::FunctionCall(id, {node->left, node->right}, node->inline_expr,
                        node->type, node->traits, node->mode, id->name);
    return visit(desugar);
  }

  antlrcpp::Any visitFunctionCall(HIR::FunctionCall_t node) override {
    // inline functions are directly evaluated
    if (node->inline_expr != nullptr) {
      return visit(node->inline_expr);
    }

    VVM::operand_t result = 0;
    std::vector<VVM::operand_t> params;
    for (auto arg: node->args) {
      VVM::operand_t p = visit(arg);
      params.push_back(p);
    }
    // get the type and reference
    HIR::expr_t func = node->func;
    HIR::datatype_t type = func->type;
    HIR::resolved_t ref = nullptr;
    if (func->expr_kind == HIR::expr_::ExprKind::kId) {
      HIR::Id_t id = dynamic_cast<HIR::Id_t>(func);
      ref = id->ref;
    }
    if (func->expr_kind == HIR::expr_::ExprKind::kTemplatedId) {
      HIR::TemplatedId_t id = dynamic_cast<HIR::TemplatedId_t>(func);
      ref = id->ref;
    }
    // generate based on the kind of reference
    if (type != nullptr && type->datatype_kind ==
        HIR::datatype_::DatatypeKind::kKind) {
      // fill members of type constructors
      result = reserve_space();
      HIR::Kind_t k = dynamic_cast<HIR::Kind_t>(type);
      VVM::operand_t typee = get_type_operand(k->type);
      emit(VVM::opcodes::alloc, {typee, result});
      for (size_t i = 0; i < params.size(); i++) {
        VVM::operand_t offset =
          VVM::encode_operand(i, VVM::OpMask::kImmediate);
        VVM::operand_t member = reserve_space();
        emit(VVM::opcodes::member, {result, offset, member});
        VVM::operand_t value = params[i];
        VVM::operand_t typee = get_type_operand(node->args[i]->type);
        emit(VVM::opcodes::assign, {value, typee, member});
      }
    } else if (ref != nullptr && ref->resolved_kind ==
               HIR::resolved_::ResolvedKind::kVVMOpRef) {
      // inline builtin functions
      HIR::VVMOpRef_t ptr = dynamic_cast<HIR::VVMOpRef_t>(ref);
      size_t opcode = ptr->opcode;
      result = reserve_space();
      params.push_back(result);
      emit(opcode, params);
    } else if (ref != nullptr && ref->resolved_kind ==
               HIR::resolved_::ResolvedKind::kFuncRef) {
      // invoke user-defined functions
      HIR::FuncRef_t fr = dynamic_cast<HIR::FuncRef_t>(ref);
      HIR::FunctionDef_t fd = dynamic_cast<HIR::FunctionDef_t>(fr->ref);
      VVM::operand_t op = func_map_get(fd);
      result = reserve_space();
      params.push_back(result);
      VVM::operand_t length =
        VVM::encode_operand(params.size(), VVM::OpMask::kImmediate);
      params.insert(params.begin(), length);
      params.insert(params.begin(), op);
      emit(VVM::opcodes::call, params);
    } else {
      nyi("FunctionCall not on builtin, function, or kind");
    }
    return result;
  }

  antlrcpp::Any visitTypeOf(HIR::TypeOf_t node) override {
    // we expect this to be a Kind
    if (is_kind_type(node->type)) {
      HIR::Kind_t k = dynamic_cast<HIR::Kind_t>(node->type);
      return get_type_operand(k->type);
    }
    return get_type_operand(node->type);
  }

  antlrcpp::Any visitTraitsOf(HIR::TraitsOf_t node) override {
    // we only reach this node if the user requests it via REPL
    return direct_repr(node->s);
  }

  antlrcpp::Any visitModeOf(HIR::ModeOf_t node) override {
    // we only reach this node if the user requests it via REPL
    return direct_repr(node->s);
  }

  antlrcpp::Any visitColumns(HIR::Columns_t node) override {
    // we only reach this node if the user requests it via REPL
    return direct_repr(node->s);
  }

  antlrcpp::Any visitCompile(HIR::Compile_t node) override {
    VVM::operand_t last_stmt_value;
    for (HIR::stmt_t s: node->body) {
      VVM::operand_t op = visit(s);
      last_stmt_value = op;
    }
    return last_stmt_value;
  }

  antlrcpp::Any visitMembersOf(HIR::MembersOf_t node) override {
    return visit(node->member_list);
  }

  antlrcpp::Any visitMember(HIR::Member_t node) override {
    // implied members should have saved the source's register already
    auto iter = implied_reg_map_.find(node->value);
    VVM::operand_t source = (iter != implied_reg_map_.end())
                              ? iter->second
                              : VVM::operand_t(visit(node->value));

    // declaration's offset was recorded by Sema
    VVM::operand_t offset = 0;
    if (node->ref->resolved_kind == HIR::resolved_::ResolvedKind::kDeclRef) {
      HIR::DeclRef_t ref = dynamic_cast<HIR::DeclRef_t>(node->ref);
      HIR::declaration_t declaration = ref->ref;
      offset = VVM::encode_operand(declaration->offset,
                                   VVM::OpMask::kImmediate);
    } else {
      nyi("Member on non-declaration");
    }

    // simply invoke the opcode
    VVM::operand_t destination = reserve_space();
    emit(VVM::opcodes::member, {source, offset, destination});
    return destination;
  }

  antlrcpp::Any visitSubscript(HIR::Subscript_t node) override {
    VVM::operand_t value = visit(node->value);
    if (node->slice->slice_kind == HIR::slice_::SliceKind::kIndex) {
      HIR::Index_t index = dynamic_cast<HIR::Index_t>(node->slice);
      VVM::operand_t i = visit(index->value);
      size_t opcode = specialize_opcode("idx", node->value->type,
                                        index->value->type);
      VVM::operand_t result = reserve_space();
      emit(opcode, {value, i, result});
      return result;
    } else {
      nyi("Subscript on slice");
      return 0;
    }
  }

  antlrcpp::Any visitUserDefinedLiteral(HIR::UserDefinedLiteral_t node)
    override {
    // user-defined literals are just syntactic sugar for funcion calls
    HIR::expr_t id = HIR::Id("suffix" + node->suffix, node->ref, nullptr,
                             empty_traits, HIR::compmode_t::kNormal,
                             node->suffix);
    HIR::expr_t desugar =
      HIR::FunctionCall(id, {node->literal}, node->inline_expr, node->type,
                        node->traits, node->mode, id->name);
    return visit(desugar);
  }

  antlrcpp::Any visitIntegerLiteral(HIR::IntegerLiteral_t node) override {
    if (VVM::is_small_int(node->n, 2)) {
      return VVM::encode_operand(node->n, VVM::OpMask::kImmediate);
    }

    return store_constant(node->n, integer_register_cache);
  }

  antlrcpp::Any visitFloatingLiteral(HIR::FloatingLiteral_t node) override {
    return store_constant(node->n, floating_register_cache);
  }

  antlrcpp::Any visitBoolLiteral(HIR::BoolLiteral_t node) override {
    return VVM::encode_operand(node->b, VVM::OpMask::kImmediate);
  }

  antlrcpp::Any visitStr(HIR::Str_t node) override {
    return store_constant(node->s, string_register_cache);
  }

  antlrcpp::Any visitChar(HIR::Char_t node) override {
    return VVM::encode_operand(node->c, VVM::OpMask::kImmediate);
  }

  antlrcpp::Any visitId(HIR::Id_t node) override {
    return visit(node->ref);
  }

  antlrcpp::Any visitImpliedMember(HIR::ImpliedMember_t node) override {
    // an ID found in a preferred scope is an implied member
    HIR::expr_t member =
      HIR::Member(node->implied_value, node->s, node->ref, node->type,
                  node->traits, node->mode, node->s);
    return visit(member);
  }

  antlrcpp::Any visitOverloadedId(HIR::OverloadedId_t node) override {
    return visit(node->refs[0]);
  }

  antlrcpp::Any visitTemplatedId(HIR::TemplatedId_t node) override {
    return visit(node->ref);
  }

  antlrcpp::Any visitList(HIR::List_t node) override {
    if (is_kind_type(node->type)) {
      // type -- eg., [Int64]
      HIR::Kind_t k = dynamic_cast<HIR::Kind_t>(node->type);
      return get_type_operand(k->type);
    } else {
      // value -- eg., [1, 2, 3]
      VVM::operand_t value = reserve_space();
      VVM::operand_t typee = get_type_operand(node->type);
      emit(VVM::opcodes::alloc, {typee, value});
      HIR::Array_t ary_type = dynamic_cast<HIR::Array_t>(node->type);
      VVM::operand_t elem_type = get_type_operand(ary_type->type);
      for (HIR::expr_t v: node->values) {
        VVM::operand_t elem = visit(v);
        emit(VVM::opcodes::append, {elem, elem_type, value});
      }
      return value;
    }
  }

  antlrcpp::Any visitParen(HIR::Paren_t node) override {
    return visit(node->subexpr);
  }

  antlrcpp::Any visitAnonData(HIR::AnonData_t node) override {
    // build the type
    std::vector<HIR::declaration_t> templates;
    HIR::stmt_t stmt = HIR::DataDef("", templates, node->body, nullptr,
                                    node->scope);
    HIR::datatype_t udt = HIR::UDT("", stmt, nullptr);
    return get_type_operand(udt);
  }

  antlrcpp::Any visitSlice(HIR::Slice_t node) override {
    nyi("Slice");
    return 0;
  }

  antlrcpp::Any visitIndex(HIR::Index_t node) override {
    invalid("Index");
    return 0;
  }

  antlrcpp::Any visitAlias(HIR::alias_t node) override {
    nyi("Alias");
    return 0;
  }

  antlrcpp::Any visitDeclaration(HIR::declaration_t node) override {
    // use comptime literal if it's available
    if (node->mode == HIR::compmode_t::kComptime &&
        node->comptime_literal != nullptr) {
      return VVM::operand_t(0);
    }

    // reserve some space
    const VVM::OpMask mask = node->is_global ? VVM::OpMask::kGlobal
                                             : VVM::OpMask::kLocal;
    VVM::operand_t target = reserve_space(mask);
    reg_map_[node] = target;
    VVM::operand_t typee = get_type_operand(node->type);
    emit(VVM::opcodes::alloc, {typee, target});

    // assign the value
    if (node->comptime_literal != nullptr) {
      VVM::operand_t value = visit(node->comptime_literal);
      emit(VVM::opcodes::assign, {value, typee, target});
    } else if (node->value != nullptr) {
      VVM::operand_t value = visit(node->value);
      emit(VVM::opcodes::assign, {value, typee, target});
    }

    return target;
  }

  antlrcpp::Any visitDecltype(HIR::decltype_t value) override {
    invalid("Decltype");
    return 0;
  }

  antlrcpp::Any visitQuerytype(HIR::querytype_t value) override {
    invalid("Querytype");
    return 0;
  }

  antlrcpp::Any visitDirection(HIR::direction_t value) override {
    invalid("Direction");
    return 0;
  }

  antlrcpp::Any visitCompmode(HIR::compmode_t value) override {
    invalid("Compmode");
    return 0;
  }

  antlrcpp::Any visitVVMType(HIR::VVMType_t node) override {
    invalid("VVMType");
    return 0;
  }

  antlrcpp::Any visitUDT(HIR::UDT_t node) override {
    invalid("UDT");
    return 0;
  }

  antlrcpp::Any visitArray(HIR::Array_t node) override {
    invalid("Array");
    return 0;
  }

  antlrcpp::Any visitFuncType(HIR::FuncType_t node) override {
    invalid("FuncType");
    return 0;
  }

  antlrcpp::Any visitTemplateType(HIR::TemplateType_t node) override {
    invalid("TemplateType");
    return 0;
  }

  antlrcpp::Any visitPlaceholder(HIR::Placeholder_t node) override {
    invalid("Placeholder");
    return 0;
  }

  antlrcpp::Any visitKind(HIR::Kind_t node) override {
    invalid("Kind");
    return 0;
  }

  antlrcpp::Any visitVoid(HIR::Void_t node) override {
    invalid("Void");
    return 0;
  }

  antlrcpp::Any visitDeclRef(HIR::DeclRef_t node) override {
    HIR::declaration_t decl = node->ref;
    // use comptime literal if it's available
    if (decl->mode == HIR::compmode_t::kComptime &&
        decl->comptime_literal != nullptr) {
      return visit(decl->comptime_literal);
    }
    // return register or make it on demand
    auto iter = reg_map_.find(decl);
    return (iter != reg_map_.end()) ? iter->second
                                    : VVM::operand_t(visit(decl));
  }

  antlrcpp::Any visitFuncRef(HIR::FuncRef_t node) override {
    // we only reach this node if the user requests it via REPL
    HIR::FunctionDef_t fd = dynamic_cast<HIR::FunctionDef_t>(node->ref);
    return direct_repr("<func: " + fd->name + ">");
  }

  antlrcpp::Any visitGenericRef(HIR::GenericRef_t node) override {
    // we only reach this node if the user requests it via REPL
    return direct_repr("<generic func>");
  }

  antlrcpp::Any visitMacroRef(HIR::MacroRef_t node) override {
    // we only reach this node if the user requests it via REPL
    return direct_repr("<macro>");
  }

  antlrcpp::Any visitTemplateRef(HIR::TemplateRef_t node) override {
    // we only reach this node if the user requests it via REPL
    return direct_repr("<template>");
  }

  antlrcpp::Any visitDataRef(HIR::DataRef_t node) override {
    return get_type_operand(node->udt);
  }

  antlrcpp::Any visitModRef(HIR::ModRef_t node) override {
    nyi("ModRef");
    return 0;
  }

  antlrcpp::Any visitVVMOpRef(HIR::VVMOpRef_t node) override {
    // we only reach this node if the user requests it via REPL
    return direct_repr("<func>");
  }

  antlrcpp::Any visitVVMTypeRef(HIR::VVMTypeRef_t node) override {
    return get_type_operand(HIR::VVMType(node->t));
  }

  antlrcpp::Any visitSemaFuncRef(HIR::SemaFuncRef_t node) override {
    // we only reach this node if the user requests it via REPL
    return direct_repr("<func>");
  }

  antlrcpp::Any visitSemaTypeRef(HIR::SemaTypeRef_t node) override {
    // we expect this to be a Kind
    if (is_kind_type(node->type)) {
      HIR::Kind_t k = dynamic_cast<HIR::Kind_t>(node->type);
      return get_type_operand(k->type);
    }
    return get_type_operand(node->type);
  }

 public:

  // return the VVM program
  VVM::Program get_program() {
    VVM::Program program;
    program.constants = constants_;
    program.types = types_;
    program.instructions = instructions_;
    return program;
  }

  void set_interactive(bool b) {
    interactive_ = b;
  }
} comptime_codegen, runtime_codegen;


// generate bytecode for VVM
VVM::Program codegen(HIR::mod_t hir, VVM::Mode mode, bool interactive,
                     bool dump_vvm) {
  // build VVM bytecode
  CodegenVisitor& codegen_visitor =
    (mode == VVM::Mode::kComptime) ? comptime_codegen : runtime_codegen;
  codegen_visitor.set_interactive(interactive);
  codegen_visitor.visit(hir);
  VVM::Program program = codegen_visitor.get_program();

  // print VVM bytebode
  if (dump_vvm) {
    std::cout << VVM::to_string(program) << std::endl;
  }

  return program;
}

