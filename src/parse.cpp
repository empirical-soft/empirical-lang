/*
 * Parse -- tokenize and parse user's code
 *
 * Copyright (C) 2019--2020 Empirical Software Solutions, LLC
 *
 * This program is distributed under the terms of the GNU Affero General
 * Public License with the Commons Clause.
 *
 */

#include <vector>
#include <sstream>
#include <iostream>

#include <antlr4-runtime.h>
#include <EmpiricalLexer.h>
#include <EmpiricalParser.h>
#include <EmpiricalVisitor.h>

#include <empirical.hpp>

#include <VVM/utils/nil.hpp>

using namespace empirical;
using namespace antlr4;

// build abstract syntax tree (AST) from parse tree
class ParseVisitor : public EmpiricalVisitor {
  // return a new string with all escaped contents
  std::string parse_string(const std::string& str) {
    size_t offset = 0, len = str.size();

    // skip leading and trailing quote characters
    offset++;
    len -= 2;

    // triple-quoted strings will have two remaing quotes on each end
    if (len >= 4 && str[offset] == '"' && str[offset + 1] == '"') {
      offset += 2;
      len -= 4;
    }

    // construct final result
    std::string result(str.c_str() + offset, len);
    return escape_string(result);
  }

  // escape characters in a string
  std::string escape_string(const std::string& str) {
    std::string result;
    for (size_t i = 0; i < str.size(); i++) {
      if (str[i] != '\\') {
        result += str[i];
      } else {
        if (i + 1 < str.size()) {
          i++;
          char c = str[i];
          switch (c) {
            case '\n': break;
            case '\\': result += '\\';   break;
            case '\'': result += '\'';   break;
            case '\"': result += '\"';   break;
            case 'b':  result += '\b';   break;
            case 'f':  result += '\014'; break;
            case 't':  result += '\t';   break;
            case 'n':  result += '\n';   break;
            case 'r':  result += '\r';   break;
            case 'v':  result += '\013'; break;
            case 'a':  result += '\007'; break;
            case '0': case '1': case '2': case '3':
            case '4': case '5': case '6': case '7': {
              // take at most three octal digits
              int n = str[i] - '0';
              if (i + 1 < str.size() && '0' <= str[i+1] && str[i+1] <= '7') {
                i++;
                n = n * 8 + (str[i] - '0');
                if (i + 1 < str.size() && '0' <= str[i+1] && str[i+1] <= '7') {
                  i++;
                  n = n * 8 + (str[i] - '0');
                }
              }
              result += static_cast<char>(n);
              break;
            }
            case 'x': {
              // hexadecimal
              if (i + 2 < str.size()) {
                uint8_t digit1 = base36_chars[str[i + 1]];
                uint8_t digit2 = base36_chars[str[i + 2]];
                if (digit1 < 16 && digit2 < 16) {
                  result += static_cast<char>(digit1 * 16 + digit2);
                  i += 2;
                  break;
                }
              }
            }
            default: result += '\\'; i--; break;
          }
        }
      }
    }
    return result;
  }

  // quick look-up for character codes
  uint8_t base36_chars[256] = {
    37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
    37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
    37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  37, 37, 37, 37, 37, 37,
    37, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
    25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 37, 37, 37, 37, 37,
    37, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
    25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 37, 37, 37, 37, 37,
    37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
    37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
    37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
    37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
    37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
    37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
    37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
    37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
  };

  // extract docstring if first statement is a literal string
  std::string extract_docstring(std::vector<AST::stmt_t>& stmts) {
    std::string docstring;
    if (!stmts.empty()) {
      AST::stmt_t stmt = stmts[0];
      if (stmt->stmt_kind == AST::stmt_::StmtKind::kExpr) {
        AST::expr_t expr = dynamic_cast<AST::Expr_t>(stmt)->value;
        if (expr->expr_kind == AST::expr_::ExprKind::kStr) {
          // save string and remove it from the statements
          docstring = dynamic_cast<AST::Str_t>(expr)->s;
          stmts.erase(stmts.begin());
        }
      }
    }
    return docstring;
  }

 protected:

  /* statements */

