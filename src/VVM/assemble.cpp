/*
 * Assemble -- produce VVM bytecode from VVM assembly
 *
 * Copyright (C) 2019 Empirical Software Solutions, LLC
 *
 * This program is distributed under the terms of the GNU Affero General
 * Public License with the Commons Clause.
 *
 */

#include <vector>
#include <string>
#include <sstream>

#include <antlr4-runtime.h>
#include <VVMAsmLexer.h>
#include <VVMAsmParser.h>
#include <VVMAsmVisitor.h>

#include <VVM/vvm.hpp>

using namespace vvmasm;
using namespace antlr4;

// build bytecode from parse tree
class AssembleVisitor : public VVMAsmVisitor {
  std::string found_label_;

  antlrcpp::Any visitProg(VVMAsmParser::ProgContext *ctx) override {
    VVM::Program program;
    VVM::Labeler<std::string> labeler;

    // instructions
    for (auto instr: ctx->instruction()) {
      if (instr->getText().back() == ':') {
        // label
        std::string lbl = instr->IDENTIFIER()->getSymbol()->getText();
        size_t loc = program.instructions.size();
        labeler.set_location(lbl, loc);
      } else {
        // regular instruction
        found_label_ = "";
        VVM::instructions_t code = visit(instr);
        program.instructions.insert(program.instructions.end(),
                                    code.begin(), code.end());
        if (!found_label_.empty()) {
          // assume only the last operand was the label
          size_t loc = program.instructions.size() - 1;
          labeler.append_dep(found_label_, loc);
        }
      }
    }
    program.instructions.push_back(size_t(VVM::opcodes::halt));

    // values (constant pool)
    for (auto v: ctx->defvalue()) {
      std::pair<VVM::operand_t, void*> constant = visit(v);
      program.constants.insert(constant);
    }

    // types
    for (auto t: ctx->deftype()) {
      std::pair<VVM::type_t,
                std::vector<VVM::named_type_t>> constant = visit(t);
      program.types.insert(constant);
    }

    // return full program
    labeler.resolve(program.instructions);
    return program;
  }

  antlrcpp::Any visitInstruction(VVMAsmParser::InstructionContext *ctx) override {
    VVM::instructions_t code;

    // encond opcode
    std::string o = ctx->IDENTIFIER()->getSymbol()->getText();
    size_t opcode = VVM::encode_opcode(o);
    code.push_back(opcode);

    // encode operands
    for (auto o: ctx->operand()) {
      VVM::operand_t operand = visit(o) ;
      code.push_back(operand);
    }

    // TODO verify arity for opcode
    return code;
  }

  antlrcpp::Any visitDefvalue(VVMAsmParser::DefvalueContext *ctx) override {
    std::string o = ctx->REGISTER()->getSymbol()->getText();
    VVM::operand_t operand = VVM::encode_operand(o);
    void* ptr = ctx->value() ? visit(ctx->value()) : visit(ctx->funcdef());
    return std::make_pair(operand, ptr);
  }

  antlrcpp::Any visitDeftype(VVMAsmParser::DeftypeContext *ctx) override {
    std::string t = ctx->UDT()->getSymbol()->getText();
    VVM::type_t typee = VVM::encode_type(t);
    std::vector<VVM::named_type_t> newtype = visit(ctx->newtype());
    return std::make_pair(typee, newtype);
  }

  antlrcpp::Any visitOperand(VVMAsmParser::OperandContext *ctx) override {
    VVM::operand_t op = VVM::encode_operand(0, VVM::OpMask::kImmediate);
    std::string text = ctx->getText();
    try {
      op = VVM::encode_operand(text);
    }
    catch (...) {
      // if the encoder fails, then assume this is a label
      found_label_ = text;
    }
    return op;
  }

  antlrcpp::Any visitIntValue(VVMAsmParser::IntValueContext *ctx) override {
    return VVM::encode_ptr(new int64_t(std::stol(ctx->getText())));
  }

  antlrcpp::Any visitFloatValue(VVMAsmParser::FloatValueContext *ctx) override {
    return VVM::encode_ptr(new double(std::stod(ctx->getText())));
  }

  antlrcpp::Any visitStrValue(VVMAsmParser::StrValueContext *ctx) override {
    std::string trimmed(ctx->getText(), 1, ctx->getText().size() - 2);
    return VVM::encode_ptr(new std::string(trimmed));
  }

  antlrcpp::Any visitFuncdef(VVMAsmParser::FuncdefContext *ctx) override {
    VVM::FunctionDef* fd = new VVM::FunctionDef;
    fd->name = ctx->name->getText();
    if (ctx->typelist()) {
      fd->args = visit(ctx->typelist()).as<decltype(fd->args)>();
    }
    fd->rettype = visit(ctx->type()).as<decltype(fd->rettype)>();
    VVM::Program program = visit(ctx->prog());
    if (!program.constants.empty()) {
      std::ostringstream oss;
      oss << "Cannot nest a constant pool in a function: " << fd->name;
      throw std::logic_error(oss.str());
    }
    if (!program.types.empty()) {
      std::ostringstream oss;
      oss << "Cannot nest type definitions in a function: " << fd->name;
      throw std::logic_error(oss.str());
    }
    fd->body = program.instructions;
    return VVM::encode_ptr(fd);
  }

  antlrcpp::Any visitNewtype(VVMAsmParser::NewtypeContext *ctx) override {
    return visit(ctx->typelist());
  }

  antlrcpp::Any visitTypelist(VVMAsmParser::TypelistContext *ctx) override {
    std::vector<VVM::named_type_t> types;
    for (auto n: ctx->ntype()) {
      VVM::named_type_t nt = visit(n);
      types.push_back(nt);
    }
    return types;
  }

  antlrcpp::Any visitNtype(VVMAsmParser::NtypeContext *ctx) override {
    VVM::named_type_t nt;
    nt.typee = visit(ctx->type());
    if (ctx->name) {
      std::string trimmed(ctx->name->getText(), 1,
                          ctx->name->getText().size() - 2);
      nt.name = trimmed;
    }
    return nt;
  }

  antlrcpp::Any visitType(VVMAsmParser::TypeContext *ctx) override {
    return VVM::encode_type(ctx->getText());
  }
};


namespace VVM {
// assemble text into a program
Program assemble(const std::string& text, bool dump_vvm) {
  // prepare tokens
  ANTLRInputStream input(text);
  VVMAsmLexer lexer(&input);
  CommonTokenStream tokens(&lexer);
  VVMAsmParser parser(&tokens);

  // build parse tree
  tree::ParseTree* tree = parser.prog();

  // build program
  AssembleVisitor assemble_visitor;
  Program program = assemble_visitor.visit(tree).as<Program>();

  // print VVM bytebode
  if (dump_vvm) {
    std::cout << to_string(program) << std::endl;
  }

  return program;
}
}  // namespace VVM