  antlrcpp::Any visitInput(EmpiricalParser::InputContext *ctx) override {
    std::vector<AST::stmt_t> results;
    for (auto s: ctx->stmt()) {
      // append each returned vector of stmt to our own single vector
      std::vector<AST::stmt_t> stmts = visit(s);
      results.insert(results.end(), stmts.begin(), stmts.end());
    }

    std::string docstring;
    if (!interactive_) {
      docstring = extract_docstring(results);
    }

    return AST::Module(results, docstring);
  }

  antlrcpp::Any visitStmt(EmpiricalParser::StmtContext *ctx) override {
    std::vector<AST::stmt_t> results;

    if (ctx->simple_stmt()) {
      results = visit(ctx->simple_stmt()).as<decltype(results)>();
    }

    if (ctx->compound_stmt()) {
      results = visit(ctx->compound_stmt()).as<decltype(results)>();
    }

    return results;
  }

  antlrcpp::Any visitSimple_stmt(EmpiricalParser::Simple_stmtContext *ctx)
    override {
    // a simple_stmt is a vector of stmt
    std::vector<AST::stmt_t> results;
    for (auto s: ctx->small_stmt()) {
      results.push_back(visit(s));
    }
    return results;
  }

  antlrcpp::Any visitSmall_stmt(EmpiricalParser::Small_stmtContext *ctx)
    override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitCompound_stmt(EmpiricalParser::Compound_stmtContext *ctx)
    override {
    // vectorize a compound_stmt to be the same format as simple_stmt
    std::vector<AST::stmt_t> results;
    results.push_back(visitChildren(ctx));
    return results;
  }

  antlrcpp::Any visitSuite(EmpiricalParser::SuiteContext *ctx) override {
    // if a simple_stmt exists, then it is the only possibilty
    if (ctx->simple_stmt()) {
      return visit(ctx->simple_stmt());
    }

    // no simple_stmt above implies we had one or more stmt
    std::vector<AST::stmt_t> results;
    for (auto s: ctx->stmt()) {
      // append each returned vector of stmt to our own single vector
      std::vector<AST::stmt_t> stmts = visit(s);
      results.insert(results.end(), stmts.begin(), stmts.end());
    }
    return results;
  }

  antlrcpp::Any visitEos(EmpiricalParser::EosContext *context) override {
    return nullptr;
  }

  /* function definition */

  antlrcpp::Any visitFuncdef(EmpiricalParser::FuncdefContext *ctx) override {
    AST::identifier name = visit(ctx->name);

    std::vector<AST::declaration_t> templates;
    if (ctx->templates) {
      templates = visit(ctx->templates).as<decltype(templates)>();
    }

    std::vector<AST::declaration_t> placeholders;
    if (ctx->placeholders) {
      placeholders = visit(ctx->placeholders).as<decltype(placeholders)>();
    }

    std::vector<AST::declaration_t> args;
    if (ctx->args) {
      args = visit(ctx->args).as<decltype(args)>();
    }

    AST::expr_t explicit_rettype = nullptr;
    if (ctx->rettype) {
      explicit_rettype = visit(ctx->rettype);
    }

    std::vector<AST::stmt_t> body;
    std::string docstring;
    if (ctx->body) {
      body = visit(ctx->body).as<decltype(body)>();
      docstring = extract_docstring(body);
    }

    AST::expr_t single = nullptr;
    bool force_inline = false;
    if (ctx->single) {
      single = visit(ctx->single);
      force_inline = (ctx->op->getText() == "=>");
    }

    // this should be prevented by the grammar, but just to be safe
    if (!body.empty() && single != nullptr) {
      parse_err_ << "Error: cannot mix expression syntax and statement syntax"
                 << std::endl;
    }

    // determine if this was a generic function or a macro
    bool is_generic = !placeholders.empty(), is_macro = false;
    for (AST::declaration_t a: args) {
      if (a->explicit_type == nullptr) {
        is_generic = true;
      }
      if (a->macro_parameter) {
        is_macro = true;
      }
    }

    if (is_generic && !templates.empty()) {
      parse_err_ << "Error: cannot currently mix generics with templates"
                 << std::endl;
    }
    if (is_generic && is_macro) {
      parse_err_ << "Error: cannot currently mix generics with macros"
                 << std::endl;
    }
    if (is_macro && !templates.empty()) {
      parse_err_ << "Error: cannot currently mix macros with templates"
                 << std::endl;
    }

    AST::stmt_t node = AST::FunctionDef(name, templates, args, body, single,
                                        force_inline, explicit_rettype,
                                        docstring);
    if (is_generic) {
      node = AST::GenericDef(node, placeholders, args, explicit_rettype);
    }
    if (!templates.empty()) {
      node = AST::TemplateDef(node, templates);
    }
    if (is_macro) {
      node = AST::MacroDef(node, args, explicit_rettype);
    }
    return node;
  }

  antlrcpp::Any visitFunc_name(EmpiricalParser::Func_nameContext *ctx)
    override {
    if (ctx->oper()) {
      return visit(ctx->oper());
    }
    return ctx->getText();
  }

  antlrcpp::Any visitDecl_list(EmpiricalParser::Decl_listContext *ctx)
    override {
    std::vector<AST::declaration_t> results;
    for (auto s: ctx->declaration()) {
      results.push_back(visit(s));
    }
    return results;
  }

  antlrcpp::Any visitDeclaration(EmpiricalParser::DeclarationContext *ctx)
    override {
    bool macro_parameter = (ctx->getText()[0] == '$');
    AST::identifier name(ctx->name->getText());
    AST::expr_t explicit_type = nullptr;
    if (ctx->type) {
      explicit_type = visit(ctx->type);
    }
    AST::expr_t value = nullptr;
    if (ctx->value) {
      value = visit(ctx->value);
    }
    return AST::declaration(name, explicit_type, value, macro_parameter);
  }

  /* data definition */

  antlrcpp::Any visitDatadef(EmpiricalParser::DatadefContext *ctx) override {
    AST::identifier name(ctx->name->getText());

    std::vector<AST::declaration_t> templates;
    if (ctx->templates) {
      templates = visit(ctx->templates).as<decltype(templates)>();
    }

    std::vector<AST::declaration_t> body;
    if (ctx->body) {
      body = visit(ctx->body).as<decltype(body)>();
    }

    AST::expr_t single = nullptr;
    if (ctx->single) {
      single = visit(ctx->single);
    }

    // this should be prevented by the grammar, but just to be safe
    if (!body.empty() && single != nullptr) {
      parse_err_ << "Error: cannot mix expression syntax and statement syntax"
                 << std::endl;
    }

    AST::stmt_t node = AST::DataDef(name, templates, body, single);
    if (!templates.empty()) {
      return AST::TemplateDef(node, templates);
    }
    return node;
  }

  /* control flow */

  antlrcpp::Any visitIf_stmt(EmpiricalParser::If_stmtContext *ctx) override {
    AST::expr_t test = visit(ctx->test);
    std::vector<AST::stmt_t> body = visit(ctx->body);
    std::vector<AST::stmt_t> else_body;

    // the last 'else' is visited first in preparation for the tree below
    size_t num_suites = ctx->suite().size() - 1;
    if (ctx->else_body) {
      else_body = visit(ctx->else_body).as<decltype(else_body)>();
      num_suites--;
    }

    // build chain of conditionals as a tree in reverse
    while (num_suites > 0) {
      AST::expr_t elif_test = visit(ctx->expr(num_suites));
      std::vector<AST::stmt_t> elif_body = visit(ctx->suite(num_suites));
      AST::stmt_t orelse = If(elif_test, elif_body, else_body);
      else_body = {orelse};
      num_suites--;
    }

    return AST::If(test, body, else_body);
  }

  antlrcpp::Any visitWhile_stmt(EmpiricalParser::While_stmtContext *ctx)
    override {
    AST::expr_t test = visit(ctx->test);
    std::vector<AST::stmt_t> body = visit(ctx->body);
    return AST::While(test, body);
  }

  /* delete statement */

  antlrcpp::Any visitDel_stmt(EmpiricalParser::Del_stmtContext *ctx) override {
    std::vector<AST::expr_t> target = visit(ctx->target);
    return Del(target);
  }

  antlrcpp::Any visitExpr_list(EmpiricalParser::Expr_listContext *ctx)
    override {
    std::vector<AST::expr_t> results;
    for (auto s: ctx->expr()) {
      results.push_back(visit(s));
    }
    return results;
  }

  /* import statement */

  antlrcpp::Any visitImport_stmt(EmpiricalParser::Import_stmtContext *ctx)
    override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitImport_name(EmpiricalParser::Import_nameContext *ctx)
    override {
    std::vector<AST::alias_t> names = visit(ctx->names);
    return AST::Import(names);
  }

  antlrcpp::Any visitDotted_as_names(EmpiricalParser::Dotted_as_namesContext
                                     *ctx) override {
    std::vector<AST::alias_t> results;
    for (auto s: ctx->dotted_as_name()) {
      results.push_back(visit(s));
    }
    return results;
  }

  antlrcpp::Any visitDotted_as_name(EmpiricalParser::Dotted_as_nameContext
                                    *ctx) override {
    AST::expr_t value = visit(ctx->name);
    AST::identifier name;
    if (ctx->asname) {
      name = std::string(ctx->asname->getText());
    }
    return AST::alias(value, name);
  }

  antlrcpp::Any visitImport_from(EmpiricalParser::Import_fromContext *ctx)
    override {
    std::vector<AST::alias_t> names;
    if (ctx->names) {
      names = visit(ctx->names).as<decltype(names)>();
    }
    AST::identifier module(ctx->module->getText());
    return AST::ImportFrom(module, names);
  }

  antlrcpp::Any visitImport_as_names(EmpiricalParser::Import_as_namesContext
                                     *ctx) override {
    std::vector<AST::alias_t> results;
    for (auto s: ctx->import_as_name()) {
      results.push_back(visit(s));
    }
    return results;
  }

  antlrcpp::Any visitImport_as_name(EmpiricalParser::Import_as_nameContext
                                    *ctx) override {
    AST::identifier id(ctx->name->getText());
    AST::expr_t value = AST::Id(id);
    AST::identifier name;
    if (ctx->asname) {
      name = std::string(ctx->asname->getText());
    }
    return AST::alias(value, name);
  }

  antlrcpp::Any visitDotted_name(EmpiricalParser::Dotted_nameContext *ctx)
    override {
    // a single name is an identifier
    AST::identifier id(ctx->NAME(0)->getText());
    AST::expr_t e = AST::Id(id);

    // build nested Members for any further names
    for (size_t i = 1; i < ctx->NAME().size(); i++) {
      AST::identifier id(ctx->NAME(i)->getText());
      e = AST::Member(e, id);
    }

    return e;
  }

  /* flow statements */

  antlrcpp::Any visitReturn_stmt(EmpiricalParser::Return_stmtContext *ctx)
    override {
    AST::expr_t e = nullptr;
    if (ctx->expr()) {
      e = visit(ctx->expr());
    }
    return AST::Return(e);
  }

  /* declarations */

  antlrcpp::Any visitDecl_stmt(EmpiricalParser::Decl_stmtContext *ctx)
    override {
    AST::decltype_t dt;
    switch (ctx->dt->getType()) {
#define CASE(TOKEN, TYPE) \
      case EmpiricalLexer::TOKEN: \
        dt = AST::decltype_t::TYPE; \
        break;
      CASE(LET, kLet)
      CASE(VAR, kVar)
#undef CASE
    }
    std::vector<AST::declaration_t> decls = visit(ctx->decls);
    return AST::Decl(dt, decls);
  }

  /* expressions */

  antlrcpp::Any visitNexpr_list(EmpiricalParser::Nexpr_listContext *ctx)
    override {
    std::vector<AST::alias_t> results;
    for (auto n: ctx->nexpr()) {
      results.push_back(visit(n));
    }
    return results;
  }

  antlrcpp::Any visitNexpr(EmpiricalParser::NexprContext *ctx)
    override {
    AST::expr_t value = visit(ctx->value);
    AST::identifier name;
    if (ctx->name) {
      name = std::string(ctx->name->getText());
    }
    return AST::alias(value, name);
  }

  antlrcpp::Any visitExpr_stmt(EmpiricalParser::Expr_stmtContext *ctx)
    override {
    const auto& exprs = ctx->expr();
    return (exprs.size() == 1) ? AST::Expr(visit(exprs[0]))
                               : AST::Assign(visit(exprs[0]), visit(exprs[1]));
  }

  antlrcpp::Any visitDirection(EmpiricalParser::DirectionContext *ctx)
    override {
    AST::direction_t d;
    switch (ctx->dt->getType()) {
#define CASE(TOKEN, TYPE) \
      case EmpiricalLexer::TOKEN: \
        d = AST::direction_t::TYPE; \
        break;
      CASE(BACKWARD, kBackward)
      CASE(FORWARD, kForward)
      CASE(NEAREST, kNearest)
#undef CASE
    }
    return d;
  }

  antlrcpp::Any visitJoin_params(EmpiricalParser::Join_paramsContext *ctx)
    override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitQueryExpr(EmpiricalParser::QueryExprContext *ctx)
    override {
    AST::querytype_t qt;
    switch (ctx->qt->getType()) {
#define CASE(TOKEN, TYPE) \
      case EmpiricalLexer::TOKEN: \
        qt = AST::querytype_t::TYPE; \
        break;
      CASE(SELECT, kSelect)
      CASE(EXEC, kExec)
#undef CASE
    }
    AST::expr_t table = visit(ctx->table);
    std::vector<AST::alias_t> cols;
    if (ctx->cols) {
      cols = visit(ctx->cols).as<decltype(cols)>();
    }
    std::vector<AST::alias_t> by;
    if (ctx->by) {
      by = visit(ctx->by).as<decltype(by)>();
    }
    AST::expr_t where = nullptr;
    if (ctx->where) {
      where = visit(ctx->where);
    }
    return AST::Query(table, qt, cols, by, where);
  }

  antlrcpp::Any visitSortExpr(EmpiricalParser::SortExprContext *ctx)
    override {
    AST::expr_t table = visit(ctx->table);
    std::vector<AST::alias_t> by = visit(ctx->by);
    return AST::Sort(table, by);
  }

  antlrcpp::Any visitJoinExpr(EmpiricalParser::JoinExprContext *ctx)
    override {
    AST::expr_t left = visit(ctx->left);
    AST::expr_t right = visit(ctx->right);

    // initial parameters are empty
    std::vector<AST::alias_t> on;
    AST::alias_t asof = nullptr;
    bool strict = false;
    AST::direction_t direction = AST::direction_t::kDefault;
    AST::expr_t within = nullptr;

    // parameters can appear in any order and might be erroneously duplicated
    for (auto jp: ctx->join_params()) {
      if (jp->on) {
        if (!on.empty()) {
          parse_err_ << "Error: 'on' already listed" << std::endl;
        }
        on = visit(jp->on).as<decltype(on)>();
      }
      if (jp->asof) {
        if (asof != nullptr) {
          parse_err_ << "Error: 'asof' already listed" << std::endl;
        }
        std::vector<AST::alias_t> asofitems = visit(jp->asof);
        if (asofitems.size() > 1) {
          parse_err_ << "Error: joins can have 'asof' on only one column"
                     << std::endl;
        }
        asof = asofitems[0];
      }
      if (jp->STRICT_()) {
        if (strict) {
          parse_err_ << "Error: 'strict' already listed" << std::endl;
        }
        strict = true;
      }
      if (jp->dt) {
        if (direction != AST::direction_t::kDefault) {
          parse_err_ << "Error: direction already listed" << std::endl;
        }
        direction = visit(jp->dt).as<decltype(direction)>();
      }
      if (jp->within) {
        if (within != nullptr) {
          parse_err_ << "Error: 'within' already listed" << std::endl;
        }
        within = visit(jp->within).as<decltype(within)>();
      }
    }

    // sanity check
    if (on.empty() && asof == nullptr) {
      parse_err_ << "Error: joins must have at least one of 'on' or 'asof'"
                 << std::endl;
    }
    if (asof == nullptr && (strict ||
        direction != AST::direction_t::kDefault || within != nullptr)) {
      parse_err_ << "Error: 'asof' expected" << std::endl;
    }

    return AST::Join(left, right, on, asof, strict, direction, within);
  }

  antlrcpp::Any visitUnOpExpr(EmpiricalParser::UnOpExprContext *ctx) override {
    std::string op = ctx->op->getText();
    AST::expr_t operand = visit(ctx->operand);
    return AST::UnaryOp(op, operand);
  }

  antlrcpp::Any visitBinOpExpr(EmpiricalParser::BinOpExprContext *ctx)
    override {
    std::string op = ctx->op->getText();
    AST::expr_t left = visit(ctx->left);
    AST::expr_t right = visit(ctx->right);
    return AST::BinOp(left, op, right);
  }

  antlrcpp::Any visitAtomExpr(EmpiricalParser::AtomExprContext *ctx) override {
    AST::expr_t e = visit(ctx->value);

    // templates must come with an Id
    if (ctx->templates) {
      std::vector<AST::expr_t> templates = visit(ctx->templates);
      if (e->expr_kind == AST::expr_::ExprKind::kId) {
        e = TemplatedId(e, templates);
      }
      else {
        parse_err_ << "Error: only an identifier can have a template"
                   << std::endl;
      }
    }

    // convert trailer from vector in CST to recursive nodes in AST
    for (auto trailer: ctx->trailer()) {
      if (trailer->arg_list()) {
        std::vector<AST::expr_t> args = visit(trailer->arg_list());
        e = AST::FunctionCall(e, args);
      } else if (trailer->subscript()) {
        AST::slice_t slice = visit(trailer->subscript());
        e = AST::Subscript(e, slice);
      } else if (trailer->NAME()) {
        AST::identifier name(trailer->NAME()->getText());
        e = AST::Member(e, name);
      } else {
        // empty function call
        std::vector<AST::expr_t> args;
        e = AST::FunctionCall(e, args);
      }
    }

    return e;
  }

  antlrcpp::Any visitTrailer(EmpiricalParser::TrailerContext *ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitParenExpr(EmpiricalParser::ParenExprContext *ctx)
    override {
    return AST::Paren(visit(ctx->expr()));
  }

  antlrcpp::Any visitAnonDataExpr(EmpiricalParser::AnonDataExprContext *ctx)
    override {
    return AST::AnonData(visit(ctx->decl_list()));
  }

  /* atoms */

  antlrcpp::Any visitNameAtom(EmpiricalParser::NameAtomContext *ctx) override {
    return AST::Id(std::string(ctx->getText()));
  }

  antlrcpp::Any visitOperAtom(EmpiricalParser::OperAtomContext *ctx) override {
    return AST::Id(visit(ctx->oper()));
  }

  antlrcpp::Any visitListAtom(EmpiricalParser::ListAtomContext *ctx) override {
    return visit(ctx->list());
  }

  antlrcpp::Any visitNumAtom(EmpiricalParser::NumAtomContext *ctx) override {
    return visit(ctx->number());
  }

  antlrcpp::Any visitStrAtom(EmpiricalParser::StrAtomContext *ctx) override {
    std::string result;
    for (auto s: ctx->string()) {
      std::string temp = visit(s);
      result += temp;
    }
    return AST::Str(result);
  }

  antlrcpp::Any visitCharAtom(EmpiricalParser::CharAtomContext *ctx) override {
    std::string result = visit(ctx->character());
    if (result.size() != 1) {
      parse_err_ << "Error: character must have exactly one item" << std::endl;
    }
    return AST::Char(result[0]);
  }

  antlrcpp::Any visitTrueAtom(EmpiricalParser::TrueAtomContext *ctx) override {
    return AST::BoolLiteral(true);
  }

  antlrcpp::Any visitFalseAtom(EmpiricalParser::FalseAtomContext *ctx)
    override {
    return AST::BoolLiteral(false);
  }

  antlrcpp::Any visitNilAtom(EmpiricalParser::NilAtomContext *ctx) override {
    return AST::IntegerLiteral(VVM::nil_value<int64_t>());
  }

  antlrcpp::Any visitNanAtom(EmpiricalParser::NanAtomContext *ctx) override {
    return AST::FloatingLiteral(VVM::nil_value<double>());
  }

  /* function arguments */

  antlrcpp::Any visitArg_list(EmpiricalParser::Arg_listContext *ctx) override {
    std::vector<AST::expr_t> results;
    for (auto s: ctx->argument()) {
      results.push_back(visit(s));
    }
    return results;
  }

  antlrcpp::Any visitPositionalArgExpr(EmpiricalParser::PositionalArgExprContext *ctx) override {
    return visit(ctx->expr());
  }

  antlrcpp::Any visitKeywordArgExpr(EmpiricalParser::KeywordArgExprContext
                                    *ctx) override {
    // TODO save the name; Python uses keywords separately in AST for this
    return visit(ctx->value);
  }

  /* array subscript */

  antlrcpp::Any visitSimpleSubscriptExpr(EmpiricalParser::SimpleSubscriptExprContext *ctx) override {
    return AST::Index(visit(ctx->expr()));
  }

  antlrcpp::Any visitSliceExpr(EmpiricalParser::SliceExprContext *ctx)
    override {
    AST::expr_t lower = nullptr;
    if (ctx->lower) {
      lower = visit(ctx->lower);
    }
    AST::expr_t upper = nullptr;
    if (ctx->upper) {
      upper = visit(ctx->upper);
    }
    AST::expr_t step = nullptr;
    if (ctx->step) {
      step = visit(ctx->step);
    }
    return AST::Slice(lower, upper, step);
  }

  antlrcpp::Any visitSliceop(EmpiricalParser::SliceopContext *ctx) override {
    AST::expr_t step = nullptr;
    if (ctx->expr()) {
      step = visit(ctx->expr());
    }
    return step;
  }

  /* operators */

  antlrcpp::Any visitOper(EmpiricalParser::OperContext *ctx) override {
    return ctx->op->getText();
  }

  /* lists */

  antlrcpp::Any visitList(EmpiricalParser::ListContext *ctx) override {
    std::vector<AST::expr_t> results;
    for (auto s: ctx->expr()) {
      results.push_back(visit(s));
    }
    return AST::List(results);
  }

  /* numbers */

  antlrcpp::Any visitIntNumber(EmpiricalParser::IntNumberContext *ctx)
    override {
    return visit(ctx->integer());
  }

  antlrcpp::Any visitFloatNumber(EmpiricalParser::FloatNumberContext *ctx)
    override {
    return parse_number(ctx->getText(), 0, 0, false);
  }

  antlrcpp::Any visitDecInt(EmpiricalParser::DecIntContext *ctx) override {
    return parse_number(ctx->getText(), 0, 10);
  }

  antlrcpp::Any visitOctInt(EmpiricalParser::OctIntContext *ctx) override {
    // skip leading "0o"
    return parse_number(ctx->getText(), 2, 8);
  }

  antlrcpp::Any visitHexInt(EmpiricalParser::HexIntContext *ctx) override {
    // skip leading "0x"
    return parse_number(ctx->getText(), 2, 16);
  }

  antlrcpp::Any visitBinInt(EmpiricalParser::BinIntContext *ctx) override {
    // skip leading "0b"
    return parse_number(ctx->getText(), 2, 2);
  }

  // parse a number taking suffix into account
  antlrcpp::Any parse_number(const std::string& str, int skip, int base,
                             bool is_int = true) {
    size_t pos = 0;
    std::string text = str.substr(skip);
    AST::expr_t literal =
      is_int ? AST::IntegerLiteral(std::stol(text, &pos, base))
             : AST::FloatingLiteral(std::stod(text, &pos));
    if (pos != text.size()) {
      std::string suffix = text.substr(pos);
      return AST::UserDefinedLiteral(literal, suffix);
    }
    return literal;
  }

  /* strings */

  antlrcpp::Any visitString(EmpiricalParser::StringContext *ctx) override {
    return parse_string(ctx->getText());
  }

  /* characters */

  antlrcpp::Any visitCharacter(EmpiricalParser::CharacterContext *ctx)
    override {
    return parse_string(ctx->getText());
  }

  /* miscellaneous */

  std::ostringstream parse_err_;

  bool interactive_;

 public:

  std::string get_errors() const {
    return parse_err_.str();
  }

  void set_interactive(bool b) {
    interactive_ = b;
  }
};


// parse text into an AST
AST::mod_t parse(const std::string& text, bool interactive,
                 bool dump_ast) {
  // prepare tokens
  ANTLRInputStream input(text);
  EmpiricalLexer lexer(&input);
  CommonTokenStream tokens(&lexer);
  EmpiricalParser parser(&tokens);

  // build parse tree
  tree::ParseTree* tree;
  try {
    std::shared_ptr<BailErrorStrategy> handler =
      std::make_shared<BailErrorStrategy>();
    parser.setErrorHandler(handler);
    tree = parser.input();
  }
  catch (ParseCancellationException& e) {
    // TODO need to state what went wrong, but ANTLR doesn't help much
    throw std::logic_error("Error: unable to parse\n");
  }

  // print parse tree
//  std::cout << tree->toStringTree(&parser) << std::endl;

  // build abstract syntax tree
  ParseVisitor parse_visitor;
  parse_visitor.set_interactive(interactive);
  AST::mod_t ast = parse_visitor.visit(tree);
  std::string msg = parse_visitor.get_errors();
  if (!msg.empty()) {
    throw std::logic_error(msg);
  }

  // print abstract syntax tree
  if (dump_ast) {
    std::cout << AST::to_string(ast) << std::endl;
  }

  return ast;
}

